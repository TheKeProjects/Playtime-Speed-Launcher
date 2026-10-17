#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

namespace SpeedrunLauncher::Services {

// Real-time ETW session on the DXGI (D3D10/11/12) and Direct3D 9 providers, reporting the
// timestamp (ms) of every Present made by one process — the technique PresentMon / RTSS use.
// Needs administrator rights, so it only runs inside the elevated --fps-helper process.
class DxgiPresentWatcher {
public:
    DxgiPresentWatcher(uint32_t targetPid, std::function<void(double)> onFrame);
    ~DxgiPresentWatcher();
    bool Start();
    void Stop();

    // ETW callback entry (static trampoline needs it public)
    void OnEvent(const void* record);

private:
    uint32_t pid;
    std::function<void(double)> onFrame;
    std::wstring sessionName;
    uint64_t session = 0;     // TRACEHANDLE
    uint64_t consumer = 0;    // TRACEHANDLE
    std::thread processThread, flushThread;
    std::atomic_bool running{ false };
    double qpcToMs = 0.0;
    int64_t startQpc = 0;
};

} // namespace SpeedrunLauncher::Services
