#include "Services/Chapters/ChapterPlaytimeStore.h"
#include <algorithm>
#include <optional>
#include "App/Paths.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/Json.h"
#include "Core/Text.h"
#include "Services/Account/AccountService.h"
#include "Services/Community/Supabase.h"
#include "Settings/HeartbeatSettings.h"

namespace SpeedrunLauncher::Services {

namespace {
constexpr double kAccountSyncSeconds = 60.0;   // while playing, the account catches up once a minute
}

void ChapterPlaytimeStore::Load(std::function<void()> onLoaded) {
    installId = Settings::Heartbeat::Current().installId;
    onLoadedCallback = onLoaded;
    MigrateLegacyLocalFile();
    const std::wstring id = installId;
    Async::Run([this, id] {
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
        const bool ok = response.Ok();
        Async::Post([this, ok] {
            installLoaded = installLoaded || ok;
            if (installLoaded && Account::AccountService::Get().SignedIn()) SyncAccount();
            if (onLoadedCallback) onLoadedCallback();
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
    const double install = it == seconds.end() ? 0.0 : it->second;
    if (!accountLoaded) return install;
    // The account's total plus what this PC played since the last sync
    auto account = accountSeconds.find(chapter);
    auto linked = linkedSeconds.find(chapter);
    const double sinceSync = std::max(0.0, install - (linked == linkedSeconds.end() ? 0.0 : linked->second));
    return (account == accountSeconds.end() ? 0.0 : account->second) + sinceSync;
}

void ChapterPlaytimeStore::AddSeconds(int chapter, double amount) {
    bool startFlush = false;
    {
        std::lock_guard lock(mutex);
        seconds[chapter] += amount;
        pending[chapter] += amount;
        if (!syncing) {
            syncing = true;
            startFlush = true;
        }
    }
    if (startFlush) Async::Run([this] { Flush(); });
    if (Account::AccountService::Get().SignedIn() && Async::Now() - lastAccountSync > kAccountSyncSeconds) SyncAccount();
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

// ── Account ─────────────────────────────────────────────────────────────────

void ChapterPlaytimeStore::AccountChanged() {
    ++accountGeneration;
    {
        std::lock_guard lock(mutex);
        accountLoaded = false;
        accountSeconds.clear();
        linkedSeconds.clear();
    }
    accountSyncAgain = false;
    if (Account::AccountService::Get().SignedIn()) SyncAccount();
    if (onLoadedCallback) onLoadedCallback();
}

void ChapterPlaytimeStore::SyncAccount() {
    if (!Account::AccountService::Get().SignedIn() || !installLoaded || installId.empty()) return;
    if (accountSyncing) {
        accountSyncAgain = true;
        return;
    }
    accountSyncing = true;
    lastAccountSync = Async::Now();
    std::map<int, double> rows;
    {
        std::lock_guard lock(mutex);
        rows = seconds;
    }
    struct Result {
        bool linked = false;
        std::optional<std::map<int, double>> totals;
    };
    const unsigned gen = accountGeneration;
    const std::wstring id = installId;
    Async::Run([id, rows] {
        auto& account = Account::AccountService::Get();
        Result result;
        Json::Value list = Json::Value::MakeArray();
        for (const auto& [chapter, secs] : rows) {
            if (secs <= 0.0) continue;
            Json::Value row = Json::Value::MakeObject();
            row.Set("chapter", chapter);
            row.Set("seconds", secs);
            list.Push(std::move(row));
        }
        Json::Value body = Json::Value::MakeObject();
        body.Set("p_install", id);
        body.Set("p_rows", std::move(list));
        result.linked = account.Rpc(L"link_install_playtime", body).Ok();
        const Http::Response response = account.Rpc(L"get_account_playtime", Json::Value::MakeObject());
        if (response.Ok()) {
            bool ok = false;
            const Json::Value json = Json::Parse(std::string_view(response.body), &ok);
            if (ok && json.IsArray()) {
                std::map<int, double> totals;
                for (const auto& row : json.Elements()) totals[static_cast<int>(row["chapter"].Int())] = row["seconds"].Number();
                result.totals = std::move(totals);
            }
        }
        return result;
    }, [this, gen, rows](Result result) {
        accountSyncing = false;
        // A failed link leaves the account counting this install as of the previous sync: the
        // totals still apply then (with the previous snapshot), but only once a link has worked
        if (gen == accountGeneration && result.totals && (result.linked || accountLoaded)) {
            std::lock_guard lock(mutex);
            accountSeconds = std::move(*result.totals);
            if (result.linked) linkedSeconds = rows;
            accountLoaded = true;
        }
        if (onLoadedCallback) onLoadedCallback();
        if (accountSyncAgain) {
            accountSyncAgain = false;
            SyncAccount();
        }
    });
}

std::wstring ChapterPlaytimeStore::Format(double totalSeconds) {
    const long long total = static_cast<long long>(totalSeconds);
    const long long hours = total / 3600, minutes = (total / 60) % 60;
    if (hours >= 1) return std::to_wstring(hours) + L"h " + std::to_wstring(minutes) + L"m";
    if (total >= 60) return std::to_wstring(total / 60) + L"m";
    if (total > 0) return std::to_wstring(total) + L"s";
    return L"0m";
}

}  // namespace SpeedrunLauncher::Services
