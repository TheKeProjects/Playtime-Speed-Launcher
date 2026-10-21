#pragma once
#include <map>
#include <string>

// Every external id, webhook, key, database and link the launcher uses, in one place.
// The services reference these instead of keeping their own copies.
namespace SpeedrunLauncher::Config {

// User-Agent sent to GitHub / GameBanana / LiveSplit / Hand Mods downloads
inline constexpr const wchar_t* UserAgent = L"PlaytimeSpeedLauncher";

// ── Launcher repository (updates, changelog, manual download) ───────────────
namespace GitHub {
inline constexpr const wchar_t* Owner = L"TheKeProjects";
inline constexpr const wchar_t* Repo = L"Playtime-Speed-Launcher";
inline constexpr const wchar_t* Branch = L"main";
inline constexpr const wchar_t* LatestExeUrl =
    L"https://github.com/TheKeProjects/Playtime-Speed-Launcher/releases/latest/download/PlaytimeSpeedLauncher.exe";
} // namespace GitHub

// ── GameBanana (alternative update channel) ─────────────────────────────────
namespace GameBanana {
inline int ToolId = 22136;   // set when published (kept non-constexpr so the GameBanana code stays compiled)
inline constexpr const wchar_t* ApiItemUrl = L"https://api.gamebanana.com/Core/Item/Data?itemtype=Tool&itemid=";
} // namespace GameBanana

// ── Hand Mods repository ────────────────────────────────────────────────────
namespace HandMods {
inline constexpr const wchar_t* Owner = L"An-Average-Developer";
inline constexpr const wchar_t* Repo = L"Hand-Mods-PP";
inline constexpr const wchar_t* Branch = L"main";
} // namespace HandMods

// ── Discord ─────────────────────────────────────────────────────────────────
namespace Discord {
// Application used for Rich Presence and for "Connect with Discord" (OAuth)
inline constexpr const wchar_t* ClientId = L"ClientId";
inline constexpr const char* ClientIdUtf8 = "ClientId";
// OAuth implicit flow: the browser comes back to a local listener
inline constexpr int OAuthPort = 45678;
inline constexpr const wchar_t* OAuthRedirectUri = L"http://localhost:45678/callback";
inline constexpr const wchar_t* OAuthAuthorizeUrl = L"https://discord.com/oauth2/authorize";
inline constexpr const wchar_t* ApiUserUrl = L"https://discord.com/api/users/@me";
inline constexpr const wchar_t* UserProfileUrl = L"https://discord.com/users/";   // + user id

// Webhooks
inline constexpr const wchar_t* BugReportWebhook =
    L"BugReportWebhook";
inline constexpr const wchar_t* SkipReportWebhook =
    L"SkipReportWebhook";
inline constexpr const wchar_t* HandModSubmissionWebhook =
    L"HandModSubmissionWebhook";

// Contributors credited in the changelogs → Discord user ids (clickable @mentions)
inline const std::map<std::wstring, std::wstring>& ContributorIds() {
    static const std::map<std::wstring, std::wstring> users{
        { L"Edwin", L"460391445690449922" },       { L"Technight", L"257300997322440704" },
        { L"AdrianPG77", L"752207247769206795" },  { L"\x1D22\x1D00\x1D07\x1D07", L"807763566849163264" },
        { L"lumpydesk_yt", L"1143596461087522857" }, { L"nickk893", L"694115352694751262" },
    };
    return users;
}
} // namespace Discord

// ── Supabase (online users counter, cloud playtime) ─────────────────────────
namespace Supabase {
inline constexpr const wchar_t* Url = L"Url";
inline constexpr const wchar_t* AnonKey = L"AnonKey";
} // namespace Supabase

// ── speedrun.com (leaderboards) ─────────────────────────────────────────────
namespace SpeedrunCom {
inline constexpr const wchar_t* ApiUrl = L"https://www.speedrun.com/api/v1";
// Chapter number → speedrun.com game id
inline const std::map<int, std::wstring>& ChapterGameIds() {
    static const std::map<int, std::wstring> ids = {
        { 1, L"w6j7vpx6" }, { 2, L"4d7nqx36" }, { 3, L"w6jge376" }, { 4, L"v1pl8ep6" }, { 5, L"w6j9lr5d" },
    };
    return ids;
}
} // namespace SpeedrunCom

// ── Public links ────────────────────────────────────────────────────────────
namespace Links {
inline constexpr const wchar_t* Website = L"https://playtimesl.com/";
inline constexpr const wchar_t* DiscordInvite = L"https://discord.gg/Z44M2yeRys";
inline constexpr const wchar_t* LiveSplitDownloads = L"https://livesplit.org/downloads/";
inline constexpr const wchar_t* SteamCmdZip = L"https://steamcdn-a.akamaihd.net/client/installer/steamcmd.zip";
inline constexpr const wchar_t* YouTubeIframeApi = L"https://www.youtube.com/iframe_api";
inline constexpr const wchar_t* YouTubeThumbnails = L"https://img.youtube.com/vi/";   // + video id + "/maxresdefault.jpg"
} // namespace Links

} // namespace SpeedrunLauncher::Config
