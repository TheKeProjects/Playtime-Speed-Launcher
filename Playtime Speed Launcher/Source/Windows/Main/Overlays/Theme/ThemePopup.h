#pragma once
#include <string>
#include "Graphics/Graphics.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// The side rail's theme picker: a popup that grows out of the rail's THEME button (to its right,
// with an arrow pointing at it), one row per theme with its launcher icon; a click changes the theme
// with the sweep (ThemeTransition). A click anywhere outside or Escape closes it.
class ThemePopup : public Overlay {
public:
    using Overlay::Overlay;
    void Toggle(const Rect& anchor);
    void Close();
    bool IsOpen() const { return open; }
    bool Visible() const override;
    void Draw(UI::Ui& ui, const Rect& screen) override;

private:
    void DrawRows(UI::Ui& ui, const Rect& box, float reveal);

    bool open = false;
    double changedAt = -10.0;   // -1: opened, -2: closed (stamped on the next frame)
    Rect anchor;
};

// Changing the theme from the rail: a snapshot of the screen in the old look covers it and a slanted
// edge sweeps across it from right to left, uncovering the new look, a light in the new accent
// running along the edge.
class ThemeTransition {
public:
    // The next frame takes the snapshot (drawn in the old look) and only then switches the theme
    void Request(const std::wstring& theme) {
        if (!Animating()) pending = theme;
    }
    bool Pending() const { return !pending.empty(); }
    const std::wstring& PendingTheme() const { return pending; }
    bool Animating() const { return Pending() || active; }
    void Begin(ComPtr<ID2D1Bitmap1> snapshot, double now);
    // Draws what's left of the snapshot over the new look
    void Draw(Graphics& g, const Rect& screen, double now, Color light);

private:
    std::wstring pending;
    bool active = false;
    double startedAt = -10.0;
    ComPtr<ID2D1Bitmap1> snapshot;
};

} // namespace SpeedrunLauncher::Main
