#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace SpeedrunLauncher::Services {

// Tracks one game's FPS through a hidden, elevated copy of the launcher (--fps-helper) that runs
// the ETW trace and streams readings back over a private named pipe. The launcher itself is never
// elevated; the helper only lives while a tracked game runs.
class GameFpsService {
public:
    static constexpr const wchar_t* HelperArg = L"--fps-helper";

    ~GameFpsService() { Stop(); }

    // `onFps` runs on the UI thread.
    void SetCallback(std::function<void(double)> onFps) { callback = std::move(onFps); }
    // PLAY pressed: ask for elevation now, while the launcher is in the foreground (Windows only
    // flashes a taskbar button for background apps, hidden behind the game). The helper then waits
    // for one of `processNames` (shipping renderer first) and attaches to it by itself.
    void Prepare(const std::vector<std::wstring>& processNames);
    // The game watcher found the game: reuses a prepared helper, otherwise asks for elevation now.
    void Start(uint32_t gamePid);
    void Stop();
    void ClearDeclined() { declined = false; }   // the game closed: ask again next time
    bool IsRunning() const;
    uint32_t ActivePid() const { return activePid; }

    // The helper process main (elevated): traces `pid` and writes one FPS value per line to the pipe.
    // With pid 0 it first waits for a process named like one of `processNames`.
    static int RunHelper(uint32_t pid, const std::wstring& pipeName, const std::vector<std::wstring>& processNames);

private:
    void Launch(uint32_t gamePid, const std::vector<std::wstring>& processNames);

    std::function<void(double)> callback;
    uint32_t activePid = 0;
    bool waitingForGame = false;   // helper started from PLAY, still looking for the game
    bool declined = false;         // UAC declined: don't keep asking while this game runs
    void* helperProcess = nullptr;   // HANDLE
    void* pipe = nullptr;            // HANDLE (server end)
    std::shared_ptr<std::atomic_bool> alive;
    std::thread reader;
};

} // namespace SpeedrunLauncher::Services
