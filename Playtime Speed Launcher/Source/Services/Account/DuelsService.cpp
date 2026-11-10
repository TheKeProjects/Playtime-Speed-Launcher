#include "Services/Account/DuelsService.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "Core/Async.h"
#include "Core/Json.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/Account/AccountApi.h"
#include "Services/Account/AccountService.h"
#include "Services/Account/FriendsService.h"
#include "Services/Audio/SoundEffects.h"
#include "Settings/DuelSettings.h"
#include "UI/Window.h"

namespace SpeedrunLauncher::Services::Account {

namespace {

constexpr unsigned kTickMs = 3000;
constexpr int kIdleTicks = 10;   // refresh every 30 s while nobody is looking

void Redraw() {
    for (UI::Window* window : UI::Window::All()) window->Invalidate();
}

std::wstring ErrorFrom(const Http::Response& response) {
    std::wstring detail;
    const std::string key = Api::ErrorKey(response, &detail);
    if (key == "acc_err_server" && !detail.empty()) return Loc::Get(key) + L": " + detail;
    return Loc::Get(key);
}

Duel::Type TypeFrom(const std::string& s) {
    if (s == "chapter") return Duel::Type::Chapter;
    if (s == "bans") return Duel::Type::Bans;
    return Duel::Type::Random;
}
const char* TypeName(Duel::Type t) {
    switch (t) {
    case Duel::Type::Chapter: return "chapter";
    case Duel::Type::Bans: return "bans";
    default: return "random";
    }
}
Duel::Status StatusFrom(const std::string& s) {
    if (s == "banning") return Duel::Status::Banning;
    if (s == "running") return Duel::Status::Running;
    if (s == "finished") return Duel::Status::Finished;
    if (s == "expired") return Duel::Status::Expired;
    if (s == "cancelled") return Duel::Status::Cancelled;
    return Duel::Status::Open;
}

DuelPlayer PlayerFrom(const Json::Value& row) {
    DuelPlayer p;
    p.profile = Profile::FromJson(row);
    p.seat = static_cast<int>(row["seat"].Int());
    p.seconds = row["seconds"].IsNumber() ? row["seconds"].Number() : -1.0;
    p.forfeited = row["forfeited"].Bool();
    p.done = row["done"].Bool();
    p.mmr = static_cast<int>(row["mmr"].Int());
    p.wins = static_cast<int>(row["wins"].Int());
    p.losses = static_cast<int>(row["losses"].Int());
    p.mmrBefore = static_cast<int>(row["mmr_before"].Int());
    p.mmrChange = static_cast<int>(row["mmr_change"].Int());
    p.placement = static_cast<int>(row["placement"].Int());
    p.secondsAgo = row["seconds_ago"].IsNumber() ? row["seconds_ago"].Number() : -1.0;
    p.playing = static_cast<int>(row["playing"].Int());
    return p;
}

Duel DuelFrom(const Json::Value& row) {
    Duel d;
    d.id = row["id"].WStr();
    d.code = row["code"].WStr();
    d.hostId = row["host"].WStr();
    d.winnerId = row["winner"].IsString() ? row["winner"].WStr() : L"";
    d.slots = std::clamp(static_cast<int>(row["slots"].Int(2)), kDuelMinSlots, kDuelMaxSlots);
    d.type = TypeFrom(row["type"].Str());
    d.chapter = static_cast<int>(row["chapter"].Int());
    d.category = row["category"].Str();
    d.bans = static_cast<int>(row["bans"].Int());
    for (const auto& v : row["ban_order"].Elements()) d.banOrder.push_back(static_cast<int>(v.Int()));
    for (const auto& v : row["banned"].Elements()) d.banned.push_back(v.Str());
    d.friendsOnly = row["visibility"].Str() == "friends";
    d.status = StatusFrom(row["status"].Str());
    d.age = row["age"].Number();
    d.runningFor = row["running_for"].IsNumber() ? row["running_for"].Number() : -1.0;
    d.fetchedAt = Async::Now();
    for (const auto& p : row["players"].Elements()) d.players.push_back(PlayerFrom(p));
    for (const auto& p : row["invited"].Elements()) d.invited.push_back(Profile::FromJson(p));
    if (row["from"].IsObject()) d.from = Profile::FromJson(row["from"]);
    for (const auto& m : row["chat"].Elements()) {
        DuelChatMessage msg;
        msg.id = m["id"].Int();
        msg.userId = m["user_id"].WStr();
        msg.name = m["name"].WStr();
        msg.text = m["text"].WStr();
        msg.ago = m["ago"].Number();
        d.chat.push_back(std::move(msg));
    }
    return d;
}

std::wstring NameOf(const Profile& p) { return p.ShownName(p.speedrunName.empty() ? Loc::Get("acc_default_name") : p.speedrunName); }

} // namespace

// ── Rules ───────────────────────────────────────────────────────────────────

const std::vector<DuelRank>& DuelRanks() {
    static const std::vector<DuelRank> ranks = {
        { "Huggy", 0, 0x2E6FE0, L"Images\\Ranks\\Huggy.png" },
        { "Mommy LongLegs", 50, 0xE0458A, L"Images\\Ranks\\MommyLongLegs.png" },
        { "Bubba Bubbaphant", 100, 0x4FA8E8, L"Images\\Ranks\\BubbaBubbaphant.png" },
        { "DogDay", 150, 0xF2A922, L"Images\\Ranks\\DogDay.png" },
        { "Catnap", 200, 0x9B5DE5, L"Images\\Ranks\\Catnap.png" },
        { "Doey", 250, 0xE0782F, L"Images\\Ranks\\Doey.png" },
        { "Lilly", 300, 0xF26DB5, L"Images\\Ranks\\Lilly.png" },
        { "Prototype", 350, 0xD9D9E0, L"Images\\Ranks\\Prototype.png" },
    };
    return ranks;
}

const DuelRank& DuelRankFor(int mmr) {
    const auto& ranks = DuelRanks();
    const DuelRank* rank = &ranks.front();
    for (const auto& r : ranks)
        if (mmr >= r.min) rank = &r;
    return *rank;
}

const DuelRank* NextDuelRank(int mmr) {
    for (const auto& r : DuelRanks())
        if (r.min > mmr) return &r;
    return nullptr;
}

const std::vector<DuelCategory>& DuelCategories() {
    static const std::vector<DuelCategory> categories = {
        { "c1_1", 1, L"Any% Unrestricted" }, { "c1_2", 1, L"Any% Restricted" },   { "c1_3", 1, L"100% Unrestricted" },
        { "c1_4", 1, L"100% Restricted" },   { "c2_1", 2, L"Any% OOB" },          { "c2_2", 2, L"Any% Inbounds" },
        { "c2_3", 2, L"Any% NMS" },          { "c2_4", 2, L"All Minigames OOB" }, { "c2_5", 2, L"All Minigames Inbounds" },
        { "c2_6", 2, L"All Minigames NMS" }, { "c3_1", 3, L"Any% OOB" },          { "c3_4", 3, L"Any% AS OOB" },
        { "c3_2", 3, L"Any% Inbounds" },     { "c3_3", 3, L"Any% NMS" },          { "c4_4", 4, L"Any% NASA OOB" },
        { "c4_1", 4, L"Any% OOB" },          { "c4_2", 4, L"Any% Inbounds" },     { "c4_3", 4, L"Any% NMS" },
        { "c5_4", 5, L"Any% Unrestricted" }, { "c5_1", 5, L"Any% OOB" },          { "c5_2", 5, L"Any% Inbounds" },
        { "c5_3", 5, L"Any% NMS" },
    };
    return categories;
}

const DuelCategory* FindDuelCategory(const std::string& id) {
    for (const auto& c : DuelCategories())
        if (id == c.id) return &c;
    return nullptr;
}

const std::vector<int>& DuelBanCounts() {
    static const std::vector<int> counts = { 2, 4, 6, 8, 10, 12 };
    return counts;
}

double DuelExpected(int a, int b) { return 1.0 / (1.0 + std::pow(10.0, (b - a) / 400.0)); }

std::wstring FormatDuelTime(double seconds) {
    if (seconds < 0.0) return L"\x2014";
    const long long ms = std::llround(seconds * 1000.0);
    const long long h = ms / 3600000, m = (ms / 60000) % 60, s = (ms / 1000) % 60, rest = ms % 1000;
    wchar_t buffer[48];
    if (h > 0) swprintf_s(buffer, L"%lld:%02lld:%02lld.%03lld", h, m, s, rest);
    else if (m > 0) swprintf_s(buffer, L"%lld:%02lld.%03lld", m, s, rest);
    else swprintf_s(buffer, L"%lld.%03lld", s, rest);
    return buffer;
}

std::optional<double> ParseDuelTime(const std::wstring& text) {
    const std::wstring clean = Text::Replace(Text::Trim(text), L",", L".");
    if (clean.empty()) return std::nullopt;
    const auto parts = Text::Split(clean, L':');
    if (parts.empty() || parts.size() > 3) return std::nullopt;
    double total = 0.0;
    for (size_t i = 0; i < parts.size(); ++i) {
        const std::wstring part = Text::Trim(parts[i]);
        if (part.empty() || part.find_first_not_of(L"0123456789.") != std::wstring::npos) return std::nullopt;
        const auto value = Text::ToDouble(part);
        if (!value || *value < 0.0) return std::nullopt;
        // Minutes / seconds after the first part stay under 60; only the last part has decimals
        if (i > 0 && *value >= 60.0) return std::nullopt;
        if (i + 1 < parts.size() && part.find(L'.') != std::wstring::npos) return std::nullopt;
        total = total * 60.0 + *value;
    }
    if (total <= 0.0 || total > 86400.0) return std::nullopt;
    return total;
}

std::wstring DuelModeLabel(int slots) {
    std::wstring s = L"1";
    for (int i = 1; i < slots; ++i) s += L"v1";
    return s;
}

// ── Duel ────────────────────────────────────────────────────────────────────

const DuelPlayer* Duel::Player(const std::wstring& userId) const {
    for (const auto& p : players)
        if (p.profile.userId == userId) return &p;
    return nullptr;
}

const DuelPlayer* Duel::BanTurn() const {
    if (status != Status::Banning || banned.size() >= banOrder.size()) return nullptr;
    const int seat = banOrder[banned.size()];
    for (const auto& p : players)
        if (p.seat == seat) return &p;
    return nullptr;
}

bool Duel::IsBanned(const std::string& categoryId) const { return std::find(banned.begin(), banned.end(), categoryId) != banned.end(); }

double Duel::LobbySecondsLeft() const { return std::max(0.0, kDuelLobbySeconds - (age + (Async::Now() - fetchedAt))); }

double Duel::RunningSeconds() const { return runningFor < 0.0 ? 0.0 : runningFor + (Async::Now() - fetchedAt); }

std::vector<const DuelPlayer*> Duel::Standings() const {
    std::vector<const DuelPlayer*> list;
    for (const auto& p : players) list.push_back(&p);
    std::stable_sort(list.begin(), list.end(), [this](const DuelPlayer* a, const DuelPlayer* b) {
        if (status == Status::Finished && a->placement > 0 && b->placement > 0) return a->placement < b->placement;
        const auto key = [](const DuelPlayer* p) { return p->forfeited ? 2 : p->seconds >= 0.0 ? 0 : 1; };
        if (key(a) != key(b)) return key(a) < key(b);
        if (key(a) == 0 && a->seconds != b->seconds) return a->seconds < b->seconds;
        return a->seat < b->seat;
    });
    return list;
}

// ── Service ─────────────────────────────────────────────────────────────────

DuelsService& DuelsService::Get() {
    static DuelsService* instance = new DuelsService();   // never destroyed: workers may still post to it at exit
    return *instance;
}

void DuelsService::Initialize() {
    if (initialized) return;
    initialized = true;
    Async::Every(kTickMs, [this] {
        if (!AccountService::Get().SignedIn()) return;
        ++ticks;
        if (watching || active || ticks % kIdleTicks == 0) Refresh();
    });
}

void DuelsService::AccountChanged() {
    ++generation;
    loaded = failed = busy = refreshing = again = false;
    myMmr = myWins = myLosses = 0;
    active.reset();
    result.reset();
    lobbies.clear();
    invites.clear();
    online.clear();
    onlineCount = 0;
    seenInvites.clear();
    dismissed.clear();
    challenged.clear();
    challengedFor.clear();
    leaderboard.clear();
    leaderboardLoading = leaderboardFailed = false;
    champions.clear();
    championsLoading = championsLoaded = false;
    forfeitBlockedUntil = 0.0;
    if (AccountService::Get().SignedIn()) Refresh();
    Redraw();
}

void DuelsService::SetWatching(bool on) {
    if (watching == on) return;
    watching = on;
    if (on) Refresh();
}

int DuelsService::AlertCount() const {
    return static_cast<int>(invites.size()) + (MyTurnToBan() || NeedsMyTime() ? 1 : 0) + (result ? 1 : 0);
}

bool DuelsService::MyTurnToBan() const {
    if (!active) return false;
    const DuelPlayer* turn = active->BanTurn();
    return turn && turn->profile.userId == AccountService::Get().UserId();
}

bool DuelsService::NeedsMyTime() const {
    if (!active || active->status != Duel::Status::Running) return false;
    const DuelPlayer* me = active->Player(AccountService::Get().UserId());
    return me && !me->done;
}

namespace {
bool EventOn(DuelsService::Event event) {
    const auto& s = Settings::Duels::Current();
    switch (event) {
    case DuelsService::Event::Invite: return s.invites;
    case DuelsService::Event::Join: return s.joins;
    case DuelsService::Event::Start: return s.starts;
    case DuelsService::Event::BanTurn: return s.banTurn;
    case DuelsService::Event::RivalFinished: return s.rivalFinished;
    case DuelsService::Event::Leave: return s.leaves;
    case DuelsService::Event::Chat: return s.roomChat;
    default: return s.results;
    }
}
} // namespace

void DuelsService::PlaySound(Event event, const wchar_t* sound) {
    if (sound && Settings::Duels::Current().sounds && EventOn(event)) Sfx::Play(sound);
}

void DuelsService::Notify(Event event, const std::wstring& title, const std::wstring& text, unsigned color, const wchar_t* sound) {
    if (!EventOn(event)) return;
    PlaySound(event, sound);
    if (onNotify && Settings::Duels::Current().notifications) onNotify(title, text, color);
}

// Toasts for what changed since the last refresh
void DuelsService::Compare(const std::optional<Duel>& oldActive, const std::optional<Duel>& oldResult) {
    const std::wstring& me = AccountService::Get().UserId();
    for (const auto& invite : invites) {
        if (!seenInvites.insert(invite.id).second) continue;
        if (!loaded) continue;   // the first load doesn't toast what was already waiting
        if (Settings::Duels::Current().friendInvitesOnly && !FriendsService::Get().IsFriend(invite.from.userId)) continue;
        Notify(Event::Invite, Loc::Get("duel_toast_challenge"), Loc::Fmt("duel_toast_challenge_text", NameOf(invite.from), DuelModeLabel(invite.slots)), 0xFFB020,
               Sfx::DuelInvite);
    }
    if (active && oldActive && active->id == oldActive->id) {
        if (oldActive->status == Duel::Status::Open && active->status != Duel::Status::Open)
            Notify(Event::Start, Loc::Get("duel_toast_started"), Loc::Get(active->status == Duel::Status::Banning ? "duel_toast_started_bans" : "duel_toast_started_run"),
                   0x00CCAA, Sfx::Chapter);
        else if (oldActive->status == Duel::Status::Banning && active->status == Duel::Status::Running)
            if (const DuelCategory* c = FindDuelCategory(active->category))
                Notify(Event::Start, Loc::Get("duel_toast_rolled"), Loc::Fmt("fr_chapter", c->chapter) + L" \x00B7 " + c->label, 0x00CCAA, Sfx::Click);
        if (oldActive->status == Duel::Status::Open && active->status == Duel::Status::Open && active->players.size() > oldActive->players.size())
            for (const auto& p : active->players)
                if (!oldActive->Player(p.profile.userId) && p.profile.userId != me)
                    Notify(Event::Join, Loc::Get("duel_toast_joined"), Loc::Fmt("duel_toast_joined_text", NameOf(p.profile)), 0x3DDC84, Sfx::DuelJoin);
        // Someone wrote in the room chat while the duels menu is closed: the newest message
        if (!watching) {
            long long seen = 0;
            for (const auto& m : oldActive->chat) seen = std::max(seen, m.id);
            const DuelChatMessage* newest = nullptr;
            for (const auto& m : active->chat)
                if (m.id > seen && m.userId != me) newest = &m;
            if (newest)
                Notify(Event::Chat, Loc::Fmt("duel_toast_chat", newest->name.empty() ? Loc::Get("acc_default_name") : newest->name), newest->text, 0x3A9BFF,
                       Sfx::Click);
        }
        // Someone left the room (or was kicked)
        if (oldActive->status == Duel::Status::Open && active->status == Duel::Status::Open)
            for (const auto& p : oldActive->players)
                if (!active->Player(p.profile.userId) && p.profile.userId != me)
                    Notify(Event::Leave, Loc::Get("duel_toast_left"), Loc::Fmt("duel_toast_left_text", NameOf(p.profile)), 0x78A0BE, nullptr);
        // The ban turn passed to you
        if (oldActive->status == Duel::Status::Banning && active->status == Duel::Status::Banning) {
            const DuelPlayer* before = oldActive->BanTurn();
            const DuelPlayer* now = active->BanTurn();
            if (now && now->profile.userId == me && (!before || before->profile.userId != me))
                Notify(Event::BanTurn, Loc::Get("duel_toast_ban_turn"), Loc::Get("duel_toast_ban_turn_text"), 0xFFB020, Sfx::Click);
        }
        // A rival sent their time or forfeited
        if (oldActive->status == Duel::Status::Running && active->status == Duel::Status::Running)
            for (const auto& p : active->players) {
                const DuelPlayer* before = oldActive->Player(p.profile.userId);
                if (p.profile.userId == me || !p.done || !before || before->done) continue;
                if (p.forfeited)
                    Notify(Event::RivalFinished, Loc::Get("duel_toast_rival_forfeit"), Loc::Fmt("duel_toast_rival_forfeit_text", NameOf(p.profile)), 0x78A0BE, nullptr);
                else
                    Notify(Event::RivalFinished, Loc::Get("duel_toast_rival_done"),
                           Loc::Fmt("duel_toast_rival_done_text", NameOf(p.profile), FormatDuelTime(p.seconds)),0xFF5A3C, Sfx::Click);
            }
    }
    if (result && (!oldResult || oldResult->id != result->id) && loaded) {
        if (const DuelPlayer* mine = result->Player(me)) {
            const std::wstring change = (mine->mmrChange >= 0 ? L"+" : L"") + std::to_wstring(mine->mmrChange) + L" MMR";
            Notify(Event::Result, Loc::Get(mine->placement == 1 ? "duel_toast_won" : "duel_toast_finished"), Loc::Fmt("duel_toast_result_text", mine->placement, change),
                   mine->placement == 1 ? 0xFFB020 : 0x78A0BE, mine->placement == 1 ? Sfx::DuelWin : Sfx::DuelLose);
        }
    }
    // The challenges we sent belong to the lobby we're in
    const std::wstring lobby = active && active->status == Duel::Status::Open ? active->id : L"";
    if (lobby != challengedFor) {
        challenged.clear();
        challengedFor = lobby;
    }
}

void DuelsService::Refresh() {
    if (!AccountService::Get().SignedIn()) return;
    if (refreshing) {
        again = true;
        return;
    }
    refreshing = true;
    const unsigned gen = generation;
    Async::Run([] { return AccountService::Get().Rpc(L"duel_state", Json::Value::MakeObject()); },
               [this, gen](Http::Response response) {
                   if (gen != generation) return;
                   refreshing = false;
                   failed = !response.Ok();
                   bool ok = false;
                   const Json::Value json = failed ? Json::Value() : Json::Parse(std::string_view(response.body), &ok);
                   if (!failed && ok && json.IsObject()) {
                       const auto oldActive = active;
                       const auto oldResult = result;
                       myMmr = static_cast<int>(json["me"]["mmr"].Int());
                       myWins = static_cast<int>(json["me"]["wins"].Int());
                       myLosses = static_cast<int>(json["me"]["losses"].Int());
                       forfeitBlockedUntil = json["forfeit_block"].IsNumber() ? Async::Now() + json["forfeit_block"].Number() : 0.0;
                       if (!championsLoaded) LoadChampions();
                       active.reset();
                       result.reset();
                       if (json["active"].IsObject()) active = DuelFrom(json["active"]);
                       if (json["result"].IsObject() && !dismissed.count(json["result"]["id"].WStr())) result = DuelFrom(json["result"]);
                       lobbies.clear();
                       for (const auto& row : json["lobbies"].Elements()) lobbies.push_back(DuelFrom(row));
                       invites.clear();
                       for (const auto& row : json["invites"].Elements()) invites.push_back(DuelFrom(row));
                       online.clear();
                       for (const auto& row : json["online"].Elements()) {
                           DuelOnlineUser u;
                           u.profile = Profile::FromJson(row);
                           u.mmr = static_cast<int>(row["mmr"].Int());
                           u.wins = static_cast<int>(row["wins"].Int());
                           u.losses = static_cast<int>(row["losses"].Int());
                           u.isFriend = row["friend"].Bool();
                           u.inDuel = row["in_duel"].Bool();
                           u.playing = static_cast<int>(row["playing"].Int());
                           online.push_back(std::move(u));
                       }
                       onlineCount = static_cast<int>(json["online_count"].Int());
                       Compare(oldActive, oldResult);
                       loaded = true;
                   } else if (!failed) {
                       failed = true;
                   }
                   Redraw();
                   if (again) {
                       again = false;
                       Refresh();
                   }
               });
}

void DuelsService::Call(const std::wstring& function, const std::string& bodyJson, Done done, bool quiet) {
    if (!AccountService::Get().SignedIn()) return;
    if (!quiet) {
        if (busy) return;
        busy = true;
        Redraw();
    }
    const unsigned gen = generation;
    Async::Run([function, bodyJson] { return AccountService::Get().Rpc(function, Json::Parse(std::string_view(bodyJson))); },
               [this, gen, quiet, done = std::move(done)](Http::Response response) {
                   if (gen != generation) return;
                   if (!quiet) busy = false;
                   std::wstring message;
                   bool ok = false;
                   if (!response.Ok()) {
                       message = ErrorFrom(response);
                   } else {
                       const Json::Value json = Json::Parse(std::string_view(response.body));
                       const std::string error = json["error"].Str();
                       ok = error.empty();
                       if (!ok) message = Loc::Get("duel_err_" + error);
                   }
                   Refresh();
                   if (done) done(message, ok);
               });
}

DuelSetup DuelsService::SetupOf(const Duel& duel) {
    DuelSetup s;
    s.slots = duel.slots;
    s.type = duel.type;
    s.chapter = duel.chapter > 0 ? duel.chapter : 1;
    s.category = duel.category;
    s.bans = duel.bans > 0 ? duel.bans : 6;
    s.friendsOnly = duel.friendsOnly;
    return s;
}

void DuelsService::Update(const std::wstring& duelId, const DuelSetup& setup, Done done) {
    Json::Value body = Json::Value::MakeObject();
    body.Set("p_duel", duelId);
    body.Set("p_slots", std::clamp(setup.slots, kDuelMinSlots, kDuelMaxSlots));
    body.Set("p_type", TypeName(setup.type));
    body.Set("p_chapter", setup.type == Duel::Type::Chapter ? Json::Value(setup.chapter) : Json::Value());
    body.Set("p_category", setup.type == Duel::Type::Chapter && !setup.category.empty() ? Json::Value(setup.category) : Json::Value());
    body.Set("p_bans", setup.type == Duel::Type::Bans ? Json::Value(setup.bans) : Json::Value());
    body.Set("p_visibility", setup.friendsOnly ? "friends" : "public");
    // Shown at once (the server agrees a moment later)
    if (active && active->id == duelId && active->status == Duel::Status::Open && setup.slots >= static_cast<int>(active->players.size())) {
        active->slots = setup.slots;
        active->type = setup.type;
        active->chapter = setup.type == Duel::Type::Chapter ? setup.chapter : 0;
        active->category = setup.type == Duel::Type::Chapter ? setup.category : "";
        active->bans = setup.type == Duel::Type::Bans ? setup.bans : 0;
        active->friendsOnly = setup.friendsOnly;
    }
    Call(L"duel_update", body.Serialize(), std::move(done));
}

void DuelsService::Kick(const std::wstring& duelId, const std::wstring& userId, Done done) {
    Json::Value body = Json::Value::MakeObject();
    body.Set("p_duel", duelId);
    body.Set("p_user", userId);
    challenged.erase(userId);
    Call(L"duel_kick", body.Serialize(), std::move(done));
}

void DuelsService::StartNow(const std::wstring& duelId, Done done) {
    Json::Value body = Json::Value::MakeObject();
    body.Set("p_duel", duelId);
    Call(L"duel_start_now", body.Serialize(), std::move(done));
}

void DuelsService::Create(const DuelSetup& setup, Done done) {
    Json::Value body = Json::Value::MakeObject();
    body.Set("p_slots", std::clamp(setup.slots, kDuelMinSlots, kDuelMaxSlots));
    body.Set("p_type", TypeName(setup.type));
    body.Set("p_chapter", setup.type == Duel::Type::Chapter ? Json::Value(setup.chapter) : Json::Value());
    body.Set("p_category", setup.type == Duel::Type::Chapter && !setup.category.empty() ? Json::Value(setup.category) : Json::Value());
    body.Set("p_bans", setup.type == Duel::Type::Bans ? Json::Value(setup.bans) : Json::Value());
    body.Set("p_visibility", setup.friendsOnly ? "friends" : "public");
    body.Set("p_invite", setup.invite.empty() ? Json::Value() : Json::Value(setup.invite));
    const std::wstring invite = setup.invite;
    Call(L"duel_create", body.Serialize(), [this, invite, done = std::move(done)](const std::wstring& message, bool ok) {
        if (ok && !invite.empty()) {
            challenged.insert(invite);
            PlaySound(Event::Invite, Sfx::DuelInvite);
        }
        if (done) done(message, ok);
    });
}

namespace {
std::string DuelBody(const std::wstring& duelId) {
    Json::Value body = Json::Value::MakeObject();
    body.Set("p_duel", duelId);
    return body.Serialize();
}
} // namespace

void DuelsService::Join(const std::wstring& duelId, Done done) {
    Call(L"duel_join", DuelBody(duelId), [done = std::move(done)](const std::wstring& message, bool ok) {
        if (ok) PlaySound(Event::Join, Sfx::DuelJoin);
        if (done) done(message, ok);
    });
}
void DuelsService::Leave(const std::wstring& duelId, Done done) { Call(L"duel_leave", DuelBody(duelId), std::move(done)); }
void DuelsService::Forfeit(const std::wstring& duelId, Done done) { Call(L"duel_forfeit", DuelBody(duelId), std::move(done)); }

void DuelsService::Decline(const std::wstring& duelId, Done done) {
    // Gone from the list right away (after this frame: the list may be being drawn); the server agrees a moment later
    const unsigned gen = generation;
    Async::Post([this, gen, duelId] {
        if (gen != generation) return;
        invites.erase(std::remove_if(invites.begin(), invites.end(), [&](const Duel& d) { return d.id == duelId; }), invites.end());
        Redraw();
    });
    Call(L"duel_decline", DuelBody(duelId), std::move(done), true);
}

void DuelsService::Say(const std::wstring& duelId, const std::wstring& text, Done done) {
    const std::wstring trimmed = Text::Trim(text);
    if (trimmed.empty()) return;
    // Shown straight away; the refresh after the call brings the server's copy
    for (auto* duel : { &active, &result })
        if (*duel && (*duel)->id == duelId) {
            DuelChatMessage local;
            local.userId = AccountService::Get().UserId();
            local.name = NameOf(AccountService::Get().Me());
            local.text = trimmed.substr(0, 200);
            (*duel)->chat.push_back(std::move(local));
        }
    Redraw();
    Json::Value body = Json::Value::MakeObject();
    body.Set("p_duel", duelId);
    body.Set("p_text", trimmed.substr(0, 200));
    Call(L"duel_say", body.Serialize(), std::move(done), true);
}

void DuelsService::Invite(const std::wstring& duelId, const std::wstring& userId, Done done) {
    Json::Value body = Json::Value::MakeObject();
    body.Set("p_duel", duelId);
    body.Set("p_user", userId);
    Call(L"duel_invite", body.Serialize(), [this, userId, duelId, done = std::move(done)](const std::wstring& message, bool ok) {
        if (ok && challengedFor == duelId) challenged.insert(userId);
        if (ok) PlaySound(Event::Invite, Sfx::DuelInvite);
        if (done) done(message, ok);
    });
}

void DuelsService::Ban(const std::wstring& duelId, const std::string& categoryId, Done done) {
    Json::Value body = Json::Value::MakeObject();
    body.Set("p_duel", duelId);
    body.Set("p_category", categoryId);
    Call(L"duel_ban", body.Serialize(), std::move(done));
}

void DuelsService::Submit(const std::wstring& duelId, double seconds, Done done) {
    Json::Value body = Json::Value::MakeObject();
    body.Set("p_duel", duelId);
    body.Set("p_seconds", seconds);
    Call(L"duel_submit", body.Serialize(), std::move(done));
}

void DuelsService::Dismiss(const std::wstring& duelId) {
    dismissed.insert(duelId);   // a refresh already on its way may still bring it back
    const unsigned gen = generation;
    Async::Post([this, gen, duelId] {
        if (gen == generation && result && result->id == duelId) result.reset();
    });
    Call(L"duel_dismiss", DuelBody(duelId), nullptr, true);
    Redraw();
}

double DuelsService::ForfeitBlockedFor() const { return std::max(0.0, forfeitBlockedUntil - Async::Now()); }

std::vector<std::string> DuelsService::TitlesOf(const std::wstring& userId) const {
    std::vector<std::string> months;
    for (const auto& c : champions)
        if (c.profile.userId == userId) months.push_back(c.month);
    return months;
}

void DuelsService::LoadChampions() {
    if (!AccountService::Get().SignedIn() || championsLoading) return;
    championsLoading = true;
    const unsigned gen = generation;
    Async::Run([] { return AccountService::Get().Rpc(L"duel_champions", Json::Value::MakeObject()); }, [this, gen](Http::Response response) {
        if (gen != generation) return;
        championsLoading = false;
        bool ok = false;
        const Json::Value rows = response.Ok() ? Json::Parse(std::string_view(response.body), &ok) : Json::Value();
        if (!ok || !rows.IsArray()) return;   // (an older server without champions: nothing to show)
        championsLoaded = true;
        champions.clear();
        for (const auto& row : rows.Elements()) {
            DuelChampion c;
            c.month = row["month"].Str();
            c.profile = Profile::FromJson(row["user"]);
            c.mmr = static_cast<int>(row["mmr"].Int());
            c.wins = static_cast<int>(row["wins"].Int());
            c.losses = static_cast<int>(row["losses"].Int());
            champions.push_back(std::move(c));
        }
        Redraw();
    });
}

std::wstring DuelMonthName(const std::string& month) {
    int year = 0, mon = 0;
    if (month.size() == 7 && month[4] == '-') {
        year = std::atoi(month.substr(0, 4).c_str());
        mon = std::atoi(month.substr(5, 2).c_str());
    } else {
        SYSTEMTIME now{};
        GetSystemTime(&now);
        year = now.wYear;
        mon = now.wMonth;
    }
    if (mon < 1 || mon > 12) return Text::Widen(month);
    return Loc::Fmt("duel_month_name", Loc::Get("month_" + std::to_string(mon)), year);
}

std::wstring DuelChampionTitle(const std::string& month) { return Loc::Fmt("duel_champion_of", DuelMonthName(month)); }

int DuelMonthDaysLeft() {
    SYSTEMTIME now{};
    GetSystemTime(&now);
    static constexpr int kDays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int days = kDays[now.wMonth - 1];
    if (now.wMonth == 2 && ((now.wYear % 4 == 0 && now.wYear % 100 != 0) || now.wYear % 400 == 0)) days = 29;
    return std::max(1, days - now.wDay + 1);
}

void DuelsService::LoadLeaderboard() {
    LoadChampions();
    if (!AccountService::Get().SignedIn() || leaderboardLoading) return;
    leaderboardLoading = true;
    const unsigned gen = generation;
    Async::Run([] { return AccountService::Get().Rpc(L"duel_leaderboard", Json::Value::MakeObject()); },
               [this, gen](Http::Response response) {
                   if (gen != generation) return;
                   leaderboardLoading = false;
                   bool ok = false;
                   const Json::Value rows = response.Ok() ? Json::Parse(std::string_view(response.body), &ok) : Json::Value();
                   leaderboardFailed = !ok || !rows.IsArray();
                   if (!leaderboardFailed) {
                       leaderboard.clear();
                       for (const auto& row : rows.Elements()) {
                           DuelStanding s;
                           s.profile = Profile::FromJson(row);
                           s.mmr = static_cast<int>(row["mmr"].Int());
                           s.wins = static_cast<int>(row["wins"].Int());
                           s.losses = static_cast<int>(row["losses"].Int());
                           leaderboard.push_back(std::move(s));
                       }
                   }
                   Redraw();
               });
}

} // namespace SpeedrunLauncher::Services::Account
