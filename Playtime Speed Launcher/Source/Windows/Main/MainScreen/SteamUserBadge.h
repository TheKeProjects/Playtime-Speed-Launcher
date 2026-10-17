#pragma once
#include <string>
#include "Graphics/Images.h"
#include "UI/Ui.h"

namespace SpeedrunLauncher::Main {

// The logged-in Steam user's name and round avatar, bottom-right above the footer.
class SteamUserBadge {
public:
    void Load();
    void Draw(UI::Ui& ui, float right, float bottom);

private:
    bool visible = false;
    std::wstring persona;
    std::wstring avatarPath;
};

} // namespace SpeedrunLauncher::Main
