#include "Services/Discord/DiscordOAuthService.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include "App/Config.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Http.h"
#include "Core/Json.h"
#include "Core/Process.h"
#include "Core/Text.h"
#include "Settings/ConfigFile.h"

namespace SpeedrunLauncher::Services::DiscordOAuth {

namespace {

constexpr const wchar_t* kClientId = Config::Discord::ClientId;
constexpr int kPort = Config::Discord::OAuthPort;
constexpr const char* kCacheFile = "discord_user.json";

// Browsers don't send URL fragments to servers: this page POSTs the fragment back to /token.
constexpr const char* kForwarderHtml = R"html(<!DOCTYPE html>
<html>
<body style="font-family:sans-serif;background:#09141E;color:#9944CC;
             text-align:center;padding:60px 40px;margin:0">
  <p id="s" style="font-size:14px">Connecting...</p>
  <script>
    var params = window.location.hash.substring(1) || 'error=no_token';
    fetch('/token', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: params
    })
    .then(function(r) { return r.text(); })
    .then(function(html) {
      document.open('text/html'); document.write(html); document.close();
    })
    .catch(function() {
      document.getElementById('s').textContent = 'Error. Please close and try again.';
    });
  </script>
</body>
</html>)html";

constexpr const char* kSuccessHtml = R"html(<!DOCTYPE html>
<html>
<body style="font-family:sans-serif;background:#09141E;color:#00CC88;
             text-align:center;padding:60px 40px;margin:0">
  <div style="font-size:52px;margin-bottom:16px">&#10003;</div>
  <h2 style="margin:0 0 10px;font-size:20px">Connected!</h2>
  <p style="color:#4A7A5A;margin:0;font-size:14px">You can close this window.</p>
</body></html>)html";

constexpr const char* kErrorHtml = R"html(<!DOCTYPE html>
<html>
<body style="font-family:sans-serif;background:#09141E;color:#CC4422;
             text-align:center;padding:60px 40px;margin:0">
  <div style="font-size:52px;margin-bottom:16px">&#10005;</div>
  <h2 style="margin:0 0 10px;font-size:20px">Not connected</h2>
  <p style="color:#7A3A22;margin:0;font-size:14px">You can close this window and try again.</p>
</body></html>)html";

std::string UrlDecode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') out += ' ';
        else if (s[i] == '%' && i + 2 < s.size()) {
            out += static_cast<char>(std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16));
            i += 2;
        } else out += s[i];
    }
    return out;
}

std::string Param(const std::string& body, const std::string& key) {
    for (const auto& pair : Text::Split(body, '&')) {
        const size_t eq = pair.find('=');
        if (eq == std::string::npos) continue;
        if (UrlDecode(pair.substr(0, eq)) == key) return UrlDecode(pair.substr(eq + 1));
    }
    return {};
}

// Sends the browser to `url` (the forwarder page writes this document over itself)
std::string RedirectHtml(const std::wstring& url) {
    std::string safe;
    for (const char c : Text::Narrow(url)) {
        // Only URL characters (the URL is built by the launcher, this just keeps the HTML well formed)
        if (c == '"' || c == '<' || c == '>' || c == '\'' || c == '\\' || static_cast<unsigned char>(c) < 0x20) continue;
        safe += c;
    }
    return "<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"0;url=" + safe +
           "\"></head><body style=\"background:#0C0805\"><script>location.replace(\"" + safe + "\");</script></body></html>";
}

void Respond(SOCKET client, int status, const char* html, bool cors) {
    const std::string body = html ? html : "";
    std::string response = "HTTP/1.1 " + std::to_string(status) + (status == 200 ? " OK" : " Not Found") + "\r\n";
    response += "Content-Type: text/html; charset=utf-8\r\n";
    response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    if (cors) response += "Access-Control-Allow-Origin: *\r\n";
    response += "Connection: close\r\n\r\n" + body;
    send(client, response.data(), static_cast<int>(response.size()), 0);
}

// Reads one HTTP request (headers + Content-Length body).
bool ReadRequest(SOCKET client, std::string& method, std::string& path, std::string& body) {
    std::string data;
    char buffer[4096];
    size_t headerEnd = std::string::npos;
    while (headerEnd == std::string::npos) {
        const int n = recv(client, buffer, sizeof(buffer), 0);
        if (n <= 0) return false;
        data.append(buffer, static_cast<size_t>(n));
        headerEnd = data.find("\r\n\r\n");
        if (data.size() > 1 << 20) return false;
    }
    const std::string head = data.substr(0, headerEnd);
    const auto lines = Text::Split(head, '\n');
    if (lines.empty()) return false;
    const auto first = Text::Split(Text::Trim(lines[0]), ' ');
    if (first.size() < 2) return false;
    method = first[0];
    path = first[1];
    const size_t q = path.find('?');
    if (q != std::string::npos) path = path.substr(0, q);
    size_t contentLength = 0;
    for (const auto& line : lines) {
        const std::string l = Text::Lower(Text::Trim(line));
        if (Text::StartsWith(l, "content-length:")) contentLength = static_cast<size_t>(std::atoll(l.substr(15).c_str()));
    }
    body = data.substr(headerEnd + 4);
    while (body.size() < contentLength) {
        const int n = recv(client, buffer, sizeof(buffer), 0);
        if (n <= 0) break;
        body.append(buffer, static_cast<size_t>(n));
    }
    return true;
}

// `avatarUrl` (optional): the user's picture on Discord's CDN, empty when they have none
std::optional<User> FetchUser(const std::string& token, std::wstring* avatarUrl = nullptr) {
    const Http::Response r = Http::Get(Config::Discord::ApiUserUrl, { { L"Authorization", L"Bearer " + Text::Widen(token) } }, 10000);
    if (!r.Ok()) return std::nullopt;
    const Json::Value doc = Json::Parse(std::string_view(r.body));
    User user{ doc["id"].WStr(), doc["username"].WStr() };
    if (user.id.empty()) return std::nullopt;
    if (avatarUrl && doc["avatar"].IsString())
        *avatarUrl = L"https://cdn.discordapp.com/avatars/" + user.id + L"/" + doc["avatar"].WStr() + L".png?size=128";
    return user;
}

} // namespace

bool Authorize(const Async::Cancel& cancel, const std::function<TokenResult(const std::string& accessToken)>& onToken, const std::wstring& deniedUrl) {
    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);
    // "localhost" may resolve to 127.0.0.1 or ::1 depending on the browser: listen on both
    std::vector<SOCKET> listeners;
    {
        SOCKET v4 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(kPort);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        if (v4 != INVALID_SOCKET && bind(v4, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0 && listen(v4, 4) == 0)
            listeners.push_back(v4);
        else if (v4 != INVALID_SOCKET) closesocket(v4);
        SOCKET v6 = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in6 addr6{};
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons(kPort);
        addr6.sin6_addr = in6addr_loopback;
        if (v6 != INVALID_SOCKET && bind(v6, reinterpret_cast<sockaddr*>(&addr6), sizeof(addr6)) == 0 && listen(v6, 4) == 0)
            listeners.push_back(v6);
        else if (v6 != INVALID_SOCKET) closesocket(v6);
    }
    const auto closeAll = [&] {
        for (SOCKET s : listeners) closesocket(s);
    };
    if (listeners.empty()) return false;
    const std::wstring url = std::wstring(Config::Discord::OAuthAuthorizeUrl) + L"?client_id=" + kClientId +
                             L"&redirect_uri=" + Text::UrlEncode(std::wstring(Config::Discord::OAuthRedirectUri)) +
                             L"&response_type=token&scope=identify";
    if (!Process::OpenUrl(url)) {
        closeAll();
        return false;
    }

    bool received = false;
    const ULONGLONG deadline = GetTickCount64() + 5 * 60 * 1000;
    while (!cancel.Requested() && GetTickCount64() < deadline) {
        fd_set set;
        FD_ZERO(&set);
        for (SOCKET s : listeners) FD_SET(s, &set);
        timeval tv{ 0, 250000 };
        if (select(0, &set, nullptr, nullptr, &tv) <= 0) continue;
        SOCKET ready = INVALID_SOCKET;
        for (SOCKET s : listeners)
            if (FD_ISSET(s, &set)) ready = s;
        if (ready == INVALID_SOCKET) continue;
        SOCKET client = accept(ready, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;
        DWORD timeout = 5000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        std::string method, path, body;
        bool done = false;
        if (ReadRequest(client, method, path, body)) {
            if (path == "/callback") {
                Respond(client, 200, kForwarderHtml, false);
            } else if (path == "/token" && method == "POST") {
                const std::string token = Param(body, "access_token");
                received = !token.empty();
                TokenResult result;
                if (received) result = onToken(token);
                else result.redirectUrl = deniedUrl;
                const std::string redirect = result.redirectUrl.empty() ? std::string() : RedirectHtml(result.redirectUrl);
                Respond(client, 200, !redirect.empty() ? redirect.c_str() : result.ok ? kSuccessHtml : kErrorHtml, true);
                done = true;
            } else {
                Respond(client, 404, "", false);
            }
        }
        shutdown(client, SD_BOTH);
        closesocket(client);
        if (done) break;
    }
    closeAll();
    return received;
}

std::optional<User> Authenticate(const Async::Cancel& cancel) {
    // The browser ends on the website's Discord page, in its "connected" mode (bug reports, hand mods)
    const std::wstring page = std::wstring(Config::Links::DiscordLinkedPage) + L"?mode=connect";
    std::optional<User> result;
    Authorize(cancel, [&](const std::string& token) {
        std::wstring avatar;
        result = FetchUser(token, &avatar);
        if (!result) return TokenResult{ false, page + L"&status=error&reason=unreachable" };
        std::wstring url = page + L"&status=ok&name=" + Text::UrlEncode(result->username);
        if (!avatar.empty()) url += L"&avatar=" + Text::UrlEncode(avatar);
        return TokenResult{ true, url };
    }, page + L"&status=error&reason=denied");
    return result;
}

std::optional<User> LoadCached() {
    bool exists = false;
    const Json::Value doc = Config::ReadJson(Text::Widen(kCacheFile), &exists);
    if (!exists) return std::nullopt;
    User user{ doc["id"].WStr(), doc["username"].WStr() };
    if (user.id.empty()) return std::nullopt;
    return user;
}

void SaveCached(const User& user) {
    Json::Value doc = Json::Value::MakeObject();
    doc.Set("id", user.id);
    doc.Set("username", user.username);
    Config::WriteJson(Text::Widen(kCacheFile), doc);
}

void ClearCached() { Files::Delete(Paths::DataFile(Text::Widen(kCacheFile))); }

} // namespace SpeedrunLauncher::Services::DiscordOAuth
