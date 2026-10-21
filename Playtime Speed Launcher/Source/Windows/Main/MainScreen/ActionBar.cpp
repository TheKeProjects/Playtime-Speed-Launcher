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

// The updates button's icon: two arrows chasing each other round a ring with a download arrow in
// the middle. Hovering lights it up teal, spins the arrows and bobs the middle arrow; while an
// update is waiting it's red and keeps turning slowly.
void DrawUpdatesIcon(UI::Ui& ui, const Rect& r, bool hovered, bool alert, bool resting) {
    auto& g = ui.g;
    const float hover = ui.Animate(ui.MakeId("updates-icon-hover"), hovered ? 1.f : 0.f, 12.f, 0.f);
    const Color idle = Hex(0x3A6A8A), lit = alert ? Rgba(230, 70, 50) : UI::Palette::Teal;
    const Color color = alert ? lit : Lerp(idle, lit, hover);
    const float cx = r.CX(), cy = r.CY();
    constexpr float kR = 15.f;

    // Spin: accumulated so it eases in and out instead of jumping
    float& angle = ui.Slot(ui.MakeId("updates-icon-angle"), -1.5707963f);
    const float speed = (alert && !resting ? 1.4f : 0.f) + 5.f * hover;
    angle = std::fmod(angle + speed * ui.Dt(), 6.2831853f);
    if (hover > 0.01f && hover < 0.99f) ui.KeepAnimating();
    if (hovered) ui.KeepAnimating();

    // Soft halo behind (stronger while hovered / alerting)
    const float halo = std::max(hover, alert ? 0.6f : 0.f);
    if (halo > 0.01f) {
        g.FillEllipse(cx, cy, kR + 8.f, kR + 8.f, color.WithAlpha(0.10f * halo));
        g.FillEllipse(cx, cy, kR + 3.f, kR + 3.f, color.WithAlpha(0.08f * halo));
    }
    // Two arcs with arrow heads
    for (int k = 0; k < 2; ++k) {
        const float a0 = angle + 3.1415927f * static_cast<float>(k);
        constexpr float kSweep = 2.3f;
        constexpr int kSegments = 16;
        for (int i = 0; i < kSegments; ++i) {
            const float s0 = a0 + kSweep * i / kSegments, s1 = a0 + kSweep * (i + 1) / kSegments;
            g.Line(cx + std::cos(s0) * kR, cy + std::sin(s0) * kR, cx + std::cos(s1) * kR, cy + std::sin(s1) * kR, color, 2.4f, true);
        }
        const float e = a0 + kSweep;
        const float px = cx + std::cos(e) * kR, py = cy + std::sin(e) * kR;
        const float tx = -std::sin(e), ty = std::cos(e);   // direction the arrow travels
        // Head: the backward direction turned ±35°
        for (const float side : { 0.6f, -0.6f }) {
            const float hx = -tx * std::cos(side) + ty * std::sin(side);
            const float hy = -tx * std::sin(side) - ty * std::cos(side);
            g.Line(px, py, px + hx * 6.f, py + hy * 6.f, color, 2.4f, true);
        }
    }
    // Download arrow in the middle, bobbing while hovered / alerting
    const float bob = (alert ? 1.5f : 0.f) * std::sin(static_cast<float>(ui.Time()) * 4.f) + 2.f * hover * std::sin(static_cast<float>(ui.Time()) * 6.f);
    UI::Glyph(ui, wchar_t(0xE896), { cx - 10.f, cy - 10.f + bob, 20.f, 20.f }, 11.f, Lerp(color, Colors::White, 0.25f * hover));
}

} // namespace

void Draw(UI::Ui& ui, MainWindow& main, const Rect& area) {
    auto& g = ui.g;
    const auto& palette = Seasonal::Current();
    // Margin="0,4,0,14"; the PLAY button stays centred on screen (Auto column between two "*" columns)
    const float top = area.y + 4.f;
    const float cy = top + kSize * 0.5f;

    const std::wstring playText = Loc::Get(App::L().InMultiplayers() ? "play_button_mp" : "play_button");
    const TextStyle playStyle = UI::Mono(15.f, true);
    const float playContentW = 16.f + 10.f + g.Measure(playText, playStyle).w;
    const float playW = std::max(260.f, playContentW + 40.f);
    const Rect play{ area.CX() - playW * 0.5f, top, playW, kSize };

    // ── Left group (right-aligned against the PLAY button's 8 px margin) ──
    float x = play.x - 8.f - kSize;
    const Rect settings{ x, top, kSize, kSize };
    {
        UI::ButtonScope b(ui, ui.MakeId("settings"), settings, SquareStyle());
        UI::Glyph(ui, UI::Glyphs::Settings, settings, 22.f, Hex(0x3A6A8A));
        ui.Tooltip(ui.MakeId("settings"), Loc::Get("settings_tooltip"));
        if (b.Clicked()) main.OpenSettings();
    }
    x -= 8.f + kSize;
    const Rect updates{ x, top, kSize, kSize };
    {
        // Updates & changelog. While an update is waiting the border pulses red and the arrows turn
        UI::ButtonStyle style = SquareStyle();
        const bool alert = main.UpdateAlert();
        if (alert) {
            // Border pulses dim blue → bright red (700 ms, auto-reverse, sine)
            const float t = Anim::EaseInOutSine(Anim::PingPong(ui.Time(), 1.4));
            style.border = Lerp(Rgba(17, 34, 51), Rgba(200, 30, 30), t);
            // A slow pulse needs ~60 fps at most, and no redraws at all while the launcher rests
            // (game running / another launcher window on top)
            if (!main.Resting()) ui.KeepAnimatingThrottled();
        }
        UI::ButtonScope b(ui, ui.MakeId("updates"), updates, style);
        DrawUpdatesIcon(ui, updates, ui.Hot(ui.MakeId("updates")), alert, main.Resting());
        ui.Tooltip(ui.MakeId("updates"), Loc::Get("updates_tooltip"));
        if (b.Clicked()) main.OpenUpdates();
    }
    if (main.UpdateAlert()) {
        // "!" badge at the top-right corner, opacity pulsing 1 → 0.55 (600 ms)
        const Rect badge{ updates.R() - 15.f, updates.y - 5.f, 20.f, 20.f };
        const float pulse = 1.f - 0.45f * Anim::EaseInOutSine(Anim::PingPong(ui.Time(), 1.2));
        g.PushOpacity(pulse);
        if (UI::TextButton(ui, ui.MakeId("updateBadge"), badge, L"!", UI::Mono(11.f, true).Center(), Colors::White,
                           { Hex(0xCC2200), Colors::Transparent, 0.f, 10.f, { 0.f }, 0.f, 20.f }))
            main.OpenUpdates();
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

    // ── Right group: quit · add to Steam ──
    const Rect quit{ play.R() + 8.f, top, kSize, kSize };
    {
        UI::ButtonScope b(ui, ui.MakeId("quit"), quit, { Hex(0x1A0606), Hex(0x440000), 1.f, 5.f, { 0.f }, kSize, kSize });
        UI::Glyph(ui, UI::Glyphs::Power, quit, 20.f, Hex(0xCC3333));
        if (b.Clicked()) main.Quit();
    }
    {
        const Rect steam{ quit.R() + 8.f, top, kSize, kSize };
        UI::ButtonScope b(ui, ui.MakeId("addToSteam"), steam, SquareStyle());
        if (auto img = Images::Load(Paths::Asset(L"Images\\SteamLogo.png"), 128)) {
            NoColorFilter keep(g);   // the logo keeps Steam's own colours in every season
            g.DrawImage(*img, steam.Center(30.f, 30.f));
        }
        ui.Tooltip(ui.MakeId("addToSteam"), Loc::Get("add_to_steam_tooltip"));
        if (b.Clicked()) main.OpenAddToSteam();
    }
    (void)cy;
}

} // namespace SpeedrunLauncher::Main::ActionBar
