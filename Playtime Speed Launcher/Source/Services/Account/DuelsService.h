#pragma once
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Services/Account/Profile.h"

// Speedrun duels between launcher accounts (UI-thread object; the network runs on workers), like
// the Discord bot's matchmaking. Server side: Tools\SupabaseDuels.sql.
//   A lobby (1v1 … 1v1v1v1v1v1) is opened for everyone online or only for friends, and people can be
//   challenged into it. Once it's full the race starts: a random chapter + category, the host's
//   pick, or a ban phase where the players take turns banning categories until one is rolled.
//   Everyone sends their time (or forfeits) and the MMR moves Elo-style.
// While the duels menu is open (or we're in a duel) the state is refreshed every few seconds,
// otherwise every half a minute so challenges still show up.
namespace SpeedrunLauncher::Services::Account {

// ── Rules shared with the server ──
struct DuelRank {
    const char* name;
    int min;          // MMR from which it applies
    unsigned color;   // 0xRRGGBB
    const wchar_t* icon;   // Assets\Images\Ranks\ (the Discord bot's rank emojis)
};
const std::vector<DuelRank>& DuelRanks();
const DuelRank& DuelRankFor(int mmr);
const DuelRank* NextDuelRank(int mmr);   // nullptr at the top

struct DuelCategory {
    const char* id;          // "c3_2" (the server's ids)
    int chapter;
    const wchar_t* label;    // "Any% Inbounds"
};
const std::vector<DuelCategory>& DuelCategories();
const DuelCategory* FindDuelCategory(const std::string& id);

inline constexpr int kDuelKBase = 20, kDuelKPlacement = 40, kDuelPlacementMatches = 5;
inline constexpr int kDuelMinSlots = 2, kDuelMaxSlots = 6;
inline constexpr double kDuelLobbySeconds = 300.0;
const std::vector<int>& DuelBanCounts();
// Chance that a player with MMR `a` beats one with MMR `b`
double DuelExpected(int a, int b);

// "1:23.456" / "1:03:23.456" / "54.321"
std::wstring FormatDuelTime(double seconds);
std::optional<double> ParseDuelTime(const std::wstring& text);
// "1v1v1"
std::wstring DuelModeLabel(int slots);

struct DuelPlayer {
    Profile profile;
    int seat = 0;
    double seconds = -1.0;     // their time, -1 = not sent (0 = last player standing)
    bool forfeited = false;
    bool done = false;
    int mmr = 0, wins = 0, losses = 0;
    int mmrBefore = 0, mmrChange = 0, placement = 0;   // once finished
    double secondsAgo = -1.0;
    int playing = 0;

    bool Online() const { return secondsAgo >= 0.0 && secondsAgo < 150.0; }
};

// A message in a duel's room chat
struct DuelChatMessage {
    long long id = 0;            // 0 = sent by us, not confirmed by the server yet
    std::wstring userId, name, text;
    double ago = 0.0;            // seconds, when fetched
};

struct Duel {
    enum class Type { Random, Chapter, Bans };
    enum class Status { Open, Banning, Running, Finished, Expired, Cancelled };

    std::wstring id, code, hostId, winnerId;
    int slots = 2;
    Type type = Type::Random;
    int chapter = 0;              // 0 = not decided yet
    std::string category;         // id, empty = not decided / random
    int bans = 0;
    std::vector<int> banOrder;    // seat of each ban turn
    std::vector<std::string> banned;
    bool friendsOnly = false;
    Status status = Status::Open;
    std::vector<DuelPlayer> players;
    std::vector<Profile> invited;  // open rooms: invited, not in yet
    Profile from;                 // invites: who challenged us
    std::vector<DuelChatMessage> chat;   // the room's chat (only for our own duel / result)

    const DuelPlayer* Player(const std::wstring& userId) const;
    const DuelPlayer* Host() const { return Player(hostId); }
    const DuelPlayer* Winner() const { return Player(winnerId); }
    bool Full() const { return static_cast<int>(players.size()) >= slots; }
    // Ban phase: whose turn it is (nullptr once over)
    const DuelPlayer* BanTurn() const;
    bool IsBanned(const std::string& categoryId) const;
    // Live clocks (from the server's ages + the time since they were fetched)
    double LobbySecondsLeft() const;
    double RunningSeconds() const;
    // The players by placement (finished) or by time sent
    std::vector<const DuelPlayer*> Standings() const;

    double age = 0.0, runningFor = -1.0, fetchedAt = 0.0;
};

struct DuelOnlineUser {
    Profile profile;
    int mmr = 0, wins = 0, losses = 0;
    bool isFriend = false;
    bool inDuel = false;
    int playing = 0;
};

struct DuelStanding {
    Profile profile;
    int mmr = 0, wins = 0, losses = 0;
};

// A month's champion: the top of that month's ranking
struct DuelChampion {
    std::string month;           // "2026-11"
    Profile profile;
    int mmr = 0, wins = 0, losses = 0;
};

// "noviembre 2026" for "2026-11" (empty = the current month, UTC like the server)
std::wstring DuelMonthName(const std::string& month = "");
// "Campeón de noviembre 2026"
std::wstring DuelChampionTitle(const std::string& month);
// Days left in the current ranking month (UTC)
int DuelMonthDaysLeft();

// What "create" sends
struct DuelSetup {
    int slots = 2;
    Duel::Type type = Duel::Type::Random;
    int chapter = 1;
    std::string category;        // Chapter type: empty = a random category of the chapter
    int bans = 6;
    bool friendsOnly = false;
    std::wstring invite;         // challenge this user (empty = nobody)
};

class DuelsService {
public:
    static DuelsService& Get();
    // Starts the refresh timer (call once at startup)
    void Initialize();
    // Signed in / out (AccountService::onAccountChanged)
    void AccountChanged();
    // The duels menu is open: refresh every few seconds
    void SetWatching(bool on);

    bool Loaded() const { return loaded; }
    bool Failed() const { return failed; }
    bool Busy() const { return busy; }
    int MyMmr() const { return myMmr; }
    int MyWins() const { return myWins; }
    int MyLosses() const { return myLosses; }
    const std::optional<Duel>& Active() const { return active; }   // the duel we're in
    const std::optional<Duel>& Result() const { return result; }   // finished, not dismissed yet
    const std::vector<Duel>& Lobbies() const { return lobbies; }
    const std::vector<Duel>& Invites() const { return invites; }
    const std::vector<DuelOnlineUser>& Online() const { return online; }
    int OnlineCount() const { return onlineCount; }
    // The side rail's badge: challenges waiting + our turn to act in a duel
    int AlertCount() const;
    bool MyTurnToBan() const;
    bool NeedsMyTime() const;   // racing and we haven't sent a time
    // Challenged by us into the lobby we're in (the online list shows "challenged")
    bool Challenged(const std::wstring& userId) const { return challenged.count(userId) > 0; }

    const std::vector<DuelStanding>& Leaderboard() const { return leaderboard; }
    bool LeaderboardLoading() const { return leaderboardLoading; }
    bool LeaderboardFailed() const { return leaderboardFailed; }
    void LoadLeaderboard();
    // Every finished month's champion, newest first (loaded with the ranking and at sign in)
    const std::vector<DuelChampion>& Champions() const { return champions; }
    void LoadChampions();
    // The months someone was champion ("2026-10"...), newest first
    std::vector<std::string> TitlesOf(const std::wstring& userId) const;
    // Forfeit cooldown (2 forfeits in a row): seconds until we can forfeit again (0 = we can)
    double ForfeitBlockedFor() const;

    void Refresh();
    // Callbacks get a translated message (empty when there's nothing to say) and whether it worked
    using Done = std::function<void(const std::wstring& message, bool ok)>;
    // Opens a room with these settings (and invites setup.invite if set)
    void Create(const DuelSetup& setup, Done done);
    // The host: the room's settings, take someone out (or withdraw their invite), start with whoever is in
    void Update(const std::wstring& duelId, const DuelSetup& setup, Done done);
    void Kick(const std::wstring& duelId, const std::wstring& userId, Done done);
    void StartNow(const std::wstring& duelId, Done done);
    // The settings of a room as a DuelSetup (to change one of them)
    static DuelSetup SetupOf(const Duel& duel);
    void Join(const std::wstring& duelId, Done done);
    void Leave(const std::wstring& duelId, Done done);
    void Forfeit(const std::wstring& duelId, Done done);
    void Decline(const std::wstring& duelId, Done done);
    void Invite(const std::wstring& duelId, const std::wstring& userId, Done done);
    void Ban(const std::wstring& duelId, const std::string& categoryId, Done done);
    void Submit(const std::wstring& duelId, double seconds, Done done);
    void Dismiss(const std::wstring& duelId);
    // Says something in the duel's room chat (shows at once, confirmed by the next refresh)
    void Say(const std::wstring& duelId, const std::wstring& text, Done done);

    // What a toast / sound is about (Duels → Settings can turn each one off)
    enum class Event { Invite, Join, Start, Result, BanTurn, RivalFinished, Leave, Chat };

    // The duels menu is open (room chat messages then don't toast)
    bool Watching() const { return watching; }
    // Something worth a toast (challenge received, duel started, result): title + text
    std::function<void(const std::wstring& title, const std::wstring& text, unsigned color)> onNotify;

private:
    DuelsService() = default;
    void Call(const std::wstring& function, const std::string& bodyJson, Done done, bool quiet = false);
    void Notify(Event event, const std::wstring& title, const std::wstring& text, unsigned color, const wchar_t* sound);
    static void PlaySound(Event event, const wchar_t* sound);
    void Compare(const std::optional<Duel>& oldActive, const std::optional<Duel>& oldResult);

    bool loaded = false, failed = false, busy = false, refreshing = false, again = false;
    bool watching = false;
    int ticks = 0;
    unsigned generation = 0;
    int myMmr = 0, myWins = 0, myLosses = 0;
    std::optional<Duel> active, result;
    std::vector<Duel> lobbies, invites;
    std::vector<DuelOnlineUser> online;
    int onlineCount = 0;
    std::set<std::wstring> seenInvites;   // challenges already toasted
    std::set<std::wstring> dismissed;     // results already closed
    std::set<std::wstring> challenged;
    std::wstring challengedFor;           // the lobby `challenged` belongs to
    std::vector<DuelStanding> leaderboard;
    std::vector<DuelChampion> champions;
    bool championsLoading = false, championsLoaded = false;
    double forfeitBlockedUntil = 0.0;   // Async::Now() time
    bool leaderboardLoading = false, leaderboardFailed = false;
    bool initialized = false;
};

} // namespace SpeedrunLauncher::Services::Account
