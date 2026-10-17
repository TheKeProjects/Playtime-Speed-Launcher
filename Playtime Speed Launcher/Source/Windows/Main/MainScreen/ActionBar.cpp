#include <algorithm>
#include <cmath>
#include "App/Launcher.h"
#include "App/Paths.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Seasonal/SeasonalTheme.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "Windows/Main/MainScreen/Bars.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main::ActionBar {

namespace {

constexpr float kSize = 58.f;

UI::ButtonStyle SquareStyle() { return { Hex(0x0A1825), Hex(0x112233), 2.f, 5.f, { 0.f }, kSize, kSize }; }

bool ImageButton(UI::Ui& ui, const char* id, const Rect& r, const std::wstring& image, const std::wstring& tooltip) {
    UI::ButtonScope b(ui, ui.MakeId(id), r, SquareStyle());
    if (auto img = Images::Load(Paths::Asset(image))) ui.g.DrawImage(*img, r.Center(26.f, 26.f));
    ui.Tooltip(ui.MakeId(id), tooltip);
    return b.Clicked();
}

} // namespace

void Draw(UI::Ui& ui, MainWindow& main, const Rect& area) {
    auto& g = ui.g;
    const auto& palette = Seasonal::Current();
    // Margin="0,4,0,14"; the PLAY button stays centred on screen (Auto column between two "*" columns)
    const float top = area.y + 4.f;
    const float cy = top + kSize * 0.5f;

    const std::wstring playText = Loc::Get("play_button");
    const TextStyle playStyle = UI::Mono(15.f, true);
    const float playContentW = 16.f + 10.f + g.Measure(playText, playStyle).w;
    const float playW = std::max(260.f, playContentW + 40.f);
    const Rect play{ area.CX() - playW * 0.5f, top, playW, kSize };

    // ── Left group (right-aligned against the PLAY button's 8 px margin) ──
    float x = play.x - 8.f - kSize;
    const Rect settings{ x, top, kSize, kSize };
    {
        UI::ButtonStyle style = SquareStyle();
        if (main.UpdateAlert()) {
            // Border pulses dim blue → bright red (700 ms, auto-reverse, sine)
            const float t = Anim::EaseInOutSine(Anim::PingPong(ui.Time(), 1.4));
            style.border = Lerp(Rgba(17, 34, 51), Rgba(200, 30, 30), t);
            // A slow pulse needs ~60 fps at most, and no redraws at all while the launcher rests
            // (game running / another launcher window on top)
            if (!main.Resting()) ui.KeepAnimatingThrottled();
        }
        UI::ButtonScope b(ui, ui.MakeId("settings"), settings, style);
        UI::Glyph(ui, UI::Glyphs::Settings, settings, 22.f, Hex(0x3A6A8A));
        ui.Tooltip(ui.MakeId("settings"), Loc::Get("settings_tooltip"));
        if (b.Clicked()) main.OpenSettings();
    }
    if (main.UpdateAlert()) {
        // "!" badge at the top-right corner, opacity pulsing 1 → 0.55 (600 ms)
        const Rect badge{ settings.R() - 15.f, settings.y - 5.f, 20.f, 20.f };
        const float pulse = 1.f - 0.45f * Anim::EaseInOutSine(Anim::PingPong(ui.Time(), 1.2));
        g.PushOpacity(pulse);
        if (UI::TextButton(ui, ui.MakeId("updateBadge"), badge, L"!", UI::Mono(11.f, true).Center(), Colors::White,
                           { Hex(0xCC2200), Colors::Transparent, 0.f, 10.f, { 0.f }, 0.f, 20.f }))
            main.OpenSettings();
        g.PopLayer();
    }
    x -= 8.f + kSize;
    if (ImageButton(ui, "leaderboard", { x, top, kSize, kSize }, L"Images\\GoldTrophy.png", L"Leaderboard")) main.OpenLeaderboard();
    x -= 8.f + kSize;
    if (ImageButton(ui, "handMods", { x, top, kSize, kSize }, L"Images\\Hand.png", L"Hand Mods")) main.OpenHandMods();

    // ── PLAY ──
    const bool canPlay = main.Info().CanPlay();
    const bool enabled = canPlay && !main.Info().Running();
    if (Seasonal::IsSeasonal()) {
        // Softly pulsing glow (DropShadowEffect BlurRadius 30, opacity 0.3 → 0.85 over 1.6 s, sine ease)
        // Follows the seasonal clock, so it also eases to a stop while a game is running
        const float t = Anim::EaseInOutSine(Anim::PingPong(Seasonal::LastAnimationTime(), 3.2));
        g.GlowRound(play, 5.f, palette.playBackground, 30.f, 0.3f + 0.55f * t);
        if (!Seasonal::AnimationStopped()) ui.KeepAnimatingThrottled();
    }
    if (!canPlay) g.PushOpacity(0.35f);
    {
        UI::ButtonScope b(ui, ui.MakeId("play"), play, { palette.playBackground, Colors::Transparent, 0.f, 5.f, { 0.f }, 260.f, kSize },
                          enabled);
        const float cx = play.CX() - playContentW * 0.5f;
        UI::Glyph(ui, UI::Glyphs::Play, { cx, top, 16.f, kSize }, 16.f, palette.playForeground);
        g.Text(playText, { cx + 26.f, top, playContentW, kSize }, palette.playForeground, playStyle);
        if (b.Clicked()) main.PlaySelected();
    }
    if (!canPlay) g.PopLayer();

    // ── Right group: quit ──
    {
        const Rect quit{ play.R() + 8.f, top, kSize, kSize };
        UI::ButtonScope b(ui, ui.MakeId("quit"), quit, { Hex(0x1A0606), Hex(0x440000), 1.f, 5.f, { 0.f }, kSize, kSize });
        UI::Glyph(ui, UI::Glyphs::Power, quit, 20.f, Hex(0xCC3333));
        if (b.Clicked()) main.Quit();
    }
    (void)cy;
}

} // namespace SpeedrunLauncher::Main::ActionBar
