#include "Services/Community/SpeedrunComService.h"
#include <cmath>
#include <cstdio>
#include "App/Config.h"
#include "Core/Http.h"
#include "Core/Json.h"
#include "Core/Text.h"

namespace SpeedrunLauncher::Services::SpeedrunCom {

namespace {

constexpr const wchar_t* kApi = Config::SpeedrunCom::ApiUrl;

Json::Value GetJson(const std::wstring& url) {
    const Http::Response r = Http::Get(url, { { L"User-Agent", Config::UserAgent } }, 20000);
    if (!r.Ok()) return {};
    return Json::Parse(std::string_view(r.body));
}

std::vector<VariableValue> Values(const Json::Value& valuesObj) {
    std::vector<VariableValue> out;
    for (const auto& [id, value] : valuesObj["values"].Items()) {
        const std::wstring wid = Text::Widen(id);
        const std::wstring label = value["label"].IsString() ? value["label"].WStr() : wid;
        out.push_back({ wid, label });
    }
    return out;
}

} // namespace

const std::map<int, std::wstring>& ChapterGameIds() { return Config::SpeedrunCom::ChapterGameIds(); }

std::vector<RunType> GetRunTypes(int chapter) {
    std::vector<RunType> results;
    auto it = ChapterGameIds().find(chapter);
    if (it == ChapterGameIds().end()) return results;
    const std::wstring gameId = it->second;
    const Json::Value doc = GetJson(std::wstring(kApi) + L"/games/" + gameId + L"/categories?embed=variables");

    // The "PC" per-game category, else the first per-game one
    const Json::Value* category = nullptr;
    for (const auto& cat : doc["data"].Elements()) {
        if (cat["type"].Str() != "per-game") continue;
        if (Text::Equals(cat["name"].WStr(), L"PC", true)) {
            category = &cat;
            break;
        }
        if (!category) category = &cat;
    }
    if (!category) return results;
    const std::wstring categoryId = (*category)["id"].WStr();
    const std::wstring categoryName = (*category)["name"].WStr();

    const Json::Value* primary = nullptr;
    std::vector<Variable> secondary;
    for (const auto& v : (*category)["variables"]["data"].Elements()) {
        if (!v["is-subcategory"].Bool(false)) continue;
        const std::wstring name = v["name"].WStr();
        // Any% / 100% split is always "Category" or "Subcategory"; the rest become selectors
        if (!primary && (Text::Equals(name, L"Category", true) || Text::Equals(name, L"Subcategory", true))) {
            primary = &v;
            continue;
        }
        auto values = Values(v["values"]);
        if (values.empty()) continue;
        const Json::Value& def = v["values"]["default"];
        secondary.push_back({ v["id"].WStr(), name, values, def.IsString() ? def.WStr() : values[0].id });
    }

    if (!primary) {
        results.push_back({ gameId, categoryId, categoryName, L"", L"", secondary });
        return results;
    }
    const std::wstring primaryId = (*primary)["id"].WStr();
    for (const auto& value : Values((*primary)["values"]))
        results.push_back({ gameId, categoryId, value.label, primaryId, value.id, secondary });   // each owns its selectors
    return results;
}

std::wstring BuildVarQuery(const RunType& runType) {
    std::vector<std::wstring> parts;
    if (!runType.primaryVarId.empty() && !runType.primaryValueId.empty())
        parts.push_back(L"var-" + runType.primaryVarId + L"=" + runType.primaryValueId);
    for (const auto& v : runType.secondaryVariables)
        if (!v.selectedValueId.empty()) parts.push_back(L"var-" + v.id + L"=" + v.selectedValueId);
    return Text::Join(parts, L"&");
}

std::vector<LeaderboardEntry> GetTopRunners(const RunType& runType) {
    std::vector<LeaderboardEntry> results;
    const std::wstring query = BuildVarQuery(runType);
    const Json::Value doc = GetJson(std::wstring(kApi) + L"/leaderboards/" + runType.gameId + L"/category/" + runType.categoryId +
                                    L"?top=10&embed=players" + (query.empty() ? L"" : L"&" + query));
    const Json::Value& data = doc["data"];
    std::map<std::wstring, std::wstring> names;
    for (const auto& p : data["players"]["data"].Elements()) {
        const std::wstring id = p["id"].WStr();
        if (id.empty()) continue;
        names[id] = p["names"]["international"].IsString() ? p["names"]["international"].WStr() : id;
    }
    for (const auto& entry : data["runs"].Elements()) {
        const int place = static_cast<int>(entry["place"].Int());
        if (place <= 0) continue;
        const Json::Value& run = entry["run"];
        std::vector<std::wstring> players;
        for (const auto& pl : run["players"].Elements()) {
            if (pl["rel"].Str() == "guest" && pl.Has("name")) players.push_back(pl["name"].IsString() ? pl["name"].WStr() : L"Guest");
            else if (pl.Has("id")) {
                const std::wstring pid = pl["id"].WStr();
                auto found = names.find(pid);
                players.push_back(found != names.end() ? found->second : pid);
            }
        }
        results.push_back({ place, players.empty() ? L"Unknown" : Text::Join(players, L" & "), run["times"]["primary_t"].Number(),
                            run["weblink"].WStr() });
        if (results.size() >= 10) break;
    }
    return results;
}

std::wstring FormatTime(double seconds) {
    // TimeSpan.FromSeconds truncates to 100 ns ticks (5:11.8159999 shows as 5:11.815)
    const long long totalMs = static_cast<long long>(seconds * 10000000.0) / 10000;
    const long long hours = totalMs / 3600000, minutes = (totalMs / 60000) % 60, secs = (totalMs / 1000) % 60, ms = totalMs % 1000;
    wchar_t buffer[48];
    if (hours >= 1) swprintf_s(buffer, L"%lld:%02lld:%02lld", hours, minutes, secs);
    else swprintf_s(buffer, L"%lld:%02lld.%03lld", minutes, secs, ms);
    return buffer;
}

} // namespace SpeedrunLauncher::Services::SpeedrunCom
