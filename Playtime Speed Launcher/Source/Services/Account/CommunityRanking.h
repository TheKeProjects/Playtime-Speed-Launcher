#pragma once
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "Services/Account/Profile.h"
#include "Services/Community/SpeedrunComService.h"

// The community ranking: every launcher account with a linked speedrun.com account, ranked by
// their personal bests on each chapter's leaderboard (straight from speedrun.com, so only verified
// runs count). Blocking calls: worker threads only.
namespace SpeedrunLauncher::Services::Account::Community {

struct PersonalBest {
    std::wstring gameId, categoryId;
    std::map<std::wstring, std::wstring> values;   // variable id -> value id
    double time = 0.0;
    int place = 0;               // on speedrun.com's leaderboard
    std::wstring date, weblink, videoUrl;
    std::wstring levelId;
    // "PC · Any% · Unrestricted · 1.3": the level (individual levels), the category and its
    // subcategory values, named from speedrun.com (empty when the names couldn't be loaded)
    std::wstring label;
};

struct RunnerData {
    bool ok = false;
    std::vector<PersonalBest> pbs;              // only the chapters' games
    std::map<std::wstring, int> verifiedRuns;   // game id -> verified runs on speedrun.com
};

// Every profile (launcher_community); nullopt when the server can't be reached
std::optional<std::vector<Profile>> FetchMembers();

RunnerData FetchRunner(const std::wstring& speedrunId);

// The runner's best run for a leaderboard (its category, subcategory and selected variables)
const PersonalBest* BestFor(const RunnerData& data, const SpeedrunCom::RunType& runType);

} // namespace SpeedrunLauncher::Services::Account::Community
