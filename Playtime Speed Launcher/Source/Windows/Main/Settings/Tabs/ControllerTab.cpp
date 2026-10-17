// Settings → Controller: the Steam Input setup tutorial.
#include "Localization/Localization.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/Tutorials/TutorialPopups.h"
#include "Windows/Main/Settings/SettingsKit.h"

namespace SpeedrunLauncher::Main::SettingsTabs {

void Controller(SettingsContext& c) {
    if (c.GlyphOption(c.ui.MakeId("steam-tutorial"), UI::Glyphs::Gamepad, Loc::Get("steam_tutorial_btn"))) c.main.Tutorials().ShowSteamTutorial();
}

} // namespace SpeedrunLauncher::Main::SettingsTabs
