#include "Windows/Popups/TutorialWindows.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Animation.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/System.h"
#include "Graphics/Images.h"
#include "Localization/Localization.h"
#include "Seasonal/SeasonalCardArt.h"
#include "Seasonal/SeasonalTheme.h"
#include "UI/Layout.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/Panel.h"
#include "Windows/Dialogs/DialogParts.h"

namespace SpeedrunLauncher::Windows {

using namespace UI;
namespace P = Dialogs::Parts;

void CenterOnOwner(UI::Window& window, HWND owner, float width, float height) {
    RECT area{};
    if (owner && IsWindowVisible(owner) && !IsIconic(owner)) GetWindowRect(owner, &area);
    else SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
    const float scale = window.DpiScale();
    const int w = static_cast<int>(width * scale), h = static_cast<int>(height * scale);
    SetWindowPos(window.Hwnd(), nullptr, area.left + (area.right - area.left - w) / 2, area.top + (area.bottom - area.top - h) / 2, w, h,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

namespace {

UI::Window::Options PopupOptions(HWND owner, const wchar_t* title, int width, int height) {
    UI::Window::Options o;
    o.title = title;
    o.style = WS_POPUP;
    o.owner = owner;
    o.width = width;
    o.height = height;
    o.showCommand = SW_HIDE;
    return o;
}

} // namespace

// ── Video ───────────────────────────────────────────────────────────────────

bool TutorialVideoWindow::Open(HWND owner) {
    const std::wstring video = Paths::Asset(L"Videos\\Tutorial.mp4");
    if (!Files::Exists(video)) return false;
    if (!Create(PopupOptions(owner, L"Tutorial", 960, 570))) return false;
    CenterOnOwner(*this, owner, 960.f, 570.f);
    player.Open(video, true, true);
    player.SetVolume(0.5);   // MediaElement's default volume
    SetContinuous(true);
    Show(SW_SHOW);
    Activate();
    return true;
}

void TutorialVideoWindow::OnFrame() {
    const float w = g.Width();
    player.Update();
    // StackPanel: the video at full width (Stretch=Uniform), then the hint
    const float videoH = player.VideoWidth() > 0.f ? w * player.VideoHeight() / player.VideoWidth() : w * 9.f / 16.f;
    player.Draw(g, { 0.f, 0.f, w, videoH });
    // "Click to close" in a softly breathing pill under the video
    const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(ui.Time() * 2.4));
    const TextStyle hint = Mono(10.f, true).Center();
    const std::wstring text = L"Click to close";
    const float pw = g.Measure(text, hint).w + 44.f;
    const Rect pill{ (w - pw) * 0.5f, videoH + (g.Height() - videoH - 22.f) * 0.5f, pw, 22.f };
    g.GlowRound(pill, 11.f, Palette::Teal, 12.f, 0.15f + 0.15f * pulse);
    g.Border(pill, 11.f, Rgba(0, 204, 170, 18), Rgba(0, 204, 170, 110), 1.f);
    Glyph(ui, Glyphs::Cancel, { pill.x + 8.f, pill.y, 16.f, pill.h }, 8.f, Palette::Teal);
    g.Text(text, pill.CutLeft(16.f), Palette::Teal, hint);
    if (input.pressed[0] || input.pressed[1] || ui.KeyPressed(VK_ESCAPE)) Close();
}

// ── Steps ───────────────────────────────────────────────────────────────────

bool TutorialStepsWindow::Open(HWND owner, std::vector<Step> list) {
    steps = std::move(list);
    if (steps.empty()) return false;
    if (!Create(PopupOptions(owner, L"Tutorial", 640, 580))) return false;
    CenterOnOwner(*this, owner, 640.f, 580.f);
    g.SetColorFilter(&Seasonal::Filter);   // follows the seasonal theme like the main window
    SetContinuous(true, true);              // badge, glow and the season's particles
    SetIdleFps(30.0);
    Show(SW_SHOW);
    Activate();
    return true;
}

void TutorialStepsWindow::GoTo(size_t next) {
    if (next == step || next >= steps.size()) return;
    direction = next > step ? 1 : -1;
    step = next;
    copied = false;
    stepChangedAt = ui.Time();
}

void TutorialStepsWindow::OnFrame() {
    if (ui.KeyPressed(VK_ESCAPE)) {
        Close();
        return;
    }
    const double now = ui.Time();
    if (openedAt < 0.0) openedAt = stepChangedAt = now;
    const float sinceOpen = static_cast<float>(now - openedAt);
    const Step& s = steps[step];
    const size_t last = steps.size() - 1;
    const Rect all{ 0.f, 0.f, g.Width(), g.Height() };

    // Panel: dark gradient, teal wash from the top, the season's particles
    g.FillLinear(all, 0.f, { { 0.f, Hex(0x0A1724) }, { 1.f, Hex(0x060C14) } }, { 0.f, 0.f }, { 0.f, 1.f });
    g.FillLinear(all.Top(200.f), 0.f, { { 0.f, Palette::Teal.WithAlpha(0.07f) }, { 1.f, Palette::Teal.WithAlpha(0.f) } }, { 0.f, 0.f }, { 0.f, 1.f });
    Seasonal::CardArt::DrawPanelParticles(g, all, 0.f, now, 0.5f);

    // Header: badge, "TUTORIAL", step dots
    const Rect header{ 0.f, 0.f, all.w, 64.f };
    g.FillRect(header, Hex(0x05090F));
    g.FillLinear({ 0.f, header.B() - 1.f, all.w, 1.f }, 0.f,
                 { { 0.f, Palette::Teal.WithAlpha(0.f) }, { 0.5f, Palette::Teal.WithAlpha(0.7f) }, { 1.f, Palette::Teal.WithAlpha(0.f) } }, { 0.f, 0.f },
                 { 1.f, 0.f });
    MenuKit::GlyphBadge(ui, { 20.f, header.CY() - 18.f, 36.f, 36.f }, s.controllerIcon ? Glyphs::Gamepad : Glyphs::Info, Palette::Teal, 16.f);
    g.Text(L"TUTORIAL", { 70.f, header.y + 13.f, 300.f, 20.f }, Palette::Teal, Mono(14.f, true));
    g.Text(std::to_wstring(step + 1) + L" / " + std::to_wstring(steps.size()), { 70.f, header.y + 34.f, 300.f, 16.f }, MenuKit::kDim, Mono(9.f, true));
    {
        float x = header.R() - 20.f;
        for (size_t i = steps.size(); i-- > 0;) {
            const float on = ui.Animate(ui.MakeId("dot", static_cast<int64_t>(i)), i == step ? 1.f : 0.f, 14.f);
            const float w = 8.f + 18.f * on;
            x -= w;
            g.FillRound({ x, header.CY() - 4.f, w, 8.f }, 4.f, Lerp(Palette::ButtonBorder, Palette::Teal, std::max(on, i < step ? 0.55f : 0.f)));
            x -= 8.f;
        }
    }

    // The step: slides in from the side it came from
    const Rect footer{ 0.f, all.h - 64.f, all.w, 64.f };
    const float panelW = all.w - 64.f;
    const float change = Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>(now - stepChangedAt) / 0.35f));
    if (change < 1.f) ui.KeepAnimating();
    ImagePtr image = s.image && Files::Exists(*s.image) ? Images::Load(*s.image) : nullptr;
    float imageW = 0.f, imageH = 0.f;
    if (image) {
        const float scale = std::min({ panelW / image->w, 300.f / image->h });
        imageW = image->w * scale;
        imageH = image->h * scale;
    }
    TextStyle captionStyle = Mono(13.f, true);
    captionStyle.align = HAlign::Center;
    captionStyle.wrap = true;
    captionStyle.valign = VAlign::Top;
    const float captionH = s.caption && !s.caption->empty() ? g.Measure(*s.caption, captionStyle, panelW).h : 0.f;
    constexpr float kPadIcon = 120.f;
    const float total = (image ? imageH + 18.f : 0.f) + (s.controllerIcon ? kPadIcon + 18.f : 0.f) + (captionH > 0.f ? captionH + 16.f : 0.f) +
                        (s.copyButton ? 38.f + 10.f + 22.f : 0.f);
    const float areaTop = header.B(), areaH = footer.y - header.B();
    Column col(32.f, areaTop + std::max(16.f, (areaH - total) * 0.5f), panelW);
    g.PushOpacity(change);
    ui.PushTranslate(28.f * (1.f - change) * static_cast<float>(direction), 0.f);

    if (image) {
        const Rect r = Rect{ col.x, col.Next(imageH, 18.f).y, panelW, imageH }.Center(imageW, imageH);
        g.GlowRound(r, 10.f, Palette::Teal, 18.f, 0.22f);
        g.DrawImageRounded(*image, r, 10.f, 1.f, Stretch::Uniform);
        g.StrokeRound(r.Inset(0.5f), 9.5f, Palette::Teal.WithAlpha(0.45f), 1.f);
    }
    if (s.controllerIcon) {
        const Rect row = col.Next(kPadIcon, 18.f);
        MenuKit::GlyphBadge(ui, row.Center(kPadIcon, kPadIcon), Glyphs::Gamepad, Palette::Teal, 56.f);
    }
    if (captionH > 0.f) g.Text(*s.caption, col.Next(captionH, 16.f), MenuKit::kTitle, captionStyle);
    if (s.copyButton) {
        const std::wstring text = Loc::Get("steam_tutorial_copy_btn");
        const float w = MenuKit::FooterButtonWidth(ui, text, Glyphs::Copy, 200.f);
        const Rect row = col.Next(38.f, 10.f);
        if (MenuKit::FooterButton(ui, ui.MakeId("tutorial-copy"), { row.CX() - w * 0.5f, row.y, w, 38.f }, text, MenuKit::Kind::Primary, true, Glyphs::Copy)) {
            wchar_t exe[MAX_PATH]{};
            GetModuleFileNameW(nullptr, exe, MAX_PATH);
            copied = System::SetClipboardText(L"\"" + std::wstring(exe) + L"\" %command%");
            if (copied) copiedAt = now;
        }
        // "Copied!" pops in as a green chip (keeps its space while hidden)
        const Rect chipRow = col.Next(22.f);
        if (copied) {
            const float k = Anim::EaseOutBack(Anim::Clamp01(static_cast<float>(now - copiedAt) / 0.35f), 1.6f);
            const std::wstring done = Loc::Get("steam_tutorial_copied");
            const float cw = g.Measure(done, Mono(9.f, true)).w + 18.f;
            ui.PushScale(0.7f + 0.3f * k, chipRow.CX(), chipRow.CY());
            MenuKit::Chip(ui, chipRow.CX() - cw * 0.5f, chipRow.CY(), done, MenuKit::kGreen);
            ui.PopTransform();
            if (k < 1.f) ui.KeepAnimating();
        }
    }
    ui.PopTransform();
    g.PopLayer();

    // Footer: BACK · NEXT / GOT IT
    g.FillRect(footer, Palette::HeaderBg);
    g.FillRect({ footer.x, footer.y, footer.w, 1.f }, Palette::Divider);
    const std::wstring prev = Loc::Get("steam_tutorial_prev");
    const std::wstring next = Loc::Get(step == last ? "steam_tutorial_done" : "steam_tutorial_next");
    const float pw = MenuKit::FooterButtonWidth(ui, prev, Glyphs::Back, 120.f), nw = MenuKit::FooterButtonWidth(ui, next, step == last ? Glyphs::CheckMark : Glyphs::Forward, 140.f);
    if (MenuKit::FooterButton(ui, ui.MakeId("tutorial-prev"), { footer.x + 18.f, footer.CY() - 18.f, pw, 36.f }, prev, MenuKit::Kind::Secondary, step > 0,
                              Glyphs::Back) ||
        (step > 0 && ui.KeyPressed(VK_LEFT)))
        GoTo(step - 1);
    if (MenuKit::FooterButton(ui, ui.MakeId("tutorial-next"), { footer.R() - 18.f - nw, footer.CY() - 18.f, nw, 36.f }, next, MenuKit::Kind::Primary, true,
                              step == last ? Glyphs::CheckMark : Glyphs::Forward) ||
        ui.KeyPressed(VK_RIGHT) || ui.KeyPressed(VK_RETURN)) {
        if (step < last) GoTo(step + 1);
        else Close();
    }
    g.StrokeRect(all, Rgba(21, 48, 72), 1.f);
    if (PanelEntranceRunning(sinceOpen)) ui.KeepAnimating();
}

} // namespace SpeedrunLauncher::Windows
