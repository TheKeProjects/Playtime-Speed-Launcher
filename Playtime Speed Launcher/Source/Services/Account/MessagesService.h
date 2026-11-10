#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "Services/Account/Profile.h"

// Private messages between friends (Tools/SupabaseMessages.sql): the conversations, the one being
// read (polled faster and marked as read) and sending. A toast for new messages comes through
// onNotify (MainWindow shows it).
namespace SpeedrunLauncher::Services::Account {

struct DirectMessage {
    long long id = 0;            // 0 = ours, not confirmed by the server yet
    bool mine = false;
    bool read = false;           // ours: they've read it
    std::wstring text;
    double ago = 0.0;            // seconds, when fetched
    double fetchedAt = 0.0;      // Async::Now() then
};

struct ChatThread {
    Profile profile;
    double secondsAgo = -1.0;    // presence (like Friend)
    int playing = 0;
    long long lastId = 0;
    std::wstring last;           // the last message
    bool lastMine = false;
    double ago = 0.0, fetchedAt = 0.0;
    int unread = 0;

    bool Online() const { return secondsAgo >= 0.0 && secondsAgo < 150.0; }
};

class MessagesService {
public:
    static MessagesService& Get();
    // Starts the poll timer (call once at startup)
    void Initialize();
    // Signed in / out (AccountService::onAccountChanged)
    void AccountChanged();

    const std::vector<ChatThread>& Threads() const { return threads; }
    int Unread() const { return unread; }
    bool Loaded() const { return loaded; }
    bool Failed() const { return failed; }

    // The conversation on screen ("" = none): polled every few seconds, what they send is marked read
    void Open(const std::wstring& userId);
    const std::wstring& OpenWith() const { return openWith; }
    const std::vector<DirectMessage>& Messages() const { return messages; }
    bool HistoryLoaded() const { return historyLoaded; }
    // The messages page is on screen: the conversations refresh faster
    void SetWatching(bool on);

    using Done = std::function<void(const std::wstring& message, bool ok)>;
    void Send(const std::wstring& toUserId, const std::wstring& text, Done done);
    void Refresh();

    // A new message from someone (not while reading their conversation): title, text
    std::function<void(const std::wstring& title, const std::wstring& text)> onNotify;

private:
    MessagesService() = default;
    void LoadHistory();

    bool initialized = false, loaded = false, failed = false, refreshing = false, historyBusy = false, historyLoaded = false, watching = false;
    unsigned generation = 0, openGeneration = 0;
    int ticks = 0;
    int unread = 0;
    std::vector<ChatThread> threads;
    std::map<std::wstring, long long> seenLast;   // the newest message id toasted per person
    std::wstring openWith;
    std::vector<DirectMessage> messages;
    long long lastId = 0;
};

} // namespace SpeedrunLauncher::Services::Account
