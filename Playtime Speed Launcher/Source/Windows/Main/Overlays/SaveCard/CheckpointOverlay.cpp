#include "Windows/Main/Overlays/SaveCard/CheckpointOverlay.h"
#include <algorithm>
#include "Localization/Localization.h"
#include "UI/Layout.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

using namespace UI;

void CheckpointOverlay::Open(int chapterNumber) {
    checkpoints = Services::SaveGames::Checkpoints(chapterNumber);
    resetScroll = true;
    popup.Open();
}

void CheckpointOverlay::Draw(Ui& ui, const Rect& screen) {
    auto& g = ui.g;
    PanelStyle style;
    style.title = Loc::Get("checkpoint_select_header");
    OverlayFrame frame(ui, ui.MakeId("checkpoints"), screen, 420.f, 0.f, style, popup.Opacity(), popup.Scale());

    const float rowH = 1.f + 9.f + std::max(g.Measure(L"\xE74E", IconFont(13.f)).h, g.Measure(L"Ag", Mono(11.f, true)).h) + 9.f + 1.f;
    const float listH = checkpoints.empty() ? 0.f : rowH * checkpoints.size() + 6.f * (checkpoints.size() - 1);
    const float viewH = std::min(listH, 360.f);
    const Rect viewport{ frame.body.x + 24.f, frame.body.y + 18.f, frame.body.w - 48.f, viewH };
    if (resetScroll) {
        ScrollArea::ScrollTo(ui, ui.MakeId("checkpoints-scroll"), 0.f);
        resetScroll = false;
    }
    {
        ScrollArea scroll(ui, ui.MakeId("checkpoints-scroll"), viewport, false, 48.f, ScrollArea::Bar::Classic);
        Column col(viewport.x, viewport.y, scroll.ContentWidth());
        const ButtonStyle s{ Palette::ButtonBg, Palette::ButtonBorder, 1.f, 4.f, { 14.f, 9.f }, 0.f, rowH };
        for (size_t i = 0; i < checkpoints.size(); ++i) {
            if (i) col.Gap(6.f);
            if (GlyphTextButton(ui, ui.MakeId("checkpoint", static_cast<int64_t>(i)), col.Next(rowH), Glyphs::Save, 13.f, Palette::Teal,
                                checkpoints[i].name, Mono(11.f, true), Hex(0xCCDDEE), s)) {
                Services::SaveGames::LoadCheckpoint(checkpoints[i]);
                popup.Close();
            }
        }
        scroll.SetContentSize(listH);
    }
    frame.SetContentHeight(18.f + viewH + 18.f);

    const std::wstring back = Loc::Get("back");
    const TextStyle text = Mono(11.f, true).Center();
    const float w = ButtonWidth(ui, back, text, Styles::Footer());
    if (TextButton(ui, ui.MakeId("checkpoints-close"), { frame.footer.R() - 14.f - w, frame.footer.CY() - 18.f, w, 36.f }, back, text, Palette::Dim,
                   Styles::Footer()))
        popup.Close();
}

} // namespace SpeedrunLauncher::Main
