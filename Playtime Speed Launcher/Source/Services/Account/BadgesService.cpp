#include "Services/Account/BadgesService.h"
#include "Core/Async.h"
#include "Core/Json.h"
#include "Core/Text.h"
#include "Services/Account/AccountService.h"
#include "UI/Window.h"

namespace SpeedrunLauncher::Services::Account {

namespace {

constexpr double kKeepSeconds = 300.0;   // fetched again after 5 minutes
constexpr size_t kBatch = 200;           // the server's limit per call

void Redraw() {
    for (UI::Window* window : UI::Window::All()) window->Invalidate();
}

} // namespace

BadgesService& BadgesService::Get() {
    static BadgesService instance;
    return instance;
}

void BadgesService::AccountChanged() {
    ++generation;
    cache.clear();
    pending.clear();
    inflight.clear();
    flushQueued = false;
}

const std::vector<Badge>* BadgesService::Of(const std::wstring& userId) {
    if (userId.empty()) return nullptr;
    Entry& e = cache[userId];
    const bool stale = !e.loaded || Async::Now() - e.fetchedAt > kKeepSeconds;
    if (stale && !pending.count(userId) && !inflight.count(userId) && AccountService::Get().SignedIn()) {
        pending.insert(userId);
        // Everything asked for during this frame goes in one call
        if (!flushQueued) {
            flushQueued = true;
            const unsigned gen = generation;
            Async::Post([this, gen] {
                if (gen != generation) return;
                flushQueued = false;
                Flush();
            });
        }
    }
    return e.loaded ? &e.badges : nullptr;
}

const Badge* BadgesService::Shown(const std::wstring& userId, const std::string* featured) {
    const auto* list = Of(userId);
    if (!list || list->empty()) return nullptr;
    std::string pick;
    if (featured) pick = *featured;
    else if (userId == AccountService::Get().UserId()) pick = AccountService::Get().Me().featuredBadge;   // the freshest copy
    else pick = cache[userId].featured;
    if (!pick.empty())
        for (const auto& b : *list)
            if (b.Id() == pick) return &b;
    return &list->front();
}

void BadgesService::Forget(const std::wstring& userId) {
    auto it = cache.find(userId);
    if (it != cache.end()) it->second.fetchedAt = -1e9;
}

void BadgesService::Flush() {
    if (pending.empty()) return;
    std::vector<std::wstring> batch;
    for (auto it = pending.begin(); it != pending.end() && batch.size() < kBatch;) {
        batch.push_back(*it);
        inflight.insert(*it);
        it = pending.erase(it);
    }
    const unsigned gen = generation;
    Async::Run(
        [batch] {
            Json::Value ids = Json::Value::MakeArray();
            for (const auto& id : batch) ids.Push(id);
            Json::Value body = Json::Value::MakeObject();
            body.Set("p_users", ids);
            return AccountService::Get().Rpc(L"profile_badges", body);
        },
        [this, gen, batch](Http::Response response) {
            if (gen != generation) return;
            bool ok = false;
            const Json::Value json = response.Ok() ? Json::Parse(std::string_view(response.body), &ok) : Json::Value();
            const double now = Async::Now();
            for (const auto& id : batch) {
                inflight.erase(id);
                Entry& e = cache[id];
                e.loaded = true;   // (a failure counts as "none" until the next try, 5 minutes later)
                e.fetchedAt = now;
                if (!ok || !json.IsObject()) continue;
                const Json::Value& row = json[Text::Narrow(id)];
                e.featured = row["featured"].Str();
                e.badges.clear();
                for (const auto& b : row["badges"].Elements()) {
                    Badge badge;
                    badge.kind = b["badge"].Str();
                    badge.detail = b["detail"].Str();
                    badge.mmr = static_cast<int>(b["mmr"].Int());
                    badge.wins = static_cast<int>(b["wins"].Int());
                    badge.losses = static_cast<int>(b["losses"].Int());
                    if (!badge.kind.empty()) e.badges.push_back(std::move(badge));
                }
            }
            if (!pending.empty()) Flush();
            Redraw();
        });
}

} // namespace SpeedrunLauncher::Services::Account
