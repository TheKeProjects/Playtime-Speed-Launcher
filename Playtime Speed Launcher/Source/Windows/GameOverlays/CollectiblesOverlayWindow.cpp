#include "Windows/GameOverlays/CollectiblesOverlayWindow.h"
#include <windowsx.h>
#include <algorithm>

namespace SpeedrunLauncher::Windows {

namespace {

constexpr UINT_PTR kTopmostTimer = 1;
constexpr int kWidth = 540;       // DIPs at scale 1 (the OBS source is 500 wide)
constexpr int kMargin = 8;        // DIPs from the screen edge (the page adds its own 12 px)
constexpr int kMinHeight = 160;   // DIPs: how far down it can go

} // namespace

CollectiblesOverlayWindow::~CollectiblesOverlayWindow() { Destroy(); }

bool CollectiblesOverlayWindow::Open(const std::wstring& link, const Placement& p) {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{ sizeof(wc) };
        wc.lpfnWndProc = Proc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursorW(nullptr, IDC_SIZENS);   // only shown in move mode
        wc.lpszClassName = ClassName;
        registered = RegisterClassExW(&wc) != 0;
    }
    url = link;
    placement = p;
    // No redirection bitmap: only the page's own pixels show. Layered + transparent: clicks go to the game.
    // No WS_EX_TOOLWINDOW, so OBS Window Capture can still list it.
    hwnd = CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE, ClassName,
                           L"100% Overlay", WS_POPUP, 0, 0, 100, 100, nullptr, nullptr, instance, this);
    if (!hwnd) return false;
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    browser.Create(hwnd);
    Place();
    browser.Navigate(url);
    SetTimer(hwnd, kTopmostTimer, 1500, nullptr);
    return true;
}

void CollectiblesOverlayWindow::SetShown(bool on) {
    if (!hwnd || on == shown) return;
    shown = on;
    if (on) {
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    } else {
        ShowWindow(hwnd, SW_HIDE);
    }
}

void CollectiblesOverlayWindow::Update(const std::wstring& link, const Placement& p) {
    if (!hwnd) return;
    if (!(p == placement)) {
        placement = p;
        Place();
    }
    if (link != url) {
        url = link;
        browser.Navigate(url);
    }
}

void CollectiblesOverlayWindow::SetMoveMode(bool on) {
    if (!hwnd || on == moving) return;
    moving = on;
    // Click-through only while not moving
    LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    ex = on ? (ex & ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT)) : (ex | WS_EX_TRANSPARENT);
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void CollectiblesOverlayWindow::Destroy() {
    if (!hwnd) return;
    browser.Destroy();
    HWND h = hwnd;
    hwnd = nullptr;
    DestroyWindow(h);
}

RECT CollectiblesOverlayWindow::WorkArea() const {
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    return work;
}

void CollectiblesOverlayWindow::Place() {
    if (!hwnd) return;
    const RECT work = WorkArea();
    const int dpi = static_cast<int>(GetDpiForWindow(hwnd));
    const int margin = MulDiv(kMargin, dpi, 96);
    const int workW = work.right - work.left;
    const int w = std::min(workW - 2 * margin, MulDiv(static_cast<int>(kWidth * std::max(1.0, placement.scale)), dpi, 96));
    const int maxTop = work.bottom - margin - MulDiv(kMinHeight, dpi, 96);
    const int y = std::clamp(static_cast<int>(work.top) + margin + MulDiv(placement.offset, dpi, 96), static_cast<int>(work.top),
                             std::max<int>(work.top, maxTop));
    const int h = work.bottom - margin - y;
    const int x = placement.right ? work.right - margin - w : work.left + margin;
    SetWindowPos(hwnd, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE);
    browser.SetBounds({ 0, 0, w, h });
}

LRESULT CALLBACK CollectiblesOverlayWindow::Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    auto* self = reinterpret_cast<CollectiblesOverlayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_DESTROY) {
        KillTimer(hwnd, kTopmostTimer);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
    }
    return self ? self->Handle(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CollectiblesOverlayWindow::Handle(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_NCHITTEST:
        // Move mode: grabbing it anywhere drags the window (Windows moves it, see WM_MOVING)
        return moving ? HTCAPTION : HTTRANSPARENT;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_MOVING: {
        // Up and down only, inside the work area, the window keeping its side
        auto* r = reinterpret_cast<RECT*>(lp);
        RECT current{};
        GetWindowRect(hwnd, &current);
        const RECT work = WorkArea();
        const int dpi = static_cast<int>(GetDpiForWindow(hwnd));
        const int maxTop = work.bottom - MulDiv(kMargin + kMinHeight, dpi, 96);
        const int top = std::clamp(static_cast<int>(r->top), static_cast<int>(work.top), std::max<int>(work.top, maxTop));
        *r = { current.left, top, current.right, top + (current.bottom - current.top) };
        return TRUE;
    }
    case WM_EXITSIZEMOVE: {
        // Keep the drop height (DIPs below the top margin) and refit the height to the screen
        RECT r{};
        GetWindowRect(hwnd, &r);
        const RECT work = WorkArea();
        const int dpi = static_cast<int>(GetDpiForWindow(hwnd));
        placement.offset = std::max(0, MulDiv(r.top - work.top, 96, dpi) - kMargin);
        Place();
        if (onMoved) onMoved(placement.offset);
        return 0;
    }
    case WM_TIMER:
        if (wp == kTopmostTimer && shown) SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        return 0;
    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
        Place();
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace SpeedrunLauncher::Windows
