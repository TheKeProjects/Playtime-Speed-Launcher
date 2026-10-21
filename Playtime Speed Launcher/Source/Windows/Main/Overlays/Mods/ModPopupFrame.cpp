#include "Windows/Main/Overlays/Mods/ModPopupFrame.h"
#include <cmath>
#include "Graphics/Animation.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Dialogs/DialogWindow.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
namespace K = UI::MenuKit;

namespace {

K::Kind KindFor(const ModButton& b) { return b.secondary ? K::Kind::Secondary : b.danger ? K::Kind::Danger : K::Kind::Primary; }

wchar_t GlyphFor(const ModButton& b) { return b.secondary ? Glyphs::Cancel : b.danger ? Glyphs::Delete : Glyphs::CheckMark; }

float Width(Ui& ui, const ModButton& b) { return K::FooterButtonWidth(ui, b.text, GlyphFor(b), b.minWidth); }

void DrawButton(Ui& ui, const ModButton& b, float x, float cy) {
    if (K::FooterButton(ui, ui.MakeId(b.id), { x, cy - 18.f, Width(ui, b), 36.f }, b.text, KindFor(b), b.enabled, GlyphFor(b)) && b.onClick)
        b.onClick();
}

} // namespace

void DrawModPopup(Ui& ui, Popup& popup, const Rect& screen, float width, const std::wstring& title, wchar_t glyph, Color accent,
                  const std::wstring& question, const std::wstring& warning, const std::vector<ModButton>& left, const std::vector<ModButton>& right) {
    PanelStyle style;
    style.title = title;
    OverlayFrame frame(ui, ui.MakeId(std::string("mod-popup-") + std::to_string(static_cast<int>(width))), screen, width, 0.f, style,
                       popup.Opacity(), popup.Scale());
    const double start = frame.OpenedAt() + 0.15;
    const float innerW = frame.body.w - 56.f;
    float y = frame.body.y + 24.f;
    {
        // Badge, popping in
        StaggerItem reveal(ui, start, 0, 0.f, 10.f);
        K::GlyphBadge(ui, { frame.body.CX() - 28.f, y, 56.f, 56.f }, glyph, accent, 24.f);
        y += 56.f + 18.f;
    }
    {
        StaggerItem reveal(ui, start, 1, 0.f, 10.f);
        TextStyle q = Mono(13.f);
        q.wrap = true;
        q.align = HAlign::Center;
        q.valign = VAlign::Top;
        const float qh = ui.g.Measure(question, q, innerW).h;
        ui.g.Text(question, { frame.body.x + 28.f, y, innerW, qh }, Palette::Question, q);
        y += qh;
    }
    if (!warning.empty()) {
        StaggerItem reveal(ui, start, 2, 0.f, 10.f);
        std::wstring text = warning;
        if (!text.empty() && text[0] == L'\x26A0') text = text.substr(text.size() > 1 && text[1] == L' ' ? 2 : 1);   // the box has its own icon
        const float wh = K::NoteHeight(ui, text, innerW);
        K::Note(ui, { frame.body.x + 28.f, y + 16.f, innerW, wh }, Glyphs::Warning, text, K::kAmber);
        y += 16.f + wh;
    }
    frame.SetContentHeight(y + 24.f - frame.body.y);

    // Footer groups (Margin 14,0)
    const Rect row = frame.footer.Inset(14.f, 0.f);
    float x = row.x;
    for (size_t i = 0; i < left.size(); ++i) {
        DrawButton(ui, left[i], x, row.CY());
        x += Width(ui, left[i]) + (i + 1 < left.size() ? left[i].gapAfter : 0.f);
    }
    float total = 0.f;
    for (size_t i = 0; i < right.size(); ++i) total += Width(ui, right[i]) + (i + 1 < right.size() ? right[i].gapAfter : 0.f);
    x = row.R() - total;
    for (size_t i = 0; i < right.size(); ++i) {
        DrawButton(ui, right[i], x, row.CY());
        x += Width(ui, right[i]) + right[i].gapAfter;
    }
}

bool DrawModWorking(Ui& ui, Popup& popup, const Rect& screen, float width, const std::wstring& title, const ModWorkState& state) {
    auto& g = ui.g;
    PanelStyle style;
    style.title = title;
    // Same frame as the question it replaces, so the panel stays where it was
    OverlayFrame frame(ui, ui.MakeId(std::string("mod-popup-") + std::to_string(static_cast<int>(width))), screen, width, 0.f, style,
                       popup.Opacity(), popup.Scale());
    const double now = ui.Time();
    const float elapsed = static_cast<float>(now - state.startedAt);
    const float since = state.finished ? static_cast<float>(now - state.finishedAt) : 0.f;
    const Color end = state.ok ? K::kGreen : K::kRed;
    const float doneT = state.finished ? Anim::EaseOutCubic(Anim::Clamp01(since / 0.4f)) : 0.f;
    const Color accent = Lerp(Palette::Teal, end, doneT);
    ui.KeepAnimating();

    // There's no real total: the arc creeps towards 90 % while working and closes once it's done
    const float target = state.finished ? 1.f : 0.9f * (1.f - std::exp(-elapsed / 1.2f));
    float& shown = ui.Slot(ui.MakeId("mod-work-progress"), 0.f);
    if (elapsed < 0.05f) shown = 0.f;   // a new job starts from empty
    shown = Anim::Approach(shown, target, state.finished ? 9.f : 5.f, ui.Dt());

    float y = frame.body.y + 26.f;
    constexpr float kRing = 46.f;
    const float shake = state.finished && !state.ok ? 8.f * std::sin(since * 40.f) * (1.f - Anim::Clamp01(since / 0.5f)) : 0.f;
    const float cx = frame.body.CX() + shake, cy = y + kRing + 8.f;
    {
        // Pop in on the first frames
        const float pop = Anim::EaseOutBack(Anim::Clamp01(elapsed / 0.35f), 1.4f);
        ui.PushScale(0.7f + 0.3f * pop, cx, cy);
        g.FillEllipse(cx, cy, kRing + 14.f, kRing + 14.f, accent.WithAlpha(0.06f));
        g.StrokeEllipse(cx, cy, kRing, kRing, Palette::ButtonBorder, 5.f);
        // Progress arc (round-capped segments) from the top
        constexpr int kSegments = 96;
        const int lit = static_cast<int>(std::round(shown * kSegments));
        const float step = 6.2831853f / kSegments;
        for (int i = 0; i < lit; ++i) {
            const float a0 = -1.5707963f + i * step, a1 = a0 + step;
            g.Line(cx + std::cos(a0) * kRing, cy + std::sin(a0) * kRing, cx + std::cos(a1) * kRing, cy + std::sin(a1) * kRing, accent, 5.f, true);
        }
        if (!state.finished) {
            // Comet at the head of the arc, a dashed orbit turning inside the ring
            const float a = -1.5707963f + shown * 6.2831853f;
            g.FillEllipse(cx + std::cos(a) * kRing, cy + std::sin(a) * kRing, 7.f, 7.f, accent.WithAlpha(0.25f));
            g.FillEllipse(cx + std::cos(a) * kRing, cy + std::sin(a) * kRing, 3.5f, 3.5f, Colors::White.WithAlpha(0.85f));
            const float spin = static_cast<float>(std::fmod(now * 1.2, 6.2831853));
            for (int i = 0; i < 12; ++i) {
                const float b = spin + i * 0.5235988f;
                g.FillEllipse(cx + std::cos(b) * (kRing - 13.f), cy + std::sin(b) * (kRing - 13.f), 1.4f, 1.4f, accent.WithAlpha(0.2f + 0.08f * (i % 3)));
            }
            Glyph(ui, state.glyph ? state.glyph : Glyphs::Install, { cx - 20.f, cy - 20.f + 3.f * std::sin(static_cast<float>(now) * 3.f), 40.f, 40.f }, 24.f, accent);
        } else {
            // Burst: an expanding ring and sparks (success), then the ✓ / ✕ pops in
            const float burst = Anim::Clamp01(since / 0.7f);
            if (burst < 1.f) {
                const float rr = kRing + 26.f * Anim::EaseOutCubic(burst);
                g.StrokeEllipse(cx, cy, rr, rr, accent.WithAlpha(0.6f * (1.f - burst)), 2.f);
                if (state.ok)
                    for (int i = 0; i < 14; ++i) {
                        const float a = 6.2831853f * (static_cast<float>(i) / 14.f) + 0.2f;
                        const float d = kRing + 6.f + 40.f * Anim::EaseOutCubic(burst);
                        const float s = 3.2f * (1.f - burst);
                        g.FillEllipse(cx + std::cos(a) * d, cy + std::sin(a) * d, s, s, i % 2 ? Palette::Teal : K::kGreen);
                    }
            }
            const float k = Anim::EaseOutBack(Anim::Clamp01((since - 0.1f) / 0.35f), 2.f);
            if (k > 0.f) {
                g.FillEllipse(cx, cy, 28.f * k, 28.f * k, accent.WithAlpha(0.18f));
                Glyph(ui, state.ok ? Glyphs::CheckMark : Glyphs::Cancel, { cx - 28.f, cy - 28.f, 56.f, 56.f }, 26.f * k, accent);
            }
        }
        ui.PopTransform();
    }
    y = cy + kRing + 24.f;

    // Label (working) or the result's first line, then the elapsed time or the rest of the message
    const float innerW = frame.body.w - 56.f;
    TextStyle head = Mono(13.f, true);
    head.wrap = true;
    head.align = HAlign::Center;
    head.valign = VAlign::Top;
    const std::wstring& heading = state.finished ? state.head : state.label;
    const float headH = g.Measure(heading, head, innerW).h;
    const float textK = state.finished ? Anim::EaseOutCubic(Anim::Clamp01((since - 0.15f) / 0.3f)) : 1.f;
    g.PushOpacity(textK);
    ui.PushTranslate(0.f, 6.f * (1.f - textK));
    g.Text(heading, { frame.body.x + 28.f, y, innerW, headH }, state.finished ? Lerp(accent, Palette::Bright, 0.35f) : K::kTitle, head);
    y += headH + 8.f;
    if (!state.finished) {
        const int secs = static_cast<int>(elapsed);
        wchar_t clock[16];
        swprintf_s(clock, L"%02d:%02d", secs / 60, secs % 60);
        g.Text(clock, { frame.body.x, y, frame.body.w, 16.f }, K::kDim, Mono(10.f, true).Center());
        y += 16.f;
    } else if (!state.body.empty()) {
        TextStyle body = Mono(10.5f);
        body.wrap = true;
        body.align = HAlign::Center;
        body.valign = VAlign::Top;
        const float bodyH = g.Measure(state.body, body, innerW).h;
        g.Text(state.body, { frame.body.x + 28.f, y, innerW, bodyH }, K::kDim, body);
        y += bodyH;
    }
    ui.PopTransform();
    g.PopLayer();
    frame.SetContentHeight(y + 24.f - frame.body.y);

    // Footer: a "working" hint while busy; OK once finished
    const Rect row = frame.footer.Inset(18.f, 0.f);
    if (!state.finished) {
        Form::Spinner(ui, row.x + 8.f, row.CY(), 7.f, K::kAmber);
        g.Text(L"Working on the game files\x2026 don't close the game folder", row.CutLeft(26.f), K::kAmber.WithAlpha(0.85f), Mono(10.f, true).Ellipsis());
        return false;
    }
    const float w = K::FooterButtonWidth(ui, L"OK", Glyphs::CheckMark, 120.f);
    return K::FooterButton(ui, ui.MakeId("mod-work-ok"), { row.R() - w, row.CY() - 18.f, w, 36.f }, L"OK", K::Kind::Primary, since > 0.3f,
                           state.ok ? Glyphs::CheckMark : Glyphs::Cancel, end) ||
           (popup.IsOpen() && since > 0.3f && (ui.KeyPressed(VK_RETURN) || ui.KeyPressed(VK_ESCAPE)));
}

void ShowModMessage(HWND owner, const std::wstring& title, const std::wstring& message, bool success) {
    Dialogs::Options o;
    o.title = title;
    o.close = L"OK";
    o.contentWidth = 380.f;
    o.seasonal = true;
    Dialogs::Show(owner, o, Dialogs::BadgeText(success ? Glyphs::CheckMark : Glyphs::Warning, success ? K::kGreen : K::kRed, message));
}

void ConfirmDeleteUe4ss(HWND owner, const std::wstring& title, std::function<void()> onYes) {
    Dialogs::Options o;
    o.title = title;
    o.primary = L"YES";
    o.close = L"CANCEL";
    o.contentWidth = 380.f;
    o.seasonal = true;
    Dialogs::Show(owner, o, Dialogs::BadgeText(Glyphs::Delete, K::kAmber, L"This requires deleting the current UE4SS files.\nDo you want to delete them?"),
                  [onYes](Dialogs::Result r) {
                      if (r == Dialogs::Result::Primary && onYes) onYes();
                  });
}

void ConfirmDeleteLegacyLoadManip(HWND owner, const std::wstring& title, std::function<void()> onYes) {
    Dialogs::Options o;
    o.title = title;
    o.primary = L"DELETE";
    o.close = L"CANCEL";
    o.contentWidth = 380.f;
    o.seasonal = true;
    Dialogs::Show(owner, o,
                  Dialogs::BadgeText(Glyphs::Delete, K::kAmber,
                                     L"An old Load Manip (LoadManipV2_P) was found in Content\\Paks.\n"
                                     L"It must be deleted before installing. Delete it now?"),
                  [onYes](Dialogs::Result r) {
                      if (r == Dialogs::Result::Primary && onYes) onYes();
                  });
}

} // namespace SpeedrunLauncher::Main
