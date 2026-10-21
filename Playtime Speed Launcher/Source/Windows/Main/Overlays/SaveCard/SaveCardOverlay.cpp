#include "Windows/Main/Overlays/SaveCard/SaveCardOverlay.h"
#include <algorithm>
#include "Localization/Localization.h"
#include "Services/GameFiles/SaveGames.h"
#include "UI/Layout.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
namespace K = UI::MenuKit;
namespace Saves = Services::SaveGames;

void SaveCardOverlay::Open(int chapterNumber) {
    chapter = chapterNumber;
    popup.Open();
}

// The chapter's heading, then one card per action (delete in red, load in teal, NG+ in purple),
// coming in one after another
void SaveCardOverlay::Draw(Ui& ui, const Rect& screen) {
    PanelStyle style;
    style.title = Loc::Get("save_card_header");
    OverlayFrame frame(ui, ui.MakeId("savecard"), screen, 420.f, 0.f, style, popup.Opacity(), popup.Scale());
    const double start = frame.OpenedAt() + 0.15;
    Column col(frame.body.x + 24.f, frame.body.y + 20.f, frame.body.w - 48.f);
    int row = 0;
    {
        StaggerItem reveal(ui, start, row++, 0.f, 10.f);
        K::Hero(ui, col.Next(K::kHeroH, 18.f), Glyphs::Save, Loc::Fmt("save_card_hero_title", chapter), Loc::Get("save_card_hero_sub"), Palette::Teal);
    }
    const float cardH = K::ActionCardHeight(true);
    const auto Card = [&](const char* id, wchar_t glyph, const std::wstring& title, const std::wstring& desc, Color accent) {
        StaggerItem reveal(ui, start, row++, 24.f);
        return K::ActionCard(ui, ui.MakeId(id), col.Next(cardH, 10.f), glyph, title, desc, accent);
    };

    const bool slots = chapter == 4 || chapter == 5;
    if (slots && Card("savecard-delete", Glyphs::Delete, Loc::Get("save_card_delete_btn"), Loc::Get("save_card_delete_desc"), K::kRed)) {
        Saves::DeleteSaves(chapter);
        popup.Close();
    }
    const std::wstring saveText = chapter == 4 ? Loc::Get("ch4_load_btn") : chapter == 5 ? Loc::Get("ch5_load_btn") : Loc::Get("save_card_save_btn");
    if (Card("savecard-save", Glyphs::Download, saveText, Loc::Get("save_card_load_desc"), Palette::Teal)) {
        Saves::LoadStartSaves(chapter);
        popup.Close();
    }
    if (chapter == 5 && Saves::HasNgPlus()) {
        const bool loaded = Saves::NgPlusLoaded();
        if (Card("savecard-ngplus", loaded ? Glyphs::Refresh : Glyphs::Forward, Loc::Get(loaded ? "ch5_ngplus_unload_btn" : "ch5_ngplus_load_btn"),
                 Loc::Get(loaded ? "ch5_ngplus_unload_desc" : "ch5_ngplus_load_desc"), Rgba(150, 110, 230))) {
            Saves::ToggleNgPlus();
            popup.Close();
        }
    }
    frame.SetContentHeight(20.f + col.Height() + 10.f);

    const std::wstring back = Loc::Get("back");
    const float w = K::FooterButtonWidth(ui, back, Glyphs::Back, 120.f);
    if (K::FooterButton(ui, ui.MakeId("savecard-close"), { frame.footer.R() - 14.f - w, frame.footer.CY() - 18.f, w, 36.f }, back, K::Kind::Secondary,
                        true, Glyphs::Back) ||
        (popup.IsOpen() && ui.KeyPressed(VK_ESCAPE)))
        popup.Close();
}

} // namespace SpeedrunLauncher::Main
