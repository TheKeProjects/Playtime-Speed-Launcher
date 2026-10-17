#include "Windows/Main/MainScreen/IntroVideo.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Localization/Localization.h"
#include "UI/Style.h"

namespace SpeedrunLauncher::Main {

bool IntroVideo::Start() {
    const std::wstring path = Paths::Asset(L"Videos\\Introduccion.mp4");
    if (!Files::Exists(path) || !player.Open(path, true, false)) {
        visible = false;
        return false;
    }
    visible = true;
    return true;
}

void IntroVideo::Hide() {
    player.Stop();
    visible = false;
}

bool IntroVideo::Draw(UI::Ui& ui, const Rect& screen) {
    if (!visible) return false;
    ui.g.FillRect(screen, Colors::Black);
    player.Update();
    player.Draw(ui.g, screen);
    TextStyle skip = UI::Mono(9.f);
    skip.align = HAlign::Right;
    skip.valign = VAlign::Bottom;
    ui.g.Text(Loc::Get("intro_skip"), { screen.x, screen.y, screen.w - 16.f, screen.h - 12.f }, Hex(0x1A2A38), skip);
    ui.KeepAnimating();
    // MouseDown anywhere skips; the end of the video (or a playback error) hides it too
    ui.Block(screen);
    const bool clicked = ui.MouseOver(screen) &&
                         (ui.In().pressed[0] || ui.In().pressed[1] || ui.In().pressed[2] || ui.In().pressed[3] || ui.In().pressed[4]);
    if (clicked || player.Ended() || player.Failed()) {
        Hide();
        return true;
    }
    return false;
}

} // namespace SpeedrunLauncher::Main
