#include "Features/LiveSplitPoller.h"
#include <windows.h>
#include <string>
#include "App/Launcher.h"
#include "Core/Async.h"
#include "Core/Text.h"
#include "Services/LiveSplit/LiveSplitServerClient.h"

namespace SpeedrunLauncher::Features {

void LiveSplitPoller::Start() {
    if (running) return;
    running = std::make_shared<std::atomic_bool>(true);
    auto alive = running;
    Async::Run([alive] {
        Services::LiveSplitServerClient client;
        while (*alive) {
            for (int i = 0; i < 10 && *alive; ++i) Sleep(100);
            if (!*alive) break;
            std::wstring line = L"LiveSplit not connected";
            if (client.IsConnected() || client.TryConnect()) {
                if (auto phase = client.TimerPhase()) {
                    if (*phase == "Running" || *phase == "Paused" || *phase == "Ended") {
                        if (auto raw = client.CurrentTime()) line = L"\x23F1 " + Services::FormatLiveSplitTime(*raw);
                    } else {
                        line = L"LiveSplit: Ready";   // open, no run started yet
                    }
                }
            }
            Async::Post([alive, line] {
                if (*alive && App::L().discord) App::L().discord->UpdateLiveSplitLine(line);
            });
        }
    });
}

void LiveSplitPoller::Stop() {
    if (running) *running = false;
    running.reset();
}

} // namespace SpeedrunLauncher::Features
