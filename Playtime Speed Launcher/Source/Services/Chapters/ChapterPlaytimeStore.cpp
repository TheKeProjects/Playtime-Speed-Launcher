#include "Services/Chapters/ChapterPlaytimeStore.h"
#include "App/Paths.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/Json.h"
#include "Core/Text.h"
#include "Services/Community/Supabase.h"
#include "Settings/HeartbeatSettings.h"

namespace SpeedrunLauncher::Services {

void ChapterPlaytimeStore::Load(std::function<void()> onLoaded) {
    installId = Settings::Heartbeat::Current().installId;
    MigrateLegacyLocalFile();
    const std::wstring id = installId;
    Async::Run([this, id, onLoaded] {
        Json::Value body = Json::Value::MakeObject();
        body.Set("p_id", id);
        const Http::Response response = Supabase::Rpc(L"get_playtime", body);
        if (response.Ok()) {
            const Json::Value rows = Json::Parse(std::string_view(response.body));
            std::lock_guard lock(mutex);
            for (const auto& row : rows.Elements()) {
                const int chapter = static_cast<int>(row["chapter"].Int());
                // Seconds added locally while the request was in flight win over the snapshot
                if (!pending.count(chapter)) seconds[chapter] = row["seconds"].Number();
            }
        }
        Async::Post([onLoaded] {
            if (onLoaded) onLoaded();
        });
    });
}

// One-time upgrade from the old local playtime.json: upload what it had, then delete it.
void ChapterPlaytimeStore::MigrateLegacyLocalFile() {
    const std::wstring legacy = Paths::DataFile(L"playtime.json");
    auto bytes = Files::ReadBytes(legacy);
    if (!bytes) return;
    bool ok = false;
    const Json::Value data = Json::Parse(std::string_view(*bytes), &ok);
    if (!ok) return;
    for (const auto& [key, value] : data.Items()) {
        auto chapter = Text::ToInt(key);
        if (chapter && value.Number() > 0) AddSeconds(static_cast<int>(*chapter), value.Number());
    }
    Files::Delete(legacy);
}

double ChapterPlaytimeStore::Seconds(int chapter) const {
    std::lock_guard lock(mutex);
    auto it = seconds.find(chapter);
    return it == seconds.end() ? 0.0 : it->second;
}

void ChapterPlaytimeStore::AddSeconds(int chapter, double amount) {
    {
        std::lock_guard lock(mutex);
        seconds[chapter] += amount;
        pending[chapter] += amount;
        if (syncing) return;
        syncing = true;
    }
    Async::Run([this] { Flush(); });
}

void ChapterPlaytimeStore::Flush() {
    while (true) {
        int chapter = 0;
        double amount = 0.0;
        std::wstring id;
        {
            std::lock_guard lock(mutex);
            if (pending.empty()) {
                syncing = false;
                return;
            }
            chapter = pending.begin()->first;
            amount = pending.begin()->second;
            pending.erase(pending.begin());
            id = installId;
        }
        Json::Value body = Json::Value::MakeObject();
        body.Set("p_id", id);
        body.Set("p_chapter", chapter);
        body.Set("p_seconds", amount);
        if (!Supabase::Rpc(L"add_playtime", body).Ok()) {
            std::lock_guard lock(mutex);
            pending[chapter] += amount;   // retried on the next AddSeconds
            syncing = false;
            return;
        }
    }
}

std::wstring ChapterPlaytimeStore::Format(double totalSeconds) {
    const long long total = static_cast<long long>(totalSeconds);
    const long long hours = total / 3600, minutes = (total / 60) % 60;
    if (hours >= 1) return std::to_wstring(hours) + L"h " + std::to_wstring(minutes) + L"m";
    if (total >= 60) return std::to_wstring(total / 60) + L"m";
    return L"0m";
}

} // namespace SpeedrunLauncher::Services
