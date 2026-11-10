#pragma once
#include <string>
#include "Graphics/Rect.h"
#include "Services/Account/BadgesService.h"
#include "UI/Ui.h"

// How each badge looks: its name, what it's for, colour and icon, and the medal drawn for it.
namespace SpeedrunLauncher::Main::BadgeDraw {

struct Look {
    std::wstring title;          // "Campeón de noviembre 2026", "Pionero"...
    std::wstring description;    // what it's for
    Color color;
    wchar_t glyph = 0;
};
Look LookOf(const Services::Account::Badge& badge);

// A round medal in the badge's colours (a light sliding over it), centred on (cx, cy)
void Medal(UI::Ui& ui, float cx, float cy, float radius, const Services::Account::Badge& badge, int seed = 0);
// A small pill with the badge's icon and name; returns its width (draws nothing when `w` = 0... it
// always draws: `maxW` limits it, ellipsis inside)
float Pill(UI::Ui& ui, float x, float cy, const Services::Account::Badge& badge, float maxW = 1e9f, float height = 20.f);
float PillWidth(UI::Ui& ui, const Services::Account::Badge& badge, float height = 20.f);

} // namespace SpeedrunLauncher::Main::BadgeDraw
