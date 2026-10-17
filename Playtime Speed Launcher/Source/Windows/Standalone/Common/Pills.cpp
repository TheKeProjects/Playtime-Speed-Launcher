#include "Windows/Standalone/Common/Pills.h"
#include <algorithm>
#include "UI/Style.h"

namespace SpeedrunLauncher::Windows::Pills {

using namespace UI;

float Height(Ui& ui) { return ui.g.Measure(L"Ag", Mono(9.f, true)).h + 6.f + 2.f; }

int Wrap(Ui& ui, Column& col, std::string_view idBase, const std::vector<std::wstring>& labels, const std::function<bool(size_t)>& active,
         float extraWidth, Rect* trailing) {
    auto& g = ui.g;
    const float h = Height(ui);
    float x = col.x, y = col.y;
    int clicked = -1;
    for (size_t i = 0; i < labels.size(); ++i) {
        const float w = g.Measure(labels[i], Mono(9.f, true)).w + 14.f + 2.f;
        if (x > col.x && x + w > (col.x + col.w)) {
            x = col.x;
            y += h + 4.f;
        }
        const Rect r{ x, y, w, h };
        const bool on = active && active(i);
        const Id id = ui.MakeId(idBase, static_cast<int64_t>(i));
        const Interaction it = ui.Interact(id, r);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        // Selection and hover fade instead of switching instantly
        const float sel = ui.Animate(HashId(id, 0x5E1E), on ? 1.f : 0.f, 16.f);
        const float hov = ui.Animate(HashId(id, 0x4074), it.hovered ? 1.f : 0.f, 18.f);
        const Color idle = Lerp(Rgba(255, 255, 255, 20), Rgba(255, 255, 255, 34), hov);
        const Color bg = Lerp(idle, Rgba(0, 204, 170, 46), sel);
        const Color border = Lerp(Lerp(Rgba(100, 140, 160, 35), Rgba(100, 140, 160, 80), hov), Rgba(0, 204, 170, 170), sel);
        g.Border(r, h * 0.5f < 10.f ? h * 0.5f : 10.f, bg, border, 1.f);
        g.Text(labels[i], r, Lerp(Lerp(Rgba(100, 140, 160, 150), Rgba(170, 200, 215, 220), hov), Palette::Teal, sel), Mono(9.f, true).Center());
        if (it.held && ui.In().pressed[0]) clicked = static_cast<int>(i);
        x += w + 4.f;
    }
    if (extraWidth > 0.f) {
        if (x > col.x && x + extraWidth > (col.x + col.w)) {
            x = col.x;
            y += h + 4.f;
        }
        if (trailing) *trailing = { x, y, extraWidth, h };
    }
    const bool any = !labels.empty() || extraWidth > 0.f;
    col.Gap(any ? (y + h + 4.f) - col.y : 0.f);
    return clicked;
}

} // namespace SpeedrunLauncher::Windows::Pills
