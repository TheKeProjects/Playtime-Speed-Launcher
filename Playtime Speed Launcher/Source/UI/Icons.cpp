#include "UI/Icons.h"
#include <algorithm>
#include "Graphics/Geometry.h"

namespace SpeedrunLauncher::UI::Icons {

namespace {

// Fits a design canvas (vw × vh) uniformly into r.
D2D1::Matrix3x2F FitTransform(const Rect& r, float vx, float vy, float vw, float vh) {
    const float s = std::min(r.w / vw, r.h / vh);
    const float ox = r.x + (r.w - vw * s) * 0.5f - vx * s, oy = r.y + (r.h - vh * s) * 0.5f - vy * s;
    return D2D1::Matrix3x2F::Scale(s, s) * D2D1::Matrix3x2F::Translation(ox, oy);
}

} // namespace

void LiveSplit(Graphics& g, const Rect& r) {
    static const auto hexagon = Geometry::FromPathData(
        "M320.733,150.5 C395.427,21.125 604.573,21.125 679.267,150.5 L918.29,564.5 C992.985,693.875 888.412,875 739.023,875 "
        "L260.977,875 C111.588,875 7.015,693.875 81.71,564.5 Z");
    static const auto shadow = Geometry::FromPathData(
        "M786.297,335.88 L620.208,169.792 L379.792,410.208 L620.208,650.624 L592.674,710.104 L412.362,529.792 L171.946,770.208 "
        "L276.738,875 L739.023,875 C888.412,875 992.985,693.875 918.29,564.5 Z");
    // The drawing spans roughly x 7..993, y 21..875 (plus the 40 px stroke)
    g.PushTransform(FitTransform(r, -15.f, 0.f, 1030.f, 895.f));
    g.FillGeometry(hexagon.Get(), Argb(0xFF404040));
    g.FillGeometry(shadow.Get(), Argb(0x20000000));
    const auto circle = [&](float cx, float cy, Color top, Color bottom) {
        auto e = Geometry::Ellipse(cx, cy, 170.f, 170.f);
        auto* brush = g.LinearBrush({ { 0.f, top }, { 1.f, bottom } }, { cx, cy - 170.f }, { cx, cy + 170.f });
        g.FillGeometry(e.Get(), brush);
    };
    circle(500.f, 290.f, Argb(0xFFD73F4A), Argb(0xFFC22B36));
    circle(707.846f, 650.f, Argb(0xFF3FCD46), Argb(0xFF27BB2E));
    circle(292.154f, 650.f, Argb(0xFF3F5CD7), Argb(0xFF2B48C2));
    g.StrokeGeometry(hexagon.Get(), Colors::White, 40.f);
    g.PopTransform();
}

void LoadManip(Graphics& g, const Rect& r, Color c) {
    static const auto folder = Geometry::FromPathData("M8,18 C8,15.8 9.8,14 12,14 H24 L29,20 H52 C54.2,20 56,21.8 56,24 V46 C56,48.2 54.2,50 52,50 H12 C9.8,50 8,48.2 8,46 Z");
    static const auto opening = Geometry::FromPathData("M8,24 H56");
    static const auto gearRing = Geometry::Ellipse(47.f, 43.f, 4.5f, 4.5f);
    static const auto gearTeeth = Geometry::FromPathData(
        "M47,35 V37 M47,51 V49 M55,43 H53 M39,43 H41 M52.7,37.3 L51.3,38.7 M52.7,48.7 L51.3,47.3 M41.3,48.7 L42.7,47.3 M41.3,37.3 L42.7,38.7");
    g.PushTransform(FitTransform(r, 6.f, 12.f, 52.f, 41.f));
    g.StrokeGeometry(folder.Get(), c, 3.f, true);
    g.StrokeGeometry(opening.Get(), c, 3.f, true);
    g.StrokeGeometry(gearRing.Get(), c, 2.5f);
    g.StrokeGeometry(gearTeeth.Get(), c, 2.5f, true);
    g.PopTransform();
}

} // namespace SpeedrunLauncher::UI::Icons
