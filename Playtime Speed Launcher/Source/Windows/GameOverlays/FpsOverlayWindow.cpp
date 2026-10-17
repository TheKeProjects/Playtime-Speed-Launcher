#include "Windows/GameOverlays/FpsOverlayWindow.h"
#include <cmath>
#include "UI/Style.h"

namespace SpeedrunLauncher::Windows {

namespace {

TextStyle FpsStyle(float size, bool poppy) {
    TextStyle s = UI::Mono(size, true);
    s.font = poppy ? Font::Poppy : Font::Mono;
    s.align = HAlign::Center;
    return s;
}

} // namespace

bool FpsOverlayWindow::Open() {
    UI::Window::Options o;
    o.title = L"FPS";
    o.style = WS_POPUP;
    o.exStyle = WS_EX_TOOLWINDOW;
    o.transparent = true;
    o.clickThrough = true;
    o.topmost = true;
    o.noActivate = true;
    o.width = 120;
    o.height = 40;
    o.showCommand = SW_HIDE;
    if (!Create(o)) return false;
    Place();
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    return true;
}

void FpsOverlayWindow::SetFps(double fps) {
    text = L"FPS: " + std::to_wstring(static_cast<long long>(std::llround(fps)));
    Invalidate();
}

void FpsOverlayWindow::SetAppearance(const std::wstring& c, float size, bool poppyFont) {
    corner = c;
    fontSize = size;
    poppy = poppyFont;
    placedW = placedH = 0.f;
    Place();
    Invalidate();
}

void FpsOverlayWindow::Place() {
    if (!hwnd) return;
    // Size to content: text + padding scaled with the font size (Padding = 1.8p, p, 1.8p, p with p = size * 0.55)
    const float pad = fontSize * 0.55f;
    const auto size = g.Ready() ? g.Measure(L"FPS: 000", FpsStyle(fontSize, poppy)) : TextSize{ fontSize * 5.f, fontSize * 1.3f };
    const float w = size.w + pad * 3.6f, h = size.h + pad * 2.f;
    if (std::abs(w - placedW) < 0.5f && std::abs(h - placedH) < 0.5f) return;
    placedW = w;
    placedH = h;
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const float scale = DpiScale();
    const int pw = static_cast<int>(w * scale), ph = static_cast<int>(h * scale), margin = static_cast<int>(20 * scale);
    const bool right = corner == L"top-right" || corner == L"bottom-right";
    const bool bottom = corner == L"bottom-left" || corner == L"bottom-right";
    const int x = right ? work.right - pw - margin : work.left + margin;
    const int y = bottom ? work.bottom - ph - margin : work.top + margin;
    SetWindowPos(hwnd, HWND_TOPMOST, x, y, pw, ph, SWP_NOACTIVATE);
}

void FpsOverlayWindow::OnFrame() {
    Place();
    if (ui.Time() - lastTopmost > 1.5) {
        lastTopmost = ui.Time();
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    const Rect box{ 0.f, 0.f, g.Width(), g.Height() };
    g.FillRound(box, 6.f, Argb(0xAA000000));
    g.Text(text, box, Colors::White, FpsStyle(fontSize, poppy));
}

} // namespace SpeedrunLauncher::Windows
