#include "Windows/Main/Overlays/AddToSteam/AddToSteamOverlay.h"
#include <windows.h>
#include "App/Paths.h"
#include "App/SteamShortcutHelper.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Graphics/Images.h"
#include "Localization/Localization.h"
#include "Services/Platforms/SteamShortcutService.h"
#include "Settings/IconThemeSettings.h"
#include "UI/Widgets/Button.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/Mods/ModPopupFrame.h"

namespace SpeedrunLauncher::Main {

using namespace UI;

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

void AddToSteamOverlay::Draw(Ui& ui, const Rect& screen) {
    auto& g = ui.g;
    PanelStyle style;
    style.title = L"[ ADD TO STEAM ]";
    OverlayFrame frame(ui, ui.MakeId("addtosteam"), screen, 460.f, 0.f, style, popup.Opacity(), popup.Scale());
    float y = frame.body.y + 20.f;
    const float innerW = frame.body.w - 48.f;

    const std::wstring label = Loc::Get("add_to_steam_icon_label");
    const float labelH = g.Measure(L"Ag", Mono(10.f, true)).h;
    g.Text(label, { frame.body.x, y, frame.body.w, labelH }, Palette::Label, Mono(10.f, true).Center());
    y += labelH + 10.f;

    // Five 52×52 icon buttons (selected = teal 2 px border)
    const float rowW = 52.f * 5.f + 8.f * 4.f;
    float x = frame.body.CX() - rowW * 0.5f;
    for (int i = 0; i < 5; ++i) {
        const bool selected = icon == kIcons[i];
        ButtonScope b(ui, ui.MakeId("steam-icon", i), { x, y, 52.f, 52.f },
                      { Palette::ButtonBg, selected ? Palette::Teal : Palette::ControlBorder, 2.f, 4.f, { 4.f }, 0.f, 52.f });
        g.DrawImage(Images::Load(IconPath(kIcons[i])), b.content);
        if (b.Clicked()) icon = kIcons[i];
        x += 52.f + 8.f;
    }
    y += 52.f;

    // Steam running / not running warning
    TextStyle warn = Mono(10.f, true);
    warn.wrap = true;
    warn.align = HAlign::Center;
    warn.valign = VAlign::Top;
    const std::wstring warning = Loc::Get(steamRunning ? "add_to_steam_warning_running" : "add_to_steam_warning_not_running");
    const float warnH = g.Measure(warning, warn, innerW).h;
    g.Text(warning, { frame.body.x + 24.f, y + 16.f, innerW, warnH }, Palette::Warning, warn);
    y += 16.f + warnH;
    frame.SetContentHeight(y + 20.f - frame.body.y);

    const Rect row = frame.footer.Inset(14.f, 0.f);
    const TextStyle text = Mono(12.f, true).Center();
    const std::wstring cancel = Loc::Get("cancel"), confirm = Loc::Get("add_to_steam_btn");
    const ButtonStyle danger = Styles::Danger(110.f), ok = Styles::Confirm(150.f);
    if (TextButton(ui, ui.MakeId("steam-cancel"), { row.x, row.CY() - 18.f, ButtonWidth(ui, cancel, text, danger), 36.f }, cancel, text,
                   Palette::DangerText, danger))
        popup.Close(true);
    const float cw = ButtonWidth(ui, confirm, text, ok);
    if (TextButton(ui, ui.MakeId("steam-confirm"), { row.R() - cw, row.CY() - 18.f, cw, 36.f }, confirm, text, Palette::Teal, ok)) Confirm();
}

} // namespace SpeedrunLauncher::Main
