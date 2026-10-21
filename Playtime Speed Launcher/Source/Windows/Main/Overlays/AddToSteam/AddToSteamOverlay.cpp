#include "Windows/Main/Overlays/AddToSteam/AddToSteamOverlay.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"
#include "App/Paths.h"
#include "App/SteamShortcutHelper.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Graphics/Images.h"
#include "Localization/Localization.h"
#include "Services/Platforms/SteamShortcutService.h"
#include "Settings/IconThemeSettings.h"
#include "UI/Layout.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/Mods/ModPopupFrame.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
namespace K = UI::MenuKit;

namespace {

constexpr const wchar_t* kIcons[] = { L"iconHD.png", L"iconHD Christmas.png", L"iconHD Halloween.png", L"iconHD LGBTQ+.png", L"iconHD Summer.png" };

std::wstring DefaultIcon() {
    const std::wstring theme = Settings::IconTheme::Current().EffectiveTheme();
    if (theme == L"lgbtq") return L"iconHD LGBTQ+.png";
    if (theme == L"summer") return L"iconHD Summer.png";
    if (theme == L"halloween") return L"iconHD Halloween.png";
    if (theme == L"christmas") return L"iconHD Christmas.png";
    return L"iconHD.png";
}

std::wstring IconPath(const std::wstring& file) { return Paths::Asset(L"Steam\\icons\\" + file); }

} // namespace

void AddToSteamOverlay::Open() {
    icon = DefaultIcon();
    steamRunning = Services::SteamShortcuts::IsSteamRunning();
    popup.Open(true);
}

void AddToSteamOverlay::Confirm() {
    popup.Close(true);
    const std::wstring exe = Files::ExecutablePath();
    if (exe.empty()) {
        ShowModMessage(main.Hwnd(), L"ADD TO STEAM", Loc::Get("add_to_steam_error_no_exe"), false);
        return;
    }
    const std::wstring source = IconPath(icon.empty() ? DefaultIcon() : icon);
    if (!Files::Exists(source)) {
        ShowModMessage(main.Hwnd(), L"ADD TO STEAM", Loc::Get("add_to_steam_error_no_icon"), false);
        return;
    }
    const std::wstring args = std::wstring(App::SteamShortcutHelperArg) + L" " + Process::QuoteArgument(exe) + L" " + Process::QuoteArgument(source);
    if (!Process::StartElevated(exe, args)) {
        ShowModMessage(main.Hwnd(), L"ADD TO STEAM", Loc::Fmt("add_to_steam_error_generic", std::wstring(L"UAC")), false);
        return;
    }
    // Hard exit: if Steam tracks this process as "the game", it must be gone before the helper
    // shuts Steam down (a graceful teardown could still be running).
    ExitProcess(0);
}

// The library preview (the picked icon floating in a glowing tile, the name under it), the five
// icons to pick from (lift on hover, the picked one glows with a check), what will happen to Steam,
// Cancel / Add to Steam. Same look as the --steam-shortcut-helper window that takes over after it.
void AddToSteamOverlay::Draw(Ui& ui, const Rect& screen) {
    auto& g = ui.g;
    const float t = static_cast<float>(ui.Time());
    PanelStyle style;
    style.title = L"[ ADD TO STEAM ]";
    OverlayFrame frame(ui, ui.MakeId("addtosteam"), screen, 480.f, 0.f, style, popup.Opacity(), popup.Scale());
    const double start = frame.OpenedAt() + 0.15;
    Column col(frame.body.x + 24.f, frame.body.y + 20.f, frame.body.w - 48.f);
    int row = 0;
    float& changedAt = ui.Slot(ui.MakeId("steam-icon-changed"), -10.f);

    // Preview tile
    {
        StaggerItem reveal(ui, start, row++, 0.f, 12.f);
        g.Text(Loc::Get("add_to_steam_preview"), col.Next(14.f, 12.f), K::kDim, Mono(9.f, true).Center());
        const Rect slot = col.Next(96.f, 10.f);
        const Rect tile = slot.Center(96.f, 96.f).Offset(0.f, 3.f * std::sin(t * 2.f));
        g.GlowRound(tile, 18.f, Palette::Teal, 22.f, 0.35f + 0.15f * std::sin(t * 2.f));
        g.FillLinear(tile, 18.f, { { 0.f, Hex(0x0D2535) }, { 1.f, Palette::HeaderBg } }, { 0.f, 0.f }, { 1.f, 1.f });
        // The picked icon pops in whenever it changes
        const float pop = Anim::EaseOutBack(Anim::Clamp01((t - changedAt) / 0.35f), 1.8f);
        if (auto img = Images::Load(IconPath(icon.empty() ? DefaultIcon() : icon), 256)) {
            NoColorFilter keep(g);   // the icon keeps its own colours
            ui.PushScale(0.8f + 0.2f * pop, tile.CX(), tile.CY());
            g.DrawImage(*img, tile.Inset(13.f));
            ui.PopTransform();
        }
        const float sweep = std::fmod(t, 3.5f) / 0.9f;
        if (sweep < 1.f)
            g.FillLinear(tile, 18.f,
                         { { std::clamp(sweep - 0.25f, 0.f, 1.f), Rgba(255, 255, 255, 0) }, { std::clamp(sweep, 0.f, 1.f), Rgba(255, 255, 255, 60) },
                           { std::clamp(sweep + 0.25f, 0.f, 1.f), Rgba(255, 255, 255, 0) } },
                         { 0.f, 0.f }, { 1.f, 1.f });
        g.StrokeRound(tile.Inset(0.75f), 17.25f, Palette::Teal.WithAlpha(0.7f), 1.5f);
        g.Text(Services::SteamShortcuts::AppName, col.Next(20.f, 18.f), K::kTitle, Mono(13.f, true).Center());
        ui.KeepAnimatingThrottled();
    }

    // Icon picker
    {
        StaggerItem reveal(ui, start, row++, 0.f, 12.f);
        g.Text(Loc::Get("add_to_steam_icon_label"), col.Next(14.f, 10.f), Palette::Label, Mono(10.f, true).Center());
        const Rect line = col.Next(58.f, 16.f);
        constexpr float kSize = 54.f, kGap = 12.f;
        float x = line.CX() - (kSize * 5.f + kGap * 4.f) * 0.5f;
        for (int i = 0; i < 5; ++i) {
            const Rect slot{ x, line.y + 2.f, kSize, kSize };
            x += kSize + kGap;
            const Id id = ui.MakeId("steam-icon", i);
            const Interaction it = ui.Interact(id, slot);
            if (it.hovered) ui.SetCursor(IDC_HAND);
            const bool selected = icon == kIcons[i];
            const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 16.f, 0.f);
            const float sel = ui.Animate(HashId(id, 2), selected ? 1.f : 0.f, 14.f);
            const Rect r = slot.Offset(0.f, -3.f * hover);
            const float lit = std::max(hover * 0.7f, sel);
            if (lit > 0.01f) g.GlowRound(r, 10.f, Palette::Teal, 14.f, 0.45f * lit);
            g.Border(r, 10.f, Lerp(Palette::ButtonBg, Rgba(0, 40, 30), sel), Lerp(Palette::ControlBorder, Palette::Teal, lit), 1.f + sel);
            if (auto img = Images::Load(IconPath(kIcons[i]), 128)) {
                NoColorFilter keep(g);
                g.DrawImage(*img, r.Inset(7.f - 2.f * hover));
            }
            if (sel > 0.01f) {
                // Check badge on the corner
                const float k = Anim::EaseOutBack(sel, 1.6f);
                g.FillEllipse(r.R() - 4.f, r.y + 4.f, 8.f * k, 8.f * k, Palette::Teal);
                Glyph(ui, Glyphs::CheckMark, { r.R() - 12.f, r.y - 4.f, 16.f, 16.f }, 7.f * k, Rgba(3, 17, 13));
            }
            if (it.clicked && !selected) {
                icon = kIcons[i];
                changedAt = t;
            }
        }
    }

    // What happens to Steam: amber with a turning arrow when it has to restart, teal otherwise
    {
        StaggerItem reveal(ui, start, row++, 0.f, 12.f);
        const std::wstring warning = Loc::Get(steamRunning ? "add_to_steam_warning_running" : "add_to_steam_warning_not_running");
        const float h = K::NoteHeight(ui, warning, col.w);
        K::Note(ui, col.Next(h), steamRunning ? Glyphs::Refresh : Glyphs::Info, warning, steamRunning ? K::kAmber : Palette::Teal, steamRunning);
    }
    frame.SetContentHeight(20.f + col.Height() + 20.f);

    const std::wstring cancel = Loc::Get("cancel"), confirm = Loc::Get("add_to_steam_btn");
    const float cw = K::FooterButtonWidth(ui, cancel, Glyphs::Cancel, 110.f), aw = K::FooterButtonWidth(ui, confirm, Glyphs::CheckMark, 170.f);
    if (K::FooterButton(ui, ui.MakeId("steam-cancel"), { frame.footer.x + 14.f, frame.footer.CY() - 18.f, cw, 36.f }, cancel, K::Kind::Secondary, true,
                        Glyphs::Cancel) ||
        (popup.IsOpen() && ui.KeyPressed(VK_ESCAPE)))
        popup.Close(true);
    if (K::FooterButton(ui, ui.MakeId("steam-confirm"), { frame.footer.R() - 14.f - aw, frame.footer.CY() - 18.f, aw, 36.f }, confirm, K::Kind::Primary,
                        true, Glyphs::CheckMark) ||
        (popup.IsOpen() && ui.KeyPressed(VK_RETURN)))
        Confirm();
}

} // namespace SpeedrunLauncher::Main
