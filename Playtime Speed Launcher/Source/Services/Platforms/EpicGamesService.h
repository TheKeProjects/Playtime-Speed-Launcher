#pragma once
#include <optional>
#include <string>

namespace SpeedrunLauncher::Services {

// Epic Games install of the chapters (all under one PoppyPlaytimeChapterOne folder). (epic.json)
class EpicGamesService {
public:
    EpicGamesService() { Load(); }

    bool IsEnabled() const { return enabled; }
    const std::optional<std::wstring>& BasePath() const { return basePath; }
    std::optional<std::wstring> ExePath(int chapter) const;
    bool HasExeForAnyChapter() const;
    void SetEnabled(bool on);
    void SetBasePath(const std::optional<std::wstring>& path);

    // Looks in the Epic launcher manifests, then searches fixed drives (4 levels) for the folder.
    std::optional<std::wstring> TryAutoDetect() const;
    static bool IsValidBase(const std::wstring& path);

private:
    void Load();
    void Save() const;

    bool enabled = false;
    std::optional<std::wstring> basePath;
};

} // namespace SpeedrunLauncher::Services
