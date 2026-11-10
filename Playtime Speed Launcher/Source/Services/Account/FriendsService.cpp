#include "Services/Account/FriendsService.h"
#include <algorithm>
#include <cmath>
#include "App/Launcher.h"
#include "Core/Async.h"
#include "Core/Json.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/Account/AccountApi.h"
#include "Services/Account/AccountService.h"
#include "UI/Window.h"

namespace SpeedrunLauncher::Services::Account {

namespace {

constexpr unsigned kIntervalMs = 60000;

void Redraw() {
    for (UI::Window* window : UI::Window::All()) window->Invalidate();
}

// What this PC is running right now: a chapter number, a multiplayer game's id, or 0
int PlayingNow() {
    auto& l = App::L();
    for (size_t i = 0; i < l.chapters.size() && i < l.gameRunning.size(); ++i)
        if (l.gameRunning[i]) return l.chapters[i].number;
    for (size_t i = 0; i < l.multiplayers.size() && i < l.multiplayerRunning.size(); ++i)
        if (l.multiplayerRunning[i]) return l.multiplayers[i].playtimeId;
    return 0;
}

std::wstring ErrorFrom(const Http::Response& response) {
    std::wstring detail;
    const std::string key = Api::ErrorKey(response, &detail);
    if (key == "acc_err_server" && !detail.empty()) return Loc::Get(key) + L": " + detail;
    return Loc::Get(key);
}

} // namespace

FriendsService& FriendsService::Get() {
    static FriendsService* instance = new FriendsService();   // never destroyed: workers may still post to it at exit
    return *instance;
}

void FriendsService::Initialize() {
    if (initialized) return;
    initialized = true;
    Async::Every(kIntervalMs, [this] {
        if (!AccountService::Get().SignedIn()) return;
        Heartbeat();
        Refresh();
    });
}

void FriendsService::AccountChanged() {
    ++generation;
    myCode.clear();
    friends.clear();
    incoming.clear();
    outgoing.clear();
    loaded = failed = busy = refreshing = false;
    if (AccountService::Get().SignedIn()) {
        Heartbeat();
        Refresh();
    }
    Redraw();
}

void FriendsService::Heartbeat() {
    const unsigned gen = generation;
    const int playing = PlayingNow();
    Async::Run([playing] {
        Json::Value body = Json::Value::MakeObject();
        body.Set("p_playing", playing);
        return AccountService::Get().Rpc(L"launcher_heartbeat", body);
    }, [this, gen](Http::Response response) {
        if (gen != generation || !response.Ok()) return;
        const Json::Value json = Json::Parse(std::string_view(response.body));
        if (json.IsString() && json.WStr() != myCode) {
            myCode = json.WStr();
            Redraw();
        }
    });
}

void FriendsService::Refresh() {
    if (!AccountService::Get().SignedIn() || refreshing) return;
    refreshing = true;
    const unsigned gen = generation;
    Async::Run([] { return AccountService::Get().Rpc(L"friends_list", Json::Value::MakeObject()); },
               [this, gen](Http::Response response) {
                   if (gen != generation) return;
                   refreshing = false;
                   failed = !response.Ok();
                   if (failed) {
                       Redraw();
                       return;
                   }
                   bool ok = false;
                   const Json::Value rows = Json::Parse(std::string_view(response.body), &ok);
                   if (!ok || !rows.IsArray()) return;
                   friends.clear();
                   incoming.clear();
                   outgoing.clear();
                   for (const auto& row : rows.Elements()) {
                       Friend f;
                       f.profile = Profile::FromJson(row);
                       f.friendCode = row["friend_code"].WStr();
                       f.secondsAgo = row["seconds_ago"].IsNumber() ? row["seconds_ago"].Number() : -1.0;
                       f.playing = static_cast<int>(row["playing"].Int());
                       const std::string relation = row["relation"].Str();
                       if (relation == "incoming") {
                           f.relation = Friend::Relation::Incoming;
                           incoming.push_back(std::move(f));
                       } else if (relation == "outgoing") {
                           f.relation = Friend::Relation::Outgoing;
                           outgoing.push_back(std::move(f));
                       } else {
                           friends.push_back(std::move(f));
                       }
                   }
                   // Online first (playing before just online), then by name
                   std::stable_sort(friends.begin(), friends.end(), [](const Friend& a, const Friend& b) {
                       const int ra = a.Online() ? (a.playing ? 0 : 1) : 2, rb = b.Online() ? (b.playing ? 0 : 1) : 2;
                       if (ra != rb) return ra < rb;
                       return Text::Lower(a.profile.ShownName(L"")) < Text::Lower(b.profile.ShownName(L""));
                   });
                   loaded = true;
                   Redraw();
               });
}

bool FriendsService::IsFriend(const std::wstring& userId) const {
    for (const auto& f : friends)
        if (f.profile.userId == userId) return true;
    return false;
}

bool FriendsService::HasRequest(const std::wstring& userId) const {
    for (const auto* list : { &incoming, &outgoing })
        for (const auto& f : *list)
            if (f.profile.userId == userId) return true;
    return false;
}

void FriendsService::SendByCode(const std::wstring& code, Done done) {
    const std::wstring clean = Text::Upper(Text::Trim(code));
    if (clean.empty()) {
        if (done) done(Loc::Get("fr_err_not_found"), false);
        return;
    }
    Send(clean, L"", std::move(done));
}

void FriendsService::SendToUser(const std::wstring& userId, Done done) { Send(L"", userId, std::move(done)); }

void FriendsService::Send(const std::wstring& code, const std::wstring& userId, Done done) {
    if (!AccountService::Get().SignedIn() || busy) return;
    busy = true;
    Redraw();
    const unsigned gen = generation;
    Async::Run([code, userId] {
        Json::Value body = Json::Value::MakeObject();
        body.Set("p_code", code.empty() ? Json::Value() : Json::Value(code));
        body.Set("p_user", userId.empty() ? Json::Value() : Json::Value(userId));
        return AccountService::Get().Rpc(L"friends_send", body);
    }, [this, gen, done = std::move(done)](Http::Response response) {
        if (gen != generation) return;
        busy = false;
        std::wstring message;
        bool ok = false;
        if (!response.Ok()) {
            message = ErrorFrom(response);
        } else {
            const Json::Value json = Json::Parse(std::string_view(response.body));
            const std::string error = json["error"].Str();
            const std::string result = json["result"].Str();
            if (!error.empty()) {
                message = Loc::Get("fr_err_" + error);
            } else {
                ok = true;
                message = Loc::Get(result == "friends" ? "fr_now_friends" : "fr_sent");
            }
        }
        Refresh();
        if (done) done(message, ok);
    });
}

void FriendsService::Simple(const std::wstring& function, const std::wstring& bodyJson, Done done) {
    if (!AccountService::Get().SignedIn() || busy) return;
    busy = true;
    Redraw();
    const unsigned gen = generation;
    Async::Run([function, bodyJson] { return AccountService::Get().Rpc(function, Json::Parse(std::wstring_view(bodyJson))); },
               [this, gen, done = std::move(done)](Http::Response response) {
                   if (gen != generation) return;
                   busy = false;
                   const bool ok = response.Ok();
                   Refresh();
                   if (done) done(ok ? L"" : ErrorFrom(response), ok);
               });
}

void FriendsService::Respond(const std::wstring& userId, bool accept, Done done) {
    Json::Value body = Json::Value::MakeObject();
    body.Set("p_from", userId);
    body.Set("p_accept", accept);
    Simple(L"friends_respond", Text::Widen(body.Serialize()), std::move(done));
}

void FriendsService::Remove(const std::wstring& userId, Done done) {
    Json::Value body = Json::Value::MakeObject();
    body.Set("p_other", userId);
    Simple(L"friends_remove", Text::Widen(body.Serialize()), std::move(done));
}

void FriendsService::Search(const std::wstring& query, std::function<void(std::optional<std::vector<Friend>>)> done) {
    if (!AccountService::Get().SignedIn()) return;
    const unsigned gen = generation, search = ++searchGeneration;
    Async::Run([query] {
        Json::Value body = Json::Value::MakeObject();
        body.Set("p_query", Text::Trim(query));
        return AccountService::Get().Rpc(L"friends_search", body);
    }, [this, gen, search, done = std::move(done)](Http::Response response) {
        if (gen != generation || search != searchGeneration) return;   // signed out / a newer search
        std::optional<std::vector<Friend>> results;
        bool ok = false;
        const Json::Value rows = response.Ok() ? Json::Parse(std::string_view(response.body), &ok) : Json::Value();
        if (ok && rows.IsArray()) {
            results.emplace();
            for (const auto& row : rows.Elements()) {
                Friend f;
                f.profile = Profile::FromJson(row);
                f.secondsAgo = row["seconds_ago"].IsNumber() ? row["seconds_ago"].Number() : -1.0;
                f.playing = static_cast<int>(row["playing"].Int());
                const std::string relation = row["relation"].Str();
                f.relation = relation == "friend"     ? Friend::Relation::Friend
                             : relation == "incoming" ? Friend::Relation::Incoming
                             : relation == "outgoing" ? Friend::Relation::Outgoing
                                                      : Friend::Relation::None;
                results->push_back(std::move(f));
            }
        }
        if (done) done(std::move(results));
    });
}

std::wstring FriendsService::StatusText(const Friend& f) {
    if (f.Online()) {
        if (f.playing <= 0) return Loc::Get("fr_online");
        for (const auto& game : App::L().multiplayers)
            if (game.playtimeId == f.playing) return Loc::Fmt("fr_playing", Loc::Get("mp_" + game.key + "_title"));
        return Loc::Fmt("fr_playing", Loc::Fmt("fr_chapter", f.playing));
    }
    if (f.secondsAgo < 0.0) return Loc::Get("fr_offline");
    const long long minutes = static_cast<long long>(f.secondsAgo / 60.0);
    if (minutes < 60) return Loc::Fmt("fr_seen_min", std::max(1ll, minutes));
    if (minutes < 60 * 24) return Loc::Fmt("fr_seen_h", minutes / 60);
    return Loc::Fmt("fr_seen_d", minutes / (60 * 24));
}

} // namespace SpeedrunLauncher::Services::Account
