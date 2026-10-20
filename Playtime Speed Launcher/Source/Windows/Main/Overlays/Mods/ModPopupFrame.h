#pragma once
#include <windows.h>
#include <functional>
#include <string>
#include <vector>
#include "UI/Ui.h"
#include "UI/Widgets/Panel.h"

namespace SpeedrunLauncher::Main {

// A footer button of the mod popups (DELETE / YES / NO / INSTALL UE4SS...).
struct ModButton {
    std::string id;
    std::wstring text;
    bool danger = false;       // red (#1A0606 / #440000) instead of green (#00503C / #00B482)
    float minWidth = 110.f;
    float padX = 16.f;
    float fontSize = 12.f;
    float gapAfter = 10.f;     // margin to the next button
    bool enabled = true;
    std::function<void()> onClick;
};

// "[ TITLE ]" + centred question (+ amber warning) + left / right footer button groups, with the
// pop animation (fade + scale 0.85 → 1).
void DrawModPopup(UI::Ui& ui, UI::Popup& popup, const Rect& screen, float width, const std::wstring& title, const std::wstring& question,
                  const std::wstring& warning, const std::vector<ModButton>& left, const std::vector<ModButton>& right);

// Result / error message dialog ("UE4SS", "LOAD MANIP", "FULLBRIGHT").
void ShowModMessage(HWND owner, const std::wstring& title, const std::wstring& message, bool success);
// "This requires deleting the current UE4SS files. Do you want to delete them?"
void ConfirmDeleteUe4ss(HWND owner, const std::wstring& title, std::function<void()> onYes);
// Chapter 4's old LoadManipV2_P files: mandatory delete before installing (the install deletes them)
void ConfirmDeleteLegacyLoadManip(HWND owner, const std::wstring& title, std::function<void()> onYes);

} // namespace SpeedrunLauncher::Main
