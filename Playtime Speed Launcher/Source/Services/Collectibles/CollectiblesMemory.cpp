#include "Services/Collectibles/CollectiblesMemory.h"
#include <mutex>
#include <set>
#include "Services/Collectibles/CollectiblesData.h"
#include "Services/Collectibles/LiveInventory.h"
#include "Services/Collectibles/SaveGameService.h"
#include "Settings/ConfigFile.h"

namespace SpeedrunLauncher::Services::Collectibles::Memory {

namespace {

constexpr const wchar_t* kFile = L"collectibles_memory.json";

// Items are remembered by their first key in Data (Chapter 4's alternative keys count as the same one)
struct ChapterMemory {
    std::set<std::string> got, played;
    uint32_t pid = 0;      // the game process last read...
    int newGames = 0;      // ...and how many new games it had seen start then
};

std::mutex lock;
bool loaded = false;
ChapterMemory chapters[5];

ChapterMemory& Of(int chapter) { return chapters[chapter >= 1 && chapter <= 5 ? chapter - 1 : 4]; }

// { "Chapters": { "1": { "Got": [...], "Played": [...] }, ... } }
void Load() {
    if (loaded) return;
    loaded = true;
    const Json::Value json = Config::ReadJson(kFile);
    const Json::Value& all = json["Chapters"];
    for (const int n : Data::kChapters) {
        const Json::Value& c = all[std::to_string(n)];
        for (const Json::Value& v : c["Got"].Elements()) Of(n).got.insert(v.Str());
        for (const Json::Value& v : c["Played"].Elements()) Of(n).played.insert(v.Str());
    }
}

void Save() {
    Json::Value all = Json::Value::MakeObject();
    for (const int n : Data::kChapters) {
        Json::Value got = Json::Value::MakeArray(), played = Json::Value::MakeArray();
        for (const std::string& k : Of(n).got) got.Push(k);
        for (const std::string& k : Of(n).played) played.Push(k);
        Json::Value c = Json::Value::MakeObject();
        c.Set("Got", got);
        c.Set("Played", played);
        all.Set(std::to_string(n), c);
    }
    Json::Value json = Json::Value::MakeObject();
    json.Set("Chapters", all);
    Config::WriteJson(kFile, json);
}

// What the live text has, added to the memory. True when something new was added.
bool Merge(int chapter, ChapterMemory& m, const std::string& text) {
    const Data::Chapter& data = Data::Get(chapter);
    const auto have = Data::Collected(data, text);
    bool changed = false;
    for (size_t c = 0; c < data.cats.size(); ++c) {
        const Data::Category& cat = data.cats[c];
        const bool tapes = Data::TracksPlayed(data) && std::string_view(cat.id) == "vhs";
        for (size_t i = 0; i < cat.items.size(); ++i) {
            const Data::Item& item = cat.items[i];
            const std::string id = item.keys.front();
            if (have[c][i]) changed |= m.got.insert(id).second;
            if (!tapes) continue;
            for (const char* key : item.keys)
                if (text.find(std::string(1, '\0') + "played:" + key + std::string(1, '\0')) != std::string::npos) {
                    changed |= m.played.insert(id).second;
                    changed |= m.got.insert(id).second;   // played: got for sure
                    break;
                }
        }
    }
    return changed;
}

// The memory as an inventory text: "\0<key>\0" (Chapters 1 to 4) / "/<key>." (Chapter 5) per item,
// "\0played:<key>\0" per tape played
std::string Text(int chapter, const ChapterMemory& m) {
    std::string text;
    for (const std::string& k : m.got) text += chapter == 5 ? "/" + k + "." : std::string(1, '\0') + k + std::string(1, '\0');
    for (const std::string& k : m.played) text += std::string(1, '\0') + "played:" + k + std::string(1, '\0');
    return text;
}

} // namespace

Result Read(int chapter) {
    // Outside the lock: reading the game takes a moment
    const auto live = Live::Read(chapter);
    const std::string along = live ? SaveGames::AlongLive(chapter) : std::string();   // Chapter 2's trophies
    std::lock_guard guard(lock);
    Load();
    ChapterMemory& m = Of(chapter);
    Result r;
    if (live) {
        r.live = true;
        bool changed = false;
        // Another run of the game: nothing seen yet from it
        if (live->pid != m.pid) {
            m.pid = live->pid;
            m.newGames = 0;
        }
        // A new game started: the run starts over
        if (live->newGames > m.newGames) {
            m.newGames = live->newGames;
            changed = !m.got.empty() || !m.played.empty();
            m.got.clear();
            m.played.clear();
        }
        changed |= Merge(chapter, m, live->text + along);
        if (changed) Save();
    }
    r.any = !m.got.empty();
    r.text = Text(chapter, m);
    return r;
}

void Reset(int chapter) {
    Live::Forget(chapter);
    std::lock_guard guard(lock);
    Load();
    ChapterMemory& m = Of(chapter);
    m.got.clear();
    m.played.clear();
    Save();
}

void ResetAll() {
    for (const int n : Data::kChapters) Live::Forget(n);
    std::lock_guard guard(lock);
    Load();
    for (ChapterMemory& m : chapters) {
        m.got.clear();
        m.played.clear();
    }
    Save();
}

} // namespace SpeedrunLauncher::Services::Collectibles::Memory
