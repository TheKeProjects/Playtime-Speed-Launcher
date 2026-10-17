#include "Windows/Toasts/ToastWindow.h"
#include <algorithm>
#include "Core/Async.h"
#include "Graphics/Animation.h"
#include "UI/Style.h"

namespace SpeedrunLauncher::Toasts {

ToastWindow::ToastWindow(Spec s) : spec(std::move(s)) {}

bool ToastWindow::Open() {
    UI::Window::Options o;
    o.title = L"Toast";
    o.style = WS_POPUP;
    o.exStyle = WS_EX_TOOLWINDOW;
    o.transparent = true;
    o.topmost = true;
    o.noActivate = true;
    o.width = static_cast<int>(spec.width);
    o.height = 80;
    o.showCommand = SW_HIDE;
    if (!Create(o)) return false;
    opened = Async::Now();
    Place(80.f);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    SetContinuous(true);
    return true;
}

void ToastWindow::Place(float heightDips) {
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const float scale = DpiScale();
    const int w = static_cast<int>(spec.width * scale), h = static_cast<int>(heightDips * scale);
    int x = work.left + static_cast<int>(20 * scale), y = work.top + static_cast<int>(20 * scale);
    if (spec.anchor == Spec::Anchor::RightMiddle) {
        x = work.right - w - static_cast<int>(20 * scale);
        y = work.top + (work.bottom - work.top - h) / 2 + static_cast<int>(spec.offsetY * scale);
    }
    SetWindowPos(hwnd, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE);
}

void ToastWindow::OnFrame() {
    const double t = Async::Now() - opened;
    if (t >= spec.duration) {
        SetContinuous(false);
        Close();
        return;
    }
    float opacity = Anim::Clamp01(static_cast<float>(t / spec.fadeIn));
    const double fadeStart = spec.duration - spec.fadeOut;
    if (t > fadeStart) opacity = 1.f - Anim::Clamp01(static_cast<float>((t - fadeStart) / spec.fadeOut));

    // Height = padding + lines + 3 px progress bar
    float h = spec.padding.V();
    for (const auto& line : spec.lines) h += line.marginTop + g.Measure(line.text, UI::Mono(line.size, line.bold)).h;
    h += 3.f + 2.f;
    if (std::abs(h - measuredHeight) > 0.5f) {
        measuredHeight = h;
        Place(h);
    }

    const Rect box{ 0.f, 0.f, g.Width(), g.Height() };
    g.PushOpacity(opacity);
    g.PushRoundClip(box, 6.f);
    g.FillRect(box, Rgba(9, 20, 30, 240));
    float y = spec.padding.t;
    for (const auto& line : spec.lines) {
        y += line.marginTop;
        const float lh = g.Measure(line.text, UI::Mono(line.size, line.bold)).h;
        g.Text(line.text, { spec.padding.l, y, box.w - spec.padding.H(), lh }, line.color, UI::Mono(line.size, line.bold));
        y += lh;
    }
    // Progress bar shrinking over the toast's lifetime
    const Rect bar{ 1.f, box.B() - 4.f, box.w - 2.f, 3.f };
    g.FillRect(bar, spec.accent.WithAlpha(40.f / 255.f));
    const float remaining = 1.f - Anim::Clamp01(static_cast<float>(t / spec.duration));
    g.FillRect({ bar.x, bar.y, bar.w * remaining, bar.h }, spec.accent);
    g.PopLayer();
    g.StrokeRound(box, 6.f, Rgba(21, 48, 72), 1.f);
    g.PopLayer();
}

void ToastSlot::Show(const Spec& spec) {
    Close();
    window = std::make_unique<ToastWindow>(spec);
    ToastWindow* raw = window.get();
    raw->onClosed = [this, raw] {
        if (window.get() == raw) window.reset();
    };
    if (!window->Open()) window.reset();
}

void ToastSlot::Close() {
    if (window) {
        window->onClosed = nullptr;
        window->Destroy();
        window.reset();
    }
}

} // namespace SpeedrunLauncher::Toasts
