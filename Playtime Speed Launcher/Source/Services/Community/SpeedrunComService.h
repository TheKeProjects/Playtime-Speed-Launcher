#pragma once
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
};

const std::map<int, std::wstring>& ChapterGameIds();
std::vector<RunType> GetRunTypes(int chapter);
std::wstring BuildVarQuery(const RunType& runType);
std::vector<LeaderboardEntry> GetTopRunners(const RunType& runType);
std::wstring FormatTime(double seconds);

} // namespace SpeedrunLauncher::Services::SpeedrunCom
