#pragma once
#include <string>
#include "Graphics/Images.h"
#include "UI/Ui.h"

namespace SpeedrunLauncher::Main {

// The logged-in Steam user's name and round avatar, in the top bar left of the version.
class SteamUserBadge {
public:
    void Load();
    // right: where the bar's own items start. Returns where the badge starts (right when hidden).
    float Draw(UI::Ui& ui, float right, const Rect& bar);

private:
    bool visible = false;
    std::wstring persona;
    std::wstring avatarPath;
};

} // namespace SpeedrunLauncher::Main
