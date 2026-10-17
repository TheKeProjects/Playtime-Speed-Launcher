#pragma once
#include <functional>
#include <optional>
#include <string>

// Installs / updates LiveSplit into %LOCALAPPDATA%\SpeedrunLauncher\LiveSplit (livesplit.cfg).
// Blocking calls: run on worker threads.
namespace SpeedrunLauncher::Services::LiveSplit {

struct Info {
    std::wstring latestVersion;
    std::wstring downloadUrl;
    std::wstring fileName;
    long long fileSize = 0;
    bool isUpdateAvailable = false;
    bool isInstalled = false;
    std::wstring installedVersion;
    std::optional<std::wstring> installPath;
};

std::wstring DefaultInstallDir();
Info Check();
bool DownloadAndInstall(const Info& info, const std::wstring& installDir, const std::function<void(int)>& progress);
void Launch(const std::wstring& installPath);

} // namespace SpeedrunLauncher::Services::LiveSplit
