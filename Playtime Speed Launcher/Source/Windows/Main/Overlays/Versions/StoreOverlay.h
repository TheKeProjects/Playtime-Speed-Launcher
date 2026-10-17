#pragma once
#include <optional>
#include <string>
#include <vector>
#include "Models/Chapter.h"
#include "Services/Platforms/SteamCredentialStore.h"
#include "UI/Ui.h"
#include "UI/Widgets/Panel.h"

namespace SpeedrunLauncher::Main {

class MainWindow;
class PresetInstaller;

// "STORE" (opened from the versions menu): the historic builds, browsed like a game store.
// The front page features one chapter in a banner and lists every chapter as a cover tile;
// a chapter's page shows its versions as tiles (cover, name underneath); a version's product page
// shows its details, the installation method (AUTO / MANUAL) and the download progress.
// Covers come from Assets\Store\Chapter<N>.jpg.
class StoreOverlay {
public:
    StoreOverlay(MainWindow& window, PresetInstaller& presetInstaller) : main(window), installer(presetInstaller) {}
    bool Visible() const { return popup.Visible(); }
    bool IsOpen() const { return popup.IsOpen(); }
    // Open and done popping in: whatever is under it is hidden (no need to draw it)
    bool Covering() const { return popup.IsOpen() && !popup.Animating(); }
    void Open(int chapterIndex);   // front page, with that chapter featured
    void Close() { popup.Close(); }
    // A download finished: opens (or moves) the store to that version's page for naming / saving it
    void ShowReady(int chapterNumber, const std::wstring& manifestId);
    void Draw(UI::Ui& ui, const Rect& screen);

private:
    void DrawHeader(UI::Ui& ui, const Rect& header);
    float DrawFront(UI::Ui& ui, float x, float y, float w);     // returns the content height
    float DrawChapter(UI::Ui& ui, float x, float y, float w);
    float DrawProduct(UI::Ui& ui, float x, float y, float w);
    // Product page steps (right column); each returns the y below it
    float DrawMethodStep(UI::Ui& ui, const Models::ChapterPreset& preset, float x, float y, float w);
    float DrawAccountStep(UI::Ui& ui, const Models::ChapterPreset& preset, int chapterNumber, float x, float y, float w);
    float DrawManualStep(UI::Ui& ui, const Models::ChapterPreset& preset, float x, float y, float w);
    float DrawWaitingStep(UI::Ui& ui, const Models::ChapterPreset& preset, int chapterNumber, float x, float y, float w);
    float DrawPhoneCard(UI::Ui& ui, const Models::ChapterPreset& preset, float x, float y, float w);
    float DrawPhoneWaitCard(UI::Ui& ui, const Models::ChapterPreset& preset, float x, float y, float w);
    // "① METHOD ─ ② ACCOUNT ─ ③ CONFIRM ─ ④ DOWNLOAD ─ ⑤ SAVE": where the install is; returns the y below
    // `failed`: the current step went wrong (drawn red with a "!")
    float DrawStepper(UI::Ui& ui, float x, float y, float w, const std::vector<std::wstring>& labels, int current, bool failed = false);
    float DrawNotFoundStep(UI::Ui& ui, const Models::ChapterPreset& preset, float x, float y, float w);
    float DrawReadyStep(UI::Ui& ui, const Models::ChapterPreset& preset, int chapterNumber, float x, float y, float w);
    float DrawMovingStep(UI::Ui& ui, const std::wstring& status, float x, float y, float w);
    float StepHeader(UI::Ui& ui, float x, float y, float w, const std::wstring& title);   // "‹ METHOD   ·   TITLE"
    void SetStep(int step);
    void DrawBanner(UI::Ui& ui, const Rect& r, int chapterIndex, bool front);
    void ShowChapter(int index);
    void ShowFront();
    void ShowProduct(int presetIndex);
    void GoBack();   // product → chapter → front page → closed

    MainWindow& main;
    PresetInstaller& installer;
    UI::Popup popup;
    int featured = 0;
    int chapter = -1;            // index of the open chapter page, -1 = front page
    int product = -1;            // preset index of the open product page (inside `chapter`), -1 = none
    int method = 0;              // product page: 0 = AUTO, 1 = MANUAL
    // Product page install steps: 0 method, 1 Steam account (AUTO), 2 steps (MANUAL), 3 waiting for Steam
    struct InstallForm {
        int step = 0;
        double shownAt = 0.0;
        std::vector<Services::SteamCredentials::Account> accounts;
        int account = -1;        // saved account index, -1 = typed in
        std::wstring user, pass;
        bool savePassword = false;
        bool confirmed = false;  // MANUAL: "I understand"
        std::wstring depotFolder;
        // Download ready: name / icon of the new installation
        std::wstring readyFor, readyName;
        std::optional<std::wstring> readyIcon;
    } form;
    double pageShownAt = 0.0;    // page changed (its tiles slide in again)
    double retriedAt = -10.0;    // "look again" pressed (the card shakes if it still fails)
    float pageFromX = 40.f;
};

} // namespace SpeedrunLauncher::Main
