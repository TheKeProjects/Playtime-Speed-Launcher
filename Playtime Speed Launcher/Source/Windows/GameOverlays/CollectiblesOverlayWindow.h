#pragma once
#include <windows.h>
#include <functional>
#include <string>
#include "Controls/TransparentBrowser.h"

namespace SpeedrunLauncher::Windows {

// 100% in game: the collectibles overlay page (the same one OBS shows) in an always-on-top,
// click-through, transparent window along the left or right edge of the screen, `offset` DIPs below
// the top of the work area. Re-asserts topmost every 1.5 s (borderless games fight for it).
// In move mode it takes the mouse instead and can be dragged up and down.
class CollectiblesOverlayWindow {
public:
    static constexpr const wchar_t* ClassName = L"SpeedrunLauncher.CollectiblesOverlay";

    struct Placement {
        bool right = false;
        double scale = 1.0;
        int offset = 0;   // DIPs from the top of the work area
        bool operator==(const Placement&) const = default;
    };

    CollectiblesOverlayWindow() = default;
    ~CollectiblesOverlayWindow();
    CollectiblesOverlayWindow(const CollectiblesOverlayWindow&) = delete;
    CollectiblesOverlayWindow& operator=(const CollectiblesOverlayWindow&) = delete;

    // After a drag in move mode: the new offset (DIPs)
    std::function<void(int offset)> onMoved;

    // url: the overlay's link (http://127.0.0.1:<port>/overlay?view=panel...). Starts hidden: SetShown.
    bool Open(const std::wstring& url, const Placement& placement);
    // Hides it without unloading the page (the game lost focus / was minimized), so it's back at once
    void SetShown(bool shown);
    void Update(const std::wstring& url, const Placement& placement);
    void SetMoveMode(bool on);
    void Destroy();
    bool IsOpen() const { return hwnd != nullptr; }

private:
    static LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT Handle(UINT msg, WPARAM wp, LPARAM lp);
    void Place();
    RECT WorkArea() const;

    HWND hwnd = nullptr;
    std::wstring url;
    Placement placement;
    bool moving = false;
    bool shown = false;
    Controls::TransparentBrowser browser;
};

} // namespace SpeedrunLauncher::Windows
