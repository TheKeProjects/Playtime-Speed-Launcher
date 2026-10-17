#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Models/Chapter.h"

namespace SpeedrunLauncher::Services {

enum class CoresMode { Normal, Slower, Freeze };

// "Cores" load manipulation: pins the game to one CPU core (Slower) and, for Freeze, also changes
// its priority. Restores the original affinity / priority when going back to Normal.
class CoresService {
public:
    using ExePathFn = std::function<std::optional<std::wstring>(const Models::Chapter&)>;
    using ShippingNameFn = std::function<std::optional<std::wstring>(const std::wstring& exePath)>;

    struct Found {
        uint32_t pid = 0;
        std::wstring processName;
        int chapter = 0;
    };

    CoresMode CurrentMode() const { return mode; }
    int DetectedChapter() const { return detectedChapter; }

    std::optional<Found> FindGameProcess(const std::vector<Models::Chapter>& chapters, const ExePathFn& exePath,
                                         const ShippingNameFn& shippingName) const;
    // Returns the affected process name, or nullopt when no game is running.
    std::optional<std::wstring> ApplyMode(CoresMode newMode, const std::vector<Models::Chapter>& chapters, const ExePathFn& exePath,
                                          const ShippingNameFn& shippingName, unsigned long freezePriority);
    void RestoreIfActive();

private:
    void RestoreCurrentMode();

    CoresMode mode = CoresMode::Normal;
    uint32_t targetPid = 0;
    uintptr_t savedAffinity = 0;
    unsigned long savedPriority = 0x20;   // NORMAL_PRIORITY_CLASS
    int detectedChapter = 0;
    std::optional<std::wstring> detectedProcessName;
};

} // namespace SpeedrunLauncher::Services
