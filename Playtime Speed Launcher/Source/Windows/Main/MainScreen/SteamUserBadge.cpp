#include "Windows/Main/MainScreen/SteamUserBadge.h"
#include "Services/Platforms/SteamCmdRunner.h"
#include "UI/Style.h"

namespace SpeedrunLauncher::Main {

void SteamUserBadge::Load() {
    auto user = Services::SteamCmd::ActiveUser();
    if (!user) return;
    persona = user->personaName;
    if (auto avatar = Services::SteamCmd::AvatarPath(user->steamId64)) avatarPath = *avatar;
    visible = true;
}

float SteamUserBadge::Draw(UI::Ui& ui, float right, const Rect& bar) {
    if (!visible) return right;
    // Name (11 bold) + 24 px round avatar, 16 px left of the bar's own items
    const float size = 24.f;
    const Rect avatar{ right - 16.f - size, bar.CY() - size * 0.5f, size, size };
    if (!avatarPath.empty()) {
        if (auto image = Images::Load(avatarPath)) ui.g.DrawImageRounded(*image, avatar, size * 0.5f);
    }
    ui.g.StrokeEllipse(avatar.CX(), avatar.CY(), size * 0.5f, size * 0.5f, Hex(0x1A3A55), 1.f);
    const TextStyle style = UI::Mono(11.f, true);
    const float w = ui.g.Measure(persona, style).w + 2.f;
    ui.g.Text(persona, { avatar.x - 8.f - w, bar.y, w, bar.h }, Hex(0x3A6A8A), style);
    return avatar.x - 8.f - w;
}

} // namespace SpeedrunLauncher::Main
