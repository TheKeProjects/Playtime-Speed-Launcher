// Settings → Icon & theme: window / Discord icon plus the seasonal look.
#include "App/Launcher.h"
#include "Localization/Localization.h"
#include "Services/Discord/DiscordPresenceService.h"
#include "Settings/IconThemeSettings.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Settings/SettingsKit.h"

namespace SpeedrunLauncher::Main::SettingsTabs {

using namespace UI;

void IconTheme(SettingsContext& c) {
    auto& ui = c.ui;
    auto& theme = Settings::IconTheme::Current();
    c.Section(Loc::Get("icontheme_section"));
    c.Paragraph(Loc::Get("icontheme_hint"), 10.f, Palette::Label, 0.f, 12.f);

    static constexpr const wchar_t* kThemes[] = { L"default", L"classic", L"lgbtq", L"summer", L"halloween", L"christmas" };
    static constexpr const char* kKeys[] = { "icontheme_default", "icontheme_classic", "icontheme_lgbtq",
                                             "icontheme_summer", "icontheme_halloween", "icontheme_christmas" };
    for (int i = 0; i < 6; ++i) {
        if (!c.Option(ui.MakeId("icon-theme", i), Loc::Get(kKeys[i]), theme.theme == kThemes[i], i < 5 ? 8.f : 0.f)) continue;
        theme.theme = kThemes[i];
        theme.Save();
        c.main.ApplyIconTheme();
        App::L().discord->RefreshIconTheme();
        c.main.ApplySeasonalPalette();
    }
}

} // namespace SpeedrunLauncher::Main::SettingsTabs
