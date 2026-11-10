#include "Windows/Main/Common/BadgeDraw.h"
#include <algorithm>
#include <cmath>
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/Account/DuelsService.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"

namespace SpeedrunLauncher::Main::BadgeDraw {

using Services::Account::Badge;

Look LookOf(const Badge& badge) {
    Look look;
    if (badge.kind == "champion") {
        look.title = Services::Account::DuelChampionTitle(badge.detail);
        look.description = Loc::Get("badge_champion_desc");
        look.color = Hex(0xFFC83D);
        look.glyph = wchar_t(0xE734);   // trophy
    } else if (badge.kind == "pioneer") {
        look.title = Loc::Get("badge_pioneer");
        look.description = Loc::Get("badge_pioneer_desc");
        look.color = Hex(0x3DE0D0);
        look.glyph = wchar_t(0xE7C1);   // flag
    } else if (badge.kind == "developer") {
        look.title = Loc::Get("badge_developer");
        look.description = Loc::Get("badge_developer_desc");
        look.color = Hex(0xFF6B3D);
        look.glyph = wchar_t(0xE943);   // code
    } else if (badge.kind == "contributor") {
        look.title = Loc::Get("badge_contributor");
        look.description = Loc::Get("badge_contributor_desc");
        look.color = Hex(0xC07CFF);
        look.glyph = wchar_t(0xEB51);   // heart
    } else {
        // A badge from a newer launcher: shown plainly
        look.title = Text::Widen(badge.kind);
        look.description = Loc::Get("badge_unknown_desc");
        look.color = Hex(0x9AB0C4);
        look.glyph = wchar_t(0xE735);
    }
    return look;
}

void Medal(UI::Ui& ui, float cx, float cy, float radius, const Badge& badge, int seed) {
    auto& g = ui.g;
    const Look look = LookOf(badge);
    NoColorFilter keep(g);
    const Color c = look.color;
    g.GlowRound({ cx - radius, cy - radius, radius * 2.f, radius * 2.f }, radius, c, radius * 0.5f, 0.25f);
    g.FillEllipse(cx, cy, radius, radius, Lerp(c, Colors::Black, 0.45f));
    g.FillEllipse(cx, cy, radius - radius * 0.1f, radius - radius * 0.1f, c);
    g.FillEllipse(cx, cy, radius * 0.77f, radius * 0.77f, Lerp(c, Colors::Black, 0.15f));
    // A light sliding across the top
    const float t = static_cast<float>(ui.Time());
    const float glint = std::fmod(t * 0.35f + seed * 0.37f, 1.f);
    g.FillEllipse(cx - radius * 0.45f + glint * radius * 0.2f, cy - radius * 0.45f, radius * 0.22f, radius * 0.14f, Colors::White.WithAlpha(0.35f));
    UI::Glyph(ui, look.glyph, { cx - radius, cy - radius, radius * 2.f, radius * 2.f }, radius * 0.8f, Lerp(c, Colors::Black, 0.65f));
    ui.KeepAnimatingThrottled();
}

float PillWidth(UI::Ui& ui, const Badge& badge, float height) {
    const float size = height * 0.45f;
    return ui.g.Measure(LookOf(badge).title, UI::Mono(size, true)).w + height + 14.f;
}

float Pill(UI::Ui& ui, float x, float cy, const Badge& badge, float maxW, float height) {
    auto& g = ui.g;
    const Look look = LookOf(badge);
    const float w = std::min(maxW, PillWidth(ui, badge, height));
    if (w < height + 10.f) return 0.f;
    const Rect r{ x, cy - height * 0.5f, w, height };
    NoColorFilter keep(g);
    g.GlowRound(r, height * 0.5f, Colors::Black, 6.f, 0.35f);
    g.Border(r, height * 0.5f, Lerp(Hex(0x0A0E14), look.color, 0.16f).WithAlpha(0.94f), look.color.WithAlpha(0.8f), 1.f);
    UI::Glyph(ui, look.glyph, { r.x + 4.f, r.y, height - 2.f, r.h }, height * 0.45f, look.color);
    TextStyle st = UI::Mono(height * 0.45f, true);
    st.ellipsis = true;
    g.Text(look.title, { r.x + height + 2.f, r.y, r.w - height - 10.f, r.h }, Lerp(look.color, Colors::White, 0.2f), st);
    return w;
}

} // namespace SpeedrunLauncher::Main::BadgeDraw
