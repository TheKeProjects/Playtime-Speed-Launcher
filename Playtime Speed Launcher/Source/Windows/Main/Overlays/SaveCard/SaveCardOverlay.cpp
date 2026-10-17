#include "Windows/Main/Overlays/SaveCard/SaveCardOverlay.h"
#include <algorithm>
#include "Localization/Localization.h"
#include "Services/GameFiles/SaveGames.h"
#include "UI/Layout.h"
#include "UI/Widgets/Button.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
namespace Saves = Services::SaveGames;

namespace {

// "glyph  TEXT  [+]" row button (Padding 16,12)
bool ActionButton(Ui& ui, Id id, Column& col, wchar_t glyph, const std::wstring& text, Color color, const ButtonStyle& style, bool plus,
                  float marginBottom) {
    auto& g = ui.g;
    const float h = 1.f + 12.f + std::max(g.Measure(std::wstring(1, glyph), IconFont(18.f)).h, g.Measure(L"+", Mono(16.f, true)).h) + 12.f + 1.f;
    const Rect r = col.Next(h, marginBottom);
    ButtonScope b(ui, id, r, style);
    float x = b.content.x;
    g.Text(std::wstring(1, glyph), { x, r.y, 18.f, r.h }, color, IconFont(18.f));
    x += 18.f + 12.f;
    const float w = g.Measure(text, Mono(11.f, true)).w + 1.f;
    g.Text(text, { x, r.y, w, r.h }, color, Mono(11.f, true));
    if (plus) g.Text(L"+", { x + w + 10.f, r.y, 20.f, r.h }, color, Mono(16.f, true));
    return b.Clicked();
}

} // namespace

void SaveCardOverlay::Open(int chapterNumber) {
    chapter = chapterNumber;
    popup.Open();
}

void SaveCardOverlay::Draw(Ui& ui, const Rect& screen) {
    PanelStyle style;
    style.title = Loc::Get("save_card_header");
    OverlayFrame frame(ui, ui.MakeId("savecard"), screen, 360.f, 0.f, style, popup.Opacity(), popup.Scale());
    Column col(frame.body.x + 24.f, frame.body.y + 18.f, frame.body.w - 48.f);
    const ButtonStyle normal{ Palette::ButtonBg, Palette::ButtonBorder, 1.f, 4.f, { 16.f, 12.f }, 0.f, 0.f };

    const bool slots = chapter == 4 || chapter == 5;
    if (slots && ActionButton(ui, ui.MakeId("savecard-delete"), col, Glyphs::Delete, Loc::Get("save_card_delete_btn"), Palette::DangerText,
                              { Palette::DangerBg, Palette::DangerBorder, 1.f, 4.f, { 16.f, 12.f }, 0.f, 0.f }, false, 12.f)) {
        Saves::DeleteSaves(chapter);
        popup.Close();
    }
    const std::wstring saveText = chapter == 4 ? Loc::Get("ch4_load_btn") : chapter == 5 ? Loc::Get("ch5_load_btn") : Loc::Get("save_card_save_btn");
    if (ActionButton(ui, ui.MakeId("savecard-save"), col, Glyphs::Save, saveText, Palette::Teal, normal, !slots, 0.f)) {
        Saves::LoadStartSaves(chapter);
        popup.Close();
    }
    if (chapter == 5 && Saves::HasNgPlus()) {
        col.Gap(12.f);
        if (ActionButton(ui, ui.MakeId("savecard-ngplus"), col, Glyphs::Save,
                         Loc::Get(Saves::NgPlusLoaded() ? "ch5_ngplus_unload_btn" : "ch5_ngplus_load_btn"), Palette::Teal, normal, false, 0.f)) {
            Saves::ToggleNgPlus();
            popup.Close();
        }
    }
    frame.SetContentHeight(18.f + col.Height() + 18.f);

    const std::wstring back = Loc::Get("back");
    const TextStyle text = Mono(11.f, true).Center();
    const float w = ButtonWidth(ui, back, text, Styles::Footer());
    if (TextButton(ui, ui.MakeId("savecard-close"), { frame.footer.R() - 14.f - w, frame.footer.CY() - 18.f, w, 36.f }, back, text, Palette::Dim,
                   Styles::Footer()))
        popup.Close();
}

} // namespace SpeedrunLauncher::Main
