#pragma once
#include <vector>
#include "Services/GameFiles/SaveGames.h"
#include "UI/Widgets/Panel.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// Chapters 1-3: pick a checkpoint (click, or arrows + Enter); its bundled save is copied over the
// game's save file.
class CheckpointOverlay : public Overlay {
public:
    using Overlay::Overlay;
    bool Visible() const override { return popup.Visible(); }
    void Draw(UI::Ui& ui, const Rect& screen) override;
    void Open(int chapterNumber);

private:
    UI::Popup popup;
    void Load(size_t index);

    std::vector<Services::SaveGames::Checkpoint> checkpoints;
    int chapter = 0;
    int focused = -1;            // keyboard selection (arrows), -1 = none yet
    bool resetScroll = false;
};

} // namespace SpeedrunLauncher::Main
