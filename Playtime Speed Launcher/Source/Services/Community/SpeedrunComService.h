#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// speedrun.com API v1: run types (Any% / 100% / ...) for a chapter's PC leaderboard and the top 10.
// Blocking calls: run on worker threads.
namespace SpeedrunLauncher::Services::SpeedrunCom {

struct VariableValue {
    std::wstring id, label;
};

// A secondary subcategory variable (Route, Restriction, Game Version, ...) shown as a selector.
struct Variable {
    std::wstring id, name;
    std::vector<VariableValue> values;
    std::wstring selectedValueId;
};

struct RunType {
    std::wstring gameId, categoryId, label;
    std::wstring primaryVarId, primaryValueId;
    std::vector<Variable> secondaryVariables;
};

struct LeaderboardEntry {
    int place = 0;
    std::wstring playerName;
    double timeSeconds = 0.0;
    std::wstring runWeblink;
    std::wstring videoUrl;        // first video attached to the run (YouTube / Twitch / ...), may be empty
    std::wstring date;            // "2026-03-14" (when it was played), may be empty
    std::wstring comment;         // the runner's comment on the run
    std::wstring country;         // ISO code of the (first) runner's country, "us"; empty for guests / unset
    std::wstring countryName;     // "United States"
    std::wstring playerWeblink;   // speedrun.com profile of the (first) runner
    uint32_t nameColor = 0;       // 0xRRGGBB of the runner's name style on speedrun.com (0 = none)

    // Derived once when fetched (the window draws these every frame)
    int daysAgo = -1;             // days since `date` (-1 = unknown)
    std::wstring youtubeId;       // the video's YouTube id when it's on YouTube (plays inside the launcher)
    std::wstring videoSite;       // "YouTube" / "Twitch" / ... for labels
    std::wstring flagUrl;         // speedrun.com picture of `country`'s flag
};

const std::map<int, std::wstring>& ChapterGameIds();
std::vector<RunType> GetRunTypes(int chapter);
std::wstring BuildVarQuery(const RunType& runType);
std::vector<LeaderboardEntry> GetTopRunners(const RunType& runType);
std::wstring FormatTime(double seconds);

} // namespace SpeedrunLauncher::Services::SpeedrunCom
