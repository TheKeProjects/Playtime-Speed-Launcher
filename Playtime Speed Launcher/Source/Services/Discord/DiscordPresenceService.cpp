#include "Services/Discord/DiscordPresenceService.h"
#include <windows.h>
#include <ctime>
#include "App/Config.h"
#include "Core/Text.h"
#include "Settings/IconThemeSettings.h"

namespace SpeedrunLauncher::Services {

namespace {

constexpr const char* kAppId = Config::Discord::ClientIdUtf8;

std::wstring LauncherImageKey() { return Settings::IconTheme::Current().DiscordImageKey(); }

long long NowUnix() { return static_cast<long long>(std::time(nullptr)); }

std::wstring ChapterKey(const Models::Chapter& c) { return L"chapter_" + std::to_wstring(c.number); }

} // namespace

DiscordPresenceService::DiscordPresenceService() {
    sessionStart = NowUnix();
    client = std::make_unique<DiscordIpcClient>(kAppId);
}

DiscordPresenceService::~DiscordPresenceService() = default;   // the client clears the presence on shutdown

std::optional<DiscordIpcClient::User> DiscordPresenceService::ConnectedUser() const {
    return client ? client->ConnectedUser() : std::nullopt;
}

void DiscordPresenceService::ApplySettings(bool activity, bool version, bool liveSplit) {
    showActivity = activity;
    showVersion = version;
    showLiveSplit = liveSplit;
    if (!showActivity) {
        if (client) client->SetActivity(Json::Value());
        return;
    }
    if (current) Send();
}

void DiscordPresenceService::RefreshIconTheme() {
    if (!current) return;
    const std::wstring key = LauncherImageKey();
    if (Text::StartsWith(current->largeImage, L"launcher")) current->largeImage = key;
    if (Text::StartsWith(current->smallImage, L"launcher")) current->smallImage = key;
    if (showActivity) Send();
}

void DiscordPresenceService::UpdateLiveSplitLine(const std::wstring& line) {
    if (line == liveSplitLine) return;
    liveSplitLine = line;
    if (!showActivity || !showLiveSplit || !current) return;
    // Discord rate limit: ~5 updates per 20 s
    const unsigned long long now = GetTickCount64();
    if (now - lastLiveSplitSend < 4000) return;
    lastLiveSplitSend = now;
    Send();
}

void DiscordPresenceService::SetBrowsing() {
    Set({ L"Speedrun Launcher", sessionStart, LauncherImageKey(), L"Playtime Speed Launcher", L"", L"" }, L"");
}

void DiscordPresenceService::SetChapterSelected(const Models::Chapter& c, const std::wstring& version) {
    Set({ c.subTitle, sessionStart, LauncherImageKey(), L"Playtime Speed Launcher", ChapterKey(c), c.subTitle }, version);
}

void DiscordPresenceService::SetInstalling(const Models::Chapter& c, const std::wstring& presetName) {
    Set({ L"Installing " + c.subTitle, NowUnix(), ChapterKey(c), c.subTitle, LauncherImageKey(), L"Installing via SteamCMD" }, presetName);
}

void DiscordPresenceService::SetGameRunning(const Models::Chapter& c, const std::wstring& version) {
    Set({ L"Running " + c.subTitle, NowUnix(), LauncherImageKey(), L"Playtime Speed Launcher", ChapterKey(c), c.subTitle }, version);
}

void DiscordPresenceService::SetWatchingTutorial(const std::wstring& videoTitle, const std::wstring& chapter) {
    Set({ L"Watching tutorial  \x00B7  " + chapter, NowUnix(), LauncherImageKey(), L"Playtime Speed Launcher", L"checkpoint", videoTitle },
        videoTitle);
}

void DiscordPresenceService::SetSelectingCheckpoint(const Models::Chapter& c, const std::wstring& version) {
    Set({ L"Selecting checkpoint  \x00B7  " + c.subTitle, NowUnix(), LauncherImageKey(), L"Playtime Speed Launcher", L"checkpoint",
          c.subTitle },
        version);
}

std::wstring DiscordPresenceService::BuildState() const {
    const bool v = showVersion && !versionLine.empty();
    const bool l = showLiveSplit && !liveSplitLine.empty();
    if (v && l) return versionLine + L"  \x00B7  " + liveSplitLine;
    if (v) return versionLine;
    if (l) return liveSplitLine;
    return L"";
}

void DiscordPresenceService::Set(const Presence& presence, const std::wstring& version) {
    versionLine = version;
    current = presence;
    if (showActivity) Send();
}

void DiscordPresenceService::Send() {
    if (!client || !current) return;
    // Discord rejects empty (and 1-char) strings, so leave those fields out
    const auto put = [](Json::Value& obj, const char* key, const std::wstring& value) {
        if (value.size() >= 2) obj.Set(key, value.size() > 128 ? value.substr(0, 128) : value);
    };
    Json::Value activity = Json::Value::MakeObject();
    put(activity, "details", current->details);
    put(activity, "state", BuildState());
    Json::Value timestamps = Json::Value::MakeObject();
    timestamps.Set("start", current->startUnix);
    activity.Set("timestamps", timestamps);
    Json::Value assets = Json::Value::MakeObject();
    put(assets, "large_image", current->largeImage);
    put(assets, "large_text", current->largeText);
    put(assets, "small_image", current->smallImage);
    put(assets, "small_text", current->smallText);
    activity.Set("assets", assets);
    Json::Value buttons = Json::Value::MakeArray();
    const std::pair<const wchar_t*, const wchar_t*> links[] = {
        { L"Download Launcher", Config::Links::Website },
        { L"Join Discord", Config::Links::DiscordInvite },
    };
    for (const auto& [label, url] : links) {
        Json::Value b = Json::Value::MakeObject();
        b.Set("label", label);
        b.Set("url", url);
        buttons.Push(b);
    }
    activity.Set("buttons", buttons);
    client->SetActivity(activity);
}

} // namespace SpeedrunLauncher::Services
