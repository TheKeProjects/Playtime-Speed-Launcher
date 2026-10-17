#pragma once
#include <functional>
#include <string>

// Launcher self-update (GitHub raw version.txt / changelog.txt, release zip) and GameBanana.
// Every function blocks: call from worker threads.
namespace SpeedrunLauncher::Services::Updates {

struct UpdateInfo {
    std::wstring latestVersion;
    std::wstring changelog;
    std::wstring downloadUrl;
    std::wstring fileName;
    long long fileSize = 0;
    bool isUpdateAvailable = false;
    std::wstring releaseUrl;
};

struct GbUpdateInfo {
    std::wstring latestVersion;
    std::wstring downloadUrl;
    std::wstring fileName;
    long long fileSize = 0;
    bool isUpdateAvailable = false;
    std::wstring changelog;
};

UpdateInfo CheckForUpdates();
// Manual fallback: download info of the latest release regardless of the local version.
UpdateInfo LatestReleaseInfo();
GbUpdateInfo CheckGameBanana();

using Progress = std::function<void(int percent)>;
// Download, extract and swap via a helper batch file, then exit the process. Returns false on failure.
bool DownloadAndInstall(const UpdateInfo& info, const Progress& progress);
bool DownloadAndInstallGameBanana(const GbUpdateInfo& info, const Progress& progress);
// Manual fallback window: downloads only PlaytimeSpeedLauncher.exe next to the running one.
bool DownloadAndInstallLauncherExe(const Progress& progress);

} // namespace SpeedrunLauncher::Services::Updates
