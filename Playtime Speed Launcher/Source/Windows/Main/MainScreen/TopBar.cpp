#include <algorithm>
#include "App/Paths.h"
#include "Seasonal/SeasonalTheme.h"
#include "Services/App/AppVersion.h"
#include "Settings/HeartbeatSettings.h"
#include "Settings/WindowModeSettings.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "Windows/Main/MainScreen/Bars.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main::TopBar {

void Draw(UI::Ui& ui, MainWindow& main, const Rect& area) {
    auto& g = ui.g;
    const auto& palette = Seasonal::Current();
    g.FillRect(area, palette.bar);
    g.FillRect({ area.x, area.B() - 1.f, area.w, 1.f }, palette.barBorder);

    // Left: trophy + "Approved by SPEEDRUN.COM" + online users
    float x = area.x + 12.f;
    if (auto trophy = Images::Load(Paths::Asset(L"Images\\GoldTrophy.png"))) g.DrawImage(*trophy, { x, area.CY() - 8.f, 16.f, 16.f });
    x += 16.f + 6.f;
    const TextStyle smallText = UI::Mono(10.f);
    const std::wstring approved = L"Approved by SPEEDRUN.COM";
    const float approvedW = g.Measure(approved, smallText).w + 2.f;
    g.Text(approved, { x, area.y, approvedW, area.h }, Hex(0x00CC66), smallText);
    x += approvedW;
    if (Settings::Heartbeat::Current().showCounter) {
        x += 14.f;
        g.FillEllipse(x + 3.f, area.CY(), 3.f, 3.f, palette.accent);
        x += 6.f + 6.f;
        g.Text(main.OnlineText(), { x, area.y, 300.f, area.h }, Hex(0x2A5A7A), smallText);
    }

    // Right: [minimize] version
    const std::wstring version = AppVersion::DisplayVersion();
    const float versionW = g.Measure(version, smallText).w + 2.f;
    float right = area.R() - 12.f;
    g.Text(version, { right - versionW, area.y, versionW, area.h }, Hex(0x1A3A55), smallText);
    right -= versionW;
    if (Settings::WindowMode::Current().Windowed()) {
        const float h = g.Measure(L"\xE921", UI::IconFont(10.f)).h + 8.f + 2.f;
        const float w = 10.f + 16.f + 2.f;
        right -= 10.f + w;
        UI::ButtonStyle style{ Colors::Transparent, Hex(0x1A3A55), 1.f, 3.f, { 8.f, 4.f }, 0.f, h };
        UI::ButtonScope b(ui, ui.MakeId("minimize"), { right, area.CY() - h * 0.5f, w, h }, style);
        UI::Glyph(ui, UI::Glyphs::Minimize, b.content, 10.f, Hex(0x2A5A7A));
        ui.Tooltip(ui.MakeId("minimize"), L"Minimize");
        if (b.Clicked()) main.Minimize();
    }
}

} // namespace SpeedrunLauncher::Main::TopBar
