#include "Seasonal/SeasonalCardArt.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <tuple>
#include "Seasonal/SeasonalArt.h"
#include "Seasonal/SeasonalTheme.h"

// Decorations for the tall chapter cards. Everything stays in the top part of the card (or sits on
// its top edge, sticking out up to ~56 px above it) so the title, playtime and action buttons at
// the bottom are never covered.
namespace SpeedrunLauncher::Seasonal::CardArt {

namespace {

using namespace Art;

constexpr Color SnowCap = Rgba(240, 247, 255);
const Pen SnowEdge{ Rgba(170, 200, 230), 1.0f };

void Halloween(Graphics& g, float w, float s, int variant) {
    using namespace Art::Halloween;
    switch (variant % 6) {
        case 0:
            DrawCobweb(g, 0, 0, s * 1.1f, false, false);
            DrawSpider(g, w - 34 * s, 0, 52 * s, s * 0.95f);
            break;
        case 1:
            DrawWitchHat(g, w - 22 * s, 3, s * 0.9f, 14);
            DrawBat(g, 30 * s, 34 * s, s);
            DrawBat(g, 54 * s, 22 * s, s * 0.7f);
            break;
        case 2:
            DrawCobweb(g, w, 0, s * 1.1f, true, false);
            DrawSpider(g, 26 * s, 0, 30 * s, s * 0.9f);
            DrawSpider(g, 50 * s, 0, 62 * s, s * 0.75f);
            break;
        case 3:
            DrawPumpkin(g, 28 * s, -8 * s, s * 1.15f);
            DrawPumpkin(g, 50 * s, -6 * s, s * 0.8f);
            DrawGhost(g, w - 26 * s, 32 * s, s * 1.1f);
            break;
        case 4:
            DrawCobweb(g, 0, 0, s * 1.0f, false, false);
            DrawWitchHat(g, w - 22 * s, 3, s * 0.9f, 14);
            break;
        default:
            DrawCobweb(g, w, 0, s * 0.9f, true, false);
            DrawBat(g, 28 * s, 30 * s, s);
            DrawPumpkin(g, w * 0.5f, -8 * s, s * 1.05f);
            break;
    }
}

void Summer(Graphics& g, float w, float s, int variant) {
    using namespace Art::Summer;
    switch (variant % 6) {
        case 0:
            DrawSun(g, 6 * s, 6 * s, s * 1.1f);
            DrawSeagull(g, w - 34 * s, 22 * s, s * 1.1f);
            DrawSeagull(g, w - 56 * s, 34 * s, s * 0.8f);
            break;
        case 1:
            DrawBeachBall(g, w - 30 * s, -9 * s, s * 1.15f);
            DrawStarfish(g, 22 * s, 22 * s, s * 1.2f, 15);
            break;
        case 2:
            DrawUmbrella(g, 30 * s, 2, s * 0.7f, -8);
            DrawShell(g, w - 22 * s, 22 * s, s * 1.2f, -20);
            break;
        case 3:
            DrawCrab(g, w - 34 * s, -2 * s, s * 1.15f);
            DrawSeagull(g, 30 * s, 26 * s, s * 1.1f);
            break;
        case 4:
            DrawIceCream(g, w - 24 * s, 26 * s, s * 1.15f);
            DrawStarfish(g, 22 * s, 22 * s, s * 1.1f, -10);
            DrawShell(g, w * 0.5f, -5 * s, s, 0);
            break;
        default:
            DrawSun(g, w - 6 * s, 6 * s, s * 1.1f);
            DrawCrab(g, 34 * s, -2 * s, s * 1.05f);
            break;
    }
}

// Wavy snow drift resting on the card's top edge, with a few icicle-like drips.
void DrawSnowCap(Graphics& g, float w, float s, int variant) {
    // Same shape every frame for a given size: build it once (cards and menu panels redraw at 60 fps)
    static std::map<std::tuple<int, int, int>, ComPtr<ID2D1PathGeometry>> cache;
    const auto key = std::make_tuple(static_cast<int>(std::lround(w * 2.f)), static_cast<int>(std::lround(s * 100.f)), variant);
    if (cache.size() > 64 && !cache.count(key)) cache.clear();   // sizes only change with the window
    auto& geo = cache[key];
    if (!geo) geo = Geometry::Build([&](ID2D1GeometrySink* sink) {
        const float x0 = 2.f, x1 = w - 2.f;
        sink->BeginFigure(D2D1::Point2F(x0, 2.f), D2D1_FIGURE_BEGIN_FILLED);
        // Lumpy top
        constexpr int bumps = 7;
        for (int i = 0; i < bumps; ++i) {
            const float a = x0 + (x1 - x0) * i / bumps;
            const float b = x0 + (x1 - x0) * (i + 1) / bumps;
            sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(a + (b - a) * 0.2f, -5 * s), D2D1::Point2F(a + (b - a) * 0.8f, -5 * s),
                                                D2D1::Point2F(b, -1 * s)));
        }
        sink->AddLine(D2D1::Point2F(x1, 4 * s));
        // Bottom edge with drips
        SeededRandom rnd(variant * 31 + 7);
        double x = x1;
        while (x > x0 + 8 * s) {
            const double step = (8 + rnd.NextDouble() * 10) * s;
            const double nx = std::max<double>(x0, x - step);
            const double depth = rnd.Next(3) == 0 ? (6 + rnd.NextDouble() * 6) * s : (3 + rnd.NextDouble() * 2) * s;
            sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(static_cast<float>(x - step * 0.2), static_cast<float>(depth)),
                                                D2D1::Point2F(static_cast<float>(nx + step * 0.2), static_cast<float>(depth)),
                                                D2D1::Point2F(static_cast<float>(nx), 4 * s)));
            x = nx;
        }
        sink->AddLine(D2D1::Point2F(x0, 4 * s));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    }, D2D1_FILL_MODE_ALTERNATE);
    Pen edge = SnowEdge;
    edge.round = true;
    FillStroke(g, geo.Get(), SnowCap, edge);
}

void Christmas(Graphics& g, float w, float s, int variant) {
    using namespace Art::Christmas;
    DrawSnowCap(g, w, s, variant);
    switch (variant % 6) {
        case 0:
            DrawSantaHat(g, 20 * s, 0, s * 1.1f, -16);
            DrawHolly(g, w - 20 * s, 22 * s, s * 1.2f, 0);
            break;
        case 1:
            DrawOrnament(g, w - 26 * s, 6 * s, 40 * s, s, OrnRed);
            DrawOrnament(g, w - 52 * s, 6 * s, 18 * s, s * 0.8f, OrnGold);
            DrawOrnament(g, 30 * s, 6 * s, 28 * s, s * 0.9f, OrnBlue);
            break;
        case 2:
            DrawCandyCane(g, w - 24 * s, 34 * s, s, 15);
            DrawSnowflake(g, 26 * s, 30 * s, s * 1.2f);
            break;
        case 3:
            DrawSantaHat(g, w - 20 * s, 0, s * 1.1f, 16);
            DrawGingerbread(g, 24 * s, 34 * s, s * 1.2f, -8);
            break;
        case 4:
            DrawHolly(g, 20 * s, 20 * s, s * 1.2f, 180);
            DrawOrnament(g, w - 30 * s, 6 * s, 30 * s, s, OrnPurple);
            DrawOrnament(g, w * 0.5f, 6 * s, 14 * s, s * 0.8f, OrnGold);
            break;
        default:
            DrawGift(g, 26 * s, 1, s * 1.1f, OrnRed);
            DrawGift(g, 48 * s, 1, s * 0.8f, OrnBlue);
            DrawSnowman(g, w - 26 * s, 2, s * 0.8f);
            break;
    }
}

} // namespace

void Draw(Graphics& g, const Rect& card, int variant, float opacity) {
    const Season season = Active();
    if (season == Season::Classic || opacity <= 0.f) return;
    const float w = card.w, h = card.h;
    if (w < 120.f || h < 160.f) return;
    const float s = std::clamp(w / 150.f, 1.f, 2.8f);

    NoColorFilter keep(g);
    if (opacity < 1.f) g.PushOpacity(opacity);
    {
        Push at(g, Translate(card.x, card.y));
        switch (season) {
            case Season::Halloween: Halloween(g, w, s, variant); break;
            case Season::Summer: Summer(g, w, s, variant); break;
            case Season::Christmas: Christmas(g, w, s, variant); break;
            default: break;
        }
    }
    if (opacity < 1.f) g.PopLayer();
}

namespace {

// Stable pseudo-random 0..1 per particle (no per-object state: everything is a function of the clock)
float Hash(int index, int salt) {
    uint32_t x = static_cast<uint32_t>(index) * 374761393u + static_cast<uint32_t>(salt) * 668265263u;
    x = (x ^ (x >> 13)) * 1274126177u;
    return static_cast<float>((x ^ (x >> 16)) & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
}

// Falling snow: mostly soft dots, every few a spinning flake
void Snow(Graphics& g, const Rect& p, double t, int count, float alpha) {
    const auto position = [&](int i, float& x, float& y) {
        const float speed = 16.f + 22.f * Hash(i, 1);
        const float span = p.h + 24.f;
        y = p.y - 12.f + std::fmod(static_cast<float>(t) * speed + Hash(i, 2) * span, span);
        x = p.x + Hash(i, 3) * p.w + std::sin(static_cast<float>(t) * (0.5f + 0.6f * Hash(i, 4)) + i) * 10.f;
    };
    for (int i = 0; i < count; ++i) {
        if (i % 6 == 0) continue;   // flakes below
        float x, y;
        position(i, x, y);
        const float r = 1.1f + 1.7f * Hash(i, 5);
        g.FillEllipse(x, y, r, r, Rgba(240, 247, 255).WithAlpha((0.27f + 0.43f * Hash(i, 6)) * alpha));
    }
    // The flake art has its own pen colour: one shared layer fades all of them
    g.PushOpacity(0.55f * alpha);
    for (int i = 0; i < count; i += 6) {
        float x, y;
        position(i, x, y);
        Art::Christmas::DrawSnowflake(g, x, y, 0.55f + 0.35f * Hash(i, 5));
    }
    g.PopLayer();
}

// Embers rising and flickering, and now and then a bat crossing the panel
void Embers(Graphics& g, const Rect& p, double t, int count, float alpha) {
    for (int i = 0; i < count; ++i) {
        const float speed = 14.f + 20.f * Hash(i, 1);
        const float span = p.h + 24.f;
        const float y = p.B() + 12.f - std::fmod(static_cast<float>(t) * speed + Hash(i, 2) * span, span);
        const float x = p.x + Hash(i, 3) * p.w + std::sin(static_cast<float>(t) * (0.8f + Hash(i, 4)) + i * 1.7f) * 8.f;
        const float flicker = 0.5f + 0.5f * std::sin(static_cast<float>(t) * (4.f + 4.f * Hash(i, 5)) + i);
        // Fade in at the bottom and out near the top
        const float life = std::clamp((p.B() - y) / (p.h * 0.25f), 0.f, 1.f) * std::clamp((y - p.y) / (p.h * 0.35f), 0.f, 1.f);
        const float a = life * (0.25f + 0.5f * flicker) * alpha;
        if (a <= 0.01f) continue;
        const float r = 1.f + 1.4f * Hash(i, 6);
        g.FillEllipse(x, y, r * 3.f, r * 3.f, Rgba(255, 120, 30).WithAlpha(0.12f * a));
        g.FillEllipse(x, y, r, r, (i % 3 ? Rgba(255, 150, 50) : Rgba(190, 120, 255)).WithAlpha(a));
    }
    constexpr double kBatPeriod = 11.0;
    const float phase = static_cast<float>(std::fmod(t, kBatPeriod) / 4.0);   // crosses in 4 s, then waits
    if (phase < 1.f) {
        const float x = p.x - 40.f + (p.w + 80.f) * phase;
        const float y = p.y + p.h * 0.32f + std::sin(phase * 14.f) * 10.f;
        g.PushOpacity(0.6f * std::sin(phase * 3.14159265f) * alpha);
        Art::Halloween::DrawBat(g, x, y, 1.6f, false);
        g.PopLayer();
    }
}

// Bubbles drifting up and sun glints twinkling
void Bubbles(Graphics& g, const Rect& p, double t, int count, float alpha) {
    for (int i = 0; i < count; ++i) {
        const float speed = 10.f + 16.f * Hash(i, 1);
        const float span = p.h + 24.f;
        const float y = p.B() + 12.f - std::fmod(static_cast<float>(t) * speed + Hash(i, 2) * span, span);
        const float x = p.x + Hash(i, 3) * p.w + std::sin(static_cast<float>(t) * (0.6f + Hash(i, 4)) + i) * 9.f;
        const float r = 2.f + 4.f * Hash(i, 5);
        const float life = std::clamp((y - p.y) / (p.h * 0.3f), 0.f, 1.f) * alpha;
        if (life <= 0.01f) continue;
        g.StrokeEllipse(x, y, r, r, Rgba(150, 225, 255).WithAlpha((0.25f + 0.25f * Hash(i, 6)) * life), 1.f);
        g.FillEllipse(x - r * 0.35f, y - r * 0.35f, r * 0.25f, r * 0.25f, Rgba(255, 255, 255).WithAlpha(0.4f * life));
    }
    for (int i = 0; i < count / 3; ++i) {
        const float cycle = 2.2f + 1.6f * Hash(i, 7);
        const float shifted = static_cast<float>(t) + Hash(i, 8) * cycle;
        const float local = std::fmod(shifted, cycle) / cycle;
        const int round = static_cast<int>(shifted / cycle);   // a new spot every cycle
        const float a = std::sin(local * 3.14159265f) * alpha;
        const float x = p.x + Hash(i * 31 + round, 9) * p.w, y = p.y + Hash(i * 31 + round, 10) * p.h;
        const float len = 3.f + 4.f * std::sin(local * 3.14159265f);
        const Color c = Rgba(255, 215, 120).WithAlpha(0.55f * a);
        g.Line(x - len, y, x + len, y, c, 1.f, true);
        g.Line(x, y - len, x, y + len, c, 1.f, true);
    }
}

} // namespace

void DrawPanelParticles(Graphics& g, const Rect& panel, float radius, double time, float opacity) {
    const Season season = Active();
    if (season == Season::Classic || opacity <= 0.f || panel.w < 60.f || panel.h < 60.f) return;
    SetClock(time);
    const int count = std::clamp(static_cast<int>(panel.w * panel.h / 9000.f), 14, 70);
    NoColorFilter keep(g);
    // Axis-aligned clip (no layer); pulled in from the rounded corners so nothing pokes out of them
    const float inset = radius * 0.3f;
    g.PushClip(panel.Inset(inset));
    switch (season) {   // `opacity` scales the colours instead of costing a layer
        case Season::Halloween: Embers(g, panel, time, count, opacity); break;
        case Season::Summer: Bubbles(g, panel, time, count, opacity); break;
        case Season::Christmas: Snow(g, panel, time, count, opacity); break;
        default: break;
    }
    g.PopClip();
}

void DrawPanelEdge(Graphics& g, const Rect& panel, double time, float opacity) {
    const Season season = Active();
    if (season == Season::Classic || opacity <= 0.f || panel.w < 200.f) return;
    SetClock(time);
    const float w = panel.w, s = 1.2f;
    NoColorFilter keep(g);
    if (opacity < 1.f) g.PushOpacity(opacity);
    {
        Push at(g, Translate(panel.x, panel.y));
        switch (season) {
            case Season::Halloween: {
                using namespace Art::Halloween;
                DrawCobweb(g, 0, 0, s * 0.8f, false, false);
                DrawBat(g, 84 * s, -26 * s, s * 1.1f);
                DrawPumpkin(g, w - 112 * s, -8 * s, s * 1.15f);
                DrawPumpkin(g, w - 86 * s, -6 * s, s * 0.8f);
                DrawWitchHat(g, w - 24 * s, 3, s * 0.85f, 14);
                break;
            }
            case Season::Summer: {
                using namespace Art::Summer;
                DrawSun(g, w + 2 * s, -2 * s, s * 0.9f);
                DrawShell(g, 54 * s, -5 * s, s, 0);
                DrawSeagull(g, 120 * s, -30 * s, s * 0.9f);
                DrawCrab(g, w - 120 * s, -2 * s, s);
                DrawBeachBall(g, w * 0.5f, -9 * s, s * 0.9f);
                break;
            }
            case Season::Christmas: {
                using namespace Art::Christmas;
                DrawSnowCap(g, w, s, 3);
                DrawSantaHat(g, 20 * s, 0, s * 1.1f, -16);
                DrawGift(g, w - 110 * s, 1, s, OrnRed);
                DrawGift(g, w - 88 * s, 1, s * 0.75f, OrnBlue);
                DrawSnowman(g, w - 30 * s, 2, s * 0.8f);
                break;
            }
            default: break;
        }
    }
    if (opacity < 1.f) g.PopLayer();
}

} // namespace SpeedrunLauncher::Seasonal::CardArt
