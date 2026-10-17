#pragma once
#include <windows.h>
#include <functional>
#include <string>

namespace SpeedrunLauncher::Features {

// The two RegisterHotKey shortcuts: checkpoint overlay (default Ctrl+Shift+Enter) and video
// tutorial overlay (default F9). While a chapter with a standalone UE4SS runs, they're temporarily
// moved to F2 / F1 (UE4SS uses the defaults itself) and restored when the game exits.
class GlobalHotkeys {
public:
    static constexpr int CheckpointId = 9001;
    static constexpr int TutorialId = 9002;

    void Register(HWND hwnd);
    void Unregister();
    // WM_HOTKEY: returns true when it was one of ours
    bool Handle(WPARAM id);

    unsigned CheckpointModifiers() const { return checkpointMod; }
    unsigned CheckpointVk() const { return checkpointVk; }
    unsigned TutorialModifiers() const { return tutorialMod; }
    unsigned TutorialVk() const { return tutorialVk; }
    void SetCheckpoint(unsigned modifiers, unsigned vk);   // re-registers and saves
    void SetTutorial(unsigned modifiers, unsigned vk);

    void ApplyUe4ssTempRemap(const std::wstring& exe);
    void RestoreUe4ss();
    void RestoreIfGameExited();
    bool TempRemapActive() const { return tempRemap; }

    std::function<void()> onCheckpoint, onTutorial, onTempRemapApplied, onTempRemapRestored;

private:
    void Apply();
    HWND window = nullptr;
    unsigned checkpointMod = 0, checkpointVk = 0, tutorialMod = 0, tutorialVk = 0;
    bool tempRemap = false;
    std::wstring tempRemapExe;
    unsigned savedCheckpointMod = 0, savedCheckpointVk = 0, savedTutorialMod = 0, savedTutorialVk = 0;
};

} // namespace SpeedrunLauncher::Features
