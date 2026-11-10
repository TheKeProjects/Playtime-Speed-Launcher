#include "Services/Account/CommunityRanking.h"
#include <windows.h>
#include <map>
#include <mutex>
#include <set>
#include "App/Config.h"
#include "Core/Http.h"
#include "Core/Json.h"
#include "Core/Text.h"
#include "Services/Community/Supabase.h"

namespace SpeedrunLauncher::Services::Account::Community {

namespace {

Json::Value GetJson(const std::wstring& url, bool* ok = nullptr) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        const Http::Response r = Http::Get(url, { { L"User-Agent", Config::UserAgent } }, 20000);
        if (r.Ok()) {
            if (ok) *ok = true;
            return Json::Parse(std::string_view(r.body));
        }
        if (r.status == 404 || r.status == 400) break;
        Sleep(700 * (attempt + 1));   // speedrun.com rate limits (420 / 429) and Cloudflare hiccups
    }
    if (ok) *ok = false;
    return {};
}

std::set<std::wstring> ChapterGames() {
    std::set<std::wstring> games;
    for (const auto& [chapter, id] : Config::SpeedrunCom::ChapterGameIds()) games.insert(id);
    return games;
}

// The names of a game's categories, levels and subcategory variables (fetched once per game)
struct GameNames {
    bool ok = false;
    std::map<std::wstring, std::wstring> categories, levels;
    struct Variable {
        std::wstring id;
        std::map<std::wstring, std::wstring> values;   // value id -> label
    };
    std::vector<Variable> subcategories;   // in speedrun.com's order
};

GameNames FetchGameNames(const std::wstring& gameId) {
    GameNames names;
    const std::wstring base = Config::SpeedrunCom::ApiUrl + std::wstring(L"/games/") + Text::UrlEncode(gameId);
    bool ok = false;
    const Json::Value categories = GetJson(base + L"/categories", &ok);
    if (!ok) return names;
    for (const auto& c : categories["data"].Elements()) names.categories[c["id"].WStr()] = c["name"].WStr();
    const Json::Value levels = GetJson(base + L"/levels");
    for (const auto& l : levels["data"].Elements()) names.levels[l["id"].WStr()] = l["name"].WStr();
    const Json::Value variables = GetJson(base + L"/variables");
    for (const auto& v : variables["data"].Elements()) {
        if (!v["is-subcategory"].Bool(false)) continue;
        GameNames::Variable variable{ v["id"].WStr(), {} };
        for (const auto& [id, value] : v["values"]["values"].Items())
            variable.values[Text::Widen(id)] = value["label"].IsString() ? value["label"].WStr() : Text::Widen(id);
        names.subcategories.push_back(std::move(variable));
    }
    names.ok = true;
    return names;
}

GameNames NamesOf(const std::wstring& gameId) {
    static std::mutex lock;
    static std::map<std::wstring, GameNames> cache;
    {
        std::lock_guard<std::mutex> guard(lock);
        auto it = cache.find(gameId);
        if (it != cache.end() && it->second.ok) return it->second;
    }
    GameNames names = FetchGameNames(gameId);
    std::lock_guard<std::mutex> guard(lock);
    GameNames& slot = cache[gameId];
    if (names.ok || !slot.ok) slot = std::move(names);
    return slot;
}

std::wstring LabelOf(const PersonalBest& pb, const GameNames& names) {
    if (!names.ok) return L"";
    std::wstring label;
    const auto add = [&](const std::wstring& part) {
        if (part.empty()) return;
        if (!label.empty()) label += L"  \x00B7  ";
        label += part;
    };
    if (auto level = names.levels.find(pb.levelId); !pb.levelId.empty() && level != names.levels.end()) add(level->second);
    if (auto category = names.categories.find(pb.categoryId); category != names.categories.end()) add(category->second);
    for (const auto& variable : names.subcategories) {
        auto value = pb.values.find(variable.id);
        if (value == pb.values.end()) continue;
        if (auto name = variable.values.find(value->second); name != variable.values.end()) add(name->second);
    }
    return label;
}

} // namespace

std::optional<std::vector<Profile>> FetchMembers() {
    const Http::Response response = Supabase::Rpc(L"launcher_community", Json::Value::MakeObject());
    if (!response.Ok()) return std::nullopt;
    bool ok = false;
    const Json::Value rows = Json::Parse(std::string_view(response.body), &ok);
    if (!ok || !rows.IsArray()) return std::nullopt;
    std::vector<Profile> out;
    for (const auto& row : rows.Elements()) out.push_back(Profile::FromJson(row));
    return out;
}

RunnerData FetchRunner(const std::wstring& speedrunId) {
    RunnerData data;
    const std::set<std::wstring> games = ChapterGames();
    const std::wstring api = Config::SpeedrunCom::ApiUrl;
    const std::wstring user = Text::UrlEncode(speedrunId);

    bool ok = false;
    const Json::Value pbs = GetJson(api + L"/users/" + user + L"/personal-bests", &ok);
    if (!ok) return data;
    for (const auto& entry : pbs["data"].Elements()) {
        const Json::Value& run = entry["run"];
        PersonalBest pb;
        pb.gameId = run["game"].WStr();
        if (!games.count(pb.gameId)) continue;
        pb.categoryId = run["category"].WStr();
        for (const auto& [var, value] : run["values"].Items()) pb.values[Text::Widen(var)] = value.WStr();
        pb.time = run["times"]["primary_t"].Number();
        pb.place = static_cast<int>(entry["place"].Int());
        pb.date = run["date"].IsString() ? run["date"].WStr() : L"";
        pb.weblink = run["weblink"].WStr();
        pb.levelId = run["level"].IsString() ? run["level"].WStr() : L"";
        for (const auto& link : run["videos"]["links"].Elements()) {
            if (link["uri"].IsString()) {
                pb.videoUrl = link["uri"].WStr();
                break;
            }
        }
        if (pb.time > 0.0) data.pbs.push_back(std::move(pb));
    }
    // Every record named (its game's names are fetched once and cached)
    std::map<std::wstring, GameNames> names;   // one lookup per game, even when it fails
    for (auto& pb : data.pbs) {
        auto it = names.find(pb.gameId);
        if (it == names.end()) it = names.emplace(pb.gameId, NamesOf(pb.gameId)).first;
        pb.label = LabelOf(pb, it->second);
    }

    // Verified runs per chapter (a few pages at most)
    for (int offset = 0; offset < 1000; offset += 200) {
        bool pageOk = false;
        const Json::Value page = GetJson(api + L"/runs?user=" + user + L"&status=verified&max=200&offset=" + std::to_wstring(offset), &pageOk);
        if (!pageOk) break;
        const auto& runs = page["data"].Elements();
        for (const auto& run : runs) {
            const std::wstring game = run["game"].WStr();
            if (games.count(game)) ++data.verifiedRuns[game];
        }
        if (runs.size() < 200) break;
    }
    data.ok = true;
    return data;
}

const PersonalBest* BestFor(const RunnerData& data, const SpeedrunCom::RunType& runType) {
    const PersonalBest* best = nullptr;
    for (const auto& pb : data.pbs) {
        if (pb.gameId != runType.gameId || pb.categoryId != runType.categoryId) continue;
        const auto value = [&](const std::wstring& var) {
            auto it = pb.values.find(var);
            return it == pb.values.end() ? std::wstring() : it->second;
        };
        if (!runType.primaryVarId.empty() && value(runType.primaryVarId) != runType.primaryValueId) continue;
        bool matches = true;
        for (const auto& v : runType.secondaryVariables) {
            const std::wstring got = value(v.id);
            if (!v.selectedValueId.empty() && !got.empty() && got != v.selectedValueId) {
                matches = false;
                break;
            }
        }
        if (!matches) continue;
        if (!best || pb.time < best->time) best = &pb;
    }
    return best;
}

} // namespace SpeedrunLauncher::Services::Account::Community
