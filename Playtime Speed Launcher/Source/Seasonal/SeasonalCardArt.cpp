#include "Seasonal/SeasonalCardArt.h"
#include <algorithm>
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
    auto geo = Geometry::Build([&](ID2D1GeometrySink* sink) {
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

} // namespace SpeedrunLauncher::Seasonal::CardArt
