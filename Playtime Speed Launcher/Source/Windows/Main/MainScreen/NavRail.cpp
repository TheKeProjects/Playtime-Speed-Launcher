#include <algorithm>
#include <cmath>
#include "App/Paths.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Seasonal/SeasonalTheme.h"
#include "Services/Account/AccountService.h"
#include "Services/Account/DuelsService.h"
#include "Services/Account/FriendsService.h"
#include "Services/Account/MessagesService.h"
#include "Services/Audio/SoundEffects.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "Windows/Main/MainScreen/Bars.h"
#include "Windows/Main/Common/ProfileDraw.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/Duels/DuelsKit.h"
#include "Windows/Main/Overlays/Theme/ThemePopup.h"

namespace SpeedrunLauncher::Main::NavRail {

namespace {

constexpr float kTileW = 76.f, kTileH = 64.f, kIcon = 26.f;

// What a tile shows in its icon slot
enum class IconKind { Glyph, Image, Updates, Duels };

struct Tile {
    const char* id;
    IconKind kind;
    wchar_t glyph = 0;
    const wchar_t* image = nullptr;
    std::wstring label;
    std::wstring tooltip;
    Color tint = Colors::Transparent;   // transparent: the season's colours
    bool keepImageColors = false;
};

// The updates icon: two arrows chasing each other round a ring with a download arrow in the
// middle. Hovering spins them; while an update is waiting it's red and keeps turning slowly.
void DrawUpdatesIcon(UI::Ui& ui, const Rect& r, float hover, bool alert, bool resting, Color idle) {
    auto& g = ui.g;
    const Color lit = alert ? Rgba(230, 70, 50) : Seasonal::Current().accent;
    const Color color = alert ? lit : Lerp(idle, lit, hover);
    const float cx = r.CX(), cy = r.CY();
    constexpr float kR = 10.f;

    float& angle = ui.Slot(ui.MakeId("updates-icon-angle"), -1.5707963f);
    const float speed = (alert && !resting ? 1.4f : 0.f) + 5.f * hover;
    angle = std::fmod(angle + speed * ui.Dt(), 6.2831853f);
    if (hover > 0.01f) ui.KeepAnimating();

    for (int k = 0; k < 2; ++k) {
        const float a0 = angle + 3.1415927f * static_cast<float>(k);
        constexpr float kSweep = 2.3f;
        constexpr int kSegments = 14;
        for (int i = 0; i < kSegments; ++i) {
            const float s0 = a0 + kSweep * i / kSegments, s1 = a0 + kSweep * (i + 1) / kSegments;
            g.Line(cx + std::cos(s0) * kR, cy + std::sin(s0) * kR, cx + std::cos(s1) * kR, cy + std::sin(s1) * kR, color, 2.f, true);
        }
        const float e = a0 + kSweep;
        const float px = cx + std::cos(e) * kR, py = cy + std::sin(e) * kR;
        const float tx = -std::sin(e), ty = std::cos(e);
        for (const float side : { 0.6f, -0.6f }) {
            const float hx = -tx * std::cos(side) + ty * std::sin(side);
            const float hy = -tx * std::sin(side) - ty * std::cos(side);
            g.Line(px, py, px + hx * 4.5f, py + hy * 4.5f, color, 2.f, true);
        }
    }
    const float bob = (alert ? 1.f : 0.f) * std::sin(static_cast<float>(ui.Time()) * 4.f) + 1.5f * hover * std::sin(static_cast<float>(ui.Time()) * 6.f);
    UI::Glyph(ui, wchar_t(0xE896), { cx - 8.f, cy - 8.f + bob, 16.f, 16.f }, 8.f, color);
}

// One tile: icon over a small label; hovering lights it up and slides in a bar on its left edge.
// Returns true when clicked.
bool DrawTile(UI::Ui& ui, MainWindow& main, const Tile& tile, const Rect& r, bool alert = false) {
    auto& g = ui.g;
    const auto& palette = Seasonal::Current();
    const UI::Id id = ui.MakeId(tile.id);
    const UI::Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    if (it.entered) Services::Sfx::Play(Services::Sfx::Hover, true);
    const float hover = ui.Animate(ui.MakeId(tile.id, 1), it.hovered ? 1.f : 0.f, 16.f);
    const float press = ui.Animate(ui.MakeId(tile.id, 2), it.held ? 1.f : 0.f, 30.f);

    if (hover > 0.01f) {
        g.FillRound(r, 10.f, palette.accent.WithAlpha(0.09f * hover));
        const float barH = 28.f * hover;
        g.FillRound({ r.x - 6.f, r.CY() - barH * 0.5f, 3.f, barH }, 1.5f, palette.accent.WithAlpha(hover));
    }
    const bool tinted = !tile.tint.Transparent();
    const Color idle = tinted ? tile.tint.WithAlpha(0.75f) : palette.muted;
    const Color lit = tinted ? tile.tint : palette.accent;
    const Color color = Lerp(idle, lit, hover);
    const float lift = -2.f * hover + 1.f * press;
    const Rect icon{ r.CX() - kIcon * 0.5f, r.y + 9.f + lift, kIcon, kIcon };

    switch (tile.kind) {
    case IconKind::Glyph: UI::Glyph(ui, tile.glyph, icon, 19.f, color); break;
    case IconKind::Image:
        if (auto img = Images::Load(Paths::Asset(tile.image), 128)) {
            if (tile.keepImageColors) {
                NoColorFilter keep(g);
                g.DrawImage(*img, icon.Center(22.f, 22.f), 0.75f + 0.25f * hover);
            } else {
                g.DrawImage(*img, icon.Center(22.f, 22.f), 0.75f + 0.25f * hover);
            }
        }
        break;
    case IconKind::Updates: DrawUpdatesIcon(ui, icon, hover, alert, main.Resting(), idle); break;
    case IconKind::Duels: DuelsKit::SpeedWatchIcon(ui, icon, 17.f, color, hover); break;
    }
    TextStyle label = UI::Mono(8.5f, true).Center();
    label.ellipsis = true;
    g.Text(tile.label, { r.x + 2.f, icon.B() + 5.f, r.w - 4.f, 14.f }, Lerp(idle, Rgba(225, 235, 245), hover * 0.7f), label);
    if (!tile.tooltip.empty()) ui.Tooltip(id, tile.tooltip);
    return it.clicked;
}

// The launcher account: its round picture and the custom name (font + effect) once signed in,
// a person icon and "ACCOUNT" while signed out. Opens the account menu.
bool DrawAccountTile(UI::Ui& ui, MainWindow& main, const Rect& r) {
    auto& g = ui.g;
    const auto& palette = Seasonal::Current();
    auto& account = Services::Account::AccountService::Get();
    const bool signedIn = account.SignedIn();
    const auto& me = account.Me();
    const UI::Id id = ui.MakeId("nav-account");
    const UI::Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    if (it.entered) Services::Sfx::Play(Services::Sfx::Hover, true);
    const float hover = ui.Animate(ui.MakeId("nav-account", 1), it.hovered ? 1.f : 0.f, 16.f);
    const float press = ui.Animate(ui.MakeId("nav-account", 2), it.held ? 1.f : 0.f, 30.f);
    if (hover > 0.01f) {
        g.FillRound(r, 10.f, palette.accent.WithAlpha(0.09f * hover));
        const float barH = 28.f * hover;
        g.FillRound({ r.x - 6.f, r.CY() - barH * 0.5f, 3.f, barH }, 1.5f, palette.accent.WithAlpha(hover));
    }
    const float lift = -2.f * hover + 1.f * press;
    const Rect icon{ r.CX() - kIcon * 0.5f, r.y + 9.f + lift, kIcon, kIcon };
    const Rect labelRect{ r.x + 2.f, icon.B() + 5.f, r.w - 4.f, 14.f };
    const Color idle = palette.muted;
    if (signedIn) {
        const std::wstring name = me.ShownName(Loc::Get("acc_default_name"));
        // Nothing on the main screen moves while a game runs (a GIF or Prisma would keep it redrawing)
        const bool still = main.Resting();
        ProfileDraw::Avatar(ui, me.AvatarUrl(), icon, me, name, 1.5f, Lerp(palette.barBorder, palette.accent, hover), !still);
        ui.PushClip(labelRect);
        if (still && me.Effect().animated) ProfileDraw::NameWithEffect(ui, name, labelRect, me, "neon", 10.f, HAlign::Center);
        else ProfileDraw::Name(ui, name, labelRect, me, 10.f, HAlign::Center);
        ui.PopClip();
    } else {
        UI::Glyph(ui, wchar_t(0xE77B), icon, 19.f, Lerp(idle, palette.accent, hover));
        TextStyle label = UI::Mono(8.5f, true).Center();
        label.ellipsis = true;
        g.Text(Loc::Get("nav_account"), labelRect, Lerp(idle, Rgba(225, 235, 245), hover * 0.7f), label);
    }
    // Friend requests waiting and unread messages: a red count on the corner
    if (const int pending = signedIn ? Services::Account::FriendsService::Get().PendingCount() + Services::Account::MessagesService::Get().Unread() : 0;
        pending > 0) {
        const Rect badge{ r.R() - 24.f, r.y + 3.f, 18.f, 18.f };
        g.FillEllipse(badge.CX(), badge.CY(), 9.f, 9.f, Hex(0xCC2200));
        g.Text(pending > 9 ? L"9+" : std::to_wstring(pending), badge, Colors::White, UI::Mono(8.5f, true).Center());
    }
    ui.Tooltip(id, Loc::Get(signedIn ? "acc_badge_tooltip" : "acc_badge_tooltip_out"));
    return it.clicked;
}

} // namespace

void Draw(UI::Ui& ui, MainWindow& main, const Rect& area) {
    auto& g = ui.g;
    const auto& palette = Seasonal::Current();
    g.FillRect(area, palette.bar.WithAlpha(0.9f));
    g.FillRect({ area.R() - 1.f, area.y, 1.f, area.h }, palette.barBorder);
    // Rail is opaque: nothing behind it reacts to the mouse
    ui.Block(area);

    const float x = area.CX() - kTileW * 0.5f;
    float y = area.y + 14.f;
    const auto next = [&] {
        const Rect r{ x, y, kTileW, kTileH };
        y += kTileH + 6.f;
        return r;
    };

    if (DrawTile(ui, main, { "nav-handmods", IconKind::Image, 0, L"Images\\Hand.png", Loc::Get("nav_handmods"), L"Hand Mods" }, next()))
        main.OpenHandMods();
    if (DrawTile(ui, main, { "nav-leaderboard", IconKind::Image, 0, L"Images\\GoldTrophy.png", Loc::Get("nav_leaderboard"), L"Leaderboard" },
                 next()))
        main.OpenLeaderboard();
    {
        const bool alert = main.UpdateAlert();
        const Rect r = next();
        if (DrawTile(ui, main, { "nav-updates", IconKind::Updates, 0, nullptr, Loc::Get("nav_updates"), Loc::Get("updates_tooltip") }, r, alert))
            main.OpenUpdates();
        if (alert) {
            // "!" badge at the top-right corner, pulsing
            const float pulse = 1.f - 0.45f * Anim::EaseInOutSine(Anim::PingPong(ui.Time(), 1.2));
            const Rect badge{ r.R() - 22.f, r.y + 4.f, 16.f, 16.f };
            g.FillEllipse(badge.CX(), badge.CY(), 8.f, 8.f, Hex(0xCC2200).WithAlpha(pulse));
            g.Text(L"!", badge, Colors::White, UI::Mono(9.f, true).Center());
            if (!main.Resting()) ui.KeepAnimatingThrottled();
        }
    }
    if (DrawTile(ui, main, { "nav-tutorials", IconKind::Glyph, UI::Glyphs::Education, nullptr, Loc::Get("nav_tutorials"), L"" }, next()))
        main.OpenBeginnerTutorials();
    if (DrawTile(ui, main, { "nav-steam", IconKind::Image, 0, L"Images\\SteamLogo.png", Loc::Get("nav_steam"), Loc::Get("add_to_steam_tooltip"),
                             Colors::Transparent, true },
                 next()))
        main.OpenAddToSteam();

    // Bottom group: bug report · settings · quit
    y = area.B() - 14.f - kTileH;
    const auto prev = [&] {
        const Rect r{ x, y, kTileW, kTileH };
        y -= kTileH + 6.f;
        return r;
    };
    if (DrawTile(ui, main, { "nav-quit", IconKind::Glyph, UI::Glyphs::Power, nullptr, Loc::Get("nav_quit"), L"", Hex(0xCC3333) }, prev()))
        main.Quit();
    if (DrawTile(ui, main, { "nav-settings", IconKind::Glyph, UI::Glyphs::Settings, nullptr, Loc::Get("nav_settings"), Loc::Get("settings_tooltip") },
                 prev()))
        main.OpenSettings();
    {
        // Theme: opens the picker to the right of the button (the palette tilts while hovered)
        const Rect r = prev();
        if (DrawTile(ui, main, { "nav-theme", IconKind::Glyph, wchar_t(0xE790), nullptr, Loc::Get("nav_theme"), Loc::Get("nav_theme_title") }, r))
            main.Themes().Toggle(r);
    }
    if (DrawTile(ui, main, { "nav-bug", IconKind::Glyph, UI::Glyphs::Bug, nullptr, Loc::Get("nav_bug"), Loc::Get("bug_report_btn"), Hex(0x8855DD) },
                 prev()))
        main.OpenBugReport();
    if (DrawAccountTile(ui, main, prev())) main.OpenAccount();
    {
        // Duels, right above the account: challenges / our turn show a red count, a duel in
        // progress a pulsing dot
        auto& duels = Services::Account::DuelsService::Get();
        const bool signedIn = Services::Account::AccountService::Get().SignedIn();
        const Rect r = prev();
        if (DrawTile(ui, main, { "nav-duels", IconKind::Duels, 0, nullptr, Loc::Get("nav_duels"), Loc::Get("nav_duels_tooltip"), Hex(0xFF5A3C) }, r))
            main.OpenDuels();
        if (const int alerts = signedIn ? duels.AlertCount() : 0; alerts > 0) {
            const Rect badge{ r.R() - 24.f, r.y + 3.f, 18.f, 18.f };
            g.FillEllipse(badge.CX(), badge.CY(), 9.f, 9.f, Hex(0xCC2200));
            g.Text(alerts > 9 ? L"9+" : std::to_wstring(alerts), badge, Colors::White, UI::Mono(8.5f, true).Center());
        } else if (signedIn && duels.Active()) {
            const float pulse = main.Resting() ? 1.f : 0.55f + 0.45f * Anim::EaseInOutSine(Anim::PingPong(ui.Time(), 1.2));
            g.FillEllipse(r.R() - 15.f, r.y + 12.f, 5.f, 5.f, Hex(0x3DDC84).WithAlpha(pulse));
            if (!main.Resting()) ui.KeepAnimatingThrottled();
        }
    }
    // Divider above the bottom group
    g.FillRect({ area.x + 22.f, y + kTileH + 2.f, area.w - 44.f, 1.f }, palette.barBorder);
}

} // namespace SpeedrunLauncher::Main::NavRail
