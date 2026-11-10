#pragma once
#include <functional>
#include <map>
#include <mutex>
#include <string>

namespace SpeedrunLauncher::Services {

// Per-chapter playtime kept in Supabase (keyed by the install id), so a bad local write can't
// wipe the hours. Increments are queued and pushed in the background.
//
// Signed in to a launcher account, the install's totals are also linked to the account (every
// minute while playing and when signing in), and the hours shown are the account's: everything
// played on every PC that account was used on, so formatting the PC doesn't lose them.
class ChapterPlaytimeStore {
public:
    // Starts the initial download; `onLoaded` runs on the UI thread when the totals arrive.
    void Load(std::function<void()> onLoaded);

    double Seconds(int chapter) const;
    void AddSeconds(int chapter, double seconds);

    // The launcher account signed in / out (UI thread)
    void AccountChanged();
    // Links this install's hours to the account and downloads the account's totals
    void SyncAccount();

    static std::wstring Format(double seconds);   // "3h 12m" / "45m" / "44s" / "0m"

private:
    void MigrateLegacyLocalFile();
    void Flush();

    std::wstring installId;
    mutable std::mutex mutex;
    std::map<int, double> seconds;           // this install
    std::map<int, double> pending;
    bool syncing = false;
    bool installLoaded = false;              // the install's totals came from the server

    // Account (UI thread, except under the mutex)
    bool accountLoaded = false;
    std::map<int, double> accountSeconds;    // the account's totals at the last sync
    std::map<int, double> linkedSeconds;     // this install's totals sent at that sync
    bool accountSyncing = false;
    bool accountSyncAgain = false;
    double lastAccountSync = -1e9;
    unsigned accountGeneration = 0;
    std::function<void()> onLoadedCallback;
};

} // namespace SpeedrunLauncher::Services
