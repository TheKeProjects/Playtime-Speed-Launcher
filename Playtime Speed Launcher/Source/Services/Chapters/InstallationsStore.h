#pragma once
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Models/Chapter.h"

namespace SpeedrunLauncher::Services {

// User-added installations per chapter, the selected one (none = automatic), preferred download
// folders and which SteamCMD manifests were already installed. (installations.json)
class InstallationsStore {
public:
    static InstallationsStore Load();

    const std::vector<Models::Installation>& Customs(int chapter) const;
    std::optional<std::wstring> SelectedPath(int chapter) const;
    void SetSelected(int chapter, const std::optional<std::wstring>& path);

    void AddCustom(int chapter, const std::wstring& name, const std::wstring& exePath);
    void UpdateCustom(int chapter, const std::wstring& exePath, const std::wstring& newName,
                      const std::optional<std::wstring>& newIconPath);
    void MoveCustom(int chapter, const std::wstring& exePath, int direction);
    void RemoveCustom(int chapter, const std::wstring& exePath);

    std::optional<std::wstring> PreferredPath(int chapter) const;
    void SetPreferredPath(int chapter, const std::optional<std::wstring>& path);

    bool IsManifestInstalled(const std::wstring& manifestId) const { return installedManifests.count(manifestId) != 0; }
    void MarkManifestInstalled(const std::wstring& manifestId);
    void UnmarkManifestInstalled(const std::wstring& manifestId);

    std::optional<std::wstring> SteamUsername() const { return steamUsername; }
    void SetSteamUsername(const std::wstring& username);

private:
    void Save() const;

    std::map<int, std::vector<Models::Installation>> customs;
    std::map<int, std::wstring> selected;
    std::map<int, std::optional<std::wstring>> preferredPaths;
    std::set<std::wstring> installedManifests;
    std::optional<std::wstring> steamUsername;
};

} // namespace SpeedrunLauncher::Services
