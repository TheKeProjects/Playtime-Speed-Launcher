// Settings → Discord: what the Rich Presence shows.
#include <utility>
#include "App/Launcher.h"
#include "Localization/Localization.h"
#include "Services/Discord/DiscordPresenceService.h"
#include "Settings/DiscordPresenceSettings.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/LiveSplit/LiveSplitOverlay.h"
#include "Windows/Main/Settings/SettingsKit.h"

namespace SpeedrunLauncher::Main::SettingsTabs {

using namespace UI;

void Discord(SettingsContext& c) {
    auto& ui = c.ui;
    auto& s = Settings::DiscordPresence::Current();

    // Status panel: Background #050B12, BorderBrush #0A1D2E, CornerRadius 3, Padding 14,12
    const Rect panel{ c.col.x, c.col.y, c.col.w, 1.f + 12.f + 26.f * 3.f + 10.f * 2.f + 12.f + 1.f };
    ui.g.Border(panel, 3.f, Hex(0x050B12), Hex(0x0A1D2E), 1.f);
    Column inner(panel.x + 15.f, panel.y + 13.f, panel.w - 30.f);
    std::swap(c.col, inner);

    bool changed = false;
    if (c.ToggleRow(ui.MakeId("discord-activity"), Loc::Get("discord_show_activity"), s.showActivity, 11.f)) {
        s.showActivity = !s.showActivity;
        changed = true;
    }
    c.Gap(10.f);
    if (c.ToggleRow(ui.MakeId("discord-version"), Loc::Get("discord_show_version"), s.showVersion, 11.f)) {
        s.showVersion = !s.showVersion;
        changed = true;
    }
    c.Gap(10.f);
    bool liveSplitTurnedOn = false;
    if (c.ToggleRow(ui.MakeId("discord-livesplit"), Loc::Get("discord_show_livesplit"), s.showLiveSplit, 11.f)) {
        s.showLiveSplit = !s.showLiveSplit;
        liveSplitTurnedOn = s.showLiveSplit;
        changed = true;
    }

    std::swap(c.col, inner);
    c.col.Next(panel.h);
    if (!changed) return;
    s.Save();
    App::L().discord->ApplySettings(s.showActivity, s.showVersion, s.showLiveSplit);
    // The timer comes from the LiveSplit Server (TCP): explain how to turn it on
    if (liveSplitTurnedOn) c.main.LiveSplit().ShowTcpNotice();
}

} // namespace SpeedrunLauncher::Main::SettingsTabs
