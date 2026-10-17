#include "Windows/Dialogs/DialogParts.h"
#include <algorithm>
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"

namespace SpeedrunLauncher::Dialogs::Parts {

using namespace UI;

void Label(Ui& ui, Column& col, const std::wstring& text, float size, Color color, bool bold, float marginBottom) {
    col.Gap(Paragraph(ui, col.x, col.y, col.w, text, Mono(size, bold), color));
    col.Gap(marginBottom);
}

void Box(Ui& ui, Column& col, const std::wstring& text, float size, Color background, Color border, Color foreground, Edges padding,
         float marginBottom, bool bold) {
    const float inner = col.w - padding.H() - 2.f;
    TextStyle style = Mono(size, bold);
    style.wrap = true;
    style.valign = VAlign::Top;
    const float textH = ui.g.Measure(text, style, inner).h;
    const Rect r = col.Next(textH + padding.V() + 2.f, marginBottom);
    ui.g.Border(r, 3.f, background, border, border.a > 0.f ? 1.f : 0.f);
    ui.g.Text(text, { r.x + 1.f + padding.l, r.y + 1.f + padding.t, inner, textH }, foreground, style);
}

bool CheckBox(Ui& ui, Id id, Column& col, const std::wstring& text, bool& checked, float size, Color textColor) {
    TextStyle style = Mono(size);
    style.wrap = true;
    style.valign = VAlign::Top;
    const float textX = col.x + 13.f + 6.f;
    const float textH = ui.g.Measure(text, style, col.w - 19.f).h;
    const Rect r = col.Next(std::max(textH, 14.f));
    const Interaction it = ui.Interact(id, r);
    ui.SetCursor(IDC_HAND);
    // Aero2 CheckBox glyph box (white, blue border on hover, check mark)
    const Rect box{ col.x, r.y + std::max(0.f, (std::min(textH, 16.f) - 13.f) * 0.5f), 13.f, 13.f };
    ui.g.Border(box, 0.f, it.hovered ? Hex(0xF3F9FF) : Hex(0xFFFFFF), it.hovered ? Hex(0x5593FF) : Hex(0x333333), 1.f);
    if (checked) {
        const Color mark = it.hovered ? Hex(0x5593FF) : Hex(0x212121);
        ui.g.Line(box.x + 2.5f, box.y + 6.5f, box.x + 5.f, box.y + 9.5f, mark, 1.6f);
        ui.g.Line(box.x + 5.f, box.y + 9.5f, box.x + 10.5f, box.y + 3.f, mark, 1.6f);
    }
    ui.g.Text(text, { textX, r.y, col.w - 19.f, textH }, textColor, style);
    if (it.clicked) {
        checked = !checked;
        return true;
    }
    return false;
}

TextBoxStyle InputStyle(bool password) {
    TextBoxStyle s;
    s.text = Mono(12.f);
    s.foreground = Rgba(200, 210, 220);
    s.background = Rgba(10, 20, 32);
    s.border = Rgba(30, 60, 90);
    s.focusBorder = Rgba(30, 60, 90);
    s.caret = Rgba(0, 204, 170);
    s.padding = { 3.f, 2.f };
    s.password = password;
    return s;
}

bool Input(Ui& ui, Id id, Column& col, std::wstring& value, bool password, float marginBottom) {
    const TextBoxStyle style = InputStyle(password);
    return TextBox(ui, id, col.Next(TextBoxHeight(ui, style), marginBottom), value, style);
}

float SmallButtonWidth(Ui& ui, const std::wstring& text, float minWidth) {
    ButtonStyle s{ Rgba(8, 30, 55), Rgba(0, 120, 100, 180), 1.f, 3.f, { 8.f, 0.f }, minWidth, 28.f };
    return ButtonWidth(ui, text, Mono(10.f, true), s);
}

bool SmallButton(Ui& ui, Id id, const Rect& r, const std::wstring& text, Color foreground, bool enabled) {
    ButtonStyle s{ Rgba(8, 30, 55), Rgba(0, 120, 100, 180), 1.f, 3.f, { 8.f, 0.f }, 0.f, r.h };
    return TextButton(ui, id, r, text, Mono(10.f, true).Center(), foreground, s, enabled);
}

} // namespace SpeedrunLauncher::Dialogs::Parts
