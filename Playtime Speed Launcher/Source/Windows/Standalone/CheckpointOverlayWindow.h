#pragma once
#include <optional>
#include <string>
#include <vector>
#include "Services/GameFiles/SaveGames.h"
#include "UI/Window.h"

namespace SpeedrunLauncher::Windows {

// The global-hotkey checkpoint picker (old HotkeyOverlay): a transparent, topmost, full-screen
// window over the game. With a chapter running it lists only that chapter (Chapters 4-5 just get
// LOAD / DELETE); otherwise Chapters 1-3 grouped by chapter. Pops in / out, the rows slide in one
// after another, ↑ ↓ / Enter / double click work, and a LOAD or DELETE shows a short confirmation
// before the window closes.
class CheckpointOverlayWindow : public UI::Window {
public:
    bool Open();

protected:
    void OnFrame() override;
    void OnClosing(bool& cancel) override;
    Color ClearColor() const override { return Colors::Transparent; }

private:
    struct Row {
        int chapter = 0;
        std::wstring number;   // "3" from "[3] Mini Huggies" (empty when unnumbered)
        std::wstring label;    // "Mini Huggies"
        std::wstring section;  // chapter label shown above it (first row of each chapter), else empty
        float y = 0.f;         // top inside the scrolled list
        Services::SaveGames::Checkpoint checkpoint;
    };

    void BeginClose();
    void Load();
    void Delete();
    void Done(const std::wstring& text, bool danger);
    void DrawDone(const Rect& body, double now);

    int detected = 0;                  // running chapter (1-based), 0 = none
    std::vector<Row> rows;             // display order, grouped by chapter
    float listH = 0.f;                 // height of the whole list
    std::wstring subtitle;             // header subtitle (built once)
    int selected = -1;
    bool armed = false;                // Chapters 4-5: LOAD works without picking a row
    bool keyboardMoved = false;        // scroll the selection into view this frame

    int lastClickRow = -1;             // double click loads
    double lastClickAt = -10.0;

    double openedAt = -10.0;           // pop-in
    double closingAt = -1.0;           // pop-out running (the window closes when it ends)
    bool finished = false;             // pop-out over: let WM_CLOSE through
    double doneAt = -1.0;              // LOAD / DELETE confirmation shown since
    std::wstring doneText;
    bool doneDanger = false;
};

} // namespace SpeedrunLauncher::Windows
