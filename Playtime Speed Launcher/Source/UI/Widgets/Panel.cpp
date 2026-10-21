#include "UI/Widgets/Panel.h"
#include <algorithm>
#include <cmath>
#include "Core/Async.h"
#include "Graphics/Animation.h"
#include "Seasonal/SeasonalCardArt.h"
#include "Seasonal/SeasonalTheme.h"

namespace SpeedrunLauncher::UI {

namespace {

constexpr double kFadeIn = 0.20, kScaleIn = 0.32, kFadeOut = 0.16;
constexpr float kEntrance = 1.1f;   // length of the whole opening sequence

// Phase of an animation that starts `delay` s into the opening and lasts `length` s (0..1)
float Phase(float t, float delay, float length) { return Anim::Clamp01((t - delay) / length); }

struct FrameClock {
    double lastDrawn = -10.0;
    double openedAt = -10.0;
};

} // namespace

void Popup::Open(bool withAnimation) {
    if (open) return;
    open = true;
    animated = withAnimation;
    changedAt = Async::Now();
}

void Popup::Close(bool withAnimation) {
    if (!open) return;
    open = false;
    animated = withAnimation;
    changedAt = Async::Now();
}

bool Popup::Visible() const {
    if (open) return true;
    return animated && Async::Now() - changedAt < kFadeOut;
}

float Popup::Opacity() const {
    if (!animated) return open ? 1.f : 0.f;
    const double t = Async::Now() - changedAt;
    if (open) return Anim::EaseOutCubic(static_cast<float>(t / kFadeIn));
    return 1.f - Anim::EaseInCubic(static_cast<float>(t / kFadeOut));
}

float Popup::Scale() const {
    if (!animated) return 1.f;
    const double t = Async::Now() - changedAt;
    if (!open) return Anim::Lerp(1.f, 0.96f, Anim::EaseInCubic(static_cast<float>(t / kFadeOut)));
    return Anim::Lerp(0.92f, 1.f, Anim::EaseOutBack(static_cast<float>(t / kScaleIn), 0.7f));
}

bool Popup::Animating() const {
    if (!animated) return false;
    const double t = Async::Now() - changedAt;
    return open ? t < kScaleIn : t < kFadeOut;
}

OverlayFrame::OverlayFrame(Ui& u, Id frameId, const Rect& screen, float width, float height, const PanelStyle& style, float opacity,
                           float scale)
    : ui(u), id(frameId) {
    float& measured = ui.Slot(HashId(id, 0xB0D7), -1.f);
    const bool unknown = height <= 0.f && measured < 0.f;
    const float bodyH = height > 0.f ? height - style.headerHeight - style.footerHeight : std::max(0.f, measured);
    float panelH = style.headerHeight + bodyH + style.footerHeight;
    panelH = std::min(panelH, screen.h - 16.f);
    const float panelW = std::min(width, screen.w - 16.f);

    // A frame that was hidden for a moment is opening again: restart the entrance
    auto& clock = ui.Data<FrameClock>(HashId(id, 0xC10C));
    const double now = ui.Time();
    if (now - clock.lastDrawn > 0.25) clock.openedAt = now;
    clock.lastDrawn = now;
    openedAt = clock.openedAt;
    const float t = static_cast<float>(now - clock.openedAt);

    if (unknown) opacity = 0.f;   // size not known yet: stay invisible for this one frame
    if (opacity < 1.f) {
        ui.g.PushOpacity(opacity);
        layer = true;
    }
    ui.g.FillRect(screen, style.backdrop);
    ui.Block(screen);

    panel = screen.Center(panelW, panelH);
    if (ui.In().pressed[0] && ui.MouseOver(screen) && !ui.MouseOver(panel)) backdropClicked = true;
    const Color accent = style.titleColor;
    if (style.decorated) PanelBackdropGlow(ui, panel, accent, t);
    const float rise = (1.f - Anim::EaseOutBack(Phase(t, 0.f, 0.42f), 0.6f)) * 26.f;
    if (scale != 1.f || rise != 0.f) {
        ui.PushScale(scale, panel.CX(), panel.CY());
        ui.PushTranslate(0.f, rise);
        scaled = true;
    }
    ui.Block(panel);
    if (style.decorated) PanelGlow(ui, panel, style.radius, accent, t);
    ui.g.Border(panel, style.radius, style.background, style.border, 1.f);
    if (style.decorated)   // faint accent wash from the top
        ui.g.FillLinear(panel.Inset(1.f).Top(std::min(panel.h * 0.5f, 220.f)), style.radius - 1.f,
                        { { 0.f, accent.WithAlpha(0.07f) }, { 1.f, accent.WithAlpha(0.f) } }, { 0.f, 0.f }, { 0.f, 1.f });

    const Rect inner = panel.Inset(1.f);
    header = inner.Top(style.headerHeight);
    footer = inner.Bottom(style.footerHeight);
    body = { inner.x, header.B(), inner.w, footer.y - header.B() };

    if (style.headerHeight > 0.f) {
        ui.g.FillRound(header, style.radius - 1.f, style.radius - 1.f, 0.f, 0.f, style.headerBackground);
        ui.g.FillRect({ header.x, header.B() - 1.f, header.w, 1.f }, style.headerBorder);
        if (!style.title.empty()) {
            // Title drops in, with a soft glow behind it
            const float p = Anim::EaseOutCubic(Phase(t, 0.08f, 0.4f));
            const Rect titleRect = header.Offset(0.f, (1.f - p) * -8.f);
            const TextStyle titleStyle = Mono(style.titleSize, true).Center();
            if (style.decorated) ui.g.GlowText(style.title, titleRect, titleStyle, style.titleColor, 10.f, 0.55f * p);
            ui.g.Text(style.title, titleRect, style.titleColor.Fade(p), titleStyle);
        }
    }
    if (style.footerHeight > 0.f) {
        ui.g.FillRound(footer, 0.f, 0.f, style.radius - 1.f, style.radius - 1.f, style.footerBackground);
        ui.g.FillRect({ footer.x, footer.y, footer.w, 1.f }, style.footerBorder);
    }
    // Seasonal theme: particles in the body (under the content), decorations resting on the top edge.
    // They follow the seasonal clock, so they ease to a stop while a game is running.
    const bool seasonal = style.seasonal && Seasonal::IsSeasonal();
    if (seasonal) Seasonal::CardArt::DrawPanelParticles(ui.g, body, 0.f, Seasonal::LastAnimationTime(), 0.45f);
    if (style.decorated)
        PanelDecor(ui, panel, style.radius, accent, t, style.headerHeight > 0.f ? header.B() - 1.f : -1.f,
                   style.footerHeight > 0.f ? footer.y : -1.f);
    if (seasonal) {
        Seasonal::CardArt::DrawPanelEdge(ui.g, panel, Seasonal::LastAnimationTime());
        if (!Seasonal::AnimationStopped()) ui.KeepAnimatingThrottled();
    }

    // Body content cascades in just after the panel: a soft reveal travels down the body (so rows
    // appear one after another, top first) while everything fades and rises into place
    const float content = Phase(t, 0.10f, 0.55f);
    if (content < 1.f) {
        const float edge = 90.f;
        const float wipeY = Anim::Lerp(body.y, body.B() + edge, Anim::EaseOutCubic(content));
        ID2D1Brush* mask = ui.g.LinearBrush({ { 0.f, Colors::White }, { 1.f, Colors::White.WithAlpha(0.f) } }, { 0.f, wipeY - edge }, { 0.f, wipeY });
        const float fade = Anim::EaseOutCubic(std::min(1.f, content * 1.8f));
        if (mask) ui.g.PushMaskLayer(fade, mask);
        else ui.g.PushOpacity(fade);
        ui.PushTranslate(0.f, (1.f - Anim::EaseOutCubic(content)) * 14.f);
        bodyLayer = true;
    }
    if (unknown || opacity < 1.f || scale != 1.f || PanelEntranceRunning(t)) ui.KeepAnimating();   // open / close animation still running
}

void PanelBackdropGlow(Ui& ui, const Rect& panel, Color accent, float t) {
    // (gradient brushes are cached by their stops, so animated strengths are quantized)
    const float spot = std::round(Anim::EaseOutCubic(Phase(t, 0.f, 0.5f)) * 10.f) / 10.f;
    ui.g.FillRadial(panel.Inset(-panel.w * 0.45f, -panel.h * 0.45f),
                    { { 0.f, accent.WithAlpha(0.10f * spot) }, { 0.55f, accent.WithAlpha(0.04f * spot) }, { 1.f, accent.WithAlpha(0.f) } });
}

void PanelGlow(Ui& ui, const Rect& panel, float radius, Color accent, float t) {
    // Flares while opening, then settles
    const float flare = 1.f - Anim::EaseOutCubic(Phase(t, 0.15f, 0.7f));
    ui.g.Shadow(panel.Offset(0.f, 10.f), radius + 4.f, 26.f, Argb(0x90000000));
    ui.g.GlowRound(panel, radius, accent, 22.f, 0.16f + 0.5f * flare * Phase(t, 0.f, 0.15f));
}

bool PanelEntranceRunning(float t) { return t >= 0.f && t < kEntrance; }

PanelRise::PanelRise(Ui& u, float t, float distance) : ui(u) {
    const float p = Phase(t, 0.f, 0.42f);
    if (p >= 1.f) return;
    ui.g.PushOpacity(Anim::EaseOutCubic(std::min(1.f, p * 2.f)));
    ui.PushTranslate(0.f, (1.f - Anim::EaseOutBack(p, 0.6f)) * distance);
    ui.KeepAnimating();
    active = true;
}

PanelRise::~PanelRise() {
    if (!active) return;
    ui.PopTransform();
    ui.g.PopLayer();
}

void PanelDecor(Ui& ui, const Rect& panel, float radius, Color accent, float t, float headerBottom, float footerTop) {
    auto& g = ui.g;

    // Accent line under the header (and over the footer): sweeps open from the centre
    const float sweep = Anim::EaseOutCubic(Phase(t, 0.12f, 0.5f));
    const auto accentLine = [&](float y, float strength) {
        strength = std::round(strength * 20.f) / 20.f;
        const float w = panel.w * sweep;
        const Rect line{ panel.CX() - w * 0.5f, y, w, 1.f };
        if (line.w < 1.f) return;
        g.FillLinear(line, 0.f, { { 0.f, accent.WithAlpha(0.f) }, { 0.5f, accent.WithAlpha(strength) }, { 1.f, accent.WithAlpha(0.f) } },
                     { 0.f, 0.f }, { 1.f, 0.f });
    };
    // Brighter while sweeping, then it calms down
    const float settle = 1.f - Anim::EaseInOutSine(Phase(t, 0.6f, 0.5f));
    if (headerBottom >= 0.f) accentLine(headerBottom, 0.55f + 0.45f * settle);
    if (footerTop >= 0.f) accentLine(footerTop, 0.25f + 0.3f * settle);

    // Light streak crossing the panel once
    const float streak = Phase(t, 0.18f, 0.75f);
    if (streak > 0.f && streak < 1.f) {
        g.PushRoundClip(panel, radius);
        const float bandW = 140.f;
        const float x = Anim::Lerp(panel.x - bandW - panel.h * 0.4f, panel.R() + panel.h * 0.4f, Anim::EaseInOutCubic(streak));
        g.PushTransform(D2D1::Matrix3x2F::Skew(-22.f, 0.f, { x + bandW * 0.5f, panel.CY() }));
        g.FillLinear({ x, panel.y, bandW, panel.h }, 0.f,
                     { { 0.f, Rgba(255, 255, 255, 0) }, { 0.5f, Rgba(255, 255, 255, 16) }, { 1.f, Rgba(255, 255, 255, 0) } }, { 0.f, 0.f },
                     { 1.f, 0.f });
        g.PopTransform();
        g.PopLayer();
    }

    // Corner marks snap in from outside
    const float snap = Anim::EaseOutBack(Phase(t, 0.2f, 0.45f), 1.2f);
    const float fade = Phase(t, 0.2f, 0.2f);
    if (fade > 0.f) {
        const float off = 5.f + (1.f - snap) * 18.f, len = 14.f;
        const Color c = accent.WithAlpha(0.75f * fade);
        const float l = panel.x - off, r = panel.R() + off, tp = panel.y - off, b = panel.B() + off;
        const auto corner = [&](float x, float y, float dx, float dy) {
            g.Line(x, y, x + dx * len, y, c, 2.f, true);
            g.Line(x, y, x, y + dy * len, c, 2.f, true);
        };
        corner(l, tp, 1.f, 1.f);
        corner(r, tp, -1.f, 1.f);
        corner(l, b, 1.f, -1.f);
        corner(r, b, -1.f, -1.f);
    }
}

OverlayFrame::~OverlayFrame() {
    if (contentHeight >= 0.f) {
        float& measured = ui.Slot(HashId(id, 0xB0D7), -1.f);
        if (std::abs(measured - contentHeight) > 0.5f) {
            measured = contentHeight;
            ui.KeepAnimating();
        }
    }
    if (bodyLayer) {
        ui.PopTransform();
        ui.g.PopLayer();
    }
    if (scaled) {
        ui.PopTransform();
        ui.PopTransform();
    }
    if (layer) ui.g.PopLayer();
}

} // namespace SpeedrunLauncher::UI
