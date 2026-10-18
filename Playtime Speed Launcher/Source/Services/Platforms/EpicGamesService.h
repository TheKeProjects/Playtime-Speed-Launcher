#pragma once
#include <functional>
#include <optional>
#include <string>
#include "Core/Async.h"

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
    // Slow (whole drives): run it on a worker. `progress` gets the stage and the folder being read.
    enum class DetectStage { Manifests, Drives };
    using DetectProgress = std::function<void(DetectStage stage, const std::wstring& folder)>;
    std::optional<std::wstring> TryAutoDetect(const Async::Cancel& cancel = {}, const DetectProgress& progress = {}) const;
    static bool IsValidBase(const std::wstring& path);

private:
    void Load();
    void Save() const;

    bool enabled = false;
    std::optional<std::wstring> basePath;
};

} // namespace SpeedrunLauncher::Services
