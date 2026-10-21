#pragma once
#include <string>
#include "UI/Style.h"
#include "UI/Ui.h"

namespace SpeedrunLauncher::UI {

// Visibility of an in-window overlay with a "pop" animation: fade in + scale 0.92→1 with a tiny
// overshoot on open, fade out + shrink to 0.96 on close.
class Popup {
public:
    void Open(bool animated = true);
    void Close(bool animated = true);
    void Toggle() { if (open) Close(); else Open(); }
    bool IsOpen() const { return open; }
    bool Visible() const;          // open, or still fading out
    float Opacity() const;
    float Scale() const;
    bool Animating() const;

private:
    bool open = false;
    bool animated = false;
    double changedAt = -1.0;
};

struct PanelStyle {
    Color background = Palette::PanelBg;
    Color border = Palette::PanelBorder;
    Color headerBackground = Palette::HeaderBg;
    Color headerBorder = Palette::Divider;
    Color footerBackground = Palette::HeaderBg;
    Color footerBorder = Palette::Divider;
    Color backdrop = Palette::Backdrop;
    Color titleColor = Palette::Teal;
    float titleSize = 13.f;
    float radius = 4.f;
    float headerHeight = 48.f;
    float footerHeight = 52.f;
    std::wstring title;            // centred in the header when not empty
    bool decorated = true;         // glow, accent lines, corner marks and the opening light sweep
    bool seasonal = true;          // the season's particles in the body and decorations on the top edge
};

// The standard modal overlay: dimmed backdrop over the whole window, a centred panel with a
// header row, a body and a footer row. Blocks input to everything drawn before it.
// With height = 0 the panel sizes itself to the body content reported by SetContentHeight
// (measured on the previous frame).
// Opening (first frame after being hidden) plays an entrance: the panel rises in, its glow flares,
// the accent line under the header sweeps open, a light streak crosses it, the corner marks snap
// in and finally the body content fades up.
class OverlayFrame {
public:
    OverlayFrame(Ui& ui, Id id, const Rect& screen, float width, float height, const PanelStyle& style, float opacity = 1.f,
                 float scale = 1.f);
    ~OverlayFrame();
    OverlayFrame(const OverlayFrame&) = delete;
    OverlayFrame& operator=(const OverlayFrame&) = delete;

    void SetContentHeight(float h) { contentHeight = h; }
    // Pressed on the backdrop outside the panel this frame.
    bool BackdropClicked() const { return backdropClicked; }
    // When the panel opened (Ui::Time clock): start of the content's staggered entrance
    double OpenedAt() const { return openedAt; }

    Rect panel, header, body, footer;

private:
    Ui& ui;
    Id id;
    float contentHeight = -1.f;
    double openedAt = 0.0;
    bool layer = false, scaled = false, backdropClicked = false;
    bool bodyLayer = false;   // body content still fading / rising in
};

// The menu-panel look shared by OverlayFrame and the standalone windows; `t` = seconds since the
// panel opened (drives the entrance).
//   PanelBackdropGlow  accent spotlight behind the panel (draw before the panel)
//   PanelGlow          drop shadow + accent glow that flares while opening (draw before the panel)
//   PanelDecor         accent lines sweeping open, light streak and corner marks (draw after it);
//                      headerBottom / footerTop < 0 skip that line
void PanelBackdropGlow(Ui& ui, const Rect& panel, Color accent, float t);
void PanelGlow(Ui& ui, const Rect& panel, float radius, Color accent, float t);
void PanelDecor(Ui& ui, const Rect& panel, float radius, Color accent, float t, float headerBottom = -1.f, float footerTop = -1.f);
bool PanelEntranceRunning(float t);

// Rise + fade of a whole panel during the opening (scoped; drawing and hit testing)
class PanelRise {
public:
    PanelRise(Ui& ui, float t, float distance = 26.f);
    ~PanelRise();
    PanelRise(const PanelRise&) = delete;
    PanelRise& operator=(const PanelRise&) = delete;

private:
    Ui& ui;
    bool active = false;
};

} // namespace SpeedrunLauncher::UI
