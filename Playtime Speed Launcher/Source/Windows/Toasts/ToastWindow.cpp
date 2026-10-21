#include "Windows/Toasts/ToastWindow.h"
#include <algorithm>
#include "Core/Async.h"
#include "Graphics/Animation.h"
#include "Seasonal/SeasonalTheme.h"
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
    g.SetColorFilter(&Seasonal::Filter);   // follows the seasonal theme like the launcher
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
    // Slides in from the screen edge it's anchored to, lines following one after another
    const float slideIn = Anim::EaseOutBack(Anim::Clamp01(static_cast<float>(t / 0.4)), 0.8f);
    const float fromX = (spec.anchor == Spec::Anchor::RightMiddle ? 1.f : -1.f) * 24.f;
    g.PushOpacity(opacity);
    g.PushTransform(D2D1::Matrix3x2F::Translation(fromX * (1.f - slideIn), 0.f));
    g.PushRoundClip(box, 8.f);
    g.FillRect(box, Rgba(9, 20, 30, 242));
    g.FillLinear(box, 0.f, { { 0.f, spec.accent.WithAlpha(0.14f) }, { 0.55f, spec.accent.WithAlpha(0.f) } }, { 0.f, 0.f }, { 1.f, 0.f });
    g.FillRect({ 0.f, 0.f, 3.f, box.h }, spec.accent);   // accent stripe on the left edge
    float y = spec.padding.t;
    int index = 0;
    for (const auto& line : spec.lines) {
        y += line.marginTop;
        const float lh = g.Measure(line.text, UI::Mono(line.size, line.bold)).h;
        const float k = Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>((t - 0.08 - 0.06 * index++) / 0.3)));
        g.Text(line.text, { spec.padding.l + 3.f + 10.f * (1.f - k), y, box.w - spec.padding.H() - 3.f, lh }, line.color.WithAlpha(line.color.a * k),
               UI::Mono(line.size, line.bold));
        y += lh;
    }
    // Progress bar shrinking over the toast's lifetime, with a light at its tip
    const Rect bar{ 3.f, box.B() - 3.f, box.w - 3.f, 3.f };
    g.FillRect(bar, spec.accent.WithAlpha(40.f / 255.f));
    const float remaining = 1.f - Anim::Clamp01(static_cast<float>(t / spec.duration));
    g.FillLinear({ bar.x, bar.y, bar.w * remaining, bar.h }, 0.f, { { 0.f, spec.accent.WithAlpha(0.6f) }, { 1.f, spec.accent } }, { 0.f, 0.f }, { 1.f, 0.f });
    g.FillEllipse(bar.x + bar.w * remaining, bar.CY(), 3.f, 3.f, Colors::White.WithAlpha(0.8f));
    g.PopLayer();
    g.StrokeRound(box.Inset(0.5f), 7.5f, Lerp(Rgba(21, 48, 72), spec.accent, 0.35f), 1.f);
    g.PopTransform();
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
