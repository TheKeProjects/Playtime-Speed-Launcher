#include "Services/App/UpdateService.h"
#include <windows.h>
#include <shellapi.h>
#include "App/Config.h"
#include "Core/Files.h"
#include "Core/Http.h"
#include "Core/Json.h"
#include "Core/Process.h"
#include "Core/Text.h"
#include "Core/Zip.h"
#include "Services/App/AppVersion.h"
#include "Services/App/VersionCompare.h"

namespace SpeedrunLauncher::Services::Updates {

namespace {

const std::vector<std::pair<std::wstring, std::wstring>> kHeaders = { { L"User-Agent", Config::UserAgent } };

std::wstring RawUrl(const wchar_t* file) {
    return std::wstring(L"https://raw.githubusercontent.com/") + AppVersion::GitHubOwner + L"/" + AppVersion::GitHubRepo + L"/" +
           AppVersion::GitHubBranch + L"/" + file;
}

std::optional<std::wstring> GetString(const std::wstring& url) {
    const Http::Response r = Http::Get(url, kHeaders);
    if (!r.Ok()) return std::nullopt;
    return Text::Widen(r.body);
}

std::optional<std::wstring> FetchLatestVersion() {
    auto text = GetString(RawUrl(L"version.txt"));
    if (!text) return std::nullopt;
    std::wstring v;
    for (wchar_t c : *text)
        if (c != L'\r' && c != L'\n' && c != L'\t') v += c;
    v = Text::Trim(v);
    if (v.empty()) return std::nullopt;
    return v;
}

std::wstring FetchChangelog() {
    auto text = GetString(RawUrl(L"changelog.txt"));
    return text ? Text::Trim(*text) : std::wstring();
}

void PopulateDownloadFields(UpdateInfo& info, const std::wstring& latest) {
    info.fileName = L"PSLauncherv" + Text::Replace(latest, L".", L"-") + L".zip";
    info.downloadUrl = std::wstring(L"https://github.com/") + AppVersion::GitHubOwner + L"/" + AppVersion::GitHubRepo +
                       L"/releases/latest/download/" + info.fileName;
    info.releaseUrl = std::wstring(L"https://github.com/") + AppVersion::GitHubOwner + L"/" + AppVersion::GitHubRepo + L"/releases/latest";
    Http::Request head;
    head.method = L"HEAD";
    head.url = info.downloadUrl;
    head.headers = kHeaders;
    const Http::Response r = Http::Send(head);
    if (r.Ok()) info.fileSize = Text::ToInt(r.Header(L"content-length")).value_or(0);
}

bool DownloadTo(const std::wstring& url, const std::wstring& path, const Progress& progress) {
    return Http::Download(url, path, [&](uint64_t received, uint64_t total) {
        if (progress && total > 0) progress(static_cast<int>(received * 100 / total));
        return true;
    }, nullptr, kHeaders);
}

std::wstring FindExecutable(const std::wstring& dir) {
    const std::wstring processName = Files::FileName(Files::ExecutablePath());
    for (const std::wstring& name : { processName, std::wstring(L"Playtime Speed Launcher.exe") }) {
        auto found = Files::ListFiles(dir, name, true);
        if (!found.empty()) return found.front();
    }
    auto any = Files::ListFiles(dir, L"*.exe", true);
    return any.empty() ? std::wstring() : any.front();
}

[[noreturn]] void RunBatchAndExit(const std::wstring& batchPath, const std::string& content) {
    Files::WriteBytes(batchPath, content);
    SHELLEXECUTEINFOW info{ sizeof(info) };
    info.lpVerb = L"open";
    info.lpFile = L"cmd.exe";
    const std::wstring args = L"/c \"" + batchPath + L"\"";
    info.lpParameters = args.c_str();
    info.nShow = SW_HIDE;
    ShellExecuteExW(&info);
    Sleep(1000);
    ExitProcess(0);
}

} // namespace

UpdateInfo CheckForUpdates() {
    UpdateInfo info;
    info.latestVersion = AppVersion::CurrentVersion;
    auto latest = FetchLatestVersion();
    if (!latest) return info;
    // Sanity check: at least "x.y" made of numbers
    const auto p = Versions::Parse(*latest);
    std::wstring digits;
    for (wchar_t c : *latest)
        if ((c >= L'0' && c <= L'9') || c == L'.') digits += c;
    if (Text::Split(digits, L'.', true).size() < 2) return info;
    (void)p;
    info.latestVersion = *latest;
    info.isUpdateAvailable = Versions::IsNewer(AppVersion::CurrentVersion, *latest);
    info.changelog = FetchChangelog();
    if (info.isUpdateAvailable) PopulateDownloadFields(info, *latest);
    return info;
}

UpdateInfo LatestReleaseInfo() {
    UpdateInfo info;
    info.latestVersion = AppVersion::CurrentVersion;
    auto latest = FetchLatestVersion();
    if (!latest) return info;
    info.latestVersion = *latest;
    info.isUpdateAvailable = true;
    info.changelog = FetchChangelog();
    PopulateDownloadFields(info, *latest);
    return info;
}

GbUpdateInfo CheckGameBanana() {
    GbUpdateInfo info;
    if (AppVersion::GameBananaToolId <= 0) return info;
    const std::wstring url = std::wstring(Config::GameBanana::ApiItemUrl) +
                             std::to_wstring(AppVersion::GameBananaToolId) + L"&fields=name,Files().aFiles()";
    const Http::Response r = Http::Get(url, kHeaders);
    if (!r.Ok()) return info;
    const Json::Value root = Json::Parse(std::string_view(r.body));
    if (!root.IsArray() || root.Size() < 2 || !root[1].IsObject()) return info;
    const auto& files = root[1].Items();
    if (files.empty()) return info;
    const Json::Value& file = files.front().second;   // only the first file is considered
    info.latestVersion = Text::Trim(file["_sVersion"].WStr());
    info.isUpdateAvailable = Versions::IsNewer(AppVersion::CurrentVersion, info.latestVersion);
    if (info.isUpdateAvailable) {
        info.downloadUrl = file["_sDownloadUrl"].WStr();
        info.fileName = file["_sFile"].WStr();
        info.fileSize = file["_nFilesize"].Int();
        info.changelog = FetchChangelog();
    }
    return info;
}

bool DownloadAndInstall(const UpdateInfo& info, const Progress& progress) {
    if (Text::Trim(info.downloadUrl).empty()) return false;
    const std::wstring tempDir = Files::Combine(Files::TempDirectory(), L"PlaytimeSpeedLauncher_Update");
    Files::DeleteDirectory(tempDir);
    Files::CreateDirectories(tempDir);
    const std::wstring zipPath = Files::Combine(tempDir, info.fileName.empty() ? L"update.zip" : info.fileName);
    const auto fail = [&] {
        Files::DeleteDirectory(tempDir);
        return false;
    };
    if (!DownloadTo(info.downloadUrl, zipPath, progress)) return fail();
    const std::wstring extractDir = Files::Combine(tempDir, L"extracted");
    if (!Zip::ExtractToDirectory(zipPath, extractDir, true)) return fail();
    const std::wstring newExe = FindExecutable(extractDir);
    if (newExe.empty()) return fail();

    const std::wstring currentExe = Files::ExecutablePath();
    const std::wstring currentDir = Files::DirectoryName(currentExe);
    const std::wstring extractedDir = Files::DirectoryName(newExe);
    const std::string batch = Text::Narrow(
        L"@echo off\r\nchcp 65001 >nul\r\ntimeout /t 2 /nobreak >nul\r\n"
        L"xcopy \"" + extractedDir + L"\\*\" \"" + currentDir + L"\" /E /Y /I /Q\r\n"
        L"if %ERRORLEVEL% NEQ 0 exit /b 1\r\n"
        L"start \"\" \"" + currentExe + L"\"\r\n"
        L"rd /s /q \"" + tempDir + L"\"\r\nexit");
    RunBatchAndExit(Files::Combine(tempDir, L"update.bat"), batch);
}

bool DownloadAndInstallGameBanana(const GbUpdateInfo& gb, const Progress& progress) {
    UpdateInfo mapped;
    mapped.downloadUrl = gb.downloadUrl;
    mapped.fileName = Text::Trim(gb.fileName).empty() ? L"gamebanana_update.zip" : gb.fileName;
    mapped.isUpdateAvailable = true;
    mapped.latestVersion = gb.latestVersion;
    return DownloadAndInstall(mapped, progress);
}

bool DownloadAndInstallLauncherExe(const Progress& progress) {
    const std::wstring url = Config::GitHub::LatestExeUrl;
    const std::wstring currentDir = Files::DirectoryName(Files::ExecutablePath());
    const std::wstring currentExe = Files::Combine(currentDir, L"PlaytimeSpeedLauncher.exe");
    const std::wstring staging = Files::Combine(currentDir, L"PlaytimeSpeedLauncher.update.exe");
    if (!DownloadTo(url, staging, progress)) {
        Files::Delete(staging);
        return false;
    }
    const std::wstring tempDir = Files::Combine(Files::TempDirectory(), L"PlaytimeSpeedLauncher_Update");
    Files::DeleteDirectory(tempDir);
    Files::CreateDirectories(tempDir);
    const std::string batch = Text::Narrow(
        L"@echo off\r\nchcp 65001 >nul\r\ntimeout /t 2 /nobreak >nul\r\nset RETRIES=10\r\n:retry\r\n"
        L"move /y \"" + staging + L"\" \"" + currentExe + L"\" >nul 2>&1\r\n"
        L"if exist \"" + staging + L"\" (\r\n    set /a RETRIES-=1\r\n    if %RETRIES% GTR 0 (\r\n"
        L"        timeout /t 1 /nobreak >nul\r\n        goto retry\r\n    )\r\n)\r\n"
        L"start \"\" \"" + currentExe + L"\"\r\n"
        L"rd /s /q \"" + tempDir + L"\"\r\nexit");
    RunBatchAndExit(Files::Combine(tempDir, L"update.bat"), batch);
}

} // namespace SpeedrunLauncher::Services::Updates
