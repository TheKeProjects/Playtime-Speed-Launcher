// Settings → LiveSplit: opens the LiveSplit panel (badge when a LiveSplit update exists).
#include <algorithm>
#include "Graphics/Animation.h"
#include "UI/Icons.h"
#include "UI/Widgets/Button.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/LiveSplit/LiveSplitOverlay.h"
#include "Windows/Main/Settings/SettingsKit.h"

namespace SpeedrunLauncher::Main::SettingsTabs {

using namespace UI;

void LiveSplit(SettingsContext& c) {
    auto& ui = c.ui;
    auto& g = ui.g;
    const bool alert = c.main.LiveSplitUpdateAlert();
    auto reveal = c.Reveal();
    const Rect r = c.col.Next(std::max(OptionButtonHeight(ui), 18.f + 22.f));
    ButtonScope b(ui, ui.MakeId("open-livesplit"), r, Styles::Option());
    const Rect content = b.content;
    Icons::LiveSplit(g, { content.x, content.CY() - 9.f, 18.f, 18.f });
    // Green once installed and current, red while an update waits
    const auto& info = c.main.LiveSplit().Info();
    const Color color = alert ? Rgba(220, 80, 30) : info && info->isInstalled && !info->isUpdateAvailable ? Rgba(0, 120, 90) : Palette::Dim;
    g.Text(L"LiveSplit", content.CutLeft(28.f), color, Mono(11.f, true));
    if (alert) {
        const float pulse = 1.f - 0.45f * Anim::EaseInOutSine(Anim::PingPong(ui.Time(), 0.6));
        g.Text(L"\x2191 UPDATE", content, Palette::Error.WithAlpha(pulse), Mono(10.f, true).Right());
        ui.KeepAnimatingThrottled();
    }
    if (b.Clicked()) c.main.OpenLiveSplit();
}

} // namespace SpeedrunLauncher::Main::SettingsTabs
