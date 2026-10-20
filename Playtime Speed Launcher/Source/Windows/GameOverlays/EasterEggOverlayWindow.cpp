#include "Windows/GameOverlays/EasterEggOverlayWindow.h"
#include <algorithm>
#include "Core/Async.h"

namespace SpeedrunLauncher::Windows {

bool EasterEggOverlayWindow::Open() {
    UI::Window::Options o;
    o.title = L"Easter egg";
    o.style = WS_POPUP;
    o.exStyle = WS_EX_TOOLWINDOW;
    o.transparent = true;
    o.clickThrough = true;
    o.topmost = true;
    o.noActivate = true;
    o.x = 0;
    o.y = 0;
    o.width = 100;
    o.height = 100;
    o.showCommand = SW_HIDE;
    if (!Create(o)) return false;
    // Cover the primary screen
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_NOACTIVATE);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    SetContinuous(true);
    if (photo) return (image = Images::Load(path)) != nullptr;
    return player.Open(path, true, false);
}

void EasterEggOverlayWindow::OnFrame() {
    // Borderless-fullscreen games keep re-asserting HWND_TOPMOST: re-assert ours every second
    if (ui.Time() - lastTopmost > 1.0) {
        lastTopmost = ui.Time();
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    const float w = g.Width(), h = g.Height();
    g.FillRect({ 0, 0, w, h }, Argb(0xCC000000));
    const Rect box = Rect{ 0, 0, w, h }.Center(std::min(960.f, w * 0.8f), std::min(570.f, h * 0.8f));
    g.Border(box, 4.f, Rgba(0x09, 0x0C, 0x1E), Rgba(0x2A, 0x10, 0x50), 1.f);
    bool done = false;
    if (image) {
        // Click-through over the game: a photo closes on its own (or with Escape)
        if (shownAt < 0.0) shownAt = ui.Time();
        g.DrawImage(*image, box.Inset(1.f));
        done = ui.Time() - shownAt > kPhotoSeconds;
    } else {
        player.Update();
        player.Draw(g, box.Inset(1.f));
        done = player.Ended() || player.Failed();
    }
    if (!finished && done) {
        finished = true;
        if (onFinished) {
            auto cb = onFinished;
            Async::Post([cb] { cb(); });
        }
    }
}

} // namespace SpeedrunLauncher::Windows
