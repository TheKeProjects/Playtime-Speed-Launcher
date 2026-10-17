#pragma once
#include <memory>
#include <optional>
#include <string>
#include "UI/Widgets/Panel.h"
#include "Windows/Main/Overlays/Overlay.h"
#include "Windows/Main/Overlays/Versions/PresetInstaller.h"
#include "Windows/Main/Overlays/Versions/StoreOverlay.h"

namespace SpeedrunLauncher::Main {

// "VERSIONS": which copy of the game PLAY launches, in the store's look. The header switches
// chapter; a banner shows the chapter and the copy in use next to the STORE button (historic
// builds, drawn on top of this panel); the versions are cover tiles — the automatic Steam / Epic
// install first, then the user's own installations (drag to reorder, edit / delete on hover) and
// a tile to add one manually.
class VersionsOverlay : public Overlay {
public:
    explicit VersionsOverlay(MainWindow& window);
    bool Visible() const override { return popup.Visible() || store->Visible(); }
    void Draw(UI::Ui& ui, const Rect& screen) override;

    void Open(int chapterIndex);
    void CancelDownloads() { installer->CancelAll(); }
    void EnsureSteamCmd() { installer->AcquireSteamCmd(); }

private:
    struct Entry {
        std::wstring name, subtitle, exePath;
        bool isAuto = false, selected = false, first = false, last = false;
        std::optional<std::wstring> icon;   // custom icon file
        std::wstring logo;                  // small logo on the cover (Steam / Epic / custom icon)
    };

    void DrawPanel(UI::Ui& ui, const Rect& screen);
    void DrawHeader(UI::Ui& ui, const Rect& header);
    void DrawBanner(UI::Ui& ui, const Rect& r, const std::wstring& inUse, size_t count);
    float DrawTiles(UI::Ui& ui, float x, float y, float w);   // returns the content height
    // `floating`: the copy that follows the mouse while it's being dragged (no buttons, no click)
    void DrawTile(UI::Ui& ui, const Rect& slot, const Entry& entry, int index, int reveal, bool floating = false);
    void DrawAddTile(UI::Ui& ui, const Rect& slot, int reveal);
    void AddManually();
    void EditInstall(const Entry& entry);
    void AskDelete(const Entry& entry);                    // opens the confirmation card
    void DrawDeleteConfirm(UI::Ui& ui, const Rect& panel);
    void Select(const std::optional<std::wstring>& exePath);
    void SwitchChapter(int delta);
    void SwitchTo(int index, int direction);   // direction: -1 the page comes from the left, 1 from the right
    void Reveal(float fromX);   // the page slides in again (after opening / switching chapter)

    UI::Popup popup;
    std::unique_ptr<PresetInstaller> installer;
    std::unique_ptr<StoreOverlay> store;
    int chapterIndex = 0;
    double revealAt = -10.0;
    float revealFromX = 40.f;
    double switchedAt = -10.0;      // chapter changed: the page slides over and the banner art settles
    float switchDir = 1.f;
    bool storeWasOpen = false;      // to replay the entrance when coming back from the store
    // Dragging a custom installation to reorder it (starts after moving a few pixels)
    struct Drag {
        std::wstring exe;
        int from = -1;              // index among the custom installations, -1 = none
        float grabX = 0.f, grabY = 0.f, startX = 0.f, startY = 0.f;
        bool active = false;
        double grabbedAt = 0.0;     // became active (the trembling loop starts after the grab sound)
        bool trembling = false;
    } drag;
    bool dragEnded = false;         // a drag was dropped this frame (its release isn't a click)
    // Removing an installation: "only from the list" or "its folder and files too" (to the Recycle Bin)
    struct PendingDelete {
        int chapterNumber = 0;
        std::wstring name, exePath;
        std::wstring folder;        // what would be deleted
        std::wstring blockedReason; // non-empty: the files can't be deleted safely (only the list entry)
        double shownAt = 0.0;
    };
    std::optional<PendingDelete> pendingDelete;
};

} // namespace SpeedrunLauncher::Main
