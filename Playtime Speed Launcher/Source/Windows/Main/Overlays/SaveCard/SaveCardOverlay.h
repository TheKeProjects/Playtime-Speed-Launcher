#pragma once
#include "UI/Widgets/Panel.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// The save button on a chapter card (Chapters 4+): delete the save slots, load the bundled start
// saves, and for Chapter 5 load / unload the NG+ saves.
class SaveCardOverlay : public Overlay {
public:
    using Overlay::Overlay;
    bool Visible() const override { return popup.Visible(); }
    void Draw(UI::Ui& ui, const Rect& screen) override;
    void Open(int chapterNumber);

private:
    UI::Popup popup;
    int chapter = 0;
};

} // namespace SpeedrunLauncher::Main
