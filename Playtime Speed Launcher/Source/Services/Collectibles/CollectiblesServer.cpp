#include "Services/Collectibles/CollectiblesServer.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include "Core/Files.h"
#include "Core/Json.h"
#include "Core/Text.h"
#include "Services/App/ResourceExtractor.h"
#include "Services/Collectibles/CollectiblesMemory.h"
#include "Services/Collectibles/LiveInventory.h"
#include "Services/Collectibles/SaveGameService.h"
#include "Settings/ConfigFile.h"
#include "Version.h"

namespace SpeedrunLauncher::Services::Collectibles::Server {

namespace {

constexpr int kDefaultPort = 8745;   // same as the standalone Poppy Collectibles Overlay
constexpr int kPortAttempts = 10;
constexpr const wchar_t* kPortFile = L"collectibles_port.cfg";
constexpr const wchar_t* kEnabledFile = L"collectibles_server.cfg";

std::string JsonString(std::string_view utf8) { return "\"" + Json::Escape(utf8) + "\""; }

// A file under Assets\Collectibles, embedded in the exe
std::string Asset(const std::wstring& relative) {
#ifdef _DEBUG
    // bin\Debug\PlaytimeSpeedLauncher.exe -> Assets\Collectibles\...: edit the page without rebuilding
    const std::wstring dev = Files::Combine(Files::DirectoryName(Files::ExecutablePath()), L"..\\..\\Assets\\Collectibles", relative);
    if (auto bytes = Files::ReadBytes(dev)) return *bytes;
#endif
    return ResourceExtractor::ReadEmbedded(L"Assets", L"Collectibles\\" + relative).value_or(std::string());
}

// "/icons/VHS.png" -> Assets\Collectibles\Icons\VHS.png; "/chapters/Chapter4.jpg" -> Chapters\Chapter4.jpg.
// Empty for anything else; type gets the MIME type.
std::string Image(const std::string& path, std::string& type) {
    struct Folder { std::string_view prefix, extension, mime; const wchar_t* dir; };
    static constexpr Folder kFolders[] = {
        { "/icons/", ".png", "image/png", L"Icons\\" },
        { "/chapters/", ".jpg", "image/jpeg", L"Chapters\\" },
    };
    for (const Folder& f : kFolders) {
        if (path.size() <= f.prefix.size() + f.extension.size() || path.compare(0, f.prefix.size(), f.prefix) != 0 ||
            path.compare(path.size() - f.extension.size(), f.extension.size(), f.extension) != 0)
            continue;
        const std::string name = path.substr(f.prefix.size(), path.size() - f.prefix.size() - f.extension.size());
        for (const char c : name)
            if (!isalnum(static_cast<unsigned char>(c))) return {};
        type = f.mime;
        return Asset(f.dir + Text::Widen(name) + Text::Widen(f.extension));
    }
    return {};
}

// Value of a request header (case-insensitive name), empty when missing
std::string Header(const std::string& request, std::string_view name) {
    size_t at = request.find("\r\n");
    while (at != std::string::npos) {
        const size_t start = at + 2;
        const size_t end = request.find("\r\n", start);
        if (end == std::string::npos || end == start) break;
        const std::string_view line(request.data() + start, end - start);
        const size_t colon = line.find(':');
        if (colon == name.size() && _strnicmp(line.data(), name.data(), name.size()) == 0) {
            std::string_view value = line.substr(colon + 1);
            while (!value.empty() && value.front() == ' ') value.remove_prefix(1);
            return std::string(value);
        }
        at = end;
    }
    return {};
}

// Value of ?key= in a query string, empty when missing
std::string QueryValue(const std::string& query, std::string_view key) {
    size_t at = 0;
    while (at <= query.size()) {
        const size_t end = std::min(query.find('&', at), query.size());
        const std::string_view part(query.data() + at, end - at);
        if (part.size() > key.size() && part.compare(0, key.size(), key) == 0 && part[key.size()] == '=')
            return std::string(part.substr(key.size() + 1));
        at = end + 1;
    }
    return {};
}

void SendAll(SOCKET s, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        const int n = send(s, data.data() + sent, static_cast<int>(std::min<size_t>(data.size() - sent, 1 << 20)), 0);
        if (n <= 0) return;
        sent += static_cast<size_t>(n);
    }
}

void Respond(SOCKET s, std::string_view status, std::string_view type, const std::string& body, const std::string& extraHeaders = {},
             bool head = false) {
    std::string out = "HTTP/1.1 ";
    out += status;
    out += "\r\nConnection: close\r\nCache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\n"
           "Access-Control-Expose-Headers: ETag, X-Chapter, X-Save-File, X-Save-State\r\n";
    if (!type.empty()) {
        out += "Content-Type: ";
        out += type;
        out += "\r\n";
    }
    out += "Content-Length: " + std::to_string(body.size()) + "\r\n" + extraHeaders + "\r\n";
    if (!head) out += body;
    SendAll(s, out);
}

std::string StatusJson(int port) {
    std::string json = "{\"app\":" + JsonString(APP_PRODUCT) + ",\"version\":" + JsonString(APP_VERSION_STRING) +
                       ",\"port\":" + std::to_string(port) + ",\"auto\":" + std::to_string(SaveGames::AutoChapter()) + ",\"chapters\":[";
    bool first = true;
    for (const int ch : SaveGames::kChapters) {
        const auto info = SaveGames::Info(ch);
        if (!first) json += ",";
        first = false;
        json += "{\"chapter\":" + std::to_string(ch) + ",\"folder\":" + JsonString(Text::Narrow(info.folder)) +
                ",\"exists\":" + (info.folderExists ? "true" : "false") + ",\"running\":" + (info.running ? "true" : "false") +
                ",\"saves\":" + std::to_string(info.saves) + ",\"newest\":" + JsonString(Text::Narrow(info.newest)) +
                ",\"waiting\":" + (info.waitingFirstSave ? "true" : "false") + "}";
    }
    return json + "]}";
}

class HttpServer {
public:
    ~HttpServer() { Stop(); }

    // Tries firstPort, then the next few ports if it is taken. Returns the port, or 0 on failure.
    int Start(int firstPort) {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 0;
        for (int p = firstPort; p < firstPort + kPortAttempts && p < 65536; ++p) {
            SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (s == INVALID_SOCKET) break;
            // Another program on the same port must not be able to share it
            BOOL exclusive = TRUE;
            setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(static_cast<u_short>(p));
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
            if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0 && listen(s, SOMAXCONN) == 0) {
                listener = s;
                port = p;
                running = true;
                thread = std::thread([this] { Run(); });
                return port;
            }
            closesocket(s);
        }
        WSACleanup();
        return 0;
    }

    void Stop() {
        if (!running.exchange(false)) return;
        closesocket(listener);   // wakes accept()
        if (thread.joinable()) thread.join();
        listener = INVALID_SOCKET;
        port = 0;
        WSACleanup();
    }

    int Port() const { return running ? port : 0; }
    void FireTest() { ++testEvents; }

private:
    void Run() {
        while (running) {
            SOCKET client = accept(listener, nullptr, nullptr);
            if (client == INVALID_SOCKET) {
                if (!running) break;
                continue;
            }
            // One at a time: requests are tiny (OBS and the 100% window polling every second or so)
            DWORD timeout = 3000;
            setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            Handle(client);
            shutdown(client, SD_SEND);
            closesocket(client);
        }
    }

    void Handle(SOCKET s) {
        std::string request;
        char buffer[4096];
        while (request.find("\r\n\r\n") == std::string::npos && request.size() < 32 * 1024) {
            const int n = recv(s, buffer, sizeof(buffer), 0);
            if (n <= 0) return;
            request.append(buffer, static_cast<size_t>(n));
        }

        // "GET /path?query HTTP/1.1"
        const size_t sp1 = request.find(' ');
        const size_t sp2 = sp1 == std::string::npos ? std::string::npos : request.find(' ', sp1 + 1);
        if (sp2 == std::string::npos) return Respond(s, "400 Bad Request", "text/plain", "Bad request");
        const std::string method = request.substr(0, sp1);
        std::string path = request.substr(sp1 + 1, sp2 - sp1 - 1);
        std::string query;
        if (const size_t q = path.find_first_of("?#"); q != std::string::npos) {
            if (path[q] == '?') query = path.substr(q + 1);
            path.resize(q);
        }

        if (method == "OPTIONS")
            return Respond(s, "204 No Content", "", "", "Access-Control-Allow-Methods: GET, HEAD\r\nAccess-Control-Allow-Headers: If-None-Match\r\n");
        const bool head = method == "HEAD";
        if (method != "GET" && !head) return Respond(s, "405 Method Not Allowed", "text/plain", "Method not allowed");

        if (path == "/" || path == "/overlay" || path == "/index.html" || path == "/overlay.html")
            return Respond(s, "200 OK", "text/html; charset=utf-8", Asset(L"overlay.html"), {}, head);
        if (path == "/api/test") {
            if (query.find("fire") != std::string::npos) ++testEvents;
            return Respond(s, "200 OK", "application/json; charset=utf-8", "{\"n\":" + std::to_string(testEvents.load()) + "}", {}, head);
        }
        if (path == "/api/status") return Respond(s, "200 OK", "application/json; charset=utf-8", StatusJson(port), {}, head);
        if (path == "/api/save") {
            const std::string ch = QueryValue(query, "ch");
            const int chapter = ch.size() == 1 && ch[0] >= '1' && ch[0] <= '5' ? ch[0] - '0' : SaveGames::AutoChapter();
            const std::string chapterHeader = "X-Chapter: " + std::to_string(chapter) + "\r\n";
            // The launcher's memory of the run, updated from the running game's inventory at every pickup
            // (the save files aren't read: a reload or going back takes collectibles away there)
            const Memory::Result memory = Memory::Read(chapter);
            if (!memory.any && !memory.live) return Respond(s, "204 No Content", "", "", chapterHeader);
            uint64_t hash = 1469598103934665603ull;   // FNV-1a of the contents
            for (const char c : memory.text) hash = (hash ^ static_cast<uint8_t>(c)) * 1099511628211ull;
            const char* kind = memory.live ? "live" : "memory";
            char etag[64];
            sprintf_s(etag, "\"%s-%d-%llx\"", kind, chapter, static_cast<unsigned long long>(hash));
            const std::string headers = chapterHeader + "ETag: " + etag + "\r\nX-Save-File: " + kind + "\r\nX-Save-State: " + kind + "\r\n";
            if (Header(request, "If-None-Match") == etag) return Respond(s, "304 Not Modified", "", "", headers);
            return Respond(s, "200 OK", "application/octet-stream", memory.text, headers, head);
        }
        std::string type;
        if (const std::string image = Image(path, type); !image.empty()) return Respond(s, "200 OK", type, image, {}, head);
        if (path == "/favicon.ico") return Respond(s, "204 No Content", "", "");
        Respond(s, "404 Not Found", "text/plain", "Not found");
    }

    SOCKET listener = INVALID_SOCKET;
    int port = 0;
    std::atomic<bool> running{ false };
    std::atomic<int> testEvents{ 0 };
    std::thread thread;
};

std::mutex lock;
HttpServer server;

int SavedPort() {
    const auto text = Config::ReadText(kPortFile);
    const auto port = text ? Text::ToInt(*text) : std::nullopt;
    return port && *port > 0 && *port < 65536 ? static_cast<int>(*port) : kDefaultPort;
}

} // namespace

int Ensure() {
    std::lock_guard guard(lock);
    if (const int running = server.Port()) return running;
    const int wanted = SavedPort();
    const int port = server.Start(wanted);
    if (port) {
        // The OBS link carries the port: keep it for the next runs
        if (port != wanted) Config::WriteText(kPortFile, std::to_wstring(port));
        Config::WriteFlag(kEnabledFile, true);
    }
    return port;
}

int Port() {
    std::lock_guard guard(lock);
    return server.Port();
}

void StartIfEnabled() {
    if (Config::ReadFlag(kEnabledFile, false)) Ensure();
}

void Stop() {
    std::lock_guard guard(lock);
    server.Stop();
}

void FireTest() { server.FireTest(); }

std::wstring Link(const std::string& query) {
    const int port = Port();
    return port ? L"http://127.0.0.1:" + std::to_wstring(port) + L"/overlay?" + Text::Widen(query) : std::wstring();
}

} // namespace SpeedrunLauncher::Services::Collectibles::Server
