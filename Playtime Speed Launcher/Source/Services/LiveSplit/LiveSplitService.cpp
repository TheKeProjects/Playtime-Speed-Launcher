#include "Services/LiveSplit/LiveSplitService.h"
#include <regex>
#include "App/Config.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Http.h"
#include "Core/Process.h"
#include "Core/Text.h"
#include "Core/Zip.h"
#include "Services/App/VersionCompare.h"

namespace SpeedrunLauncher::Services::LiveSplit {

namespace {

const std::wstring& ConfigFile() {
    static const std::wstring path = Paths::DataFile(L"livesplit.cfg");
    return path;
}

void LoadInstalled(std::wstring& version, std::wstring& path) {
    for (const auto& line : Files::ReadLines(ConfigFile())) {
        if (Text::StartsWith(line, L"version=")) version = line.substr(8);
        else if (Text::StartsWith(line, L"path=")) path = line.substr(5);
    }
}

void SaveInstalled(const std::wstring& version, const std::wstring& path) {
    Files::WriteText(ConfigFile(), L"version=" + version + L"\r\npath=" + path + L"\r\n");
}

} // namespace

std::wstring DefaultInstallDir() { return Paths::DataFile(L"LiveSplit"); }

Info Check() {
    Info info;
    std::wstring installedVersion, installPath;
    LoadInstalled(installedVersion, installPath);
    if (!installPath.empty()) {
        info.isInstalled = Files::Exists(Files::Combine(installPath, L"LiveSplit.exe"));
        info.installedVersion = installedVersion;
        info.installPath = installPath;
    }
    const Http::Response r = Http::Get(Config::Links::LiveSplitDownloads, { { L"User-Agent", Config::UserAgent } });
    if (r.Ok()) {
        const std::wstring html = Text::Widen(r.body);
        static const std::wregex re(
            LR"re(href="(https://github\.com/LiveSplit/LiveSplit/releases/download/([\d.]+)/LiveSplit_([\d.]+)\.zip)")re",
            std::regex::icase);
        std::wsmatch m;
        if (std::regex_search(html, m, re)) {
            info.downloadUrl = m[1].str();
            info.latestVersion = m[2].str();
            info.fileName = L"LiveSplit_" + info.latestVersion + L".zip";
        }
    }
    if (!info.latestVersion.empty() && info.isInstalled) info.isUpdateAvailable = Versions::IsNewer(installedVersion, info.latestVersion);
    return info;
}

bool DownloadAndInstall(const Info& info, const std::wstring& installDir, const std::function<void(int)>& progress) {
    if (Text::Trim(info.downloadUrl).empty()) return false;
    const std::wstring tempDir = Files::Combine(Files::TempDirectory(), L"PlaytimeSpeedLauncher_LiveSplit");
    Files::DeleteDirectory(tempDir);
    Files::CreateDirectories(tempDir);
    const std::wstring zipPath = Files::Combine(tempDir, info.fileName.empty() ? L"livesplit.zip" : info.fileName);
    const bool downloaded = Http::Download(info.downloadUrl, zipPath, [&](uint64_t received, uint64_t total) {
        if (progress && total > 0) progress(static_cast<int>(received * 100 / total));
        return true;
    }, nullptr, { { L"User-Agent", Config::UserAgent } });
    const std::wstring extractDir = Files::Combine(tempDir, L"extracted");
    if (!downloaded || !Zip::ExtractToDirectory(zipPath, extractDir, true)) {
        Files::DeleteDirectory(tempDir);
        return false;
    }
    // The zip may extract into a single subfolder
    std::wstring source = extractDir;
    const auto subDirs = Files::ListDirectories(extractDir);
    if (subDirs.size() == 1 && !Files::Exists(Files::Combine(extractDir, L"LiveSplit.exe"))) source = subDirs[0];
    Files::DeleteDirectory(installDir);
    if (!Files::CopyDirectory(source, installDir, true)) {
        Files::DeleteDirectory(tempDir);
        return false;
    }
    SaveInstalled(info.latestVersion, installDir);
    Files::DeleteDirectory(tempDir);
    return true;
}

void Launch(const std::wstring& installPath) {
    const std::wstring exe = Files::Combine(installPath, L"LiveSplit.exe");
    if (Files::Exists(exe)) Process::Open(exe, L"", installPath);
}

} // namespace SpeedrunLauncher::Services::LiveSplit
