#pragma once
#include <atomic>
#include <memory>

namespace SpeedrunLauncher::Features {

// Asks the LiveSplit Server for the timer once a second and hands the line ("⏱ 1:23.45",
// "LiveSplit: Ready", "LiveSplit not connected") to Discord Rich Presence.
class LiveSplitPoller {
public:
    ~LiveSplitPoller() { Stop(); }
    void Start();
    void Stop();

private:
    std::shared_ptr<std::atomic_bool> running;
};

} // namespace SpeedrunLauncher::Features
