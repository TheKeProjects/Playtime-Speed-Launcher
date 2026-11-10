// Profile cosmetics, all drawn in code (no pictures): decorations around the profile picture,
// frames around the profile card and nameplates behind the name in lists. The ids are the
// catalogue in Services\Account\Profile.cpp; an unknown id draws nothing.
#include <algorithm>
#include <cmath>
#include <functional>
#include "Graphics/AnimatedImages.h"
#include "Graphics/Geometry.h"
#include "UI/Style.h"
#include "Windows/Main/Common/ProfileDraw.h"

namespace SpeedrunLauncher::Main::ProfileDraw {

namespace {

constexpr float kPi = 3.14159265f, kTau = 6.2831853f;

Color HueColor(float h, float light = 0.15f) {
    h = h - std::floor(h);
    const float r = std::clamp(std::fabs(h * 6.f - 3.f) - 1.f, 0.f, 1.f);
    const float g = std::clamp(2.f - std::fabs(h * 6.f - 2.f), 0.f, 1.f);
    const float b = std::clamp(2.f - std::fabs(h * 6.f - 4.f), 0.f, 1.f);
    return Lerp(Color{ r, g, b, 1.f }, Colors::White, light);
}

// Small pseudo-random number in 0..1 from an index (stable between frames)
float Hash01(int i) {
    const float x = std::sin(static_cast<float>(i) * 12.9898f + 78.233f) * 43758.547f;
    return x - std::floor(x);
}

// A four-pointed sparkle
void Sparkle(Graphics& g, float cx, float cy, float s, Color c) {
    if (s <= 0.2f) return;
    g.FillEllipse(cx, cy, s * 0.9f, s * 0.9f, c.WithAlpha(c.a * 0.18f));
    g.Line(cx - s, cy, cx + s, cy, c, std::max(1.f, s * 0.28f), true);
    g.Line(cx, cy - s, cx, cy + s, c, std::max(1.f, s * 0.28f), true);
    g.FillEllipse(cx, cy, s * 0.22f, s * 0.22f, Colors::White.WithAlpha(c.a));
}

// A six-armed snowflake
void Snowflake(Graphics& g, float cx, float cy, float s, float angle, Color c) {
    for (int k = 0; k < 3; ++k) {
        const float a = angle + k * kPi / 3.f;
        const float dx = std::cos(a) * s, dy = std::sin(a) * s;
        g.Line(cx - dx, cy - dy, cx + dx, cy + dy, c, std::max(1.f, s * 0.18f), true);
        // little branches near the tips
        for (const float side : { 1.f, -1.f }) {
            const float bx = cx + dx * 0.6f * side, by = cy + dy * 0.6f * side;
            const float a1 = a + 0.6f, a2 = a - 0.6f;
            g.Line(bx, by, bx + std::cos(a1) * s * 0.3f * side, by + std::sin(a1) * s * 0.3f * side, c, std::max(0.8f, s * 0.12f), true);
            g.Line(bx, by, bx + std::cos(a2) * s * 0.3f * side, by + std::sin(a2) * s * 0.3f * side, c, std::max(0.8f, s * 0.12f), true);
        }
    }
}

// An arc of a circle as short segments
void Arc(Graphics& g, float cx, float cy, float radius, float from, float to, Color c, float width) {
    const int steps = std::max(4, static_cast<int>(std::fabs(to - from) / 0.12f));
    for (int i = 0; i < steps; ++i) {
        const float a0 = from + (to - from) * i / steps, a1 = from + (to - from) * (i + 1) / steps;
        g.Line(cx + std::cos(a0) * radius, cy + std::sin(a0) * radius, cx + std::cos(a1) * radius, cy + std::sin(a1) * radius, c, width, true);
    }
}

void AddRound(ID2D1GeometrySink* sink, const Rect& rect, float r) {
    const D2D1_RECT_F q = rect.D2D();
    r = std::min({ r, rect.w * 0.5f, rect.h * 0.5f });
    const auto arc = [&](float x, float y) {
        sink->AddArc(D2D1::ArcSegment(D2D1::Point2F(x, y), D2D1::SizeF(r, r), 0.f, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_SMALL));
    };
    sink->BeginFigure(D2D1::Point2F(q.left + r, q.top), D2D1_FIGURE_BEGIN_FILLED);
    sink->AddLine(D2D1::Point2F(q.right - r, q.top));
    arc(q.right, q.top + r);
    sink->AddLine(D2D1::Point2F(q.right, q.bottom - r));
    arc(q.right - r, q.bottom);
    sink->AddLine(D2D1::Point2F(q.left + r, q.bottom));
    arc(q.left, q.bottom - r);
    sink->AddLine(D2D1::Point2F(q.left, q.top + r));
    arc(q.left + r, q.top);
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
}

// The band between two rounded rectangles (a frame)
Geometry::ComPtr<ID2D1PathGeometry> Ring(const Rect& outer, float outerR, const Rect& inner, float innerR) {
    return Geometry::Build(
        [&](ID2D1GeometrySink* sink) {
            AddRound(sink, outer, outerR);
            AddRound(sink, inner, innerR);
        },
        D2D1_FILL_MODE_ALTERNATE);
}

// Unit shapes (drawn with a transform)
ID2D1Geometry* HeartShape() {
    static auto heart = Geometry::FromPathData("M0,0.32 C0,-0.05 -0.5,-0.12 -0.5,0.22 C-0.5,0.47 -0.2,0.66 0,0.82 C0.2,0.66 0.5,0.47 0.5,0.22 C0.5,-0.12 0,-0.05 0,0.32 Z");
    return heart.Get();
}
ID2D1Geometry* CrownShape() {
    static auto crown = Geometry::FromPathData("M-1,0.45 L-1.05,-0.45 L-0.5,0.02 L0,-0.62 L0.5,0.02 L1.05,-0.45 L1,0.45 Z");
    return crown.Get();
}
ID2D1Geometry* HandShape() {
    // A wide palm with four fingers up and the thumb out to the side (like a GrabPack hand)
    static auto hand = Geometry::FromPathData(
        "M-0.42,0.05 L-0.42,-0.55 C-0.42,-0.68 -0.26,-0.68 -0.26,-0.55 L-0.26,-0.2 L-0.2,-0.2 L-0.2,-0.72 C-0.2,-0.86 -0.04,-0.86 -0.04,-0.72 "
        "L-0.04,-0.22 L0.02,-0.22 L0.02,-0.74 C0.02,-0.88 0.18,-0.88 0.18,-0.74 L0.18,-0.2 L0.24,-0.2 L0.24,-0.58 C0.24,-0.72 0.4,-0.72 0.4,-0.58 "
        "L0.4,0.0 L0.58,-0.22 C0.66,-0.32 0.8,-0.22 0.72,-0.1 L0.42,0.36 C0.36,0.48 0.26,0.55 0.1,0.55 L-0.18,0.55 C-0.34,0.55 -0.42,0.42 -0.42,0.3 Z");
    return hand.Get();
}

void FillShape(Graphics& g, ID2D1Geometry* shape, float cx, float cy, float scale, float angleDeg, Color fill, Color outline = Colors::Transparent,
               float outlineW = 0.f) {
    if (!shape) return;
    g.PushTransform(D2D1::Matrix3x2F::Scale(scale, scale) * D2D1::Matrix3x2F::Rotation(angleDeg) * D2D1::Matrix3x2F::Translation(cx, cy));
    g.FillGeometry(shape, fill);
    if (outlineW > 0.f) g.StrokeGeometry(shape, outline, outlineW / scale, true);
    g.PopTransform();
}

// A glow only outside a card (inside it would wash the card's colours out)
void OuterGlow(Graphics& g, const Rect& card, float radius, Color color, float blur, float opacity) {
    const float pad = blur * 2.f + 4.f;
    auto outside = Ring(card.Outset({ pad, pad, pad, pad }), radius + pad, card, radius);
    g.PushLayer(1.f, outside.Get());
    g.GlowRound(card, radius, color, blur, opacity);
    g.PopLayer();
}

// ── Playtime Co. pieces (after the factory in the game) ──
constexpr Color kPtYellow = Hex(0xF5C518), kPtRed = Hex(0xE0302A), kPtBlue = Hex(0x2A5FD8), kPtGreen = Hex(0x2EA84A), kPtInk = Hex(0x141414);

// One of the giant toy blocks the factory's walls are made of: a bevelled coloured block
void ToyBlock(Graphics& g, const Rect& r, Color c) {
    g.FillRound(r, std::min(2.5f, r.h * 0.2f), Lerp(c, Colors::Black, 0.35f));
    const Rect face = r.Inset(std::max(1.f, r.h * 0.12f));
    g.FillLinear(face, 1.5f, { { 0.f, Lerp(c, Colors::White, 0.25f) }, { 1.f, c } }, { 0.f, 0.f }, { 0.f, 1.f });
}

// The lobby's big yellow sign: "Playtime" in black on yellow, with the little blue "Co." tag
void PlaytimeSign(UI::Ui& ui, const Rect& r, bool cables) {
    auto& g = ui.g;
    if (cables) {
        g.Line(r.x + r.w * 0.2f, r.y - r.h * 0.7f, r.x + r.w * 0.2f, r.y + 1.f, Hex(0x2A2A2A), 1.2f);
        g.Line(r.R() - r.w * 0.2f, r.y - r.h * 0.7f, r.R() - r.w * 0.2f, r.y + 1.f, Hex(0x2A2A2A), 1.2f);
    }
    g.Shadow(r, 3.f, 6.f, Rgba(0, 0, 0, 120));
    g.FillRound(r, r.h * 0.18f, Hex(0x9A7A0A));
    g.FillLinear(r.Inset(1.2f), r.h * 0.16f, { { 0.f, Hex(0xFFE04A) }, { 1.f, kPtYellow } }, { 0.f, 0.f }, { 0.f, 1.f });
    Profile poppy;
    poppy.font = "poppy";
    // The word fits the plaque (its size shrinks until it does)
    const Rect textBox = r.Inset(r.h * 0.22f, 0.f);
    float size = r.h * 0.62f;
    TextStyle st = NameStyle(poppy, size);
    while (size > 4.f && g.Measure(L"Playtime", st).w > textBox.w) {
        size *= 0.92f;
        st = NameStyle(poppy, size);
    }
    st.align = HAlign::Center;
    g.Text(L"Playtime", textBox, kPtInk, st);
    // "Co." tag hanging off the top-right corner, a little tilted, outside the word
    const float th = r.h * 0.48f, tw = th * 1.45f;
    const float tx = r.R() - tw * 0.45f, ty = r.y - th * 0.62f;
    g.PushTransform(D2D1::Matrix3x2F::Rotation(-12.f, D2D1::Point2F(tx + tw * 0.5f, ty + th * 0.5f)));
    g.FillRound({ tx, ty, tw, th }, th * 0.25f, kPtBlue);
    TextStyle co = NameStyle(poppy, th * 0.72f);
    co.align = HAlign::Center;
    g.Text(L"Co.", { tx, ty, tw, th }, kPtYellow, co);
    g.PopTransform();
}

// A hand scanner button: hazard-striped ring, steel and the red hand print
void HandScanner(Graphics& g, float cx, float cy, float rad, float glow) {
    g.FillEllipse(cx, cy, rad, rad, kPtInk);
    // hazard ring: yellow wedges
    for (int i = 0; i < 8; ++i) {
        const float a0 = i / 8.f * kTau, a1 = a0 + kTau / 16.f;
        auto wedge = Geometry::Build([&](ID2D1GeometrySink* s) {
            s->BeginFigure(D2D1::Point2F(cx + std::cos(a0) * rad * 0.68f, cy + std::sin(a0) * rad * 0.68f), D2D1_FIGURE_BEGIN_FILLED);
            s->AddLine(D2D1::Point2F(cx + std::cos(a0) * rad, cy + std::sin(a0) * rad));
            s->AddLine(D2D1::Point2F(cx + std::cos(a1) * rad, cy + std::sin(a1) * rad));
            s->AddLine(D2D1::Point2F(cx + std::cos(a1) * rad * 0.68f, cy + std::sin(a1) * rad * 0.68f));
            s->EndFigure(D2D1_FIGURE_END_CLOSED);
        });
        g.FillGeometry(wedge.Get(), kPtYellow);
    }
    g.FillEllipse(cx, cy, rad * 0.66f, rad * 0.66f, Hex(0x5A6068));
    g.FillEllipse(cx, cy, rad * 0.56f, rad * 0.56f, Hex(0xC8CED6));
    if (glow > 0.f) g.FillEllipse(cx, cy, rad * 0.56f, rad * 0.56f, kPtRed.WithAlpha(0.25f * glow));
    FillShape(g, HandShape(), cx, cy + rad * 0.05f, rad * 0.5f, 0.f, kPtRed);
}

} // namespace

// ── Decorations around the profile picture ──────────────────────────────────

void AvatarDecoration(UI::Ui& ui, const Rect& r, const Profile& p, bool animate) {
    const std::string& id = p.avatarDeco;
    if (id.empty() || id == "none") return;
    auto& g = ui.g;
    NoColorFilter keep(g);
    const float t = animate ? static_cast<float>(ui.Time()) : 0.f;
    const float cx = r.CX(), cy = r.CY(), R = r.w * 0.5f;
    const float u = std::max(1.f, R / 20.f);   // 1 at a 40 px picture
    const Color c1 = Hex(p.color1), c2 = Hex(p.color2);
    bool moving = true;

    if (id == "sparkles") {
        // Constellation: a dotted orbit and sparkles twinkling around it
        for (int i = 0; i < 28; ++i) {
            const float a = i / 28.f * kTau + t * 0.15f;
            g.FillEllipse(cx + std::cos(a) * R * 1.13f, cy + std::sin(a) * R * 1.13f, 0.7f * u, 0.7f * u, Rgba(170, 200, 255, 120));
        }
        for (int i = 0; i < 8; ++i) {
            const float a = i / 8.f * kTau + Hash01(i) * 0.6f + t * 0.15f;
            const float dist = R * (1.08f + 0.18f * Hash01(i + 9));
            const float tw = 0.5f + 0.5f * std::sin(t * 3.f + i * 1.9f);
            Sparkle(g, cx + std::cos(a) * dist, cy + std::sin(a) * dist, (2.2f + 2.6f * tw) * u, Lerp(Hex(0x9FC4FF), Colors::White, tw * 0.6f).WithAlpha(0.6f + 0.4f * tw));
        }
    } else if (id == "flames") {
        // Speedrun on fire: flame tongues licking up round the picture
        g.StrokeEllipse(cx, cy, R + 1.5f * u, R + 1.5f * u, Hex(0xFF7A1A).WithAlpha(0.8f), 2.f * u);
        for (int i = 0; i < 16; ++i) {
            const float a = i / 16.f * kTau;
            const float up = 0.55f + 0.45f * std::max(0.f, std::sin(a));   // taller at the bottom (they rise up the sides)
            const float h = R * (0.22f + 0.14f * std::sin(t * 9.f + i * 1.7f)) * (1.2f - 0.5f * up);
            const float bx = cx + std::cos(a) * (R + 1.f * u), by = cy + std::sin(a) * (R + 1.f * u);
            // Each tongue: outward and a little upwards, three layers getting hotter inside
            const float dx = std::cos(a) * 0.7f, dy = std::sin(a) * 0.7f - 0.5f;
            const float len = std::sqrt(dx * dx + dy * dy);
            const float ux = dx / len, uy = dy / len;
            const Color layers[] = { Hex(0xE8401C), Hex(0xFF9A1F), Hex(0xFFE27A) };
            for (int k = 0; k < 3; ++k) {
                const float s = 1.f - k * 0.3f;
                const float tip = h * s;
                g.Line(bx, by, bx + ux * tip, by + uy * tip, layers[k].WithAlpha(0.9f), (4.2f - k * 1.2f) * u * s + 1.f, true);
            }
        }
        // Embers
        for (int i = 0; i < 6; ++i) {
            const float life = std::fmod(t * 0.6f + Hash01(i), 1.f);
            const float a = kPi * (0.15f + 0.7f * Hash01(i + 3)) + kPi;
            g.FillEllipse(cx + std::cos(a) * R * (1.1f + 0.3f * life), cy - R * 0.4f - life * R * 0.9f, 1.2f * u, 1.2f * u,
                          Hex(0xFFC04A).WithAlpha(1.f - life));
        }
    } else if (id == "neon") {
        // A soft glow kept outside the picture (thin strokes stepping outwards)
        const float pulse = 0.6f + 0.4f * std::sin(t * 2.5f);
        for (int k = 1; k <= 4; ++k) g.StrokeEllipse(cx, cy, R + (3.f + k * 1.4f) * u, R + (3.f + k * 1.4f) * u, c1.WithAlpha(0.07f * pulse * (5 - k) / 4.f), 1.6f * u);
        g.StrokeEllipse(cx, cy, R + 3.f * u, R + 3.f * u, Lerp(c1, Colors::White, 0.5f), 1.6f * u);
        g.StrokeEllipse(cx, cy, R + 6.5f * u, R + 6.5f * u, c2.WithAlpha(0.8f), 1.f * u);
        // a light running round the tube
        const float a = t * 2.2f;
        Arc(g, cx, cy, R + 3.f * u, a, a + 0.9f, Colors::White.WithAlpha(0.9f), 2.2f * u);
        Arc(g, cx, cy, R + 6.5f * u, -a * 0.8f, -a * 0.8f + 0.6f, Lerp(c2, Colors::White, 0.5f), 1.6f * u);
    } else if (id == "grabpack" || id == "grabpack67") {
        // The two GrabPack hands holding the picture: blue on the left, red on the right, with their cables.
        // GrabPack 67: the hands do the "6-7" (palms up, going up and down in turns like a seesaw,
        // weighing the 6 in the blue hand against the 7 in the red one)
        const bool sixSeven = id == "grabpack67";
        const float swing = 3.6f * u;
        const float bob = sixSeven ? std::sin(t * 5.5f + 1.2f) * swing : std::sin(t * 2.f) * 1.2f * u;
        const float s = R * 0.62f;
        // 6-7: the hands lie almost flat, palms up (fingers pointing out, thumbs up), a bit lower
        // and closer, so the numbers can rest on them; the GrabPack grips the picture
        const float lean = sixSeven ? -78.f : 25.f;
        const float lx = cx - R * (sixSeven ? 0.92f : 0.95f), rx = cx + R * (sixSeven ? 0.92f : 0.95f), hy = cy + R * (sixSeven ? 0.74f : 0.55f);
        // The wrist (where the cuff and the cable are), turned with the hand
        const float leanRad = lean * kTau / 360.f;
        const float wristDx = sixSeven ? -s * 0.55f * std::sin(leanRad) : -s * 0.1f, wristDy = sixSeven ? s * 0.55f * std::cos(leanRad) : s * 0.55f;
        // The cables: a dark sheath with an electric line flickering along it
        const float zap = 0.5f + 0.5f * std::sin(t * 14.f);
        for (const float side : { -1.f, 1.f }) {
            const float hb = side < 0 ? bob : -bob;
            const float hx = (side < 0 ? lx : rx) - side * wristDx;   // the wrist mirrors on the right
            const float ex = cx + side * R * 0.25f, ey = cy + R * 1.3f;
            g.Line(hx, hy + wristDy + hb, ex, ey, Hex(0x2A2A30), 2.6f * u, true);
            g.Line(hx, hy + wristDy + hb, ex, ey, Hex(0x6FE8FF).WithAlpha(0.35f + 0.45f * zap), 0.9f * u, true);
        }
        // Each hand on its orange cuff
        const auto hand = [&](float x, float y, Color fill, Color edge) {
            g.FillRound({ x - s * 0.36f, y + s * 0.42f, s * 0.72f, s * 0.3f }, s * 0.1f, Hex(0xE07A1E));
            g.Line(x - s * 0.3f, y + s * 0.52f, x + s * 0.3f, y + s * 0.52f, Hex(0x9A4A0A), 0.8f * u);
            FillShape(g, HandShape(), x, y, s, 0.f, fill, edge, 1.4f * u);
        };
        g.PushTransform(D2D1::Matrix3x2F::Rotation(lean, D2D1::Point2F(lx, hy + bob)));
        hand(lx, hy + bob, Hex(0x2F6FE8), Hex(0x10285A));
        g.PopTransform();
        g.PushTransform(D2D1::Matrix3x2F::Scale(-1.f, 1.f, D2D1::Point2F(rx, hy - bob)) * D2D1::Matrix3x2F::Rotation(-lean, D2D1::Point2F(rx, hy - bob)));
        hand(rx, hy - bob, Hex(0xE23A3A), Hex(0x5A1010));
        g.PopTransform();
        if (sixSeven) {
            // The 6 on the blue palm, the 7 on the red one: big yellow digits with a dark outline,
            // sitting on the flat hands (and tilting as they go up and down)
            const TextStyle digit = UI::Mono(s * 0.9f, true).Center();
            const auto number = [&](const wchar_t* text, float x, float y, float tilt, Color glow) {
                const Rect box{ x - s * 0.5f, y - s * 0.55f, s, s * 1.1f };
                g.PushTransform(D2D1::Matrix3x2F::Rotation(tilt, D2D1::Point2F(x, y + s * 0.45f)));
                g.FillEllipse(x, y, s * 0.4f, s * 0.4f, glow.WithAlpha(0.28f));
                const float o = std::max(1.f, 1.1f * u);
                for (const auto& d : { D2D1::Point2F(-o, 0.f), D2D1::Point2F(o, 0.f), D2D1::Point2F(0.f, -o), D2D1::Point2F(0.f, o) })
                    g.Text(text, box.Offset(d.x, d.y), kPtInk, digit);
                g.Text(text, box, kPtYellow, digit);
                g.PopTransform();
            };
            // Resting on the palm: just above the hand's top edge (the thumb side), over the fingers
            const float tilt = bob / swing * 8.f;
            number(L"6", lx - s * 0.2f, hy + bob - s * 1.05f, -tilt, Hex(0x2F6FE8));
            number(L"7", rx + s * 0.2f, hy - bob - s * 1.05f, -tilt, Hex(0xE23A3A));
        }
    } else if (id == "scanner") {
        // Playtime Co. hand scanner: a hazard-striped bezel round the picture, the red hand print
        // button on the corner and the door number plate on top
        const float rr = R + 3.f * u;
        g.StrokeEllipse(cx, cy, rr, rr, kPtInk, 5.f * u);
        const int stripes = 18;
        for (int i = 0; i < stripes; ++i) {
            const float a = i / static_cast<float>(stripes) * kTau + t * 0.15f;
            Arc(g, cx, cy, rr, a, a + kTau / stripes * 0.5f, kPtYellow, 4.f * u);
        }
        g.StrokeEllipse(cx, cy, rr + 2.8f * u, rr + 2.8f * u, Hex(0x5A6068), 1.f * u);
        g.StrokeEllipse(cx, cy, rr - 2.8f * u, rr - 2.8f * u, Hex(0x5A6068), 1.f * u);
        const float pulse = 0.5f + 0.5f * std::sin(t * 3.f);
        HandScanner(g, cx + R * 0.74f, cy + R * 0.74f, R * 0.36f, pulse);
        // number plate
        const Rect plate{ cx - 6.f * u, cy - rr - 9.f * u, 12.f * u, 9.f * u };
        g.FillRound(plate, 1.5f * u, Colors::White);
        g.StrokeRound(plate, 1.5f * u, kPtInk, 0.8f * u);
        TextStyle num = UI::Mono(6.5f * u, true).Center();
        g.Text(L"1", plate, kPtInk, num);
    } else if (id == "stopwatch") {
        // A stopwatch bezel: ticks, the crown on top and the lap sweeping round
        const float rr = R + 4.f * u;
        g.StrokeEllipse(cx, cy, rr, rr, Hex(0x2A3440), 5.f * u);
        g.StrokeEllipse(cx, cy, rr, rr, Hex(0xC8D2DC), 1.2f * u);
        for (int i = 0; i < 12; ++i) {
            const float a = i / 12.f * kTau;
            const float in = rr - (i % 3 == 0 ? 2.5f : 1.5f) * u, out = rr + (i % 3 == 0 ? 2.5f : 1.5f) * u;
            g.Line(cx + std::cos(a) * in, cy + std::sin(a) * in, cx + std::cos(a) * out, cy + std::sin(a) * out, Hex(0xE6EDF3), 1.f * u);
        }
        const float lap = std::fmod(t / 3.f, 1.f);
        Arc(g, cx, cy, rr, -kPi / 2.f, -kPi / 2.f + lap * kTau, c1, 2.2f * u);
        const float hx = cx + std::cos(-kPi / 2.f + lap * kTau) * rr, hy = cy + std::sin(-kPi / 2.f + lap * kTau) * rr;
        g.FillEllipse(hx, hy, 2.6f * u, 2.6f * u, Lerp(c1, Colors::White, 0.5f));
        // crown + side button
        g.FillRound({ cx - 3.f * u, cy - rr - 8.f * u, 6.f * u, 5.f * u }, 1.5f * u, Hex(0xC8D2DC));
        g.FillRound({ cx - 5.f * u, cy - rr - 11.f * u, 10.f * u, 3.5f * u }, 1.5f * u, Hex(0xE6EDF3));
        const float ba = -kPi / 4.f;
        g.Line(cx + std::cos(ba) * (rr + 2.f * u), cy + std::sin(ba) * (rr + 2.f * u), cx + std::cos(ba) * (rr + 7.f * u), cy + std::sin(ba) * (rr + 7.f * u),
               Hex(0xC8D2DC), 3.f * u, true);
    } else if (id == "toxic") {
        // Toxic gas: a green glow and bubbles rising round the edge
        for (int k = 1; k <= 3; ++k) g.StrokeEllipse(cx, cy, R + (2.f + k * 1.5f) * u, R + (2.f + k * 1.5f) * u, Hex(0x6CFF3A).WithAlpha(0.06f * (4 - k)), 1.6f * u);
        g.StrokeEllipse(cx, cy, R + 1.5f * u, R + 1.5f * u, Hex(0x7CFF4A), 1.5f * u);
        for (int i = 0; i < 10; ++i) {
            const float life = std::fmod(t * 0.35f + Hash01(i), 1.f);
            const float side = i % 2 ? 1.f : -1.f;
            const float a = kPi / 2.f + side * (0.3f + life * 2.3f);   // from the bottom up each side
            const float dist = R * (1.08f + 0.1f * Hash01(i + 4));
            const float rad = (1.5f + 2.5f * Hash01(i + 7)) * u * (0.6f + 0.4f * std::sin(life * kPi));
            const Color c = Hex(0x8CFF5A).WithAlpha(0.85f * std::sin(life * kPi));
            g.StrokeEllipse(cx + std::cos(a) * dist, cy + std::sin(a) * dist, rad, rad, c, 1.f * u);
            g.FillEllipse(cx + std::cos(a) * dist - rad * 0.3f, cy + std::sin(a) * dist - rad * 0.3f, rad * 0.25f, rad * 0.25f, Colors::White.WithAlpha(c.a));
        }
    } else if (id == "hearts") {
        for (int i = 0; i < 6; ++i) {
            const float life = std::fmod(t * 0.25f + i / 6.f, 1.f);
            const float a = -kPi / 2.f + (i % 2 ? 1.f : -1.f) * (0.9f + 0.5f * Hash01(i)) + std::sin(t + i) * 0.1f;
            const float dist = R * (1.05f + 0.2f * life);
            const float s = (5.f + 3.f * Hash01(i + 2)) * u * (0.7f + 0.3f * std::sin(life * kPi));
            const Color c = (i % 3 == 0 ? Hex(0xFF4D7A) : i % 3 == 1 ? Hex(0xFF8AB0) : c1).WithAlpha(std::sin(life * kPi));
            FillShape(g, HeartShape(), cx + std::cos(a) * dist, cy + std::sin(a) * dist - life * 6.f * u, s, std::sin(t * 2.f + i) * 12.f, c);
        }
    } else if (id == "frost") {
        for (int k = 1; k <= 3; ++k) g.StrokeEllipse(cx, cy, R + (2.f + k * 1.4f) * u, R + (2.f + k * 1.4f) * u, Hex(0x9EE8FF).WithAlpha(0.06f * (4 - k)), 1.5f * u);
        g.StrokeEllipse(cx, cy, R + 2.f * u, R + 2.f * u, Hex(0xDFF7FF), 1.4f * u);
        for (int i = 0; i < 6; ++i) {
            const float a = i / 6.f * kTau + t * 0.2f;
            const float s = (3.5f + 2.f * Hash01(i)) * u;
            Snowflake(g, cx + std::cos(a) * R * 1.12f, cy + std::sin(a) * R * 1.12f, s, t * 0.8f + i, Hex(0xE8FBFF).WithAlpha(0.9f));
        }
    } else if (id == "crown") {
        // A gold crown sitting on the picture, a little tilted, with gems
        const float s = R * 0.42f;
        const float x = cx + R * 0.55f, y = cy - R * 0.92f + std::sin(t * 2.f) * 0.8f * u;
        FillShape(g, CrownShape(), x, y, s, 18.f, Hex(0xFFC83A), Hex(0x8A5A00), 1.5f * u);
        const float rad = 18.f * kPi / 180.f;
        const auto at = [&](float lx, float ly) { return D2D1::Point2F(x + (lx * std::cos(rad) - ly * std::sin(rad)) * s, y + (lx * std::sin(rad) + ly * std::cos(rad)) * s); };
        const D2D1_POINT_2F gems[] = { at(-0.5f, 0.22f), at(0.f, 0.2f), at(0.5f, 0.22f) };
        const Color gemColors[] = { Hex(0xFF3A5A), Hex(0x3AC8FF), Hex(0x6CFF6A) };
        for (int i = 0; i < 3; ++i) g.FillEllipse(gems[i].x, gems[i].y, 1.8f * u, 1.8f * u, gemColors[i]);
        const float tw = 0.5f + 0.5f * std::sin(t * 3.f);
        const auto tip = at(0.f, -0.62f);
        Sparkle(g, tip.x, tip.y - 1.f * u, (2.f + 2.f * tw) * u, Colors::White.WithAlpha(0.5f + 0.5f * tw));
    } else if (id == "halo") {
        // A glowing halo floating over the head
        const float y = cy - R * 1.02f + std::sin(t * 1.8f) * 1.5f * u;
        for (int k = 1; k <= 3; ++k) g.StrokeEllipse(cx, y, R * 0.62f + k * 1.2f * u, R * 0.17f + k * 1.2f * u, Hex(0xFFE27A).WithAlpha(0.07f * (4 - k)), 1.5f * u);
        g.StrokeEllipse(cx, y, R * 0.62f, R * 0.17f, Hex(0xFFF2B0), 2.2f * u);
        Sparkle(g, cx + R * 0.7f, y - R * 0.1f, (2.f + 1.5f * std::sin(t * 3.f)) * u, Colors::White.WithAlpha(0.8f));
    } else {
        moving = false;
    }
    if (moving && animate) ui.KeepAnimatingThrottled();
}

// ── Frames around the profile card ──────────────────────────────────────────

void ProfileFrame(UI::Ui& ui, const Rect& card, float radius, const Profile& p) {
    const std::string& id = p.frame;
    if (id.empty() || id == "none") return;
    auto& g = ui.g;
    NoColorFilter keep(g);
    const float t = static_cast<float>(ui.Time());
    const float T = std::clamp(card.w * 0.025f, 4.f, 8.f);   // thickness
    const Rect outer = card.Outset({ T * 0.5f, T * 0.5f, T * 0.5f, T * 0.5f });
    const Rect inner = card.Inset(T * 0.5f);
    const float outerR = radius + T * 0.5f, innerR = std::max(2.f, radius - T * 0.5f);
    auto ring = Ring(outer, outerR, inner, innerR);
    const Color c1 = Hex(p.color1), c2 = Hex(p.color2);
    bool moving = true;

    // Fills the frame's band with whatever `paint` draws (stripes, checks, gradients...)
    const auto band = [&](const std::function<void()>& paint) {
        g.PushLayer(1.f, ring.Get());
        paint();
        g.PopLayer();
    };

    if (id == "gold") {
        band([&] {
            g.FillLinear(outer, 0.f, { { 0.f, Hex(0xFFE58A) }, { 0.35f, Hex(0xC8901E) }, { 0.6f, Hex(0xFFD45A) }, { 1.f, Hex(0x8A5A10) } }, { 0.f, 0.f },
                         { 1.f, 1.f });
            // a glint sliding across
            const float x = outer.x + std::fmod(t * 0.25f, 1.6f) / 1.6f * (outer.w + 120.f) - 60.f;
            g.FillLinear({ x - 30.f, outer.y, 60.f, outer.h }, 0.f, { { 0.f, Rgba(255, 255, 255, 0) }, { 0.5f, Rgba(255, 255, 255, 170) }, { 1.f, Rgba(255, 255, 255, 0) } },
                         { 0.f, 0.f }, { 1.f, 0.f });
        });
        g.StrokeRound(outer, outerR, Hex(0x5A3A08), 1.f);
        g.StrokeRound(inner, innerR, Hex(0x5A3A08), 1.f);
        // Gems at the corners
        const D2D1_POINT_2F corners[] = { { outer.x + T, outer.y + T }, { outer.R() - T, outer.y + T }, { outer.x + T, outer.B() - T }, { outer.R() - T, outer.B() - T } };
        for (const auto& c : corners) {
            g.FillEllipse(c.x, c.y, T * 0.75f, T * 0.75f, Hex(0x8A1030));
            g.FillEllipse(c.x, c.y, T * 0.55f, T * 0.55f, Hex(0xFF3A6A));
            g.FillEllipse(c.x - T * 0.15f, c.y - T * 0.15f, T * 0.18f, T * 0.18f, Colors::White);
        }
    } else if (id == "neon") {
        const float pulse = 0.65f + 0.35f * std::sin(t * 2.4f);
        OuterGlow(g, outer, outerR, c1, 10.f, 0.3f * pulse);
        g.StrokeRound(card.Outset({ 2.f, 2.f, 2.f, 2.f }), radius + 2.f, Lerp(c1, Colors::White, 0.45f), 2.f);
        g.StrokeRound(card.Outset({ 5.f, 5.f, 5.f, 5.f }), radius + 5.f, c2.WithAlpha(0.8f), 1.f);
        // a light running along the edge
        const float per = 2.f * (outer.w + outer.h);
        float d = std::fmod(t * 160.f, per);
        float x, y;
        if (d < outer.w) { x = outer.x + d; y = outer.y; }
        else if ((d -= outer.w) < outer.h) { x = outer.R(); y = outer.y + d; }
        else if ((d -= outer.h) < outer.w) { x = outer.R() - d; y = outer.B(); }
        else { d -= outer.w; x = outer.x; y = outer.B() - d; }
        g.FillEllipse(x, y, 6.f, 6.f, Colors::White.WithAlpha(0.35f));
        g.FillEllipse(x, y, 2.5f, 2.5f, Colors::White);
    } else if (id == "hazard") {
        // Playtime Co.: the factory lobby's giant toy blocks all round, the big yellow "Playtime"
        // sign hanging over the top edge and hand scanners on the bottom corners
        band([&] {
            const Color colors[] = { kPtRed, kPtBlue, kPtYellow, kPtGreen };
            const float len = T * 2.6f;
            int k = 0;
            for (float x = outer.x; x < outer.R(); x += len, ++k) {
                ToyBlock(g, { x, outer.y, len, T + 0.5f }, colors[k % 4]);
                ToyBlock(g, { x, outer.B() - T - 0.5f, len, T + 0.5f }, colors[(k + 2) % 4]);
            }
            k = 1;
            for (float y = outer.y + T; y < outer.B() - T; y += len, ++k) {
                ToyBlock(g, { outer.x, y, T + 0.5f, len }, colors[k % 4]);
                ToyBlock(g, { outer.R() - T - 0.5f, y, T + 0.5f, len }, colors[(k + 1) % 4]);
            }
        });
        g.StrokeRound(outer, outerR, kPtInk, 1.2f);
        g.StrokeRound(inner, innerR, kPtInk.WithAlpha(0.7f), 1.f);
        // The sign
        const float sw = std::clamp(card.w * 0.4f, 44.f, 150.f), sh = sw * 0.28f;
        PlaytimeSign(ui, { card.CX() - sw * 0.5f, outer.y - sh * 0.3f, sw, sh }, false);
        // Hand scanners at the bottom corners (the hand glows now and then), bolts at the top ones
        const float pulse = std::max(0.f, std::sin(t * 1.6f));
        const float sr = T * 1.25f;
        HandScanner(g, outer.x + T * 0.9f, outer.B() - T * 0.9f, sr, pulse);
        HandScanner(g, outer.R() - T * 0.9f, outer.B() - T * 0.9f, sr, 1.f - pulse);
        for (const float x : { outer.x + T * 0.9f, outer.R() - T * 0.9f }) {
            g.FillEllipse(x, outer.y + T * 0.9f, T * 0.6f, T * 0.6f, Hex(0x8A8F96));
            g.FillEllipse(x, outer.y + T * 0.9f, T * 0.42f, T * 0.42f, Hex(0xC8CDD3));
            g.Line(x - T * 0.3f, outer.y + T * 0.9f, x + T * 0.3f, outer.y + T * 0.9f, Hex(0x5A5F66), 1.f);
        }
    } else if (id == "toxic") {
        band([&] {
            g.FillLinear(outer, 0.f, { { 0.f, Hex(0x2E7A12) }, { 0.5f, Hex(0x6CFF3A) }, { 1.f, Hex(0x2E7A12) } }, { 0.f, 0.f }, { 0.f, 1.f });
        });
        OuterGlow(g, outer, outerR, Hex(0x6CFF3A), 8.f, 0.18f + 0.07f * std::sin(t * 2.f));
        // drips hanging from the top edge
        for (int i = 0; i < 7; ++i) {
            const float x = outer.x + outerR + (outer.w - outerR * 2.f) * (i + 0.5f) / 7.f + (Hash01(i) - 0.5f) * 10.f;
            const float len = T * (1.2f + 2.2f * (0.5f + 0.5f * std::sin(t * (0.8f + Hash01(i + 3)) + i * 2.f)));
            g.Line(x, outer.y + T * 0.5f, x, outer.y + T * 0.5f + len, Hex(0x6CFF3A), T * 0.55f, true);
            g.FillEllipse(x, outer.y + T * 0.5f + len, T * 0.42f, T * 0.5f, Hex(0x8CFF5A));
        }
    } else if (id == "frost") {
        band([&] {
            g.FillLinear(outer, 0.f, { { 0.f, Hex(0xE8FBFF) }, { 0.5f, Hex(0x8ED8F2) }, { 1.f, Hex(0xDFF5FF) } }, { 0.f, 0.f }, { 1.f, 1.f });
        });
        OuterGlow(g, outer, outerR, Hex(0x9EE8FF), 8.f, 0.18f);
        // icicles under the top edge and snowflakes on the corners
        for (int i = 0; i < 11; ++i) {
            const float x = outer.x + outerR + (outer.w - outerR * 2.f) * i / 10.f;
            const float len = T * (1.f + 1.6f * Hash01(i));
            auto spike = Geometry::Build([&](ID2D1GeometrySink* s) {
                s->BeginFigure(D2D1::Point2F(x - T * 0.45f, inner.y), D2D1_FIGURE_BEGIN_FILLED);
                s->AddLine(D2D1::Point2F(x + T * 0.45f, inner.y));
                s->AddLine(D2D1::Point2F(x, inner.y + len));
                s->EndFigure(D2D1_FIGURE_END_CLOSED);
            });
            g.FillGeometry(spike.Get(), Hex(0xDFF7FF).WithAlpha(0.9f));
        }
        const D2D1_POINT_2F corners[] = { { outer.x + T, outer.y + T }, { outer.R() - T, outer.y + T }, { outer.x + T, outer.B() - T }, { outer.R() - T, outer.B() - T } };
        for (int i = 0; i < 4; ++i) Snowflake(g, corners[i].x, corners[i].y, T * 1.6f, t * 0.5f + i, Colors::White);
    } else if (id == "checkered") {
        // The finish line flag
        band([&] {
            g.FillRect(outer, Colors::White);
            const float s = T * 0.75f;
            for (float y = outer.y; y < outer.B(); y += s)
                for (float x = outer.x; x < outer.R(); x += s)
                    if ((static_cast<int>((x - outer.x) / s) + static_cast<int>((y - outer.y) / s)) % 2 == 0) g.FillRect({ x, y, s, s }, Hex(0x141414));
        });
        g.StrokeRound(outer, outerR, Hex(0x141414), 1.f);
        moving = false;
    } else if (id == "prisma") {
        const float phase = std::fmod(t * 0.15f, 1.f);
        band([&] {
            std::vector<GradientStop> stops;
            for (int k = 0; k <= 12; ++k) stops.push_back({ k / 12.f, HueColor(k / 12.f + phase) });
            g.FillLinear(outer, 0.f, stops, { 0.f, 0.f }, { 1.f, 1.f });
        });
        OuterGlow(g, outer, outerR, HueColor(phase), 9.f, 0.22f);
    } else if (id == "ember") {
        band([&] {
            g.FillLinear(outer, 0.f, { { 0.f, Hex(0x3A0A04) }, { 0.6f, Hex(0xB8300E) }, { 1.f, Hex(0xFF7A1A) } }, { 0.f, 0.f }, { 0.f, 1.f });
        });
        OuterGlow(g, outer, outerR, Hex(0xFF5A1A), 8.f, 0.18f + 0.05f * std::sin(t * 3.f));
        // sparks rising from the bottom edge along the sides
        for (int i = 0; i < 14; ++i) {
            const float life = std::fmod(t * (0.25f + 0.2f * Hash01(i)) + Hash01(i + 5), 1.f);
            const bool left = i % 2 == 0;
            const float x = (left ? outer.x : outer.R()) + (left ? -1.f : 1.f) * (2.f + 6.f * Hash01(i + 2)) + std::sin(life * 9.f + i) * 3.f;
            const float y = outer.B() - life * outer.h;
            g.FillEllipse(x, y, 1.6f, 1.6f, Hex(0xFFC04A).WithAlpha(1.f - life));
        }
    } else {
        moving = false;
    }
    if (moving) ui.KeepAnimatingThrottled();
}

// ── Nameplates behind a list row ────────────────────────────────────────────
// Strongest on the right, fading out to the left so the picture and the name stay readable.

bool HasNameplate(const Profile& p) { return !p.nameplate.empty() && p.nameplate != "none"; }

void PlateScrim(UI::Ui& ui, const Rect& bounds) {
    auto& g = ui.g;
    NoColorFilter keep(g);
    const Rect veil = bounds.Outset({ 7.f, 2.f, 7.f, 2.f });
    g.GlowRound(veil, veil.h * 0.5f, Colors::Black, 7.f, 0.45f);
    g.FillRound(veil, veil.h * 0.5f, Rgba(4, 8, 14, 115));
}

void RowText(UI::Ui& ui, const std::wstring& text, const Rect& r, Color color, const TextStyle& style, bool onPlate) {
    auto& g = ui.g;
    if (!onPlate || text.empty()) {
        g.Text(text, r, color, style);
        return;
    }
    // A soft dark veil behind the words, a dark halo hugging the letters (so they read on a bright,
    // busy or animated plate) and the colour lifted a little
    NoColorFilter keep(g);
    {
        const TextSize size = g.Measure(text, style);
        const float tw = std::min(size.w, r.w), th = std::min(size.h, r.h);
        float x = r.x;
        if (style.align == HAlign::Center) x = r.CX() - tw * 0.5f;
        else if (style.align == HAlign::Right) x = r.R() - tw;
        float y = r.CY() - th * 0.5f;
        if (style.valign == VAlign::Top) y = r.y;
        else if (style.valign == VAlign::Bottom) y = r.B() - th;
        PlateScrim(ui, { x, y, tw, th });
    }
    const float o = std::max(1.2f, style.size * 0.11f);
    const Color halo = Rgba(0, 0, 0, 200);
    for (const auto& d : { D2D1::Point2F(-o, 0.f), D2D1::Point2F(o, 0.f), D2D1::Point2F(0.f, -o), D2D1::Point2F(0.f, o), D2D1::Point2F(-o, -o),
                           D2D1::Point2F(o, -o), D2D1::Point2F(-o, o), D2D1::Point2F(o, o) })
        g.Text(text, r.Offset(d.x, d.y), halo, style);
    g.Text(text, r.Offset(0.f, o * 1.4f), Rgba(0, 0, 0, 120), style);   // a soft drop shadow
    g.Text(text, r, Lerp(color, Colors::White, 0.15f).WithAlpha(1.f), style);
}

void Nameplate(UI::Ui& ui, const Rect& row, float radius, const Profile& p) {
    const std::string& id = p.nameplate;
    if (id.empty() || id == "none") return;
    auto& g = ui.g;
    NoColorFilter keep(g);
    const float t = static_cast<float>(ui.Time());
    bool moving = false;
    g.PushRoundClip(row, radius);
    // The banner keeps the profile card's proportions (height / 0.36) on the right, so it shows whole
    const float bannerW = std::min(row.w * 0.8f, row.h / 0.36f);
    const Rect bannerRect{ row.R() - bannerW, row.y, bannerW, row.h };
    // Fade mask: from nothing (left) to full (right); the banner fades in over its own left edge
    float fadeFrom = row.x, fadeTo = row.R();
    if (id == "banner") {
        fadeFrom = bannerRect.x;
        fadeTo = bannerRect.x + bannerW * 0.35f;
    } else if (id == "hazard") {
        fadeFrom = row.x + row.w * 0.25f;
        fadeTo = row.x + row.w * 0.6f;
    }
    if (id == "banner" || id == "hazard")
        g.PushMaskLayer(1.f, g.LinearBrush({ { 0.f, Rgba(0, 0, 0, 0) }, { 1.f, Rgba(0, 0, 0, 255) } }, { fadeFrom, row.y }, { fadeTo, row.y }));
    else
        g.PushMaskLayer(1.f, g.LinearBrush({ { 0.f, Rgba(0, 0, 0, 0) }, { 0.3f, Rgba(0, 0, 0, 40) }, { 0.75f, Rgba(0, 0, 0, 220) }, { 1.f, Rgba(0, 0, 0, 255) } },
                                           { row.x, row.y }, { row.R(), row.y }));
    const auto gradient = [&](std::vector<GradientStop> stops) { g.FillLinear(row, 0.f, stops, { 0.f, 0.f }, { 1.f, 0.4f }); };

    if (id == "banner") {
        const Color base = Hex(p.bannerColor);
        g.FillLinear(row, 0.f, { { 0.f, Lerp(base, Colors::White, 0.12f) }, { 1.f, Lerp(base, Colors::Black, 0.3f) } }, { 0.f, 0.f }, { 1.f, 1.f });
        const std::wstring source = p.BannerUrl();
        if (!source.empty())
            if (auto animation = AnimatedImages::Get(source, kBannerSide)) {
                if (auto frame = animation->At(t)) g.DrawImageRounded(*frame, bannerRect, 0.f, 1.f, Stretch::UniformToFill);
                if (animation->Animated()) moving = true;
            }
    } else if (id == "name") {
        gradient({ { 0.f, Hex(p.color1) }, { 1.f, Hex(p.color2) } });
    } else if (id == "sunset") {
        gradient({ { 0.f, Hex(0x4A1050) }, { 0.5f, Hex(0xE2457A) }, { 1.f, Hex(0xFFB347) } });
    } else if (id == "ocean") {
        gradient({ { 0.f, Hex(0x0A2A5A) }, { 0.6f, Hex(0x1E7AD8) }, { 1.f, Hex(0x3DE0D0) } });
    } else if (id == "galaxy") {
        gradient({ { 0.f, Hex(0x120A2E) }, { 0.6f, Hex(0x3A1A7A) }, { 1.f, Hex(0x7A3AD8) } });
        for (int i = 0; i < 14; ++i) {
            const float tw = 0.5f + 0.5f * std::sin(t * (1.5f + Hash01(i)) + i * 2.3f);
            const float x = row.x + row.w * (0.35f + 0.65f * Hash01(i)), y = row.y + row.h * Hash01(i + 20);
            if (i % 4 == 0) Sparkle(g, x, y, 2.f + 2.f * tw, Colors::White.WithAlpha(0.4f + 0.6f * tw));
            else g.FillEllipse(x, y, 0.9f, 0.9f, Colors::White.WithAlpha(0.3f + 0.7f * tw));
        }
        moving = true;
    } else if (id == "toxic") {
        gradient({ { 0.f, Hex(0x0A2A0A) }, { 0.6f, Hex(0x2E7A12) }, { 1.f, Hex(0x6CFF3A) } });
        for (int i = 0; i < 9; ++i) {
            const float life = std::fmod(t * (0.3f + 0.2f * Hash01(i)) + Hash01(i + 3), 1.f);
            const float x = row.x + row.w * (0.4f + 0.6f * Hash01(i)), y = row.B() - life * (row.h + 8.f);
            const float rad = 2.f + 3.f * Hash01(i + 9);
            g.StrokeEllipse(x, y, rad, rad, Hex(0xCCFFAA).WithAlpha(0.8f * (1.f - life)), 1.f);
        }
        moving = true;
    } else if (id == "ember") {
        gradient({ { 0.f, Hex(0x1A0604) }, { 0.6f, Hex(0x8A1E08) }, { 1.f, Hex(0xFF6A1A) } });
        for (int i = 0; i < 12; ++i) {
            const float life = std::fmod(t * (0.35f + 0.25f * Hash01(i)) + Hash01(i + 7), 1.f);
            const float x = row.x + row.w * (0.4f + 0.6f * Hash01(i)) + std::sin(life * 8.f + i) * 4.f, y = row.B() - life * (row.h + 6.f);
            g.FillEllipse(x, y, 1.4f, 1.4f, Hex(0xFFC04A).WithAlpha(1.f - life));
        }
        moving = true;
    } else if (id == "hazard") {
        // Playtime Co.: the lobby's coloured floor tiles, a row of toy blocks, a GrabPack hand print
        // and the yellow "Playtime" sign on the right
        g.FillRect(row, Hex(0x14161C));
        const Color tiles[] = { kPtRed, kPtBlue, kPtYellow, Hex(0x3A3E48) };
        const float s = row.h / 3.f;
        int k = 0;
        for (float y = row.y; y < row.B(); y += s)
            for (float x = row.x; x < row.R(); x += s, ++k)
                g.FillRect({ x + 1.f, y + 1.f, s - 2.f, s - 2.f }, Lerp(tiles[(k * 7 + static_cast<int>(y)) % 4], Hex(0x14161C), 0.45f));
        const float bh = row.h * 0.34f;
        const Color blocks[] = { kPtRed, kPtBlue, kPtYellow, kPtGreen };
        for (int i = 0; i < 6; ++i) ToyBlock(g, { row.R() - row.w * 0.55f + i * bh * 1.05f, row.B() - bh, bh, bh }, blocks[i % 4]);
        FillShape(g, HandShape(), row.R() - row.w * 0.42f, row.y + row.h * 0.42f, row.h * 0.32f, -15.f, kPtBlue.WithAlpha(0.85f));
        const float sh = row.h * 0.46f, sw = sh * 3.3f;
        PlaytimeSign(ui, { row.R() - sw - row.h * 0.25f, row.y + row.h * 0.16f, sw, sh }, false);
    } else if (id == "checkered") {
        g.FillRect(row, Colors::White);
        const float s = row.h / 4.f;
        for (float y = row.y; y < row.B(); y += s)
            for (float x = row.x; x < row.R(); x += s)
                if ((static_cast<int>((x - row.x) / s) + static_cast<int>((y - row.y) / s)) % 2 == 0) g.FillRect({ x, y, s, s }, Hex(0x141414));
    } else if (id == "prisma") {
        const float phase = std::fmod(t * 0.15f, 1.f);
        std::vector<GradientStop> stops;
        for (int k = 0; k <= 8; ++k) stops.push_back({ k / 8.f, HueColor(k / 8.f + phase, 0.05f) });
        gradient(stops);
        moving = true;
    }
    g.PopLayer();
    g.PopLayer();
    if (moving) ui.KeepAnimatingThrottled();
}

} // namespace SpeedrunLauncher::Main::ProfileDraw
