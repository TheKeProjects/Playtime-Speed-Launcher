#include "Services/Community/SpeedrunComService.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include "App/Config.h"
#include "Core/Http.h"
#include "Core/Json.h"
#include "Core/Text.h"
#include "Services/Community/VideoTutorialService.h"

namespace SpeedrunLauncher::Services::SpeedrunCom {

namespace {

constexpr const wchar_t* kApi = Config::SpeedrunCom::ApiUrl;

Json::Value GetJson(const std::wstring& url) {
    const Http::Response r = Http::Get(url, { { L"User-Agent", Config::UserAgent } }, 20000);
    if (!r.Ok()) return {};
    return Json::Parse(std::string_view(r.body));
}

// Pictures in a run comment (speedrun.com markdown, "![alt](/static/blob/x.jpg)"): takes them out of
// the text and returns their absolute URLs
std::vector<std::wstring> TakeCommentImages(std::wstring& comment) {
    std::vector<std::wstring> urls;
    for (size_t start; (start = comment.find(L"![")) != std::wstring::npos;) {
        const size_t open = comment.find(L"](", start);
        const size_t close = open == std::wstring::npos ? std::wstring::npos : comment.find(L')', open);
        if (close == std::wstring::npos) break;
        std::wstring url = Text::Trim(comment.substr(open + 2, close - open - 2));
        url = url.substr(0, url.find(L' '));   // drops an optional "title"
        if (Text::StartsWith(url, L"//")) url = L"https:" + url;
        else if (Text::StartsWith(url, L"/")) url = L"https://www.speedrun.com" + url;
        if (Text::StartsWith(url, L"http", true)) urls.push_back(url);
        comment.erase(start, close + 1 - start);
    }
    comment = Text::Trim(comment);
    return urls;
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

// Days since "YYYY-MM-DD" (-1 when it can't be read)
int DaysSince(const std::wstring& date) {
    int y = 0, m = 0, d = 0;
    if (swscanf_s(date.c_str(), L"%d-%d-%d", &y, &m, &d) != 3) return -1;
    std::tm t{};
    t.tm_year = y - 1900;
    t.tm_mon = m - 1;
    t.tm_mday = d;
    t.tm_hour = 12;
    const std::time_t then = std::mktime(&t);
    if (then == -1) return -1;
    return std::max(0, static_cast<int>(std::difftime(std::time(nullptr), then) / 86400.0));
}

// "#E77471" → 0xE77471 (0 when missing / malformed)
uint32_t ParseHexColor(const std::string& text) {
    if (text.size() != 7 || text[0] != '#') return 0;
    uint32_t value = 0;
    for (size_t i = 1; i < text.size(); ++i) {
        const char c = text[i];
        const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (digit < 0) return 0;
        value = value * 16 + static_cast<uint32_t>(digit);
    }
    return value;
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
    struct Player {
        std::wstring name, weblink, country, countryName, avatar;
        uint32_t color = 0;
    };
    std::map<std::wstring, Player> players;
    for (const auto& p : data["players"]["data"].Elements()) {
        const std::wstring id = p["id"].WStr();
        if (id.empty()) continue;
        Player info;
        info.name = p["names"]["international"].IsString() ? p["names"]["international"].WStr() : id;
        info.weblink = p["weblink"].WStr();
        info.country = p["location"]["country"]["code"].WStr();
        info.countryName = p["location"]["country"]["names"]["international"].WStr();
        const Json::Value& image = p["assets"]["image"]["uri"];
        info.avatar = image.IsString() ? image.WStr() : L"";
        // Name style: a solid colour, or the start of a gradient ("dark" reads best on the dark UI)
        const Json::Value& style = p["name-style"];
        const Json::Value& color = style["style"].Str() == "gradient" ? style["color-from"] : style["color"];
        info.color = ParseHexColor(color["dark"].Str());
        players[id] = std::move(info);
    }
    for (const auto& entry : data["runs"].Elements()) {
        const int place = static_cast<int>(entry["place"].Int());
        if (place <= 0) continue;
        const Json::Value& run = entry["run"];
        LeaderboardEntry e;
        e.place = place;
        std::vector<std::wstring> names;
        for (const auto& pl : run["players"].Elements()) {
            if (pl["rel"].Str() == "guest" && pl.Has("name")) {
                names.push_back(pl["name"].IsString() ? pl["name"].WStr() : L"Guest");
            } else if (pl.Has("id")) {
                const std::wstring pid = pl["id"].WStr();
                auto found = players.find(pid);
                if (found == players.end()) {
                    names.push_back(pid);
                    continue;
                }
                names.push_back(found->second.name);
                if (e.playerWeblink.empty()) {   // details of the first runner
                    e.playerWeblink = found->second.weblink;
                    e.country = found->second.country;
                    e.countryName = found->second.countryName;
                    e.nameColor = found->second.color;
                    e.avatarUrl = found->second.avatar;
                }
            }
        }
        e.playerName = names.empty() ? L"Unknown" : Text::Join(names, L" & ");
        e.timeSeconds = run["times"]["primary_t"].Number();
        e.runWeblink = run["weblink"].WStr();
        e.date = run["date"].IsString() ? run["date"].WStr() : L"";
        e.comment = run["comment"].IsString() ? Text::Trim(run["comment"].WStr()) : L"";
        e.commentImages = TakeCommentImages(e.comment);
        for (const auto& link : run["videos"]["links"].Elements()) {
            const std::wstring uri = link["uri"].WStr();
            if (!uri.empty()) {
                e.videoUrl = uri;
                break;
            }
        }
        // What the window shows every frame, worked out once here (regexes, date maths)
        e.daysAgo = DaysSince(e.date);
        if (!e.videoUrl.empty()) {
            e.youtubeId = VideoTutorials::ExtractVideoId(e.videoUrl).value_or(L"");
            const std::wstring lower = Text::Lower(e.videoUrl);
            e.videoSite = lower.find(L"youtu") != std::wstring::npos      ? L"YouTube"
                          : lower.find(L"twitch") != std::wstring::npos   ? L"Twitch"
                          : lower.find(L"bilibili") != std::wstring::npos ? L"Bilibili"
                                                                          : L"";   // unknown site: the window says "the browser" in the launcher's language
        }
        if (!e.country.empty())
            e.flagUrl = L"https://www.speedrun.com/images/flags/" + Text::Lower(e.country.substr(0, e.country.find(L'/'))) + L".png";   // "ca/qc" → ca
        results.push_back(std::move(e));
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
