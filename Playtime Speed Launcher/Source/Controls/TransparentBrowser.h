#pragma once
#include <windows.h>
#include <functional>
#include <memory>
#include <string>

namespace SpeedrunLauncher::Controls {

// A WebView2 drawn through DirectComposition with a transparent background, for overlays over the
// game: the window behind it must be WS_EX_NOREDIRECTIONBITMAP, so only what the page paints shows.
// Display only (no input is forwarded: the overlay windows are click-through).
// Shares the user data folder of Controls::WebBrowser (%LOCALAPPDATA%\SpeedrunLauncher\WebView2 Collectibles).
class TransparentBrowser {
public:
    TransparentBrowser();
    ~TransparentBrowser();
    TransparentBrowser(const TransparentBrowser&) = delete;
    TransparentBrowser& operator=(const TransparentBrowser&) = delete;

    std::function<void(const std::wstring& message)> onError;

    // Starts asynchronously; a Navigate before it's ready waits for it
    void Create(HWND parent);
    void Destroy();
    void SetBounds(const RECT& physical);   // client coordinates of the parent, in pixels
    void Navigate(const std::wstring& url);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    std::wstring pendingUrl;
    RECT bounds{};
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

} // namespace SpeedrunLauncher::Controls
