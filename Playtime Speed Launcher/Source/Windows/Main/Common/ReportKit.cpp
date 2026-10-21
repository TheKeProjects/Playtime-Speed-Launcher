#include "Windows/Main/Common/ReportKit.h"
#include <algorithm>
#include <cmath>
#include "Core/Files.h"
#include "Core/System.h"
#include "Graphics/Animation.h"
#include "Graphics/Images.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/MenuKit.h"

namespace SpeedrunLauncher::Main::ReportKit {

using namespace UI;

namespace {

constexpr Color kGreen = MenuKit::kGreen, kRed = MenuKit::kRed, kAmber = MenuKit::kAmber, kTitle = MenuKit::kTitle;
constexpr Color kSub = Rgba(140, 165, 185);

// Arc from `start` (radians) sweeping `sweep`, as round-capped segments
void Arc(Graphics& g, float cx, float cy, float radius, float start, float sweep, Color c, float width) {
    const int segments = std::max(2, static_cast<int>(std::abs(sweep) / 0.12f));
    const float step = sweep / segments;
    for (int i = 0; i < segments; ++i) {
        const float a0 = start + i * step, a1 = a0 + step;
        g.Line(cx + std::cos(a0) * radius, cy + std::sin(a0) * radius, cx + std::cos(a1) * radius, cy + std::sin(a1) * radius, c, width, true);
    }
}

TextStyle Wrapped(float size, bool bold = false) {
    TextStyle s = Mono(size, bold);
    s.wrap = true;
    s.valign = VAlign::Top;
    return s;
}

} // namespace

void Banner(Ui& ui, const Rect& r, wchar_t glyph, const std::wstring& title, const std::wstring& subtitle, const std::wstring& pill, Color accent) {
    auto& g = ui.g;
    const float t = static_cast<float>(ui.Time());
    g.PushRoundClip(r, kRadius);
    g.FillLinear(r, 0.f, { { 0.f, Lerp(accent, Colors::Black, 0.62f) }, { 0.55f, Hex(0x0B1622) }, { 1.f, Hex(0x07101A) } }, { 0.f, 0.f }, { 1.f, 1.f });
    const float lx = r.x + r.w * (0.65f + 0.25f * std::sin(t * 0.6f));
    g.FillRadial({ lx - 200.f, r.y - 90.f, 400.f, 260.f }, { { 0.f, accent.WithAlpha(0.2f) }, { 1.f, accent.WithAlpha(0.f) } });
    // Ripples out of the badge
    const float bx = r.x + 56.f, by = r.CY();
    const float ring = std::fmod(t, 3.f) / 3.f;
    for (int i = 0; i < 2; ++i) {
        const float k = std::fmod(ring + 0.5f * static_cast<float>(i), 1.f);
        g.StrokeEllipse(bx, by, 30.f + 46.f * k, 30.f + 46.f * k, accent.WithAlpha(0.25f * (1.f - k)), 1.5f);
    }
    g.PopLayer();
    g.StrokeRound(r.Inset(0.5f), kRadius - 0.5f, accent.WithAlpha(0.4f), 1.f);
    // The badge's glyph wobbles
    const Rect badge{ bx - 28.f, by - 28.f, 56.f, 56.f };
    g.PushTransform(D2D1::Matrix3x2F::Rotation(8.f * std::sin(t * 2.6f), D2D1::Point2F(badge.CX(), badge.CY())));
    MenuKit::GlyphBadge(ui, badge, glyph, accent, 24.f);
    g.PopTransform();
    const float pw = pill.empty() ? 0.f : g.Measure(pill, Mono(9.f, true)).w + 18.f;
    const float tx = r.x + 104.f, tw = r.R() - tx - (pw > 0.f ? pw + 30.f : 20.f);
    g.Text(title, { tx, r.CY() - 25.f, tw, 24.f }, kTitle, Mono(17.f, true).Ellipsis());
    g.Text(subtitle, { tx, r.CY() + 3.f, tw, 18.f }, kSub, Mono(10.5f).Ellipsis());
    if (pw > 0.f) MenuKit::Chip(ui, r.R() - 20.f - pw, r.CY(), pill, accent);
    ui.KeepAnimatingThrottled();
}

Rect Card(Ui& ui, const Rect& r, Color accent, float pad) {
    ui.g.Border(r, kRadius, Rgba(8, 15, 24, 235), Lerp(Palette::ButtonBorder, accent, 0.18f), 1.f);
    ui.g.FillLinear(r.Inset(1.f).Top(std::min(r.h * 0.5f, 120.f)), kRadius - 1.f, { { 0.f, accent.WithAlpha(0.06f) }, { 1.f, accent.WithAlpha(0.f) } },
                    { 0.f, 0.f }, { 0.f, 1.f });
    return r.Inset(pad);
}

float Caption(Ui& ui, float x, float y, float w, wchar_t glyph, const std::wstring& text, Color accent) {
    ui.g.Border({ x, y, 22.f, 22.f }, 6.f, accent.WithAlpha(0.12f), accent.WithAlpha(0.45f), 1.f);
    Glyph(ui, glyph, { x, y, 22.f, 22.f }, 11.f, accent);
    ui.g.Text(text, { x + 32.f, y, w - 32.f, 22.f }, accent, Mono(11.f, true).Ellipsis());
    return 22.f;
}

void FieldLabel(Ui& ui, const Rect& r, const std::wstring& text, size_t count, size_t limit, Color accent, bool required) {
    auto& g = ui.g;
    const TextStyle label = Mono(10.f, true);
    g.Text(text, r, Rgba(190, 205, 218), label);
    if (required) {
        const float w = g.Measure(text, label).w;
        g.Text(L"*", { r.x + w + 4.f, r.y, 10.f, r.h }, accent, label);
    }
    const bool full = count >= limit;
    g.Text(std::to_wstring(count) + L" / " + std::to_wstring(limit), r, full ? kAmber : Rgba(90, 115, 135), Mono(9.f).Right());
}

void Screenshot(Ui& ui, Id id, const Rect& r, std::optional<std::wstring>& image, Color accent, HWND owner, const std::wstring& title,
                const std::wstring& formats, const std::wstring& attached) {
    auto& g = ui.g;
    const bool has = image.has_value();
    const Interaction it = ui.Interact(id, r);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 14.f, 0.f);
    const float shown = ui.Animate(HashId(id, 2), has ? 1.f : 0.f, 10.f, has ? 1.f : 0.f);
    g.FillRound(r, kRadius, Lerp(Rgba(255, 255, 255, 8), accent.WithAlpha(0.1f), hover));

    // Dashes round the outline, drifting while hovered
    {
        const Color dash = Lerp(Rgba(120, 140, 160, 90), accent, hover);
        const float offset = std::fmod(static_cast<float>(ui.Time()) * 30.f * hover, 16.f);
        if (hover > 0.01f) ui.KeepAnimating();
        const Rect in = r.Inset(1.f);
        constexpr float kC = 12.f;
        for (float x = in.x + kC + offset - 16.f; x < in.R() - kC; x += 16.f) {
            const float a = std::max(x, in.x + kC), b = std::min(x + 8.f, in.R() - kC);
            if (b > a) {
                g.Line(a, in.y, b, in.y, dash, 1.5f);
                g.Line(a, in.B(), b, in.B(), dash, 1.5f);
            }
        }
        for (float y = in.y + kC + offset - 16.f; y < in.B() - kC; y += 16.f) {
            const float a = std::max(y, in.y + kC), b = std::min(y + 8.f, in.B() - kC);
            if (b > a) {
                g.Line(in.x, a, in.x, b, dash, 1.5f);
                g.Line(in.R(), a, in.R(), b, dash, 1.5f);
            }
        }
        Arc(g, in.x + kC, in.y + kC, kC, 3.1415927f, 1.5707963f, dash, 1.5f);
        Arc(g, in.R() - kC, in.y + kC, kC, -1.5707963f, 1.5707963f, dash, 1.5f);
        Arc(g, in.R() - kC, in.B() - kC, kC, 0.f, 1.5707963f, dash, 1.5f);
        Arc(g, in.x + kC, in.B() - kC, kC, 1.5707963f, 1.5707963f, dash, 1.5f);
    }

    const Rect remove{ r.R() - 42.f, r.CY() - 15.f, 30.f, 30.f };
    bool removed = false;
    if (!has) {
        g.PushOpacity(1.f - shown);
        const float bob = -3.f * hover + 1.5f * std::sin(static_cast<float>(ui.Time()) * 2.f);
        ui.KeepAnimatingThrottled();
        const float icx = r.x + 46.f, icy = r.CY() + bob;
        g.FillEllipse(icx, icy, 24.f, 24.f, accent.WithAlpha(0.14f + 0.1f * hover));
        Glyph(ui, Glyphs::Image, { icx - 20.f, icy - 20.f, 40.f, 40.f }, 18.f, accent);
        g.Text(title, { r.x + 84.f, r.CY() - 19.f, r.w - 100.f, 20.f }, kTitle, Mono(11.5f, true).Ellipsis());
        g.Text(formats, { r.x + 84.f, r.CY() + 3.f, r.w - 100.f, 16.f }, kSub, Mono(9.5f).Ellipsis());
        g.PopLayer();
    } else {
        g.PushOpacity(shown);
        ui.PushTranslate(16.f * (1.f - shown), 0.f);
        const Rect thumb{ r.x + 12.f, r.y + 10.f, (r.h - 20.f) * 1.6f, r.h - 20.f };
        if (auto picture = Images::Load(*image, 512)) g.DrawImageRounded(*picture, thumb, 8.f, 1.f, Stretch::UniformToFill);
        else g.FillRound(thumb, 8.f, Rgba(255, 255, 255, 18));
        g.StrokeRound(thumb.Inset(0.5f), 7.5f, accent.WithAlpha(0.4f), 1.f);
        const float tx = thumb.R() + 14.f, tw = remove.x - 10.f - tx;
        Glyph(ui, Glyphs::CheckMark, { tx, r.CY() - 21.f, 16.f, 20.f }, 10.f, kGreen);
        g.Text(Files::FileName(*image), { tx + 22.f, r.CY() - 21.f, tw - 22.f, 20.f }, kTitle, Mono(11.f, true).Ellipsis());
        g.Text(attached, { tx, r.CY() + 3.f, tw, 16.f }, kSub, Mono(9.5f).Ellipsis());
        ui.PopTransform();
        g.PopLayer();
        // Registered after the zone, so it takes the click instead of it
        if (MenuKit::FooterButton(ui, HashId(id, 3), remove, L"", MenuKit::Kind::Danger, true, Glyphs::Cancel)) {
            image.reset();
            removed = true;
        }
    }
    if (it.clicked && !removed && !(has && ui.MouseOver(remove))) {
        const std::wstring file = System::PickFile(owner, L"", { { L"Images", L"*.jpg;*.jpeg;*.png;*.gif;*.webp;*.bmp" } });
        if (!file.empty()) image = file;
    }
}

void Checklist(Ui& ui, const Rect& r, const std::vector<Check>& checks, Color accent, double start, const std::wstring& caption,
               const std::wstring& readyText, const std::wstring& pendingText, const std::wstring& optionalText, const std::wstring& tip) {
    auto& g = ui.g;
    const Rect c = Card(ui, r, accent);
    int required = 0, done = 0;
    for (const auto& check : checks)
        if (!check.optional) {
            ++required;
            if (check.done) ++done;
        }
    const bool ready = done == required;

    // Header + progress ring
    g.Text(caption, { c.x, c.y, c.w - 64.f, 18.f }, accent, Mono(11.f, true));
    g.Text(ready ? readyText : pendingText, { c.x, c.y + 20.f, c.w - 64.f, 16.f }, ready ? kGreen : kSub, Mono(9.5f, ready));
    const float rcx = c.R() - 24.f, rcy = c.y + 18.f;
    const float fraction = ui.Animate(HashId(ui.MakeId("report-progress"), static_cast<int64_t>(required)), required ? static_cast<float>(done) / required : 1.f, 6.f, 0.f);
    g.StrokeEllipse(rcx, rcy, 20.f, 20.f, Rgba(255, 255, 255, 20), 4.f);
    if (fraction > 0.005f) Arc(g, rcx, rcy, 20.f, -1.5707963f, 6.2831853f * fraction, ready ? kGreen : accent, 4.f);
    g.Text(std::to_wstring(done) + L"/" + std::to_wstring(required), { rcx - 20.f, rcy - 10.f, 40.f, 20.f }, kTitle, Mono(10.f, true).Center());

    float y = c.y + 58.f;
    const TextStyle hintStyle = Wrapped(9.5f);
    for (size_t i = 0; i < checks.size(); ++i) {
        const Check& check = checks[i];
        const Id id = ui.MakeId("report-check", static_cast<int64_t>(i));
        const float hintH = g.Measure(check.hint, hintStyle, c.w - 42.f).h;
        if (y + 22.f + hintH > c.B()) break;
        // Each step slides in from the right, one after another
        const float k = Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>((ui.Time() - start - 0.07 * static_cast<double>(i)) / 0.4)));
        if (k < 1.f) ui.KeepAnimating();
        g.PushOpacity(k);
        ui.PushTranslate(18.f * (1.f - k), 0.f);
        const float on = ui.Animate(id, check.done ? 1.f : 0.f, 12.f);
        // A pulse ring when a step gets done
        float& doneAt = ui.Slot(HashId(id, 1), -10.f);
        bool& wasDone = ui.Data<bool>(HashId(id, 2));
        if (check.done && !wasDone) doneAt = static_cast<float>(ui.Time());
        wasDone = check.done;
        const float pulse = 1.f - Anim::Clamp01((static_cast<float>(ui.Time()) - doneAt) / 0.6f);
        if (pulse > 0.f) ui.KeepAnimating();
        const float cx = c.x + 12.f, cy = y + 11.f;
        if (pulse > 0.f) g.StrokeEllipse(cx, cy, 12.f + 12.f * (1.f - pulse), 12.f + 12.f * (1.f - pulse), kGreen.WithAlpha(pulse), 2.f);
        g.FillEllipse(cx, cy, 12.f * (1.f + 0.2f * pulse), 12.f * (1.f + 0.2f * pulse), Lerp(Rgba(255, 255, 255, 14), kGreen, on));
        if (on < 1.f) g.StrokeEllipse(cx, cy, 11.5f, 11.5f, Rgba(160, 180, 200, static_cast<int>(70 * (1.f - on))), 1.f);
        if (on > 0.01f) Glyph(ui, Glyphs::CheckMark, { cx - 12.f, cy - 12.f, 24.f, 24.f }, 10.f, Colors::White.WithAlpha(on));
        if (on < 0.99f) g.Text(std::to_wstring(i + 1), { cx - 12.f, cy - 12.f, 24.f, 24.f }, Rgba(160, 180, 200, static_cast<int>(170 * (1.f - on))), Mono(10.f, true).Center());
        const float tx = c.x + 34.f;
        const TextStyle name = Mono(11.f, true);
        g.Text(check.title, { tx, y, c.w - 38.f, 22.f }, check.done ? Rgba(150, 170, 185) : kTitle, name);
        if (check.optional) {
            const float tw = g.Measure(check.title, name).w;
            const float ow = g.Measure(optionalText, Mono(8.f, true)).w + 14.f;
            const Rect pill{ tx + tw + 8.f, y + 4.f, ow, 15.f };
            if (pill.R() < c.R()) {
                g.FillRound(pill, 7.5f, Rgba(255, 255, 255, 18));
                g.Text(optionalText, pill, kSub, Mono(8.f, true).Center());
            }
        }
        g.Text(check.hint, { tx, y + 22.f, c.w - 38.f, hintH }, Rgba(110, 135, 155), hintStyle);
        // Line to the next step, filling in once this one is done
        if (i + 1 < checks.size()) g.FillRect({ cx - 1.f, cy + 15.f, 2.f, hintH + 12.f }, Lerp(Rgba(255, 255, 255, 22), kGreen.WithAlpha(0.5f), on));
        ui.PopTransform();
        g.PopLayer();
        y += 22.f + hintH + 16.f;
    }

    // Tip at the bottom, when there's room
    const TextStyle tipStyle = Wrapped(9.5f);
    const float th = g.Measure(tip, tipStyle, c.w - 52.f).h;
    const float tipH = th + 22.f;
    if (!tip.empty() && c.B() - y > tipH + 6.f) {
        const Rect box{ c.x, c.B() - tipH, c.w, tipH };
        g.FillRound(box, 10.f, accent.WithAlpha(0.08f));
        Glyph(ui, Glyphs::Info, { box.x + 10.f, box.CY() - 12.f, 24.f, 24.f }, 13.f, accent);
        g.Text(tip, { box.x + 42.f, box.CY() - th * 0.5f, box.w - 52.f, th }, Rgba(170, 190, 205), tipStyle);
    }
}

bool SendButton(Ui& ui, Id id, const Rect& r, const std::wstring& label, Color accent, bool ready, Form::SendState state) {
    auto& g = ui.g;
    const float t = static_cast<float>(ui.Time());
    const bool enabled = state == Form::SendState::Idle;
    const Interaction it = ui.Interact(id, r, enabled);
    if (it.hovered && enabled) ui.SetCursor(IDC_HAND);
    const float hover = ui.Animate(HashId(id, 1), it.hovered && enabled ? 1.f : 0.f, 14.f, 0.f);
    const float press = ui.Animate(HashId(id, 2), it.held && it.hovered ? 1.f : 0.f, 30.f, 0.f);
    const float radius = r.h * 0.5f;

    // The moment it's sent: a green ring bursting out
    float& sentAt = ui.Slot(HashId(id, 3), -10.f);
    bool& wasSent = ui.Data<bool>(HashId(id, 4));
    const bool sent = state == Form::SendState::Sent;
    if (sent && !wasSent) sentAt = t;
    wasSent = sent;
    const float since = t - sentAt;
    if (since >= 0.f && since < 1.f) {
        ui.KeepAnimating();
        const float k = Anim::EaseOutCubic(since);
        g.StrokeRound(r.Inset(-14.f * k), radius + 14.f * k, kGreen.WithAlpha(1.f - since), 3.f);
    }
    const Color bg = sent ? kGreen : accent;
    if ((ready && enabled) || sent) {
        g.GlowRound(r, radius, bg, 16.f, 0.3f + 0.2f * std::sin(t * 3.f) + 0.2f * hover);
        ui.KeepAnimatingThrottled();
    }
    ui.PushScale(1.f + 0.03f * hover - 0.04f * press, r.CX(), r.CY());
    g.FillRound(r, radius, enabled || sent || state == Form::SendState::Sending ? (ready || sent ? bg : Lerp(bg, Palette::ButtonBg, 0.45f)) : bg.WithAlpha(0.5f));
    if (hover > 0.01f) g.FillRound(r, radius, Colors::White.WithAlpha(0.08f * hover));
    const Color fg = ready || sent ? Rgba(3, 17, 13) : Rgba(220, 230, 238);
    const TextStyle text = Mono(12.f, true);
    const float lw = g.Measure(label, text).w + 1.f;
    const float cx = r.CX() - (20.f + 10.f + lw) * 0.5f;
    if (state == Form::SendState::Sending) {
        Arc(g, cx + 10.f, r.CY(), 8.f, t * 6.f, 4.5f, fg, 2.5f);
        ui.KeepAnimating();
    } else {
        // The paper plane takes off on hover
        const float fly = 5.f * hover;
        g.PushTransform(D2D1::Matrix3x2F::Rotation(-18.f * hover, D2D1::Point2F(cx + 10.f, r.CY())) * D2D1::Matrix3x2F::Translation(fly, -fly * 0.6f));
        Glyph(ui, sent ? Glyphs::CheckMark : wchar_t(0xE724), { cx, r.y, 20.f, r.h }, 14.f, fg);
        g.PopTransform();
    }
    g.Text(label, { cx + 30.f, r.y, lw + 2.f, r.h }, fg, text);
    ui.PopTransform();
    return it.clicked && enabled;
}

void StatusPill(Ui& ui, Id id, const Rect& r, const std::wstring& text, Form::Status kind) {
    struct State {
        std::wstring text;
        double at = -10.0;
    };
    auto& s = ui.Data<State>(id);
    if (s.text != text) {
        s.text = text;
        s.at = ui.Time();
    }
    if (text.empty()) return;
    auto& g = ui.g;
    const float k = Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>((ui.Time() - s.at) / 0.4)));
    if (k < 1.f) ui.KeepAnimating();
    const Color c = kind == Form::Status::Success ? kGreen : kind == Form::Status::Error ? kRed : kind == Form::Status::Warning ? kAmber : Palette::Teal;
    const wchar_t glyph = kind == Form::Status::Success ? Glyphs::CheckMark : kind == Form::Status::Info ? Glyphs::Info : Glyphs::Warning;
    TextStyle style = Mono(10.f, true);
    style.ellipsis = true;
    const float pw = std::min(r.w, g.Measure(text, style).w + 52.f);
    g.PushOpacity(k);
    ui.PushTranslate(-24.f * (1.f - k), 0.f);
    const Rect pill{ r.x, r.CY() - 18.f, pw, 36.f };
    g.Border(pill, 18.f, c.WithAlpha(0.14f), c.WithAlpha(0.5f), 1.f);
    Glyph(ui, glyph, { pill.x + 12.f, pill.y, 18.f, pill.h }, 11.f, c);
    g.Text(text, { pill.x + 38.f, pill.y, pw - 50.f, pill.h }, Lerp(c, Palette::Bright, 0.3f), style);
    ui.PopTransform();
    g.PopLayer();
}

} // namespace SpeedrunLauncher::Main::ReportKit
