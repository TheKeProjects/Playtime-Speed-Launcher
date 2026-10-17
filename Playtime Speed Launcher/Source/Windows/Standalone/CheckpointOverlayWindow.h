#pragma once
#include <optional>
#include <string>
#include <vector>
#include "Services/GameFiles/SaveGames.h"
#include "UI/Window.h"

namespace SpeedrunLauncher::Windows {

// The global-hotkey checkpoint picker (old HotkeyOverlay): a transparent, topmost, full-screen
// window over the game. With a chapter running it lists only that chapter (Chapters 4-5 just get
// LOAD / DELETE); otherwise Chapters 1-3 with section labels.
class CheckpointOverlayWindow : public UI::Window {
public:
    bool Open();

protected:
    void OnFrame() override;
    Color ClearColor() const override { return Colors::Transparent; }

private:
    struct Section {
        int chapter = 0;
        std::vector<Services::SaveGames::Checkpoint> rows;
    };

    int detected = 0;                  // running chapter (1-based), 0 = none
    std::vector<Section> sections;
    int selectedSection = -1, selectedRow = -1;
    bool armed = false;                // Chapters 4-5: LOAD works without picking a row
};

} // namespace SpeedrunLauncher::Windows
