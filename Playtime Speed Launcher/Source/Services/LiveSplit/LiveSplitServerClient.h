#pragma once
#include <optional>
#include <string>

namespace SpeedrunLauncher::Services {

// LiveSplit Server component (Control → Start Server, default port 16834). Short timeouts so a
// stalled LiveSplit never blocks. Use from one worker thread at a time.
class LiveSplitServerClient {
public:
    static constexpr int DefaultPort = 16834;

    ~LiveSplitServerClient() { Disconnect(); }
    bool IsConnected() const { return sock != ~0ull; }
    bool TryConnect(int port = DefaultPort);
    void Disconnect();

    std::optional<std::string> TimerPhase() { return Send("getcurrenttimerphase"); }   // NotRunning | Running | Paused | Ended
    std::optional<std::string> CurrentTime() { return Send("getcurrenttime"); }        // "0:01:23.4500000"

private:
    std::optional<std::string> Send(const std::string& command);
    unsigned long long sock = ~0ull;
    std::string buffer;
};

// "1:23.45" / "1:02:03.45" from LiveSplit's TimeSpan text.
std::wstring FormatLiveSplitTime(const std::string& raw);

} // namespace SpeedrunLauncher::Services
