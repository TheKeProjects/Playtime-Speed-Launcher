// Settings → Steam: copy the launch option for Steam, or add the launcher as a non-Steam game.
#include <windows.h>
#include "Core/System.h"
#include "Localization/Localization.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/Tutorials/TutorialPopups.h"
#include "Windows/Main/Settings/SettingsKit.h"

namespace SpeedrunLauncher::Main::SettingsTabs {

void Steam(SettingsContext& c) {
    auto& ui = c.ui;
    if (c.ImageOption(ui.MakeId("steam-copy"), L"Images\\Steam.jpg", Loc::Get("steam_launch_btn"), 8.f)) {
        // "<this exe>" %command%  →  the game's Steam launch options
        wchar_t exe[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        System::SetClipboardText(L"\"" + std::wstring(exe) + L"\" %command%");
        c.main.Tutorials().ShowVideoPopup();
    }
    if (c.ImageOption(ui.MakeId("steam-add"), L"Images\\Steam.jpg", Loc::Get("add_to_steam_btn"))) c.main.OpenAddToSteam();
}

} // namespace SpeedrunLauncher::Main::SettingsTabs
