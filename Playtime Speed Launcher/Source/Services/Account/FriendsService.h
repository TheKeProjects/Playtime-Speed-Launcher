#pragma once
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Services/Account/Profile.h"

// Friends of the launcher account (UI-thread object; the network runs on workers).
// Server side: Tools\SupabaseAccounts.sql (friend codes, requests, friendships, presence).
// While signed in, every minute the launcher says what's running (launcher_heartbeat) and
// refreshes the list, so friends see each other online and what they're playing.
namespace SpeedrunLauncher::Services::Account {

struct Friend {
    enum class Relation { Friend, Incoming, Outgoing, None };
    Relation relation = Relation::Friend;
    Profile profile;
    std::wstring friendCode;        // friends only
    double secondsAgo = -1.0;       // since they were last seen (-1 = never)
    int playing = 0;                // chapter number / multiplayer game id, 0 = none

    bool Online() const { return secondsAgo >= 0.0 && secondsAgo < 150.0; }
};

class FriendsService {
public:
    static FriendsService& Get();
    // Starts the heartbeat / refresh timer (call once at startup)
    void Initialize();
    // Signed in / out (AccountService::onAccountChanged)
    void AccountChanged();

    const std::wstring& MyCode() const { return myCode; }
    const std::vector<Friend>& Friends() const { return friends; }
    const std::vector<Friend>& Incoming() const { return incoming; }
    const std::vector<Friend>& Outgoing() const { return outgoing; }
    bool Loaded() const { return loaded; }
    bool Failed() const { return failed; }   // the last refresh got no answer
    bool Busy() const { return busy; }
    // Requests waiting for an answer (the account button's badge)
    int PendingCount() const { return static_cast<int>(incoming.size()); }
    // How we're related to someone (for the ranking's "add friend" button)
    bool IsFriend(const std::wstring& userId) const;
    bool HasRequest(const std::wstring& userId) const;   // sent or received

    void Refresh();
    // Callbacks get a translated message and whether it worked
    using Done = std::function<void(const std::wstring& message, bool ok)>;
    void SendByCode(const std::wstring& code, Done done);
    void SendToUser(const std::wstring& userId, Done done);
    void Respond(const std::wstring& userId, bool accept, Done done);
    void Remove(const std::wstring& userId, Done done);   // unfriend or withdraw a request
    // People by name (display name or speedrun.com name) or friend code; `done` gets nullopt when
    // the search failed. Only the latest search answers.
    void Search(const std::wstring& query, std::function<void(std::optional<std::vector<Friend>> results)> done);

    // "Playing Chapter 3" / "Online" / "Seen 5 min ago" / "Offline"
    static std::wstring StatusText(const Friend& f);

private:
    FriendsService() = default;
    void Heartbeat();
    void Send(const std::wstring& code, const std::wstring& userId, Done done);
    void Simple(const std::wstring& function, const std::wstring& body, Done done);

    std::wstring myCode;
    std::vector<Friend> friends, incoming, outgoing;
    bool loaded = false, failed = false, busy = false, refreshing = false;
    unsigned generation = 0;
    unsigned searchGeneration = 0;
    bool initialized = false;
};

} // namespace SpeedrunLauncher::Services::Account
