#pragma once
#include <vector>
#include "Services/GameFiles/SaveGames.h"
#include "UI/Widgets/Panel.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// Chapters 1-3: pick a checkpoint; its bundled save is copied over the game's save file.
class CheckpointOverlay : public Overlay {
public:
    using Overlay::Overlay;
    bool Visible() const override { return popup.Visible(); }
    void Draw(UI::Ui& ui, const Rect& screen) override;
    void Open(int chapterNumber);

private:
    UI::Popup popup;
    std::vector<Services::SaveGames::Checkpoint> checkpoints;
    bool resetScroll = false;
};

} // namespace SpeedrunLauncher::Main
