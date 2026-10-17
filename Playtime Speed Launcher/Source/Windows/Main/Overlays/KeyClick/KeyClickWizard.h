#pragma once
#include "UI/Widgets/Panel.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// Settings → Controls → Key to click → CONFIGURE: a two-step wizard over the settings panel.
//   1. a keyboard: "choose the key to replace" (click a key or press it)
//   2. a mouse: "action to perform" (left / middle / right button), with the spam-while-held
//      switch and its clicks-per-second below it
// Saves into Settings::KeyClick and re-arms the hook.
class KeyClickWizard : public Overlay {
public:
    explicit KeyClickWizard(MainWindow& window) : Overlay(window) {}
    bool Visible() const override { return popup.Visible(); }
    void Draw(UI::Ui& ui, const Rect& screen) override;

    void Open(int bindingIndex = -1);   // -1: a new key; otherwise edits that binding (starts on the mouse)

private:
    void DrawKeyboard(UI::Ui& ui, const Rect& area);
    void DrawMouse(UI::Ui& ui, const Rect& area);
    void ChooseKey(unsigned vk);
    void SetStep(int s);
    void Save();
    bool KeyTaken(unsigned vk) const;   // used by another binding

    UI::Popup popup;
    int editing = -1;
    int step = 0;                 // 0 keyboard, 1 mouse
    double stepAt = 0.0;
    float stepFrom = 40.f;
    unsigned vk = 0;
    int button = 0;               // 0 left, 1 right, 2 middle
    bool spam = false;
    int clicksPerSecond = 15;
};

} // namespace SpeedrunLauncher::Main
