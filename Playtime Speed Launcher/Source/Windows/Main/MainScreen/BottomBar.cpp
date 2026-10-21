#include "Localization/Localization.h"
#include "Seasonal/SeasonalTheme.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "Windows/Main/MainScreen/Bars.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main::BottomBar {

namespace {

// Height 24, CornerRadius 3, inner StackPanel Margin="8,0": glyph (10) + 5 px + label (9 bold)
float FooterButtonWidth(UI::Ui& ui, const std::wstring& label) {
    return 8.f + 12.f + 5.f + ui.g.Measure(label, UI::Mono(9.f, true)).w + 8.f + 2.f;
}

bool FooterButton(UI::Ui& ui, const char* id, const Rect& r, wchar_t glyph, const std::wstring& label, Color background, Color border,
                  Color glyphColor, Color textColor) {
    UI::ButtonScope b(ui, ui.MakeId(id), r, { background, border, 1.f, 3.f, { 0.f }, 0.f, 24.f });
    UI::Glyph(ui, glyph, { r.x + 8.f, r.y, 12.f, r.h }, 10.f, glyphColor);
    ui.g.Text(label, { r.x + 8.f + 12.f + 5.f, r.y, r.w, r.h }, textColor, UI::Mono(9.f, true));
    return b.Clicked();
}

} // namespace

void Draw(UI::Ui& ui, MainWindow& main, const Rect& area) {
    auto& g = ui.g;
    const auto& palette = Seasonal::Current();
    g.FillRect(area, palette.bar);
    g.FillRect({ area.x, area.y, area.w, 1.f }, palette.barBorder);
    const float y = area.CY() - 12.f;

    const std::wstring bug = Loc::Get("bug_report_btn");
    const float bugW = FooterButtonWidth(ui, bug);
    if (FooterButton(ui, "bugReport", { area.x + 12.f, y, bugW, 24.f }, UI::Glyphs::Bug, bug, Hex(0x0C0814), Hex(0x28104A), Hex(0x6633AA),
                     Hex(0x5522AA)))
        main.OpenBugReport();

    const std::wstring tutorials = L"TUTORIALS";
    const float tutW = FooterButtonWidth(ui, tutorials);
    if (FooterButton(ui, "tutorials", { area.CX() - tutW * 0.5f, y, tutW, 24.f }, UI::Glyphs::Education, tutorials, Hex(0x060F18),
                     Hex(0x003322), Hex(0x007755), Hex(0x007755)))
        main.OpenBeginnerTutorials();

}

} // namespace SpeedrunLauncher::Main::BottomBar
