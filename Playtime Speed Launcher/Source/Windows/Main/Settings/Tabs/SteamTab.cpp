// Settings → Steam: copy the launch option for Steam (adding the launcher as a non-Steam game is on
// the main screen, next to the quit button).
#include <windows.h>
#include "Core/System.h"
#include "Localization/Localization.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/Tutorials/TutorialPopups.h"
#include "Windows/Main/Settings/SettingsKit.h"

namespace SpeedrunLauncher::Main::SettingsTabs {

void Steam(SettingsContext& c) {
    auto& ui = c.ui;
    if (c.ImageOption(ui.MakeId("steam-copy"), L"Images\\Steam.jpg", Loc::Get("steam_launch_btn"))) {
        // "<this exe>" %command%  →  the game's Steam launch options
        wchar_t exe[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        System::SetClipboardText(L"\"" + std::wstring(exe) + L"\" %command%");
        c.main.Tutorials().ShowVideoPopup();
    }
}

} // namespace SpeedrunLauncher::Main::SettingsTabs
