#include "Seasonal/SeasonalBackdrop.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "Graphics/Animation.h"
#include "Seasonal/SeasonalArt.h"
#include "Seasonal/SeasonalTheme.h"

// Full-window seasonal scenery drawn behind the whole launcher UI:
//   Halloween — huge moon with clouds and circling bats, jagged mountains, haunted house, glowing
//               eyes, fog, a graveyard with jack-o'-lanterns, bunting.
//   Summer    — sunset with lit clouds, sun setting over the sea, islands and a sailboat mirrored
//               in the water, a beach with palms, umbrella and the surf lapping, bunting.
//   Christmas — moon and a three-colour aurora, snowy mountains mirrored on a frozen lake, village
//               lights, pine forest, cabin with rising smoke, street lamp, tree, string lights.
// The focal pieces sit in the open sky between the chapter text and the poster, the props on the
// ground next to the chapter cards. Extra life: mouse parallax per layer, shooting stars, lightning
// (Halloween), sea glints (Summer). Everything follows the seasonal clock, so it all eases to a stop
// while a game is running.
namespace SpeedrunLauncher::Seasonal::Backdrop {

namespace {

using namespace Art;
constexpr double Pi = 3.14159265358979323846;

// ── Shared animations ──
float TwinkleA() { return Wave(1.f, 0.2f, 1.7); }
float TwinkleB() { return Wave(0.25f, 1.f, 2.3); }
float TwinkleC() { return Wave(0.9f, 0.35f, 3.1); }
float Pulse() { return Wave(1.f, 0.45f, 4.5); }   // aurora mask, sun shimmer on the sea
// Flickering candle light (haunted house / cabin windows)
Color WindowGlow() {
    const float opacity = Keyframes({ { 0.0, 1.f, false }, { 0.16, 0.6f, false }, { 0.3, 0.95f, false },
                                      { 0.9, 0.75f, false }, { 1.4, 1.f, false }, { 2.6, 1.f, false } });
    return Rgba(255, 176, 60).Fade(opacity);
}
float Sway() { return Wave(-4.f, 4.f, 2.6); }                      // bunting / palm fronds (skew X)
float Drift(float w) { return Loop(0.f, -w, 70.0); }               // fog / waves, repeats every width
float DriftSlow(float w) { return Loop(-w, 0.f, 140.0); }          // clouds

// ── Mouse parallax ──
constexpr float kParallaxX = 18.f, kParallaxY = 8.f;   // shift of the nearest layer at the window edge
constexpr float kBleed = 40.f;                          // shapes reaching the edges extend this far past them
float parallaxX = 0.f, parallaxY = 0.f;                 // -1..1, set by Draw

// Shifts one layer against the mouse: depth 0 = sky (still) … 1 = foreground
class Layer {
public:
    Layer(Graphics& g, float depthX, float depthY = -1.f)
        : push(g, Translate(-parallaxX * depthX * kParallaxX, -parallaxY * (depthY < 0.f ? depthX : depthY) * kParallaxY)) {}

private:
    Push push;
};

// ── Size-dependent geometry, rebuilt when the window size or the season changes ──
struct Star {
    float x, y, r;
};
struct Streak {
    Rect r;
    float opacity;
};

struct Cache {
    float w = 0.f, h = 0.f;
    Season season = Season::Classic;
    std::unordered_map<std::string, ComPtr<ID2D1Geometry>> items;
    std::vector<Star> stars;
    std::unordered_map<std::string, std::vector<Streak>> streaks;
} cache;

ID2D1Geometry* Cached(const std::string& key, const std::function<ComPtr<ID2D1Geometry>()>& build) {
    auto it = cache.items.find(key);
    if (it == cache.items.end()) it = cache.items.emplace(key, build()).first;
    return it->second.Get();
}

template <typename T>
ComPtr<ID2D1Geometry> AsGeometry(const ComPtr<T>& g) {
    ComPtr<ID2D1Geometry> out;
    if (g) g.As(&out);
    return out;
}

D2D1_POINT_2F P(double x, double y) { return D2D1::Point2F(static_cast<float>(x), static_cast<float>(y)); }

// Rolling-hills top edge: a sum of sines
double HillY(double x, double w, double baseY, double amp, double seed) {
    const double t = x / std::max(w, 1.0) * Pi * 2;
    return baseY - amp * (0.55 * std::sin(t * 1.3 + seed) + 0.3 * std::sin(t * 3.1 + seed * 2.1) + 0.15 * std::sin(t * 7.3 + seed * 0.7));
}

// Closed silhouette whose top edge follows HillY, filled down to `bottom`
ID2D1Geometry* Hills(double w, double baseY, double bottom, double amp, double seed) {
    const std::string key = "hills" + std::to_string(baseY) + "/" + std::to_string(amp) + "/" + std::to_string(seed);
    return Cached(key, [=] {
        return AsGeometry(Geometry::Build([&](ID2D1GeometrySink* sink) {
            // Past the window edges so the parallax shift never uncovers a gap
            sink->BeginFigure(P(-kBleed, bottom + kBleed), D2D1_FIGURE_BEGIN_FILLED);
            constexpr int steps = 84;
            for (int i = 0; i <= steps; ++i) {
                const double x = -kBleed + (w + 2.0 * kBleed) * i / steps;
                sink->AddLine(P(x, HillY(x, w, baseY, amp, seed)));
            }
            sink->AddLine(P(w + kBleed, bottom + kBleed));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        }, D2D1_FILL_MODE_ALTERNATE));
    });
}

// Vertical LinearGradientBrush relative to a box (StartPoint 0,0 → EndPoint 0,1)
void FillVertical(Graphics& g, ID2D1Geometry* geo, const D2D1_RECT_F& box, const std::vector<GradientStop>& stops) {
    if (!geo) return;
    auto* brush = g.LinearBrush(stops, { box.left, box.top }, { box.left, box.bottom });
    if (brush) g.FillGeometry(geo, brush);
}

void FillVerticalRect(Graphics& g, const Rect& r, const std::vector<GradientStop>& stops) {
    g.FillLinear(r, 0.f, stops, { 0.f, 0.f }, { 0.f, 1.f });
}

void DrawStars(Graphics& g, float w, float maxY, int count, Color color) {
    if (cache.stars.empty()) {
        SeededRandom rnd(1337);
        for (int i = 0; i < count; ++i) {
            const float x = static_cast<float>(rnd.NextDouble() * w);
            const float y = static_cast<float>(rnd.NextDouble() * maxY);
            const float r = static_cast<float>(0.5 + rnd.NextDouble() * 1.3);
            cache.stars.push_back({ x, y, r });
        }
    }
    const Color twinkles[] = { Colors::White.Fade(TwinkleA()), Colors::White.Fade(TwinkleB()), Colors::White.Fade(TwinkleC()) };
    for (int i = 0; i < static_cast<int>(cache.stars.size()); ++i) {
        const auto [x, y, r] = cache.stars[i];
        const int group = i % 4;
        Ellipse(g, x, y, r, r, group == 3 ? color : twinkles[group]);
        // A few bigger stars get a soft cross sparkle
        if (i % 23 == 0) {
            const Pen pen{ twinkles[i % 3], 0.8f, false };
            Line(g, x - r * 4, y, x + r * 4, y, pen);
            Line(g, x, y - r * 4, x, y + r * 4, pen);
        }
    }
}

// Now and then a shooting star crosses the upper sky (deterministic from the clock: ~2 of every 3
// 6.5 s slots get one), with a fading gradient tail and a small glowing head.
void DrawShootingStars(Graphics& g, float w, float maxY, Color color) {
    constexpr double kSlot = 6.5;
    const double now = Clock();
    const long long slot = static_cast<long long>(std::floor(now / kSlot));
    for (long long k = slot - 1; k <= slot; ++k) {   // one may still be crossing from the previous slot
        if (k < 0) continue;
        SeededRandom rnd(static_cast<int>(k % 100000) * 7919 + 101);
        if (rnd.NextDouble() < 0.35) continue;
        const double start = k * kSlot + rnd.NextDouble() * kSlot * 0.6;
        const double duration = 0.7 + rnd.NextDouble() * 0.5;
        const float x0 = static_cast<float>(w * (0.1 + rnd.NextDouble() * 0.8));
        const float y0 = static_cast<float>(maxY * (0.05 + rnd.NextDouble() * 0.5));
        const double slope = (18.0 + rnd.NextDouble() * 20.0) * Pi / 180.0;
        const bool leftward = rnd.Next(2) == 0;
        const float distance = static_cast<float>(260.0 + rnd.NextDouble() * 220.0);
        const float length = static_cast<float>(90.0 + rnd.NextDouble() * 90.0);
        const double p = (now - start) / duration;
        if (p < 0.0 || p > 1.0) continue;

        const float dx = static_cast<float>(std::cos(slope)) * (leftward ? -1.f : 1.f), dy = static_cast<float>(std::sin(slope));
        const float travel = Anim::EaseOutCubic(static_cast<float>(p)) * distance;
        const float alpha = static_cast<float>(std::sin(Pi * p));
        const D2D1_POINT_2F head{ x0 + dx * travel, y0 + dy * travel };
        const float tail = length * std::min(1.f, static_cast<float>(p) * 3.f);
        const D2D1_POINT_2F end{ head.x - dx * tail, head.y - dy * tail };
        if (auto* brush = g.LinearBrush({ { 0.f, color.WithAlpha(0.f) }, { 1.f, color } }, end, head, alpha))
            g.Context()->DrawLine(end, head, brush, 1.6f);
        if (auto* glow = g.RadialBrush({ { 0.f, color }, { 1.f, color.WithAlpha(0.f) } }, head, 5.f, 5.f, alpha))
            g.Context()->FillEllipse(D2D1::Ellipse(head, 5.f, 5.f), glow);
    }
}

// Halloween lightning: about every other 11 s slot has a strike with a quick double flicker.
// Returns the flash strength (0 = none) and the slot / x of the bolt.
float Lightning(float w, long long& boltSeed, float& boltX) {
    constexpr double kSlot = 11.0;
    const double now = Clock();
    const long long slot = static_cast<long long>(std::floor(now / kSlot));
    SeededRandom rnd(static_cast<int>(slot % 100000) * 4271 + 17);
    if (rnd.NextDouble() < 0.45) return 0.f;
    const double strike = slot * kSlot + 1.0 + rnd.NextDouble() * (kSlot - 3.0);
    boltX = static_cast<float>(w * (0.38 + rnd.NextDouble() * 0.24));   // the open sky between the cards and the hills
    boltSeed = slot;
    const double dt = now - strike;
    if (dt < 0.0 || dt > 1.2) return 0.f;
    float flash = static_cast<float>(std::exp(-dt * 7.0));
    if (dt > 0.16) flash = std::max(flash, 0.75f * static_cast<float>(std::exp(-(dt - 0.16) * 9.0)));
    return flash;
}

void DrawBolt(Graphics& g, float x, float bottom, long long seed, float strength) {
    SeededRandom rnd(static_cast<int>(seed % 100000) * 31 + 5);
    std::vector<D2D1_POINT_2F> main{ { x, -10.f } };
    const int segments = 9;
    for (int i = 1; i <= segments; ++i)
        main.push_back({ main.back().x + static_cast<float>((rnd.NextDouble() - 0.5) * 46.0), -10.f + (bottom + 10.f) * i / segments });
    const size_t forkAt = 3 + static_cast<size_t>(rnd.Next(3));
    std::vector<D2D1_POINT_2F> fork{ main[forkAt] };
    for (int i = 1; i <= 4; ++i)
        fork.push_back({ fork.back().x + static_cast<float>(14.0 + rnd.NextDouble() * 22.0), fork.back().y + static_cast<float>(18.0 + rnd.NextDouble() * 20.0) });
    auto geo = Geometry::Build([&](ID2D1GeometrySink* sink) {
        for (const auto* line : { &main, &fork }) {
            sink->BeginFigure((*line)[0], D2D1_FIGURE_BEGIN_HOLLOW);
            for (size_t i = 1; i < line->size(); ++i) sink->AddLine((*line)[i]);
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
        }
    });
    Stroke(g, geo.Get(), Pen{ Rgba(190, 160, 255).Fade(0.35f * strength), 9.f });   // glow
    Stroke(g, geo.Get(), Pen{ Rgba(245, 240, 255).Fade(strength), 2.4f });          // core
}

// Two copies of a band, one width apart, shifted by `drift` so it loops seamlessly
void DrawLooping(Graphics& g, float w, float drift, const std::function<void(float)>& drawAtOffset) {
    Push shift(g, Translate(drift, 0.f));
    drawAtOffset(0.f);
    drawAtOffset(w);
}

// ── Bunting (Halloween / Summer) ──
void DrawBunting(Graphics& g, float w, float top, const std::vector<Color>& flags, const Pen& stringPen, bool bats) {
    const int segments = std::max(3, static_cast<int>(std::round(w / 320.f)));
    const float segW = w / segments;
    constexpr float sag = 26.f, flagW = 18.f;   // flags are 18 x 24
    const int perSeg = std::max(4, static_cast<int>(segW / 34.f));
    const float sway = Sway();
    static ID2D1PathGeometry* flagShape = Path("M -9,0 L 9,0 L 0,24 Z");

    auto* strings = Cached("bunting", [=] {
        return AsGeometry(Geometry::Build([&](ID2D1GeometrySink* sink) {
            for (int s = 0; s < segments; ++s) {
                const float x0 = s * segW;
                sink->BeginFigure({ x0, top - 2.f }, D2D1_FIGURE_BEGIN_HOLLOW);
                sink->AddQuadraticBezier(D2D1::QuadraticBezierSegment({ x0 + segW / 2.f, top - 2.f + sag * 2.f }, { x0 + segW, top - 2.f }));
                sink->EndFigure(D2D1_FIGURE_END_OPEN);
            }
        }));
    });
    Stroke(g, strings, stringPen);

    for (int s = 0; s < segments; ++s) {
        const float x0 = s * segW;
        const D2D1_POINT_2F start{ x0, top - 2.f }, end{ x0 + segW, top - 2.f }, ctrl{ x0 + segW / 2.f, top - 2.f + sag * 2.f };

        for (int i = 1; i < perSeg; ++i) {
            const float t = static_cast<float>(i) / perSeg;
            const float px = (1 - t) * (1 - t) * start.x + 2 * (1 - t) * t * ctrl.x + t * t * end.x;
            const float py = (1 - t) * (1 - t) * start.y + 2 * (1 - t) * t * ctrl.y + t * t * end.y;
            // Tangent so the flags follow the string's angle
            const float dx = 2 * (1 - t) * (ctrl.x - start.x) + 2 * t * (end.x - ctrl.x);
            const float dy = 2 * (1 - t) * (ctrl.y - start.y) + 2 * t * (end.y - ctrl.y);
            const float angle = static_cast<float>(std::atan2(dy, dx) * 180.0 / Pi);

            Push at(g, Translate(px, py));
            Push rotate(g, Rotate(angle));
            Push swaying(g, SkewX(sway));
            if (bats && (s * perSeg + i) % 5 == 2) {
                // Little bat hanging upside down from the string
                Push flip(g, Scale(1.1f, -1.1f));
                Art::Halloween::DrawBat(g, 0.f, -6.f, 1.f, false);
            } else {
                Fill(g, flagShape, flags[(s * perSeg + i) % flags.size()]);
                Line(g, -flagW / 2, 0.5f, flagW / 2, 0.5f, stringPen);
            }
        }
    }
}

// ── Halloween ───────────────────────────────────────────────────────────────

constexpr Color HwFarHill = Rgba(30, 14, 44);
constexpr Color HwMidHill = Rgba(20, 9, 30);
constexpr Color HwGround = Rgba(11, 5, 17);
constexpr Color HwStone = Rgba(24, 14, 34);
const Pen HwRim{ Rgba(160, 90, 230, 70), 1.2f };
const Pen HwFence{ Rgba(11, 5, 17), 2.4f };

void DrawHauntedHouse(Graphics& g, float x, float baseY, float s) {
    Push at(g, Translate(x, baseY));
    Push size(g, Scale(s));
    Fill(g, Path("M -46,0 L -46,-40 L -54,-40 L -30,-62 L -6,-40 L -14,-40 L -14,0 Z "   // left wing
                 "M -16,0 L -16,-58 L -24,-58 L 4,-92 L 32,-58 L 24,-58 L 24,0 Z "        // main house
                 "M 22,0 L 22,-70 L 18,-70 L 32,-110 L 46,-70 L 42,-70 L 42,0 Z "         // tower
                 "M 0,-80 L 0,-100 L 6,-100 L 6,-86 Z"),                                  // chimney
         HwFarHill);
    const Color glow = WindowGlow();
    constexpr Rect windows[] = {
        { -38, -32, 7, 9 }, { -26, -32, 7, 9 }, { -8, -48, 8, 11 }, { 8, -48, 8, 11 }, { -1, -22, 8, 22 },
        { 28.5f, -62, 7, 10 }, { 28.5f, -40, 7, 10 },
    };
    for (const Rect& r : windows) {
        GlowEllipse(g, r.CX(), r.CY(), r.w * 1.6f, r.h * 1.3f, Rgba(255, 170, 60, 80));
        Rectangle(g, r, glow);
    }
    Ellipse(g, 4.f, -68.f, 4.f, 4.f, glow);
}

// Gnarled bare tree silhouette (recursive branches) with its base at (x, y)
void DrawBareTree(Graphics& g, float x, float y, float height, int seed, Color fill) {
    const std::string key = "tree" + std::to_string(x) + "/" + std::to_string(y) + "/" + std::to_string(height) + "/" + std::to_string(seed);
    auto* geo = Cached(key, [=] {
        SeededRandom rnd(seed);
        return AsGeometry(Geometry::Build([&](ID2D1GeometrySink* sink) {
            std::function<void(double, double, double, double, double, int)> branch =
                [&](double fx, double fy, double angle, double length, double width, int depth) {
                    const double tx = fx + std::cos(angle) * length, ty = fy + std::sin(angle) * length;
                    // Slight bend so branches look gnarled
                    const double bend = (rnd.NextDouble() - 0.5) * length * 0.35;
                    const double nx = -std::sin(angle), ny = std::cos(angle);
                    const double mx = (fx + tx) / 2 + nx * bend, my = (fy + ty) / 2 + ny * bend;
                    const double tip = std::max(0.6, width * 0.62);
                    sink->BeginFigure(P(fx + nx * width / 2, fy + ny * width / 2), D2D1_FIGURE_BEGIN_FILLED);
                    sink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(P(mx + nx * (width + tip) / 4, my + ny * (width + tip) / 4),
                                                                          P(tx + nx * tip / 2, ty + ny * tip / 2)));
                    sink->AddLine(P(tx - nx * tip / 2, ty - ny * tip / 2));
                    sink->AddQuadraticBezier(D2D1::QuadraticBezierSegment(P(mx - nx * (width + tip) / 4, my - ny * (width + tip) / 4),
                                                                          P(fx - nx * width / 2, fy - ny * width / 2)));
                    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                    if (depth <= 0 || length < 4) return;
                    const int children = depth > 3 ? 2 : 2 + rnd.Next(2);
                    for (int i = 0; i < children; ++i) {
                        // Fixed draw order: the layout depends on the sequence of random numbers
                        const double spread = (i - (children - 1) / 2.0) * (0.55 + rnd.NextDouble() * 0.35);
                        const double childAngle = angle + spread + (rnd.NextDouble() - 0.5) * 0.3;
                        const double childLength = length * (0.62 + rnd.NextDouble() * 0.18);
                        branch(tx, ty, childAngle, childLength, tip, depth - 1);
                    }
                };
            const double startAngle = -Pi / 2 + (rnd.NextDouble() - 0.5) * 0.2;
            branch(x, y, startAngle, height * 0.36, height * 0.075, 5);
        }, D2D1_FILL_MODE_ALTERNATE));
    });
    Fill(g, geo, fill);
    // Roots / mound
    Ellipse(g, x, y + 2.f, height * 0.12f, height * 0.025f, fill);
}

void DrawTombstone(Graphics& g, float x, float y, float u) {
    Push at(g, Translate(x, y));
    Push size(g, Scale(u * 1.6f));
    FillStroke(g, Path("M -10,0 L -10,-20 C -10,-32 10,-32 10,-20 L 10,0 Z"), HwStone, HwRim);
    Line(g, -5.f, -18.f, 5.f, -18.f, HwRim);
    Line(g, -4.f, -13.f, 4.f, -13.f, HwRim);
}

void DrawCross(Graphics& g, float x, float y, float u) {
    Push at(g, Translate(x, y));
    Push size(g, Scale(u * 1.6f));
    Push tilt(g, Rotate(-6.f));
    FillStroke(g, Path("M -2.5,0 L -2.5,-22 L -9,-22 L -9,-27 L -2.5,-27 L -2.5,-34 L 2.5,-34 L 2.5,-27 L 9,-27 L 9,-22 L 2.5,-22 L 2.5,0 Z"),
               HwStone, HwRim);
}

void DrawFence(Graphics& g, float x0, float x1, float baseY, float w, float h, float u) {
    const double spacing = 11 * u * 1.4;
    const double height = 34 * u * 1.4;
    std::vector<D2D1_POINT_2F> top;
    for (double x = x0; x <= x1; x += spacing) {
        const double y = HillY(x, w, baseY, h * 0.015, 2.5) + 4;
        Line(g, static_cast<float>(x), static_cast<float>(y), static_cast<float>(x), static_cast<float>(y - height), HwFence);
        top.push_back(P(x, y - height * 0.75));
    }
    // Pointed tips of all the posts, built once
    auto* tips = Cached("fence" + std::to_string(x0), [=] {
        return AsGeometry(Geometry::Build([&](ID2D1GeometrySink* sink) {
            for (double x = x0; x <= x1; x += spacing) {
                const double y = HillY(x, w, baseY, h * 0.015, 2.5) + 4;
                sink->BeginFigure(P(x - 3 * u * 1.4, y - height), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddLine(P(x, y - height - 7 * u * 1.4));
                sink->AddLine(P(x + 3 * u * 1.4, y - height));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            }
        }));
    });
    Fill(g, tips, HwGround);
    const float half = static_cast<float>(height * 0.5);
    for (size_t i = 1; i < top.size(); ++i) {
        Line(g, top[i - 1].x, top[i - 1].y, top[i].x, top[i].y, HwFence);
        Line(g, top[i - 1].x, top[i - 1].y + half, top[i].x, top[i].y + half, HwFence);
    }
}

// ── Summer ──────────────────────────────────────────────────────────────────

constexpr Color SmSilhouette = Rgba(22, 10, 30);
const Pen SmShine{ Rgba(255, 200, 120, 160), 2.2f };
const Pen SmWave{ Rgba(255, 170, 120, 60), 1.4f };

void DrawPalmSilhouette(Graphics& g, float x, float baseY, float s, int lean) {
    Push at(g, Translate(x, baseY));
    Push size(g, Scale(s * lean, s));
    Fill(g, Path("M -3,0 C 0,-20 4,-40 14,-58 L 18,-56 C 9,-38 5,-20 4,0 Z"), SmSilhouette);
    Push top(g, Translate(16.f, -57.f));
    Push sway(g, SkewX(Sway()));
    for (float angle : { -170.f, -140.f, -100.f, -60.f, -25.f, 10.f, 160.f }) {
        Push rotate(g, Rotate(angle));
        Fill(g, Path("M 0,0 C 10,-10 26,-12 40,-2 C 34,-4 30,-2 28,2 C 24,-2 20,-2 18,3 C 14,-1 10,0 8,4 C 6,1 3,0 0,0 Z"), SmSilhouette);
    }
    Ellipse(g, -2.f, 3.f, 3.4f, 3.4f, SmSilhouette);
    Ellipse(g, 3.f, 4.f, 3.4f, 3.4f, SmSilhouette);
}

// ── Christmas ───────────────────────────────────────────────────────────────

constexpr Color XmFarHill = Rgba(36, 58, 82);
constexpr Color XmPine = Rgba(10, 34, 26);
constexpr Color XmPineSnow = Rgba(220, 234, 245, 230);
constexpr Color XmCabin = Rgba(58, 34, 28);
constexpr Color XmRoof = Rgba(235, 244, 252);

void DrawAurora(Graphics& g, float w, float h, float y, Color color, int alpha, double phase) {
    const std::string key = "aurora" + std::to_string(phase);
    auto* geo = Cached(key, [=] {
        return AsGeometry(Geometry::Build([&](ID2D1GeometrySink* sink) {
            constexpr int steps = 60;
            D2D1_POINT_2F top[steps + 1];
            for (int i = 0; i <= steps; ++i) {
                const double x = static_cast<double>(w) * i / steps;
                const double t = static_cast<double>(i) / steps * Pi * 2;
                top[i] = P(x, y + std::sin(t * 1.5 + phase) * h * 0.05 + std::sin(t * 3.7 + phase) * h * 0.015);
            }
            sink->BeginFigure(top[0], D2D1_FIGURE_BEGIN_FILLED);
            for (int i = 1; i <= steps; ++i) sink->AddLine(top[i]);
            for (int i = steps; i >= 0; --i) sink->AddLine(P(top[i].x, top[i].y + h * (0.08 + 0.04 * std::sin(i * 0.3 + phase))));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        }, D2D1_FILL_MODE_ALTERNATE));
    });
    if (geo) {
        D2D1_RECT_F bounds{};
        geo->GetBounds(nullptr, &bounds);
        FillVertical(g, geo, bounds, { { 0.f, color.WithAlpha(0.f) }, { 0.45f, color.WithAlpha(alpha / 255.f) }, { 1.f, color.WithAlpha(0.f) } });
    }

    // Vertical "curtain" streaks (layout computed once per size)
    auto& streaks = cache.streaks[key];
    if (streaks.empty()) {
        SeededRandom rnd(static_cast<int>(phase * 100) + 3);
        for (double x = 0.0; x < w; x += 7 + rnd.NextDouble() * 9) {
            const double t = x / w * Pi * 2;
            const double top = y + std::sin(t * 1.5 + phase) * h * 0.05 + std::sin(t * 3.7 + phase) * h * 0.015;
            const double len = h * (0.05 + rnd.NextDouble() * 0.07);
            const int a = static_cast<int>(static_cast<unsigned char>(alpha * (0.4 + rnd.NextDouble() * 0.6)));
            const double width = 2 + rnd.NextDouble() * 2.5;
            streaks.push_back({ { static_cast<float>(x), static_cast<float>(top), static_cast<float>(width), static_cast<float>(len) }, a / 255.f });
        }
    }
    // Every streak is the same gradient (color → transparent, top to bottom) at its own opacity
    auto* brush = g.LinearBrush({ { 0.f, color }, { 1.f, color.WithAlpha(0.f) } }, { 0.f, 0.f }, { 0.f, 1.f });
    if (!brush) return;
    for (const auto& s : streaks) {
        brush->SetStartPoint({ s.r.x, s.r.y });
        brush->SetEndPoint({ s.r.x, s.r.B() });
        brush->SetOpacity(s.opacity);
        g.Context()->FillRectangle(s.r.D2D(), brush);
    }
    brush->SetOpacity(1.f);
}

void DrawPine(Graphics& g, float x, float baseY, float s, bool snowy) {
    Push at(g, Translate(x, baseY));
    Push size(g, Scale(s));
    Rectangle(g, { -1.5f, -4.f, 3.f, 4.f }, XmCabin);
    Fill(g, Path("M 0,-34 L 7,-23 L 4,-23 L 10,-13 L 6,-13 L 12,-3 L -12,-3 L -6,-13 L -10,-13 L -4,-23 L -7,-23 Z"), XmPine);
    if (snowy)
        Fill(g, Path("M 0,-34 L 3.5,-28.5 C 1,-27.5 -1,-28.5 -3.5,-28.5 Z "
                     "M -4,-23 L 4,-23 L 6,-19.5 C 2,-18.5 -2,-20 -6,-19.5 Z "
                     "M -6,-13 L 6,-13 L 8,-9.5 C 3,-8.5 -3,-10 -8,-9.5 Z"),
             XmPineSnow);
}

void DrawCabin(Graphics& g, float x, float baseY, float s) {
    Push at(g, Translate(x, baseY));
    Push size(g, Scale(s));
    Rectangle(g, { 11, -42, 6, 12 }, XmCabin);
    Rectangle(g, { -22, -24, 44, 24 }, XmCabin);
    Fill(g, Path("M -27,-22 L 0,-40 L 27,-22 L 24,-19 L 0,-35 L -24,-19 Z"), XmRoof);
    const Color glow = WindowGlow();
    for (const Rect& r : { Rect{ -16, -17, 8, 8 }, Rect{ 8, -17, 8, 8 } }) {
        GlowRect(g, { r.x - 7, r.y - 7, r.w + 14, r.h + 14 }, Rgba(255, 190, 90, 110));
        Rectangle(g, r, glow);
    }
    Rectangle(g, { -3.5f, -13, 7, 13 }, Rgba(30, 18, 14));
}

// ── Shared scenery pieces (new layout) ──────────────────────────────────────
// The main screen leaves the sky in the middle open (between the chapter text on the left and the
// poster on the right) and the ground at the bottom right (next to the chapter cards): the focal
// pieces (moon, sun, aurora) go in that open sky and the props on that ground.

// Mirrors what's drawn after it around the horizontal line y (reflections on water / ice)
M Mirror(float y) { return M::Scale(1.f, -1.f) * M::Translation(0.f, 2.f * y); }

// Jagged mountain ridge: sharp peaks (1 - |sin|) over a few octaves
double RidgeY(double x, double w, double baseY, double amp, double seed) {
    const double t = x / std::max(w, 1.0) * Pi * 2;
    const double a = 1.0 - std::abs(std::sin(t * 1.6 + seed));
    const double b = 1.0 - std::abs(std::sin(t * 4.3 + seed * 1.7));
    const double c = std::sin(t * 13.0 + seed * 0.5);
    return baseY - amp * (0.62 * std::pow(a, 1.4) + 0.3 * b + 0.04 * c);
}

ID2D1Geometry* Ridge(double w, double baseY, double bottom, double amp, double seed) {
    const std::string key = "ridge" + std::to_string(baseY) + "/" + std::to_string(amp) + "/" + std::to_string(seed);
    return Cached(key, [=] {
        return AsGeometry(Geometry::Build([&](ID2D1GeometrySink* sink) {
            sink->BeginFigure(P(-kBleed, bottom + kBleed), D2D1_FIGURE_BEGIN_FILLED);
            constexpr int steps = 180;
            for (int i = 0; i <= steps; ++i) {
                const double x = -kBleed + (w + 2.0 * kBleed) * i / steps;
                sink->AddLine(P(x, RidgeY(x, w, baseY, amp, seed)));
            }
            sink->AddLine(P(w + kBleed, bottom + kBleed));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        }, D2D1_FILL_MODE_ALTERNATE));
    });
}

// A mountain range: rock gradient, then snow on the peaks above the snowline (0 = highest peak,
// 1 = the range's base; a transparent `snow` leaves them bare)
void DrawRange(Graphics& g, float w, float baseY, float bottom, float amp, float seed, Color top, Color base, Color snow, float snowline) {
    auto* geo = Ridge(w, baseY, bottom, amp, seed);
    const D2D1_RECT_F box{ 0.f, baseY - amp, w, baseY };
    FillVertical(g, geo, box, { { 0.f, top }, { 1.f, base } });
    if (!snow.Transparent())
        FillVertical(g, geo, box, { { 0.f, snow }, { snowline, snow.Fade(0.8f) }, { snowline + 0.08f, snow.WithAlpha(0.f) }, { 1.f, snow.WithAlpha(0.f) } });
}

// Soft horizontal band of haze (between mountain layers, so they read as far apart)
void Haze(Graphics& g, float w, float y, float height, Color color) {
    FillVerticalRect(g, { -kBleed, y, w + 2.f * kBleed, height }, { { 0.f, color.WithAlpha(0.f) }, { 0.5f, color }, { 1.f, color.WithAlpha(0.f) } });
}

// Darkens the left side (under the chapter title / PLAY button) and the very top, so the scenery
// never fights with the text
void Vignette(Graphics& g, float w, float h, Color dark) {
    g.FillLinear({ 0.f, 0.f, w * 0.52f, h }, 0.f, { { 0.f, dark.WithAlpha(0.62f) }, { 0.55f, dark.WithAlpha(0.3f) }, { 1.f, dark.WithAlpha(0.f) } },
                 { 0.f, 0.f }, { 1.f, 0.f });
    g.FillLinear({ 0.f, 0.f, w, h * 0.16f }, 0.f, { { 0.f, dark.WithAlpha(0.4f) }, { 1.f, dark.WithAlpha(0.f) } }, { 0.f, 0.f }, { 0.f, 1.f });
}

// y of a cubic Bézier at a given x (x grows along the curve)
float BezierYAtX(D2D1_POINT_2F p0, D2D1_POINT_2F p1, D2D1_POINT_2F p2, D2D1_POINT_2F p3, float x) {
    const auto at = [&](float t, float a, float b, float c, float d) {
        const float u = 1.f - t;
        return u * u * u * a + 3.f * u * u * t * b + 3.f * u * t * t * c + t * t * t * d;
    };
    float lo = 0.f, hi = 1.f;
    for (int i = 0; i < 24; ++i) {
        const float mid = (lo + hi) * 0.5f;
        if (at(mid, p0.x, p1.x, p2.x, p3.x) < x) lo = mid;
        else hi = mid;
    }
    return at((lo + hi) * 0.5f, p0.y, p1.y, p2.y, p3.y);
}

// ── Halloween ───────────────────────────────────────────────────────────────

// Glowing eyes peeking out of the dark: each pair fades in and out slowly and blinks now and then
void DrawEyes(Graphics& g, float x, float y, float u, int i) {
    const double now = Clock();
    const float visible = std::max(0.f, static_cast<float>(std::sin(now * 0.35 + i * 2.1)));
    if (visible < 0.02f) return;
    const double period = 3.2 + i * 0.7;
    const bool closed = std::fmod(now + i * 1.3, period) < 0.14;
    if (closed) return;
    const Color eye = Rgba(255, 214, 70).Fade(visible);
    GlowEllipse(g, x, y, 14.f * u, 7.f * u, Rgba(255, 190, 40, 60).Fade(visible));
    Ellipse(g, x - 4.f * u, y, 2.f * u, 1.3f * u, eye);
    Ellipse(g, x + 4.f * u, y, 2.f * u, 1.3f * u, eye);
}

void DrawHalloween(Graphics& g, float w, float h, float garlandTop) {
    const float u = h / 1000.f;
    FillVerticalRect(g, { 0, 0, w, h }, { { 0.f, Rgba(6, 3, 12) }, { 0.38f, Rgba(22, 9, 38) }, { 0.6f, Rgba(62, 20, 66) },
                                          { 0.71f, Rgba(130, 44, 54) }, { 0.78f, Rgba(44, 15, 42) }, { 1.f, Rgba(12, 5, 16) } });
    {
        Layer layer(g, 0.12f);
        DrawStars(g, w, h * 0.6f, 170, Rgba(230, 210, 255));
        DrawShootingStars(g, w, h * 0.45f, Rgba(235, 220, 255));
    }

    // The moon: huge, in the open sky, with a breathing halo, clouds sliding across it and a flock of
    // bats circling it (the ones on the far side of their loop pass behind it)
    const float moonR = std::clamp(h * 0.15f, 70.f, 190.f);
    const float mx = w * 0.6f, my = h * 0.3f;
    {
        Layer layer(g, 0.2f);
        const double now = Clock();
        const float breathe = 1.f + 0.06f * static_cast<float>(std::sin(now * 0.8));
        GlowEllipse(g, mx, my, moonR * 4.2f * breathe, moonR * 3.2f * breathe, Rgba(255, 150, 90, 46));
        GlowEllipse(g, mx, my, moonR * 2.2f, moonR * 2.2f, Rgba(255, 200, 130, 50));
        const auto bats = [&](bool behind) {
            for (int i = 0; i < 7; ++i) {
                const double a = now * (0.32 + i * 0.045) + i * 0.9;
                if ((std::sin(a) < 0.0) != behind) continue;
                const float rx = moonR * (1.3f + 0.14f * (i % 3)), ry = moonR * (0.5f + 0.12f * (i % 2));
                const float x = mx + static_cast<float>(std::cos(a)) * rx, y = my - moonR * 0.1f + static_cast<float>(std::sin(a)) * ry;
                const float s = moonR / 70.f * (behind ? 0.6f : 0.85f) * (0.75f + 0.15f * (i % 3));
                Art::Halloween::DrawBat(g, x, y, s);
            }
        };
        bats(true);
        Art::Halloween::DrawMoon(g, mx, my, moonR);
        // Clouds crossing the moon: dark bodies with a faint lit edge
        DrawLooping(g, w, Loop(0.f, -w, 160.0), [&](float ox) {
            const struct { float fx, fy, rx, ry; } clouds[] = {
                { 0.58f, -0.35f, 0.13f, 0.016f }, { 0.9f, 0.45f, 0.1f, 0.013f }, { 0.25f, 0.1f, 0.12f, 0.015f }, { 0.7f, 0.75f, 0.09f, 0.012f },
            };
            for (const auto& c : clouds) {
                const float cx = ox + w * c.fx, cy = my + moonR * c.fy;
                GlowEllipse(g, cx, cy - h * 0.006f, w * c.rx, h * c.ry, Rgba(220, 150, 230, 34));
                GlowEllipse(g, cx, cy, w * c.rx, h * c.ry, Rgba(28, 10, 40, 200));
            }
        });
        bats(false);
    }

    // Lightning: the sky flashes and a bolt strikes behind the mountains
    long long boltSeed = 0;
    float boltX = 0.f;
    if (const float flash = Lightning(w, boltSeed, boltX); flash > 0.f) {
        g.FillRect({ 0, 0, w, h }, Rgba(190, 170, 255).Fade(0.24f * flash));
        if (flash > 0.2f) {
            Layer layer(g, 0.3f);
            DrawBolt(g, boltX, h * 0.7f, boltSeed, flash);
        }
    }

    // Far jagged mountains, then haze
    {
        Layer layer(g, 0.35f);
        DrawRange(g, w, h * 0.72f, h, h * 0.17f, 1.9f, Rgba(52, 22, 70), Rgba(26, 10, 38), Colors::Transparent, 0.f);
        Haze(g, w, h * 0.66f, h * 0.1f, Rgba(170, 80, 200, 40));
    }
    // Hills with the haunted house standing against the moonlight, and bare trees
    const float midBase = h * 0.8f;
    {
        Layer layer(g, 0.55f);
        const float houseX = w * 0.7f;
        DrawHauntedHouse(g, houseX, static_cast<float>(HillY(houseX, w, midBase, h * 0.04, 1.2)) + 8.f, h / 430.f);
        FillStroke(g, Hills(w, midBase, h, h * 0.04, 1.2), HwMidHill, HwRim);
        const struct { float fx, height; int seed; } trees[] = { { 0.47f, 0.13f, 8 }, { 0.56f, 0.1f, 5 }, { 0.82f, 0.15f, 3 } };
        for (const auto& t : trees) {
            const float x = w * t.fx;
            DrawBareTree(g, x, static_cast<float>(HillY(x, w, midBase, h * 0.04, 1.2)) + 4.f, h * t.height, t.seed, HwGround);
        }
        const struct { float fx, fy; } eyes[] = { { 0.5f, 0.815f }, { 0.64f, 0.8f }, { 0.86f, 0.81f }, { 0.77f, 0.83f } };
        for (int i = 0; i < 4; ++i) DrawEyes(g, w * eyes[i].fx, h * eyes[i].fy, u, i);
    }
    // Fog rolling through the valley
    {
        Layer layer(g, 0.75f);
        DrawLooping(g, w, Drift(w), [&](float ox) {
            for (int i = 0; i < 7; ++i)
                GlowEllipse(g, ox + w * (0.05f + i * 0.15f), h * (0.84f + (i % 2) * 0.03f), w * 0.13f, h * 0.05f, Rgba(190, 150, 235, 40));
        });
    }
    // Graveyard on the ground next to the chapter cards: tombstones, crosses, a fence, glowing
    // jack-o'-lanterns and a big dead tree on the right edge
    const float groundBase = h * 0.92f;
    {
        Layer layer(g, 1.f);
        FillStroke(g, Hills(w, groundBase, h, h * 0.018, 2.5), HwGround, HwRim);
        const auto groundY = [&](float x) { return static_cast<float>(HillY(x, w, groundBase, h * 0.018, 2.5)); };
        DrawBareTree(g, w * 0.985f, groundY(w * 0.985f) + 4.f, h * 0.36f, 11, HwGround);
        int k = 0;
        for (double fx : { 0.66, 0.7, 0.72, 0.81, 0.87, 0.925 }) {
            const float x = static_cast<float>(w * fx);
            if (k++ % 3 == 2) DrawCross(g, x, groundY(x) + 6 * u, u * 1.2f);
            else DrawTombstone(g, x, groundY(x) + 6 * u, u * 1.2f);
        }
        DrawFence(g, w * 0.735f, w * 0.795f, groundBase, w, h, u * 1.15f);
        const double now = Clock();
        const struct { float fx, sc; } pumpkins[] = { { 0.685f, 2.4f }, { 0.84f, 1.8f }, { 0.9f, 2.1f }, { 0.96f, 2.8f } };
        int i = 0;
        for (const auto& p : pumpkins) {
            const float x = w * p.fx, y = groundY(x) - 6 * u;
            const float flicker = 0.8f + 0.2f * static_cast<float>(std::sin(now * 9.0 + i * 1.7) * std::sin(now * 5.3 + i));
            GlowEllipse(g, x, y - 4 * u, 40 * u * p.sc / 2.f, 26 * u * p.sc / 2.f, Rgba(255, 130, 30, 90).Fade(flicker));
            Art::Halloween::DrawPumpkin(g, x, y, p.sc * u * 1.25f);
            ++i;
        }
        // Low ground fog in front
        DrawLooping(g, w, Loop(0.f, -w, 50.0), [&](float ox) {
            for (int j = 0; j < 6; ++j) GlowEllipse(g, ox + w * (0.1f + j * 0.18f), h * 0.97f, w * 0.12f, h * 0.035f, Rgba(200, 170, 240, 34));
        });
    }

    Vignette(g, w, h, Rgba(6, 3, 12));
    DrawBunting(g, w, garlandTop,
                { Rgba(255, 122, 26), Rgba(106, 47, 181), Rgba(30, 16, 44), Rgba(255, 167, 77), Rgba(155, 93, 229) },
                Pen{ Rgba(80, 50, 110), 1.2f }, true);
}

// ── Summer ──────────────────────────────────────────────────────────────────

void DrawSailboat(Graphics& g, float x, float y, float s, Color color) {
    Push at(g, Translate(x, y));
    Push size(g, Scale(s));
    Fill(g, Path("M -14,0 L 14,0 L 10,4 L -10,4 Z"), color);
    Line(g, 0.f, 0.f, 0.f, -27.f, Pen{ color, 1.f });
    Fill(g, Path("M 1,-26 L 1,-2 L 14,-2 Z"), color);
    Fill(g, Path("M -1,-22 L -1,-2 L -11,-2 Z"), color);
}

void DrawSummer(Graphics& g, float w, float h, float garlandTop) {
    const float u = h / 1000.f;
    const float horizon = h * 0.66f;
    // Sunset sky down to the horizon, then the sea
    FillVerticalRect(g, { 0, 0, w, horizon }, { { 0.f, Rgba(12, 14, 50) }, { 0.3f, Rgba(54, 30, 98) }, { 0.58f, Rgba(150, 52, 118) },
                                                { 0.8f, Rgba(240, 96, 104) }, { 0.93f, Rgba(255, 152, 88) }, { 1.f, Rgba(255, 204, 120) } });
    FillVerticalRect(g, { 0, horizon, w, h - horizon }, { { 0.f, Rgba(250, 150, 104) }, { 0.12f, Rgba(170, 70, 108) },
                                                          { 0.5f, Rgba(52, 30, 84) }, { 1.f, Rgba(12, 16, 44) } });
    {
        Layer layer(g, 0.12f);
        DrawStars(g, w, h * 0.28f, 80, Rgba(255, 240, 220));
        DrawShootingStars(g, w, h * 0.25f, Rgba(255, 245, 230));
    }

    // Long clouds lit from below, drifting slowly
    {
        Layer layer(g, 0.25f);
        DrawLooping(g, w, DriftSlow(w), [&](float ox) {
            const struct { float fx, fy, rx, ry; Color c; } clouds[] = {
                { 0.15f, 0.16f, 0.16f, 0.012f, Rgba(120, 70, 150, 90) },  { 0.55f, 0.12f, 0.2f, 0.014f, Rgba(110, 60, 140, 90) },
                { 0.82f, 0.3f, 0.15f, 0.013f, Rgba(255, 120, 150, 90) },  { 0.35f, 0.38f, 0.18f, 0.014f, Rgba(255, 130, 140, 100) },
                { 0.7f, 0.5f, 0.22f, 0.013f, Rgba(255, 170, 120, 120) },  { 0.1f, 0.56f, 0.16f, 0.01f, Rgba(255, 180, 120, 110) },
                { 0.95f, 0.6f, 0.14f, 0.009f, Rgba(255, 200, 140, 120) },
            };
            for (const auto& c : clouds) {
                GlowEllipse(g, ox + w * c.fx, h * c.fy, w * c.rx, h * c.ry, c.c);
                GlowEllipse(g, ox + w * (c.fx + 0.04f), h * c.fy - h * 0.006f, w * c.rx * 0.6f, h * c.ry * 0.8f, c.c.Fade(0.8f));
            }
        });
    }

    // The sun setting in the open sky: banded disc cut by the horizon, a big glow that breathes
    const float sunR = std::clamp(h * 0.14f, 80.f, 190.f);
    const float sx = w * 0.6f;
    {
        Layer sunLayer(g, 0.2f, 0.f);
        const float breathe = 1.f + 0.05f * static_cast<float>(std::sin(Clock() * 0.9));
        GlowEllipse(g, sx, horizon, sunR * 4.f * breathe, sunR * 2.4f * breathe, Rgba(255, 170, 70, 110));
        auto* sun = Cached("sun", [=] {
            auto disc = Geometry::Ellipse(sx, horizon + sunR * 0.2f, sunR, sunR);
            std::vector<ComPtr<ID2D1RectangleGeometry>> rects;
            std::vector<ID2D1Geometry*> parts;
            for (int i = 0; i < 6; ++i) {
                const float bh = 3.f + i * 2.4f;
                const float by = horizon - sunR * 0.72f + i * sunR * 0.16f;
                rects.push_back(Geometry::Rectangle(D2D1::RectF(sx - sunR, by, sx + sunR, by + bh)));
                parts.push_back(rects.back().Get());
            }
            ComPtr<ID2D1GeometryGroup> bands;
            Device::Get().D2DFactory()->CreateGeometryGroup(D2D1_FILL_MODE_ALTERNATE, parts.data(), static_cast<UINT32>(parts.size()), &bands);
            return AsGeometry(Geometry::Build([&](ID2D1GeometrySink* sink) {
                if (disc && bands) disc->CombineWithGeometry(bands.Get(), D2D1_COMBINE_MODE_EXCLUDE, nullptr, sink);
            }, D2D1_FILL_MODE_ALTERNATE));
        });
        if (sun) {
            D2D1_RECT_F bounds{};
            sun->GetBounds(nullptr, &bounds);
            g.PushClip({ 0, 0, w, horizon });
            FillVertical(g, sun, bounds, { { 0.f, Rgba(255, 240, 150) }, { 1.f, Rgba(255, 90, 96) } });
            g.PopClip();
        }

        // Its reflection: a shimmering column of lines, and glints flashing around it
        const float pulse = Pulse();
        for (int i = 0; i < 12; ++i) {
            const float y = horizon + 6 + i * (h - horizon) / 14;
            const float half = sunR * (0.95f - i * 0.05f) * (1.f + 0.08f * static_cast<float>(std::sin(Clock() * 1.3 + i)));
            const float fade = 1.f - i * 0.07f;
            Line(g, sx - half, y, sx + half, y, Pen{ Rgba(255, 214, 150).Fade(pulse * fade), 2.4f, false });
            Line(g, sx - half * 0.5f, y + 3, sx + half * 0.6f, y + 3, Pen{ SmShine.color.Fade(fade), SmShine.width });
        }
        SeededRandom rnd(4242);
        const double now = Clock();
        for (int i = 0; i < 50; ++i) {
            const double depth = std::pow(rnd.NextDouble(), 1.3);
            const float gy = horizon + 6.f + static_cast<float>(depth) * (h - horizon - 10.f);
            const float gx = sx + static_cast<float>((rnd.NextDouble() * 2.0 - 1.0) * sunR * (0.8 + depth * 1.8));
            const double period = 1.6 + rnd.NextDouble() * 2.2, phase = rnd.NextDouble();
            const float size = static_cast<float>(2.0 + rnd.NextDouble() * 3.0);
            const float wave = static_cast<float>(std::sin((now / period + phase) * Pi * 2.0));
            if (wave <= 0.f) continue;
            const float a = std::pow(wave, 8.f);
            if (a < 0.02f) continue;
            const Pen pen{ Rgba(255, 244, 210).Fade(a), 1.f, true };
            Line(g, gx - size, gy, gx + size, gy, pen);
            Line(g, gx, gy - size * 0.6f, gx, gy + size * 0.6f, pen);
        }
    }

    // Distant islands on the horizon, mirrored faintly in the sea
    auto* islands = Cached("islands-far", [=] {
        return AsGeometry(Geometry::Build([&](ID2D1GeometrySink* sink) {
            const struct { float cx, half, height; } bumps[] = { { 0.2f, 0.12f, 0.05f }, { 0.31f, 0.06f, 0.025f }, { 0.82f, 0.1f, 0.06f }, { 0.93f, 0.07f, 0.03f } };
            for (const auto& b : bumps) {
                const double cx = w * b.cx, half = w * b.half, top = horizon - h * b.height;
                sink->BeginFigure(P(cx - half, horizon + 1), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddBezier(D2D1::BezierSegment(P(cx - half * 0.5, top + (horizon - top) * 0.2), P(cx - half * 0.15, top), P(cx + half * 0.1, top)));
                sink->AddBezier(D2D1::BezierSegment(P(cx + half * 0.4, top), P(cx + half * 0.6, top + (horizon - top) * 0.5), P(cx + half, horizon + 1)));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            }
        }, D2D1_FILL_MODE_WINDING));
    });
    {
        Layer layer(g, 0.4f, 0.f);
        Fill(g, islands, Rgba(92, 38, 92));
        Push mirror(g, Mirror(horizon));
        g.PushOpacity(0.28f);
        Fill(g, islands, Rgba(92, 38, 92));
        g.PopLayer();
    }

    // A sailboat crossing the horizon very slowly
    {
        Layer layer(g, 0.45f, 0.f);
        const double p = std::fmod(Clock() / 260.0 + 0.35, 1.0);
        const float bx = -80.f + static_cast<float>(p) * (w + 160.f);
        const float bob = 1.2f * static_cast<float>(std::sin(Clock() * 1.3));
        DrawSailboat(g, bx, horizon + 3.f * u + bob, u * 1.6f, Rgba(60, 24, 66));
        Push mirror(g, Mirror(horizon + 3.f * u + bob + 4.f * u * 1.6f));
        g.PushOpacity(0.25f);
        DrawSailboat(g, bx, horizon + 3.f * u + bob, u * 1.6f, Rgba(60, 24, 66));
        g.PopLayer();
    }

    // Waves rolling across the sea (one copy built once, drawn at both offsets)
    auto* waves = Cached("waves", [=] {
        return AsGeometry(Geometry::Build([&](ID2D1GeometrySink* sink) {
            for (int row = 0; row < 6; ++row) {
                const float y = horizon + 14 + row * (h - horizon) / 7;
                for (float x = static_cast<float>((row % 2) * 40); x < w; x += 80.f + row * 16.f) {
                    const float span = 26.f + row * 6.f;
                    sink->BeginFigure(D2D1::Point2F(x, y), D2D1_FIGURE_BEGIN_HOLLOW);
                    sink->AddBezier(D2D1::BezierSegment(D2D1::Point2F(x + span / 3, y - 4 - row * 0.6f), D2D1::Point2F(x + span * 2 / 3, y - 4 - row * 0.6f),
                                                        D2D1::Point2F(x + span, y)));
                    sink->EndFigure(D2D1_FIGURE_END_OPEN);
                }
            }
        }, D2D1_FILL_MODE_ALTERNATE));
    });
    {
        Layer layer(g, 0.6f, 0.f);
        DrawLooping(g, w, Drift(w), [&](float ox) {
            Push shift(g, Translate(ox, 0.f));
            Stroke(g, waves, SmWave);
        });
    }

    // The beach next to the chapter cards: warm sand with the surf lapping at its edge, palms,
    // an umbrella, a beach ball, a crab and a few shells
    const D2D1_POINT_2F s0{ w * 0.44f, h * 0.99f }, s1{ w * 0.55f, h * 0.86f }, s2{ w * 0.75f, h * 0.83f }, s3{ w + kBleed, h * 0.815f };
    const auto sandY = [&](float x) { return BezierYAtX(s0, s1, s2, s3, x); };
    {
        Layer layer(g, 1.f);
        auto* beach = Cached("beach", [=] {
            return AsGeometry(Geometry::Build([&](ID2D1GeometrySink* sink) {
                sink->BeginFigure(P(s0.x, h + kBleed), D2D1_FIGURE_BEGIN_FILLED);
                sink->AddLine(s0);
                sink->AddBezier(D2D1::BezierSegment(s1, s2, s3));
                sink->AddLine(P(w + kBleed, h + kBleed));
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            }, D2D1_FILL_MODE_ALTERNATE));
        });
        auto* surf = Cached("surf", [=] {
            return AsGeometry(Geometry::Build([&](ID2D1GeometrySink* sink) {
                sink->BeginFigure(s0, D2D1_FIGURE_BEGIN_HOLLOW);
                sink->AddBezier(D2D1::BezierSegment(s1, s2, s3));
                sink->EndFigure(D2D1_FIGURE_END_OPEN);
            }));
        });
        // Wet sand + foam: the surf line slides up and down the shore
        const float lap = static_cast<float>(std::sin(Clock() * 0.7));
        {
            Push wash(g, Translate(-6.f * u * lap, 4.f * u * lap));
            Stroke(g, surf, Pen{ Rgba(255, 236, 220).Fade(0.35f + 0.25f * (1.f - lap) * 0.5f), 7.f * u });
        }
        FillVertical(g, beach, { 0.f, h * 0.81f, w, h }, { { 0.f, Rgba(255, 196, 136) }, { 0.45f, Rgba(214, 140, 106) }, { 1.f, Rgba(120, 66, 80) } });
        Stroke(g, surf, Pen{ Rgba(255, 246, 236, 150), 2.f * u });

        DrawPalmSilhouette(g, w * 0.975f, sandY(w * 0.975f) + 6 * u, u * 3.6f, -1);
        DrawPalmSilhouette(g, w * 0.92f, sandY(w * 0.92f) + 6 * u, u * 2.7f, 1);
        Art::Summer::DrawUmbrella(g, w * 0.685f, sandY(w * 0.685f) + 8 * u, u * 3.6f, -8.f);
        Art::Summer::DrawBeachBall(g, w * 0.745f, sandY(w * 0.745f) + 4 * u, u * 2.3f);
        Art::Summer::DrawCrab(g, w * 0.8f, sandY(w * 0.8f) + 22 * u, u * 2.f);
        Art::Summer::DrawStarfish(g, w * 0.655f, sandY(w * 0.655f) + 34 * u, u * 1.8f, 18.f);
        Art::Summer::DrawShell(g, w * 0.86f, sandY(w * 0.86f) + 30 * u, u * 1.7f, -12.f);
        Art::Summer::DrawShell(g, w * 0.72f, sandY(w * 0.72f) + 48 * u, u * 1.4f, 20.f);
    }

    Vignette(g, w, h, Rgba(12, 14, 50));
    DrawBunting(g, w, garlandTop,
                { Rgba(255, 112, 67), Rgba(255, 213, 79), Rgba(38, 198, 218), Rgba(255, 245, 225), Rgba(240, 98, 146) },
                Pen{ Rgba(255, 230, 200, 140), 1.2f }, false);
}

// ── Christmas ───────────────────────────────────────────────────────────────

// Smoke puffs rising from a chimney and drifting with the wind
void DrawSmoke(Graphics& g, float x, float y, float s) {
    const double now = Clock();
    for (int i = 0; i < 7; ++i) {
        const float t = static_cast<float>(std::fmod(now * 0.16 + i / 7.0, 1.0));
        const float px = x + (t * 34.f + static_cast<float>(std::sin(t * 6.0 + i)) * 4.f) * s;
        const float py = y - t * 90.f * s;
        const float r = (3.f + t * 13.f) * s;
        const float a = static_cast<float>(std::sin(Pi * t)) * 0.38f;
        GlowEllipse(g, px, py, r * 1.3f, r, Rgba(215, 225, 240).Fade(a));
    }
}

// Street lamp with a warm halo and a pool of light on the snow
void DrawLamp(Graphics& g, float x, float baseY, float s) {
    Push at(g, Translate(x, baseY));
    Push size(g, Scale(s));
    GlowEllipse(g, 0.f, 0.f, 26.f, 6.f, Rgba(255, 200, 120, 70));
    Line(g, 0.f, 0.f, 0.f, -34.f, Pen{ Rgba(18, 24, 32), 2.f });
    GlowEllipse(g, 0.f, -36.f, 24.f, 24.f, WindowGlow().Fade(0.45f));
    Rectangle(g, { -3.f, -40.f, 6.f, 7.f }, Rgba(18, 24, 32));
    Ellipse(g, 0.f, -36.5f, 2.f, 2.4f, Rgba(255, 224, 160));
    Rectangle(g, { -4.f, -41.5f, 8.f, 1.8f }, XmRoof);
}

void DrawChristmas(Graphics& g, float w, float h, float garlandTop) {
    const float u = h / 1000.f;
    FillVerticalRect(g, { 0, 0, w, h }, { { 0.f, Rgba(3, 6, 18) }, { 0.35f, Rgba(8, 18, 42) }, { 0.52f, Rgba(16, 42, 66) },
                                          { 0.61f, Rgba(34, 76, 96) }, { 0.66f, Rgba(16, 36, 52) }, { 1.f, Rgba(10, 22, 32) } });
    {
        Layer layer(g, 0.12f);
        DrawStars(g, w, h * 0.62f, 230, Rgba(230, 245, 255));
        DrawShootingStars(g, w, h * 0.45f, Rgba(225, 245, 255));
    }

    // Pale moon high up in the open sky
    {
        Layer layer(g, 0.16f);
        const float r = std::clamp(h * 0.042f, 24.f, 56.f);
        const float mx = w * 0.47f, my = h * 0.15f;
        GlowEllipse(g, mx, my, r * 6.f, r * 6.f, Rgba(160, 210, 255, 34));
        GlowEllipse(g, mx, my, r * 2.4f, r * 2.4f, Rgba(200, 230, 255, 70));
        Ellipse(g, mx, my, r, r, Rgba(232, 240, 250));
        Ellipse(g, mx - r * 0.3f, my - r * 0.2f, r * 0.2f, r * 0.2f, Rgba(204, 216, 232));
        Ellipse(g, mx + r * 0.35f, my + r * 0.3f, r * 0.14f, r * 0.14f, Rgba(204, 216, 232));
    }

    // Aurora: three ribbons (green, teal, violet) across the open sky, pulsing, drifting sideways
    // and stretching like curtains in the wind
    const auto aurora = [&](float opacity) {
        const double now = Clock();
        g.PushOpacity(Pulse() * opacity);
        const struct { float y; Color c; int alpha; double phase, speed; } ribbons[] = {
            { 0.2f, Rgba(80, 255, 170), 95, 0.0, 0.11 }, { 0.27f, Rgba(60, 210, 255), 70, 2.0, 0.08 }, { 0.33f, Rgba(170, 110, 255), 50, 4.0, 0.06 },
        };
        for (const auto& r : ribbons) {
            Push sway(g, Translate(static_cast<float>(std::sin(now * r.speed + r.phase)) * w * 0.03f, 0.f));
            Push stretch(g, M::Scale(1.f, 1.f + 0.14f * static_cast<float>(std::sin(now * (r.speed * 2.4) + r.phase)), D2D1::Point2F(0.f, h * r.y)));
            DrawAurora(g, w, h, h * r.y, r.c, r.alpha, r.phase);
        }
        g.PopLayer();
    };
    {
        Layer layer(g, 0.25f);
        aurora(1.f);
    }

    // Far snowy mountains, haze, nearer mountains
    const float farBase = h * 0.6f, nearBase = h * 0.63f, shore = h * 0.635f;
    const auto mountains = [&] {
        DrawRange(g, w, farBase, h, h * 0.21f, 0.7f, Rgba(74, 104, 140), Rgba(30, 50, 72), Rgba(236, 244, 252, 235), 0.42f);
        Haze(g, w, h * 0.51f, h * 0.12f, Rgba(120, 180, 210, 36));
        DrawRange(g, w, nearBase, h, h * 0.12f, 2.3f, Rgba(46, 70, 98), Rgba(20, 34, 50), Rgba(225, 236, 248, 220), 0.32f);
    };
    {
        Layer layer(g, 0.4f);
        mountains();
    }

    // Frozen lake in front of them: the mountains and the aurora mirrored on the ice, with a few
    // cold glints sliding over it
    const float lakeBottom = h * 0.76f;
    {
        Layer layer(g, 0.4f);
        FillVerticalRect(g, { -kBleed, shore, w + 2 * kBleed, lakeBottom - shore }, { { 0.f, Rgba(40, 84, 104) }, { 1.f, Rgba(14, 32, 46) } });
        g.PushClip({ -kBleed, shore, w + 2 * kBleed, lakeBottom - shore });
        {
            Push mirror(g, Mirror(shore));
            g.PushOpacity(0.32f);
            mountains();
            g.PopLayer();
            aurora(0.35f);
        }
        const double now = Clock();
        for (int i = 0; i < 9; ++i) {
            const float y = shore + (lakeBottom - shore) * (0.15f + i * 0.09f);
            const float x = static_cast<float>(std::fmod(now * (10.0 + i * 3.0) + i * 211.0, static_cast<double>(w) + 300.0)) - 150.f;
            Line(g, x, y, x + 60.f + i * 8.f, y, Pen{ Rgba(220, 240, 255, 50), 1.f, true });
        }
        g.PopClip();
    }

    // Village lights twinkling on the far shore, and a pine forest along it
    {
        Layer layer(g, 0.55f);
        SeededRandom rnd(911);
        const double now = Clock();
        for (int i = 0; i < 16; ++i) {
            const float x = static_cast<float>(w * (0.4 + rnd.NextDouble() * 0.2));
            const float y = shore - static_cast<float>(rnd.NextDouble() * h * 0.006);
            const float flicker = 0.65f + 0.35f * static_cast<float>(std::sin(now * (1.0 + rnd.NextDouble() * 2.0) + i));
            GlowEllipse(g, x, y, 7.f * u, 4.f * u, Rgba(255, 190, 100, 110).Fade(flicker));
            Ellipse(g, x, y, 1.3f * u, 1.3f * u, Rgba(255, 220, 160).Fade(flicker));
        }
        SeededRandom forest(77);
        for (double x = -kBleed; x < w + kBleed; x += (10.0 + forest.NextDouble() * 16.0) * u) {
            const double fx = x / w;
            const double y = shore + 3 * u + forest.NextDouble() * h * 0.004;
            const double scale = (0.8 + forest.NextDouble() * 0.7) * u * 1.25;
            if (fx > 0.39 && fx < 0.61 && forest.NextDouble() < 0.6) continue;   // a gap for the village
            DrawPine(g, static_cast<float>(x), static_cast<float>(y), static_cast<float>(scale), true);
        }
    }

    Layer front(g, 1.f);   // everything below is the foreground

    // Snowy hills in front, with the cabin, a lamp, the decorated tree, gifts and a snowman on the
    // ground next to the chapter cards
    const float midBase = h * 0.9f;
    const float midTop = midBase - h * 0.06f;
    auto* hillGeo = Hills(w, midBase, h, h * 0.03, 3.3);
    const std::vector<GradientStop> hillStops = { { 0.f, Rgba(232, 242, 252) }, { 0.35f, Rgba(158, 186, 210) }, { 1.f, Rgba(40, 60, 82) } };
    if (auto* hillFill = g.LinearBrush(hillStops, { 0.f, midTop }, { 0.f, h })) g.FillGeometry(hillGeo, hillFill);
    Haze(g, w, midTop - h * 0.04f, h * 0.06f, Rgba(200, 225, 245, 30));

    // Lowest point of the slope under an object, so nothing floats with one corner in the air
    const auto ground = [&](float x, float halfWidth) {
        return static_cast<float>(std::max(HillY(x, w, midBase, h * 0.03, 3.3),
                                           std::max(HillY(x - halfWidth, w, midBase, h * 0.03, 3.3), HillY(x + halfWidth, w, midBase, h * 0.03, 3.3))));
    };

    const float cabinX = w * 0.7f;
    const float cabinS = u * 2.7f;
    const float cabinY = ground(cabinX, 22 * cabinS) + 2;
    DrawSmoke(g, cabinX + 14 * cabinS, cabinY - 42 * cabinS, cabinS);
    DrawCabin(g, cabinX, cabinY, cabinS);
    if (auto* drift = g.LinearBrush(hillStops, { 0.f, midTop }, { 0.f, h })) {
        auto ellipse = Geometry::Ellipse(cabinX, cabinY + 2 * cabinS, 30 * cabinS, 5 * cabinS);
        g.FillGeometry(ellipse.Get(), drift);
    }
    const float lampX = w * 0.655f;
    DrawLamp(g, lampX, ground(lampX, 4.f) + 3.f, u * 2.4f);

    const struct { float fx, sc; } pines[] = { { 0.975f, 3.8f } };
    for (const auto& p : pines) {
        const float x = w * p.fx;
        DrawPine(g, x, static_cast<float>(HillY(x, w, midBase, h * 0.03, 3.3)) + 8, p.sc * u * 1.6f, true);
    }

    // Decorated tree with gifts on both sides and the snowman next to it
    const float treeX = w * 0.865f;
    const float treeS = u * 5.2f;
    GlowEllipse(g, treeX, ground(treeX, 15 * treeS) - 50 * treeS * 0.5f, 34 * treeS, 40 * treeS, Rgba(255, 210, 120, 26));
    Art::Christmas::DrawTree(g, treeX, ground(treeX, 15 * treeS) + 2, treeS);
    const float redS = u * 2.2f;
    const float redX = treeX + 15 * treeS + 9 * redS + 6 * u;
    Art::Christmas::DrawGift(g, redX, ground(redX, 9 * redS) + 3, redS, Art::Christmas::OrnRed);
    const float goldS = u * 1.6f;
    const float goldX = redX + 9 * redS + 9 * goldS + 2 * u;
    Art::Christmas::DrawGift(g, goldX, ground(goldX, 9 * goldS) + 3, goldS, Art::Christmas::OrnGold);
    const float blueS = u * 1.9f;
    const float blueX = treeX - 15 * treeS - 9 * blueS - 6 * u;
    Art::Christmas::DrawGift(g, blueX, ground(blueX, 9 * blueS) + 3, blueS, Art::Christmas::OrnBlue);
    const float snowmanS = u * 3.3f;
    const float snowmanX = blueX - 9 * blueS - 7.5f * snowmanS - 6 * u;
    Art::Christmas::DrawSnowman(g, snowmanX, ground(snowmanX, 7.5f * snowmanS) + 3, snowmanS);

    // Low mist over the snow
    DrawLooping(g, w, Drift(w), [&](float ox) {
        for (int i = 0; i < 5; ++i) GlowEllipse(g, ox + w * (0.1f + i * 0.2f), h * 0.9f, w * 0.15f, h * 0.04f, Rgba(220, 235, 255, 30));
    });

    Vignette(g, w, h, Rgba(3, 6, 18));
}

void DrawChristmasLights(Graphics& g, float w, float garlandTop) {
    // String lights hanging from the top bar (attached to it: no parallax)
    Art::Christmas::DrawLights(g, 0, w, garlandTop - 1, 1.7f);
}

} // namespace

void Draw(Graphics& g, const Rect& screen, float topBarHeight, double time, float mouseX, float mouseY) {
    const Season season = Active();
    if (season == Season::Classic) return;
    const float w = screen.w, h = screen.h;
    if (w < 200.f || h < 200.f) return;
    SetClock(time);
    parallaxX = std::clamp(mouseX, -1.f, 1.f);
    parallaxY = std::clamp(mouseY, -1.f, 1.f);
    if (cache.w != w || cache.h != h || cache.season != season) {
        cache.items.clear();
        cache.stars.clear();
        cache.streaks.clear();
        cache.w = w;
        cache.h = h;
        cache.season = season;
    }

    Push origin(g, Translate(screen.x, screen.y));
    g.PushClip({ 0, 0, w, h });
    switch (season) {
        case Season::Halloween: DrawHalloween(g, w, h, topBarHeight); break;
        case Season::Summer: DrawSummer(g, w, h, topBarHeight); break;
        case Season::Christmas:
            DrawChristmas(g, w, h, topBarHeight);
            DrawChristmasLights(g, w, topBarHeight);
            break;
        default: break;
    }
    g.PopClip();
}

} // namespace SpeedrunLauncher::Seasonal::Backdrop
