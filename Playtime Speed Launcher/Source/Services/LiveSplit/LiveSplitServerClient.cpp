#include "Services/LiveSplit/LiveSplitServerClient.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include "Core/Text.h"

#pragma comment(lib, "ws2_32.lib")

namespace SpeedrunLauncher::Services {

namespace {

constexpr DWORD kTimeoutMs = 500;

void EnsureWinsock() {
    static bool started = [] {
        WSADATA data{};
        return WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }();
    (void)started;
}

} // namespace

bool LiveSplitServerClient::TryConnect(int port) {
    Disconnect();
    EnsureWinsock();
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;
    DWORD timeout = kTimeoutMs;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closesocket(s);
        return false;
    }
    sock = static_cast<unsigned long long>(s);
    buffer.clear();
    return true;
}

void LiveSplitServerClient::Disconnect() {
    if (sock != ~0ull) closesocket(static_cast<SOCKET>(sock));
    sock = ~0ull;
    buffer.clear();
}

std::optional<std::string> LiveSplitServerClient::Send(const std::string& command) {
    if (!IsConnected()) return std::nullopt;
    const std::string line = command + "\r\n";
    if (send(static_cast<SOCKET>(sock), line.data(), static_cast<int>(line.size()), 0) != static_cast<int>(line.size())) {
        Disconnect();
        return std::nullopt;
    }
    // Read one line
    while (true) {
        const size_t newline = buffer.find('\n');
        if (newline != std::string::npos) {
            std::string result = buffer.substr(0, newline);
            buffer.erase(0, newline + 1);
            if (!result.empty() && result.back() == '\r') result.pop_back();
            return result;
        }
        char chunk[512];
        const int n = recv(static_cast<SOCKET>(sock), chunk, sizeof(chunk), 0);
        if (n <= 0) {
            Disconnect();
            return std::nullopt;
        }
        buffer.append(chunk, static_cast<size_t>(n));
    }
}

std::wstring FormatLiveSplitTime(const std::string& rawText) {
    const std::string raw = Text::Trim(rawText);
    // [-][d.]hh:mm:ss[.fffffff]
    int days = 0, hours = 0, minutes = 0, seconds = 0;
    double fraction = 0.0;
    std::string body = raw;
    bool negative = false;
    if (!body.empty() && body[0] == '-') {
        negative = true;
        body.erase(0, 1);
    }
    const auto parts = Text::Split(body, ':');
    if (parts.size() != 3) return Text::Widen(raw);
    const std::string& h = parts[0];
    const size_t dot = h.find('.');
    if (dot != std::string::npos) {
        days = std::atoi(h.substr(0, dot).c_str());
        hours = std::atoi(h.substr(dot + 1).c_str());
    } else {
        hours = std::atoi(h.c_str());
    }
    minutes = std::atoi(parts[1].c_str());
    const std::string& s = parts[2];
    const size_t sdot = s.find('.');
    seconds = std::atoi(s.substr(0, sdot).c_str());
    if (sdot != std::string::npos) fraction = std::atof(("0" + s.substr(sdot)).c_str());
    const int totalHours = days * 24 + hours;
    const int centis = static_cast<int>(fraction * 1000.0) / 10;
    wchar_t out[64];
    if (totalHours > 0) swprintf_s(out, L"%s%d:%02d:%02d.%02d", negative ? L"-" : L"", totalHours, minutes, seconds, centis);
    else swprintf_s(out, L"%s%d:%02d.%02d", negative ? L"-" : L"", minutes, seconds, centis);
    return out;
}

} // namespace SpeedrunLauncher::Services
