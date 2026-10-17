#include "Windows/Main/Overlays/EasterEgg/EasterEggOverlay.h"
#include "Features/EasterEggs.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

void EasterEggOverlay::Play(const std::wstring& path) {
    visible = player.Open(path, true, false);
    if (!visible) main.Eggs().Hide();
}

void EasterEggOverlay::Stop() {
    player.Stop();
    visible = false;
}

void EasterEggOverlay::Draw(UI::Ui& ui, const Rect& screen) {
    if (!visible) return;
    auto& g = ui.g;
    g.FillRect(screen, Argb(0xCC000000));
    ui.Block(screen);
    const Rect box = screen.Center(960.f, 570.f);
    g.Border(box, 4.f, Rgba(0x09, 0x0C, 0x1E), Rgba(0x2A, 0x10, 0x50), 1.f);
    player.Update();
    player.Draw(g, box.Inset(1.f));
    ui.KeepAnimating();
    // MouseDown anywhere, the end of the video or an error closes it
    if ((ui.MouseOver(screen) && (ui.In().pressed[0] || ui.In().pressed[1] || ui.In().pressed[2])) || player.Ended() || player.Failed())
        main.Eggs().Hide();
}

} // namespace SpeedrunLauncher::Main
