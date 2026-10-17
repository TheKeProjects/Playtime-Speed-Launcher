// Settings → Updates: opens the updates panel; turns red with a pulsing "↑ UPDATE" badge when a
// new version is waiting.
#include <algorithm>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Widgets/Button.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Settings/SettingsKit.h"

namespace SpeedrunLauncher::Main::SettingsTabs {

using namespace UI;

void Updates(SettingsContext& c) {
    auto& ui = c.ui;
    auto& g = ui.g;
    const bool alert = c.main.UpdateAlert();
    auto reveal = c.Reveal();
    const Rect r = c.col.Next(std::max(OptionButtonHeight(ui), 16.f + 22.f));
    ButtonScope b(ui, ui.MakeId("open-updates"), r, Styles::Option());
    const Rect content = b.content;
    g.Text(std::wstring(1, Glyphs::Sync), { content.x, content.y, 16.f, content.h }, alert ? Rgba(200, 60, 20) : Palette::Muted, IconFont(16.f));
    g.Text(Loc::Get("updates_header"), content.CutLeft(26.f), alert ? Rgba(220, 80, 30) : Palette::Dim, Mono(11.f, true));
    if (alert) {
        const float pulse = 1.f - 0.45f * Anim::EaseInOutSine(Anim::PingPong(ui.Time(), 0.6));
        g.Text(L"\x2191 UPDATE", content, Palette::Error.WithAlpha(pulse), Mono(10.f, true).Right());
        ui.KeepAnimatingThrottled();
    }
    if (b.Clicked()) c.main.OpenUpdates();
}

} // namespace SpeedrunLauncher::Main::SettingsTabs
