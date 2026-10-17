#include "Services/Community/OnlineUsersService.h"
#include <windows.h>
#include <atomic>
#include <memory>
#include "Core/Async.h"
#include "Services/Community/Supabase.h"
#include "Settings/HeartbeatSettings.h"

namespace SpeedrunLauncher::Services::OnlineUsers {

namespace {

std::shared_ptr<std::atomic_bool> running;

} // namespace

void Start(std::function<void(int)> onCount) {
    if (running) return;
    running = std::make_shared<std::atomic_bool>(true);
    auto alive = running;
    const std::wstring id = Settings::Heartbeat::Current().installId;
    Async::Run([alive, id, onCount] {
        while (*alive) {
            Json::Value heartbeat = Json::Value::MakeObject();
            heartbeat.Set("p_id", id);
            Supabase::Rpc(L"heartbeat", heartbeat);

            const Http::Response response = Supabase::Rpc(L"get_online_count", Json::Value::MakeObject());
            if (response.Ok()) {
                bool ok = false;
                const Json::Value count = Json::Parse(std::string_view(response.body), &ok);
                if (ok && count.IsNumber()) {
                    const int n = static_cast<int>(count.Int());
                    Async::Post([alive, onCount, n] {
                        if (*alive && onCount) onCount(n);
                    });
                }
            }
            // Wait a minute, waking up early when stopped
            for (int i = 0; i < 600 && *alive; ++i) Sleep(100);
        }
    });
}

void Stop() {
    if (running) *running = false;
    running.reset();
}

} // namespace SpeedrunLauncher::Services::OnlineUsers
