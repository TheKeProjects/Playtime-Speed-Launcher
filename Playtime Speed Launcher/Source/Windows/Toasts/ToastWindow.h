#pragma once
#include <memory>
#include <string>
#include <vector>
#include "Graphics/Color.h"
#include "UI/Window.h"

namespace SpeedrunLauncher::Toasts {

// A small always-on-top notification with a shrinking progress bar that fades out on its own
// (game started / tutorial hotkey / cores mode / UE4SS hotkey remap toasts).
struct Spec {
    struct Line {
        std::wstring text;
        float size = 10.f;
        bool bold = false;
        Color color;
        float marginTop = 0.f;
    };
    std::vector<Line> lines;
    float width = 330.f;
    Edges padding{ 14.f, 12.f, 14.f, 10.f };
    Color accent = Hex(0x00CCAA);    // progress bar
    double duration = 10.0;          // seconds until it closes
    double fadeOut = 0.8;            // fade duration at the end
    double fadeIn = 0.25;
    enum class Anchor { TopLeft, RightMiddle } anchor = Anchor::TopLeft;
    float offsetY = 0.f;             // extra vertical offset (RightMiddle stacking)
};

class ToastWindow : public UI::Window {
public:
    explicit ToastWindow(Spec spec);
    bool Open();

protected:
    void OnFrame() override;
    Color ClearColor() const override { return Colors::Transparent; }

private:
    void Place(float heightDips);
    Spec spec;
    double opened = 0.0;
    float measuredHeight = 0.f;
};

// Owns a toast slot: Show() replaces the previous toast in this slot.
class ToastSlot {
public:
    void Show(const Spec& spec);
    void Close();
    bool Visible() const { return window && window->IsOpen(); }

private:
    std::unique_ptr<ToastWindow> window;
};

} // namespace SpeedrunLauncher::Toasts
