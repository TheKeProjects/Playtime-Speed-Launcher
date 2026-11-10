#include "Services/Account/MessagesService.h"
#include <algorithm>
#include "Core/Async.h"
#include "Core/Json.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/Account/AccountApi.h"
#include "Services/Account/AccountService.h"
#include "Services/Audio/SoundEffects.h"
#include "Settings/DuelSettings.h"
#include "UI/Window.h"

namespace SpeedrunLauncher::Services::Account {

namespace {

constexpr unsigned kTickMs = 3000;
constexpr int kIdleTicks = 6;   // the conversations every 18 s while no chat is on screen
constexpr size_t kMaxText = 500;

void Redraw() {
    for (UI::Window* window : UI::Window::All()) window->Invalidate();
}

std::wstring ErrorFrom(const Http::Response& response) {
    std::wstring detail;
    const std::string key = Api::ErrorKey(response, &detail);
    if (key == "acc_err_server" && !detail.empty()) return Loc::Get(key) + L": " + detail;
    return Loc::Get(key);
}

std::wstring NameOf(const Profile& p) { return p.ShownName(p.speedrunName.empty() ? Loc::Get("acc_default_name") : p.speedrunName); }

} // namespace

MessagesService& MessagesService::Get() {
    static MessagesService instance;
    return instance;
}

void MessagesService::Initialize() {
    if (initialized) return;
    initialized = true;
    Async::Every(kTickMs, [this] {
        if (!AccountService::Get().SignedIn()) return;
        ++ticks;
        if (!openWith.empty()) LoadHistory();
        if (watching || !openWith.empty() || ticks % kIdleTicks == 0) Refresh();
    });
    if (AccountService::Get().SignedIn()) Refresh();
}

void MessagesService::AccountChanged() {
    ++generation;
    ++openGeneration;
    loaded = failed = refreshing = historyBusy = historyLoaded = false;
    unread = 0;
    threads.clear();
    seenLast.clear();
    openWith.clear();
    messages.clear();
    lastId = 0;
    if (AccountService::Get().SignedIn()) Refresh();
    Redraw();
}

void MessagesService::SetWatching(bool on) {
    if (watching == on) return;
    watching = on;
    if (on) Refresh();
}

void MessagesService::Open(const std::wstring& userId) {
    if (openWith == userId) return;
    openWith = userId;
    ++openGeneration;
    messages.clear();
    lastId = 0;
    historyBusy = false;
    historyLoaded = false;
    if (!userId.empty()) {
        // Reading it: its unread count goes (the server marks them read with the history)
        for (auto& t : threads)
            if (t.profile.userId == userId) {
                unread = std::max(0, unread - t.unread);
                t.unread = 0;
            }
        LoadHistory();
    }
    Redraw();
}

void MessagesService::LoadHistory() {
    if (openWith.empty() || historyBusy || !AccountService::Get().SignedIn()) return;
    historyBusy = true;
    const unsigned gen = openGeneration;
    const std::wstring with = openWith;
    const long long after = lastId;
    Async::Run(
        [with, after] {
            Json::Value body = Json::Value::MakeObject();
            body.Set("p_with", with);
            body.Set("p_after", after);
            return AccountService::Get().Rpc(L"msg_history", body);
        },
        [this, gen](Http::Response response) {
            if (gen != openGeneration) return;
            historyBusy = false;
            if (!response.Ok()) return;
            bool ok = false;
            const Json::Value json = Json::Parse(std::string_view(response.body), &ok);
            if (!ok || !json.IsObject()) return;
            historyLoaded = true;
            const double now = Async::Now();
            bool mineArrived = false;
            std::vector<DirectMessage> arrived;
            for (const auto& row : json["messages"].Elements()) {
                DirectMessage m;
                m.id = row["id"].Int();
                m.mine = row["mine"].Bool();
                m.read = row["read"].Bool();
                m.text = row["text"].WStr();
                m.ago = row["ago"].Number();
                m.fetchedAt = now;
                if (m.id <= lastId) continue;
                lastId = std::max(lastId, m.id);
                mineArrived = mineArrived || m.mine;
                arrived.push_back(std::move(m));
            }
            if (arrived.empty()) return;
            // Our own messages came back from the server: the local copies go
            if (mineArrived)
                messages.erase(std::remove_if(messages.begin(), messages.end(), [](const DirectMessage& m) { return m.id == 0; }), messages.end());
            const auto pending = std::find_if(messages.begin(), messages.end(), [](const DirectMessage& m) { return m.id == 0; });
            messages.insert(pending, arrived.begin(), arrived.end());
            Redraw();
        });
}

void MessagesService::Refresh() {
    if (!AccountService::Get().SignedIn() || refreshing) return;
    refreshing = true;
    const unsigned gen = generation;
    Async::Run([] { return AccountService::Get().Rpc(L"msg_threads", Json::Value::MakeObject()); }, [this, gen](Http::Response response) {
        if (gen != generation) return;
        refreshing = false;
        failed = !response.Ok();
        bool ok = false;
        const Json::Value json = failed ? Json::Value() : Json::Parse(std::string_view(response.body), &ok);
        if (failed || !ok || !json.IsObject()) {
            failed = true;
            Redraw();
            return;
        }
        const double now = Async::Now();
        std::vector<ChatThread> list;
        for (const auto& row : json["threads"].Elements()) {
            ChatThread t;
            const Json::Value& user = row["user"];
            t.profile = Profile::FromJson(user);
            t.secondsAgo = user["seconds_ago"].IsNumber() ? user["seconds_ago"].Number() : -1.0;
            t.playing = static_cast<int>(user["playing"].Int());
            t.lastId = row["last_id"].Int();
            t.last = row["last"].WStr();
            t.lastMine = row["last_mine"].Bool();
            t.ago = row["ago"].Number();
            t.fetchedAt = now;
            t.unread = static_cast<int>(row["unread"].Int());
            // The conversation on screen is being read (the history call marks it on the server)
            if (t.profile.userId == openWith) t.unread = 0;
            list.push_back(std::move(t));
        }
        int total = 0;
        for (const auto& t : list) total += t.unread;

        // Toasts: what arrived since the last look (not on the first load, not for the open chat)
        const auto& settings = Settings::Duels::Current();
        for (const auto& t : list) {
            long long& seen = seenLast[t.profile.userId];
            const bool fresh = t.lastId > seen;
            seen = std::max(seen, t.lastId);
            if (!loaded || !fresh || t.lastMine || t.unread == 0) continue;
            if (t.profile.userId == openWith && watching) continue;
            if (!settings.directMessages) continue;
            if (settings.sounds) Sfx::Play(Sfx::Click);
            if (onNotify && settings.notifications) onNotify(Loc::Fmt("msg_toast_title", NameOf(t.profile)), t.last);
        }
        threads = std::move(list);
        unread = total;
        loaded = true;
        Redraw();
    });
}

void MessagesService::Send(const std::wstring& toUserId, const std::wstring& text, Done done) {
    std::wstring trimmed = Text::Trim(text);
    if (trimmed.empty() || toUserId.empty()) return;
    if (trimmed.size() > kMaxText) trimmed.resize(kMaxText);
    if (toUserId == openWith) {
        DirectMessage local;
        local.mine = true;
        local.text = trimmed;
        local.fetchedAt = Async::Now();
        messages.push_back(std::move(local));
        Redraw();
    }
    const unsigned gen = generation;
    Async::Run(
        [toUserId, trimmed] {
            Json::Value body = Json::Value::MakeObject();
            body.Set("p_to", toUserId);
            body.Set("p_text", trimmed);
            return AccountService::Get().Rpc(L"msg_send", body);
        },
        [this, gen, toUserId, done = std::move(done)](Http::Response response) {
            if (gen != generation) return;
            std::wstring message;
            bool ok = false;
            if (!response.Ok()) {
                message = ErrorFrom(response);
            } else {
                const Json::Value json = Json::Parse(std::string_view(response.body));
                const std::string error = json["error"].Str();
                ok = error.empty();
                if (!ok) message = Loc::Get("msg_err_" + error);
            }
            if (!ok && toUserId == openWith) {
                // Not sent: the local copy goes
                messages.erase(std::remove_if(messages.begin(), messages.end(), [](const DirectMessage& m) { return m.id == 0; }), messages.end());
            }
            LoadHistory();
            Refresh();
            if (done) done(message, ok);
            Redraw();
        });
}

} // namespace SpeedrunLauncher::Services::Account
