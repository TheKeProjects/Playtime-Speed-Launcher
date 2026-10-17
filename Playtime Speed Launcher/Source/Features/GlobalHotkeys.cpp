#include "Features/GlobalHotkeys.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Settings/HotkeySettings.h"

namespace SpeedrunLauncher::Features {

void GlobalHotkeys::Register(HWND hwnd) {
    window = hwnd;
    const auto& s = Settings::Hotkeys::Current();
    checkpointMod = s.checkpointModifiers;
    checkpointVk = s.checkpointVk;
    tutorialMod = s.tutorialModifiers;
    tutorialVk = s.tutorialVk;
    Apply();
}

void GlobalHotkeys::Apply() {
    if (!window) return;
    UnregisterHotKey(window, CheckpointId);
    UnregisterHotKey(window, TutorialId);
    RegisterHotKey(window, CheckpointId, checkpointMod, checkpointVk);
    RegisterHotKey(window, TutorialId, tutorialMod, tutorialVk);
}

void GlobalHotkeys::Unregister() {
    if (!window) return;
    UnregisterHotKey(window, CheckpointId);
    UnregisterHotKey(window, TutorialId);
}

bool GlobalHotkeys::Handle(WPARAM id) {
    if (id == CheckpointId) {
        if (onCheckpoint) onCheckpoint();
        return true;
    }
    if (id == TutorialId) {
        if (onTutorial) onTutorial();
        return true;
    }
    return false;
}

void GlobalHotkeys::SetCheckpoint(unsigned modifiers, unsigned vk) {
    checkpointMod = modifiers;
    checkpointVk = vk;
    auto& s = Settings::Hotkeys::Current();
    s.checkpointModifiers = modifiers;
    s.checkpointVk = vk;
    s.SaveCheckpoint();
    Apply();
}

void GlobalHotkeys::SetTutorial(unsigned modifiers, unsigned vk) {
    tutorialMod = modifiers;
    tutorialVk = vk;
    auto& s = Settings::Hotkeys::Current();
    s.tutorialModifiers = modifiers;
    s.tutorialVk = vk;
    s.SaveTutorial();
    Apply();
}

void GlobalHotkeys::ApplyUe4ssTempRemap(const std::wstring& exe) {
    if (tempRemap) return;
    savedCheckpointMod = checkpointMod;
    savedCheckpointVk = checkpointVk;
    savedTutorialMod = tutorialMod;
    savedTutorialVk = tutorialVk;
    checkpointMod = 0;
    checkpointVk = VK_F2;
    tutorialMod = 0;
    tutorialVk = VK_F1;
    Apply();
    tempRemap = true;
    tempRemapExe = exe;
    if (onTempRemapApplied) onTempRemapApplied();
}

void GlobalHotkeys::RestoreUe4ss() {
    if (!tempRemap) return;
    checkpointMod = savedCheckpointMod;
    checkpointVk = savedCheckpointVk;
    tutorialMod = savedTutorialMod;
    tutorialVk = savedTutorialVk;
    Apply();
    tempRemap = false;
    tempRemapExe.clear();
    if (onTempRemapRestored) onTempRemapRestored();
}

void GlobalHotkeys::RestoreIfGameExited() {
    if (!tempRemap || tempRemapExe.empty()) return;
    if (Process::FindByName(Files::FileNameWithoutExtension(tempRemapExe)).empty()) RestoreUe4ss();
}

} // namespace SpeedrunLauncher::Features
