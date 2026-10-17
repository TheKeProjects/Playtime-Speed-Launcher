#pragma once
#include <functional>

namespace SpeedrunLauncher::Services::OnlineUsers {

// Sends a heartbeat every minute and reports how many installs checked in recently.
// `onCount` runs on the UI thread.
void Start(std::function<void(int)> onCount);
void Stop();

} // namespace SpeedrunLauncher::Services::OnlineUsers
