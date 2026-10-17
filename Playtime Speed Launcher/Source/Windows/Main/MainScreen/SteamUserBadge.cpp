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

void SteamUserBadge::Draw(UI::Ui& ui, float right, float bottom) {
    if (!visible) return;
    // StackPanel Margin="0,0,20,14": name (13 bold, margin-right 12) + 44 px round avatar
    const Rect avatar{ right - 20.f - 44.f, bottom - 14.f - 44.f, 44.f, 44.f };
    if (!avatarPath.empty()) {
        if (auto image = Images::Load(avatarPath)) ui.g.DrawImageRounded(*image, avatar, 22.f);
    }
    const TextStyle style = UI::Mono(13.f, true);
    const float w = ui.g.Measure(persona, style).w + 2.f;
    ui.g.Text(persona, { avatar.x - 12.f - w, avatar.y, w, avatar.h }, Hex(0x3A6A8A), style);
}

} // namespace SpeedrunLauncher::Main
