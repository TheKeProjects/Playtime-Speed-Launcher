#pragma once
#include <deque>
#include <mutex>

namespace SpeedrunLauncher::Services {

// Rolling window of frame timestamps (ms). FPS comes from the span between the oldest and newest
// frame in the window, which avoids the jitter of counting frames per fixed bucket.
class FrameTimeWindow {
public:
    explicit FrameTimeWindow(double windowMs) : window(windowMs) {}

    void AddFrame(double timestampMs) {
        std::lock_guard lock(mutex);
        stamps.push_back(timestampMs);
        newest = timestampMs;
        while (!stamps.empty() && newest - stamps.front() > window) stamps.pop_front();
    }

    double CurrentFps() const {
        std::lock_guard lock(mutex);
        if (stamps.size() < 2) return 0.0;
        const double span = (newest - stamps.front()) / 1000.0;
        return span > 0.0 ? static_cast<double>(stamps.size() - 1) / span : 0.0;
    }

private:
    double window;
    mutable std::mutex mutex;
    std::deque<double> stamps;
    double newest = 0.0;
};

} // namespace SpeedrunLauncher::Services
