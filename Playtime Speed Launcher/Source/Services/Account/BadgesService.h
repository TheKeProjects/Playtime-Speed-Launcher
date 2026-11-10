#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

// Everyone's badges (Tools/SupabaseBadges.sql): monthly duels champion, pioneer (an account from
// the start), contributor... Asked for when first needed, a batch of players at a time, and kept
// for a few minutes.
namespace SpeedrunLauncher::Services::Account {

struct Badge {
    std::string kind;            // "champion", "pioneer", "contributor"...
    std::string detail;          // champion: "2026-11"
    int mmr = 0, wins = 0, losses = 0;   // champion: that month's numbers
    std::string Id() const { return kind + ":" + detail; }   // what Profile::featuredBadge holds
};

class BadgesService {
public:
    static BadgesService& Get();
    void AccountChanged();

    // Their badges, newest first; nullptr while loading (the first call asks the server)
    const std::vector<Badge>* Of(const std::wstring& userId);
    // The one shown next to their name: the one they picked, else the newest (nullptr = none).
    // `featured` overrides their pick (e.g. the editor's unsaved choice); ours comes from the account.
    const Badge* Shown(const std::wstring& userId, const std::string* featured = nullptr);
    // Fetch again (after a badge was given or picked)
    void Forget(const std::wstring& userId);

private:
    BadgesService() = default;
    void Flush();

    struct Entry {
        std::vector<Badge> badges;
        std::string featured;
        double fetchedAt = -1e9;
        bool loaded = false;
    };
    std::map<std::wstring, Entry> cache;
    std::set<std::wstring> pending, inflight;
    bool flushQueued = false;
    unsigned generation = 0;
};

} // namespace SpeedrunLauncher::Services::Account
