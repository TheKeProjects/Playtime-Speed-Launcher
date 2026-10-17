#pragma once
#include <functional>
#include <map>
#include <mutex>
#include <string>

namespace SpeedrunLauncher::Services {

// Per-chapter playtime kept in Supabase (keyed by the install id), so a bad local write can't
// wipe the hours. Increments are queued and pushed in the background.
class ChapterPlaytimeStore {
public:
    // Starts the initial download; `onLoaded` runs on the UI thread when the totals arrive.
    void Load(std::function<void()> onLoaded);

    double Seconds(int chapter) const;
    void AddSeconds(int chapter, double seconds);

    static std::wstring Format(double seconds);   // "3h 12m" / "45m" / "0m"

private:
    void MigrateLegacyLocalFile();
    void Flush();

    std::wstring installId;
    mutable std::mutex mutex;
    std::map<int, double> seconds;
    std::map<int, double> pending;
    bool syncing = false;
};

} // namespace SpeedrunLauncher::Services
