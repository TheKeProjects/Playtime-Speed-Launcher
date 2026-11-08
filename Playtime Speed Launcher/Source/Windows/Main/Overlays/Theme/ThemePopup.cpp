#include "Windows/Main/Overlays/Theme/ThemePopup.h"
#include <algorithm>
#include <map>
#include <vector>
#include "Graphics/Animation.h"
#include "Graphics/Geometry.h"
#include "Graphics/Images.h"
#include "Localization/Localization.h"
#include "Seasonal/SeasonalTheme.h"
#include "Settings/IconThemeSettings.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

namespace {

constexpr double kOpen = 0.3, kClose = 0.16;
constexpr float kCaret = 8.f, kGap = 6.f;
constexpr float kPadding = 10.f, kHeadingH = 26.f, kRowH = 42.f, kRowGap = 2.f, kWidth = 230.f, kRadius = 12.f;

struct Look {
    const wchar_t* theme;
    const char* label;
};
constexpr Look kLooks[] = {
    { L"default", "icontheme_default" }, { L"classic", "icontheme_classic" }, { L"lgbtq", "icontheme_lgbtq" },
    { L"summer", "icontheme_summer" },   { L"halloween", "icontheme_halloween" }, { L"christmas", "icontheme_christmas" },
};

// The launcher icon of a theme, from the exe's ICON resources (Assets\Icons isn't unpacked), as an
// image: decoded once per theme
ImagePtr ThemeIcon(const std::wstring& theme) {
    static std::map<std::wstring, ImagePtr> cache;
    if (auto it = cache.find(theme); it != cache.end()) return it->second;
    Settings::IconTheme look;
    look.theme = theme;
    const int id = look.IconResourceId();
    ImagePtr image;
    constexpr int kSize = 64;
    if (HICON icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(id ? id : 101), IMAGE_ICON, kSize, kSize, LR_DEFAULTCOLOR))) {
        // Drawn into a 32-bit DIB: its alpha comes out premultiplied, as Direct2D wants it
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = kSize;
        bi.bmiHeader.biHeight = -kSize;   // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HDC dc = CreateCompatibleDC(nullptr);
        if (HBITMAP dib = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0)) {
            HGDIOBJ old = SelectObject(dc, dib);
            DrawIconEx(dc, 0, 0, icon, kSize, kSize, 0, nullptr, DI_NORMAL);
            GdiFlush();
            image = Images::FromPixels(kSize, kSize, static_cast<const uint32_t*>(bits));
            SelectObject(dc, old);
            DeleteObject(dib);
        }
        DeleteDC(dc);
        DestroyIcon(icon);
    }
    return cache[theme] = image;
}

} // namespace

void ThemePopup::Toggle(const Rect& at) {
    if (open) {
        Close();
        return;
    }
    anchor = at;
    open = true;
    changedAt = -1.0;
}

void ThemePopup::Close() {
    if (!open) return;
    open = false;
    changedAt = -2.0;
}

bool ThemePopup::Visible() const { return open || changedAt > -10.0; }

// Opens with a little bounce, growing out of the button while it unfolds; closes with a quick fade.
// To the right of the button, bottom edges lined up, kept inside the window.
void ThemePopup::Draw(UI::Ui& ui, const Rect& screen) {
    auto& g = ui.g;
    const double now = ui.Time();
    if (changedAt == -1.0 || changedAt == -2.0) changedAt = now;
    const float raw = Anim::Clamp01(static_cast<float>((now - changedAt) / (open ? kOpen : kClose)));
    if (!open && raw >= 1.f) {
        changedAt = -10.0;
        return;
    }
    if (raw < 1.f) ui.KeepAnimating();
    const float fade = open ? Anim::EaseOutCubic(raw) : 1.f - Anim::EaseInCubic(raw);
    const float scale = open ? Anim::Lerp(0.9f, 1.f, Anim::EaseOutBack(raw, 0.9f)) : Anim::Lerp(0.95f, 1.f, fade);
    const auto& palette = Seasonal::Current();

    const float n = static_cast<float>(std::size(kLooks));
    const float h = kPadding * 2.f + kHeadingH + n * kRowH + (n - 1.f) * kRowGap;
    Rect box{ anchor.R() + kCaret + kGap, anchor.B() - h + 10.f, kWidth, h };
    box.y = std::clamp(box.y, screen.y + 44.f, screen.B() - h - 8.f);
    const float caretY = std::clamp(anchor.CY(), box.y + 18.f, box.B() - 18.f);

    if (open) {
        // The popup takes the mouse; a press outside or Escape only closes it
        ui.Block(screen);
        if ((ui.In().pressed[0] && !box.Contains(ui.MouseX(), ui.MouseY())) || ui.KeyPressed(VK_ESCAPE))
            Close();
    }

    g.PushOpacity(std::max(0.f, fade));
    ui.PushTranslate(-10.f * (1.f - fade), 0.f);
    ui.PushScale(scale, box.x, caretY);
    ui.Block(box);
    const Color fill = palette.bar.WithAlpha(0.97f), edge = palette.accent.WithAlpha(0.7f);
    g.Shadow(box, kRadius, 24.f, Rgba(0, 0, 0, 150));
    if (open && raw < 1.f) g.GlowRound(box, kRadius, palette.accent, 16.f, 0.5f * (1.f - raw));
    g.Border(box, kRadius, fill, edge, 1.5f);
    // Arrow on the left edge pointing at the button (the border's colour around the fill)
    const auto triangle = [&](float size, float x) {
        return Geometry::Build([&](ID2D1GeometrySink* sink) {
            sink->BeginFigure(D2D1::Point2F(x, caretY - size), D2D1_FIGURE_BEGIN_FILLED);
            sink->AddLine(D2D1::Point2F(x - size, caretY));
            sink->AddLine(D2D1::Point2F(x, caretY + size));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        });
    };
    auto outer = triangle(kCaret + 1.5f, box.x + 1.5f);
    auto inner = triangle(kCaret - 0.5f, box.x + 2.f);
    g.FillGeometry(outer.Get(), edge);
    g.FillGeometry(inner.Get(), fill);

    DrawRows(ui, box.Inset(kPadding), open ? raw : 1.f);
    ui.PopTransform();
    ui.PopTransform();
    g.PopLayer();
}

void ThemePopup::DrawRows(UI::Ui& ui, const Rect& inner, float reveal) {
    auto& g = ui.g;
    const auto& palette = Seasonal::Current();
    // Each row comes in a moment after the one above it
    const auto rowIn = [&](int i) { return Anim::EaseOutCubic(Anim::Clamp01((reveal - 0.12f * i / 7.f * 4.f) / 0.55f)); };

    {
        const float k = rowIn(0);
        g.PushOpacity(k);
        g.Text(Loc::Get("nav_theme_title"), { inner.x + 4.f, inner.y, inner.w, 16.f }, palette.accent, UI::Mono(10.f, true));
        g.PopLayer();
    }
    const std::wstring& current = Settings::IconTheme::Current().theme;
    float y = inner.y + kHeadingH;
    int index = 0;
    for (const Look& look : kLooks) {
        const Rect r{ inner.x, y, inner.w, kRowH };
        y += kRowH + kRowGap;
        ++index;
        const float k = rowIn(index);
        g.PushOpacity(k);
        ui.PushTranslate(14.f * (1.f - k), 0.f);

        const UI::Id id = ui.MakeId("theme-row", index);
        const UI::Interaction it = ui.Interact(id, r);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        const float hover = ui.Animate(ui.MakeId("theme-row-hover", index), it.hovered ? 1.f : 0.f, 18.f);
        const float press = ui.Animate(ui.MakeId("theme-row-press", index), it.held ? 1.f : 0.f, 30.f);
        const bool selected = current == look.theme;
        if (selected) g.FillRound(r, 9.f, palette.accent.WithAlpha(0.16f));
        else if (hover > 0.01f) g.FillRound(r, 9.f, Rgba(255, 255, 255, 18).Fade(hover));

        // The icon hops a little under the mouse
        const Rect icon{ r.x + 8.f, r.CY() - 14.f, 28.f, 28.f };
        ui.PushScale(1.f + 0.14f * Anim::EaseOutBack(hover, 0.9f) - 0.08f * press, icon.CX(), icon.CY());
        if (auto image = ThemeIcon(look.theme)) {
            NoColorFilter keep(g);   // each theme's icon in its own colours
            g.DrawImage(*image, icon);
        }
        ui.PopTransform();

        TextStyle name = UI::Mono(11.f, selected);
        name.ellipsis = true;
        const Color text = selected ? Rgba(240, 246, 252) : Lerp(palette.muted, Rgba(225, 235, 245), 0.4f + 0.6f * hover);
        g.Text(Loc::Get(look.label), { icon.R() + 12.f, r.y, r.R() - icon.R() - 40.f, r.h }, text, name);
        if (selected) UI::Glyph(ui, UI::Glyphs::CheckMark, { r.R() - 30.f, r.y, 20.f, r.h }, 12.f, palette.accent);

        ui.PopTransform();
        g.PopLayer();
        if (it.clicked && open) {
            if (!selected) main.ChangeTheme(look.theme);
            Close();
        }
    }
}

// ── Sweep ───────────────────────────────────────────────────────────────────

namespace {
constexpr double kSweep = 0.8;
constexpr float kLean = 0.9f;   // how far the edge leans: x shifts left this much per DIP down
} // namespace

void ThemeTransition::Begin(ComPtr<ID2D1Bitmap1> bitmap, double now) {
    pending.clear();
    active = bitmap != nullptr;
    snapshot = std::move(bitmap);
    startedAt = now;
}

void ThemeTransition::Draw(Graphics& g, const Rect& screen, double now, Color light) {
    if (!active) return;
    const double t = now - startedAt;
    if (t >= kSweep || !snapshot) {
        active = false;
        snapshot.Reset();
        return;
    }
    // The edge's top end goes from past the right side (all old) to past the left one (all new)
    const float p = Anim::EaseInOutCubic(static_cast<float>(t / kSweep));
    const float pad = 40.f;
    const float from = screen.R() + kLean * screen.h + pad, to = screen.x - pad;
    const float top = Anim::Lerp(from, to, p), bottom = top - kLean * screen.h;

    NoColorFilter keep(g);
    // The old look, left of the edge
    const auto old = Geometry::Build([&](ID2D1GeometrySink* sink) {
        sink->BeginFigure(D2D1::Point2F(screen.x - pad - kLean * screen.h, screen.y), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(top, screen.y));
        sink->AddLine(D2D1::Point2F(bottom, screen.B()));
        sink->AddLine(D2D1::Point2F(screen.x - pad - kLean * screen.h, screen.B()));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    });
    g.PushLayer(1.f, old.Get());
    g.DrawBitmap(snapshot.Get(), screen);
    g.PopLayer();

    // The light along the edge: a soft wide band and a sharp line
    g.PushClip(screen);
    g.Line(top, screen.y, bottom, screen.B(), light.Fade(0.18f), 28.f);
    g.Line(top, screen.y, bottom, screen.B(), light.Fade(0.35f), 10.f);
    g.Line(top, screen.y, bottom, screen.B(), light, 2.5f);
    g.PopClip();
}

} // namespace SpeedrunLauncher::Main
