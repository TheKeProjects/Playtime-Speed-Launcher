#include "Windows/Popups/TutorialWindows.h"
#include <algorithm>
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/System.h"
#include "Graphics/Images.h"
#include "Localization/Localization.h"
#include "Seasonal/SeasonalTheme.h"
#include "UI/Layout.h"
#include "UI/Style.h"
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
    TextStyle hint;
    hint.font = Font::Ui;
    hint.size = 11.f;
    hint.align = HAlign::Center;
    hint.valign = VAlign::Top;
    g.Text(L"Click to close", { 0.f, videoH + 4.f, w, 20.f }, Rgba(0, 204, 170, 180), hint);
    if (input.pressed[0] || input.pressed[1] || ui.KeyPressed(VK_ESCAPE)) Close();
}

// ── Steps ───────────────────────────────────────────────────────────────────

bool TutorialStepsWindow::Open(HWND owner, std::vector<Step> list) {
    steps = std::move(list);
    if (steps.empty()) return false;
    if (!Create(PopupOptions(owner, L"Tutorial", 640, 580))) return false;
    CenterOnOwner(*this, owner, 640.f, 580.f);
    Show(SW_SHOW);
    Activate();
    return true;
}

void TutorialStepsWindow::OnFrame() {
    if (ui.KeyPressed(VK_ESCAPE)) {
        Close();
        return;
    }
    const Step& s = steps[step];
    const size_t last = steps.size() - 1;
    const float panelW = g.Width() - 56.f;

    // Measure the StackPanel (Margin 28, VerticalAlignment=Center), then draw it
    ImagePtr image = s.image && Files::Exists(*s.image) ? Images::Load(*s.image) : nullptr;
    float imageW = 0.f, imageH = 0.f;
    if (image) {
        const float scale = std::min({ panelW / image->w, 320.f / image->h });
        imageW = image->w * scale;
        imageH = image->h * scale;
    }
    TextStyle captionStyle = Mono(13.f, true);
    captionStyle.align = HAlign::Center;
    captionStyle.wrap = true;
    captionStyle.valign = VAlign::Top;
    const float captionH = s.caption && !s.caption->empty() ? g.Measure(*s.caption, captionStyle, panelW).h : 0.f;
    const float iconH = s.controllerIcon ? g.Measure(L"\xE7FC", IconFont(72.f)).h : 0.f;
    const float copiedH = g.Measure(L"Ag", Mono(11.f)).h;
    const float total = (image ? imageH + 14.f : 0.f) + (s.controllerIcon ? 30.f + iconH + 20.f : 0.f) + (captionH > 0.f ? captionH + 12.f : 0.f) +
                        (s.copyButton ? 28.f : 0.f) + 8.f + copiedH + 16.f + 28.f;
    Column col(28.f, std::max(28.f, (g.Height() - total) * 0.5f), panelW);

    if (image) g.DrawImage(*image, Rect{ col.x, col.Next(imageH, 14.f).y, panelW, imageH }.Center(imageW, imageH));
    if (s.controllerIcon) {
        col.Gap(30.f);
        g.Text(L"\xE7FC", col.Next(iconH, 20.f), Palette::Teal, IconFont(72.f));
    }
    if (captionH > 0.f) g.Text(*s.caption, col.Next(captionH, 12.f), Palette::Label, captionStyle);
    if (s.copyButton) {
        const std::wstring text = Loc::Get("steam_tutorial_copy_btn");
        const float w = P::SmallButtonWidth(ui, text, 170.f);
        const Rect row = col.Next(28.f);
        if (P::SmallButton(ui, ui.MakeId("tutorial-copy"), { row.CX() - w * 0.5f, row.y, w, 28.f }, text, Palette::Teal)) {
            wchar_t exe[MAX_PATH]{};
            GetModuleFileNameW(nullptr, exe, MAX_PATH);
            copied = System::SetClipboardText(L"\"" + std::wstring(exe) + L"\" %command%");
        }
    }
    // "Copied!" keeps its space even while hidden (Visibility=Hidden)
    col.Gap(8.f);
    const Rect copiedRow = col.Next(copiedH);
    if (copied) g.Text(Loc::Get("steam_tutorial_copied"), copiedRow, Palette::Teal, Mono(11.f).Center());

    // Prev · n / N · Next / Done
    col.Gap(16.f);
    const Rect nav = col.Next(28.f);
    const std::wstring prev = Loc::Get("steam_tutorial_prev");
    const std::wstring next = Loc::Get(step == last ? "steam_tutorial_done" : "steam_tutorial_next");
    const float pw = P::SmallButtonWidth(ui, prev, 90.f), nw = P::SmallButtonWidth(ui, next, 90.f);
    if (P::SmallButton(ui, ui.MakeId("tutorial-prev"), { nav.x, nav.y, pw, 28.f }, prev, Palette::Label, step > 0)) {
        --step;
        copied = false;
    }
    g.Text(std::to_wstring(step + 1) + L" / " + std::to_wstring(steps.size()), nav, Palette::Label, Mono(10.f).Center());
    if (P::SmallButton(ui, ui.MakeId("tutorial-next"), { nav.R() - nw, nav.y, nw, 28.f }, next, Palette::Teal)) {
        if (step < last) {
            ++step;
            copied = false;
        } else {
            Close();
        }
    }
}

} // namespace SpeedrunLauncher::Windows
