#include "Windows/Main/MainScreen/ChapterInfoPanel.h"
#include <algorithm>
#include <cmath>
#include <map>
#include "App/Launcher.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Seasonal/SeasonalCardArt.h"
#include "Seasonal/SeasonalTheme.h"
#include "UI/Icons.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "Windows/Main/MainScreen/ChapterCards.h"
#include "Windows/Main/MainScreen/MainLayout.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

namespace {

constexpr float kPadX = MainLayout::ContentPad;
constexpr float kColumnMax = 660.f;
constexpr float kChipH = 26.f, kRowH = 34.f, kPlayH = 62.f, kToolH = 32.f;
constexpr float kDescriptionLines = 4.f;

// Screenshot carousel
constexpr double kSlideSeconds = 5.5;    // each screenshot stays this long
constexpr double kFadeSeconds = 0.7;     // crossfade into the next one
constexpr float kDotsH = 30.f;           // room under the carousel for the dots
constexpr float kDot = 6.f, kPillW = 30.f, kDotGap = 8.f;
constexpr int kMaxScreenshots = 5;

// The developers' screenshots for a folder under Assets\Screenshots (1.jpg … 5.jpg), resolved once
const std::vector<std::wstring>& ScreenshotsIn(const std::wstring& folder) {
    static std::map<std::wstring, std::vector<std::wstring>> cache;
    auto it = cache.find(folder);
    if (it != cache.end()) return it->second;
    std::vector<std::wstring> found;
    for (int i = 1; i <= kMaxScreenshots; ++i) {
        const std::wstring path = Paths::Asset(L"Screenshots\\" + folder + L"\\" + std::to_wstring(i) + L".jpg");
        if (Files::Exists(path)) found.push_back(path);
    }
    return cache[folder] = std::move(found);
}

TextStyle EyebrowStyle() { return UI::Mono(11.f, true); }
TextStyle TitleStyle(Font font) {
    // The chapters' own fonts have no bold of their own: only the launcher's font is drawn bold
    TextStyle s = UI::Mono(44.f, font == Font::Mono);
    s.font = font;
    s.wrap = true;
    s.valign = VAlign::Top;
    return s;
}
TextStyle DescriptionStyle(Font font) {
    TextStyle s = UI::Mono(font == Font::Mono ? 14.f : 16.f);
    s.font = font;
    s.wrap = true;
    s.valign = VAlign::Top;
    return s;
}

// The status texts start with a symbol ("● INSTALLED", "▶ PLAYING"): the chip draws its own dot
std::wstring WithoutSymbol(const std::wstring& s) {
    size_t i = 0;
    while (i < s.size() && !iswalnum(s[i])) ++i;
    return s.substr(i);
}

} // namespace

void ChapterInfoPanel::Refresh() {
    auto& l = App::L();
    multiplayer = l.InMultiplayers();
    picked = l.HasSelection();
    if (!picked) {
        // Nothing picked yet in this tab: the hero only asks to pick a card
        title = Loc::Get(multiplayer ? "hero_pick_multiplayer" : "hero_pick_chapter");
        description = Loc::Get("hero_pick_hint");
        eyebrow = Loc::Get(multiplayer ? "section_multiplayers" : "section_chapters");
        titleFont = bodyFont = multiplayer ? Font::Mono : Font::Poppy;
        screenshots.clear();
        canPlay = running = false;
        return;
    }
    if (multiplayer) {
        // Multiplayer games: Steam only, no versions
        const auto& game = l.SelectedMultiplayer();
        title = Loc::Get("mp_" + game.key + "_title");
        description = Loc::Get("mp_" + game.key + "_desc");
        eyebrow = Loc::Get("section_multiplayers");
        banner = Paths::Asset(L"Banners\\" + game.banner);
        // "Multiplayers\EFP.jpg" → Screenshots\Multiplayers\EFP
        screenshots = ScreenshotsIn(L"Multiplayers\\" + Files::FileNameWithoutExtension(game.banner));
        variant = 100 + l.selectedMultiplayer;
        titleFont = bodyFont = Font::Mono;
        version.clear();
        canPlay = game.IsInstalled();
        running = canPlay && !Process::FindByName(Files::FileNameWithoutExtension(*game.gameExePath)).empty();
        state = running ? State::Playing : canPlay ? State::Installed : game.isAvailable ? State::NotFound : State::ComingSoon;
    } else {
        const auto& ch = l.Selected();
        const std::string n = std::to_string(ch.number);
        title = Loc::Get("ch" + n + "_title");
        description = Loc::Get("ch" + n + "_desc");
        eyebrow = Loc::Get("store_chapter", { std::to_wstring(ch.number) });
        banner = ChapterCards::BannerPath(ch.number);
        screenshots = ScreenshotsIn(L"Chapter " + std::to_wstring(ch.number));
        variant = l.selected;
        titleFont = bodyFont = Font::Poppy;   // the Poppy Playtime font (VCR OSD Mono) for every chapter

        if (auto selected = l.store.SelectedPath(ch.number)) {
            std::wstring name = Files::FileNameWithoutExtension(*selected);
            for (const auto& custom : l.store.Customs(ch.number))
                if (custom.exePath == *selected) name = custom.name;
            version = name;
        } else if (l.epic.IsEnabled() && l.epic.ExePath(ch.number)) {
            version = Loc::Get("version_auto_epic");
        } else if (l.epic.IsEnabled()) {
            version = ch.isAvailable ? Loc::Get("version_not_installed") : Loc::Get("version_none");
        } else {
            version = ch.IsInstalled() ? Loc::Get("version_auto_steam") : ch.isAvailable ? Loc::Get("version_not_installed") : Loc::Get("version_none");
        }

        auto exe = l.ActiveExePath(ch);
        canPlay = exe && !exe->empty() && Files::Exists(*exe);
        running = exe && !exe->empty() && !Process::FindByName(Files::FileNameWithoutExtension(*exe)).empty();
        state = running ? State::Playing : canPlay ? State::Installed : ch.isAvailable ? State::NotFound : State::ComingSoon;
    }
    status = WithoutSymbol(Loc::Get(state == State::Playing     ? "status_playing"
                                    : state == State::Installed ? "status_installed"
                                    : state == State::NotFound  ? "status_not_found"
                                                                : "status_coming_soon"));
}

void ChapterInfoPanel::Draw(UI::Ui& ui, const Rect& area) {
    auto& g = ui.g;

    // Chapter change: the new one fades in, the text rising and the poster sliding in from the right
    if (title != shownTitle) {
        if (!shownTitle.empty()) changedAt = ui.Time();
        shownTitle = title;
        // The carousel starts over from the first screenshot
        slide = 0;
        previousSlide = -1;
        slideStartedAt = ui.Time();
        slideChangedAt = -10.0;
    }
    const float enter = Anim::EaseOutCubic(static_cast<float>((ui.Time() - changedAt) / 0.4));
    if (enter < 1.f) ui.KeepAnimating();

    if (!picked) {
        if (enter < 1.f) {
            g.PushOpacity(enter);
            ui.PushTranslate(0.f, (1.f - enter) * 12.f);
        }
        DrawPrompt(ui, area);
        if (enter < 1.f) {
            ui.PopTransform();
            g.PopLayer();
        }
        return;
    }

    // On the right (only when there's room for it next to the text): the screenshot carousel, a
    // 16:9 frame with its dots underneath; the chapter's poster when it has no screenshots
    Rect poster{};
    if (!screenshots.empty()) {
        if (area.w >= 1000.f && area.h >= 300.f) {
            const float maxH = std::min(area.h - 48.f - kDotsH, 520.f);
            const float w = std::min(area.w * 0.44f, maxH * 16.f / 9.f);
            const float h = w * 9.f / 16.f;
            poster = { area.R() - kPadX - w, area.CY() - (h + kDotsH) * 0.5f, w, h };
            DrawCarousel(ui, poster, enter);
        }
    } else if (area.w >= 1000.f && area.h >= 360.f) {
        if (const ImagePtr art = Images::Load(banner); art && art->h > 0.f) {
            const float h = std::min(area.h - 64.f, 600.f);
            const float w = std::min(h * art->w / art->h, area.w * 0.36f);
            poster = { area.R() - kPadX - w, area.CY() - h * 0.5f, w, h };
            DrawArt(ui, area, poster, enter);
        }
    }

    // Text column, vertically centred
    const float x = area.x + kPadX;
    const float right = poster.w > 0.f ? poster.x - 56.f : area.R() - kPadX;
    const float width = std::min(kColumnMax, right - x);
    const float h = ContentHeight(ui, width);
    const float y = std::max(area.y + 24.f, area.CY() - h * 0.5f);

    if (enter < 1.f) {
        g.PushOpacity(enter);
        ui.PushTranslate(0.f, (1.f - enter) * 12.f);
    }
    DrawContent(ui, x, y, width);
    if (enter < 1.f) {
        ui.PopTransform();
        g.PopLayer();
    }
}

void ChapterInfoPanel::DrawBackdrop(UI::Ui& ui, const Rect& area) {
    if (Seasonal::IsSeasonal() || !picked) return;
    auto& g = ui.g;
    const auto& palette = Seasonal::Current();
    const Color bg = palette.background;
    const float enter = title == shownTitle ? Anim::EaseOutCubic(static_cast<float>((ui.Time() - changedAt) / 0.6)) : 0.f;

    g.PushClip(area);
    // The image, faint; it follows the carousel's crossfade
    const auto drawImage = [&](const std::wstring& path, float opacity) {
        if (opacity <= 0.f) return;
        if (const ImagePtr image = Images::Load(path)) g.DrawImage(*image, area, opacity * 0.24f * enter, Stretch::UniformToFill);
    };
    if (!screenshots.empty()) {
        const int count = static_cast<int>(screenshots.size());
        const int current = std::clamp(slide, 0, count - 1);
        const float fade = static_cast<float>((ui.Time() - slideChangedAt) / kFadeSeconds);
        if (previousSlide >= 0 && previousSlide < count && fade < 1.f) {
            drawImage(screenshots[static_cast<size_t>(previousSlide)], 1.f);
            drawImage(screenshots[static_cast<size_t>(current)], Anim::EaseInOutSine(fade));
        } else {
            drawImage(screenshots[static_cast<size_t>(current)], 1.f);
        }
    } else if (!banner.empty()) {
        drawImage(banner, 0.8f);
    }
    // Readability: darker behind the text column (left) and the shelf (bottom)
    g.FillLinear(area, 0.f, { { 0.f, bg.WithAlpha(0.92f) }, { 0.4f, bg.WithAlpha(0.6f) }, { 1.f, bg.WithAlpha(0.2f) } },
                 { 0.f, 0.f }, { 1.f, 0.f });
    g.FillLinear(area, 0.f, { { 0.f, bg.WithAlpha(0.4f) }, { 0.35f, bg.WithAlpha(0.f) }, { 0.62f, bg.WithAlpha(0.35f) }, { 1.f, bg.WithAlpha(0.95f) } },
                 { 0.f, 0.f }, { 0.f, 1.f });
    // A soft accent glow under the carousel's side
    g.FillRadial({ area.x + area.w * 0.45f, area.y - area.h * 0.15f, area.w * 0.75f, area.h * 0.9f },
                 { { 0.f, palette.accent.WithAlpha(0.07f * enter) }, { 1.f, palette.accent.WithAlpha(0.f) } });
    g.PopClip();
    if (enter < 1.f) ui.KeepAnimating();
}

// The chapter's poster on the right.
void ChapterInfoPanel::DrawArt(UI::Ui& ui, const Rect& area, const Rect& poster, float enter) {
    auto& g = ui.g;
    const auto& palette = Seasonal::Current();
    const bool seasonal = Seasonal::IsSeasonal();
    const ImagePtr art = Images::Load(banner);
    if (!art || poster.w <= 0.f) return;

    const Rect r = poster.Offset((1.f - enter) * 28.f, 0.f);
    g.PushOpacity(enter);
    g.GlowRound(r, 14.f, palette.accent, 40.f, seasonal ? 0.55f : 0.35f);
    g.Shadow(r, 14.f, 30.f, Rgba(0, 0, 0, 160));
    g.PushRoundClip(r, 14.f);
    g.DrawImage(*art, r, 1.f, Stretch::UniformToFill);
    // A soft sheen at the top and a dark foot
    g.FillLinear(r, 0.f, { { 0.f, Rgba(255, 255, 255, 18) }, { 0.25f, Rgba(255, 255, 255, 0) }, { 0.75f, Rgba(0, 0, 0, 0) }, { 1.f, Rgba(0, 0, 0, 120) } },
                 { 0.f, 0.f }, { 0.f, 1.f });
    DrawLock(ui, r);
    g.PopLayer();
    g.StrokeRound(r, 14.f, palette.accent.WithAlpha(0.55f), 1.5f);
    if (seasonal) Seasonal::CardArt::Draw(g, r, variant, 1.f);
    g.PopLayer();
}

// Nothing picked yet: eyebrow, a big "Pick a chapter", a hint and an arrow bobbing towards the
// cards below.
void ChapterInfoPanel::DrawPrompt(UI::Ui& ui, const Rect& area) {
    auto& g = ui.g;
    const auto& palette = Seasonal::Current();
    const bool seasonal = Seasonal::IsSeasonal();
    const float x = area.x + kPadX;
    const float width = std::min(kColumnMax, area.R() - kPadX - x);
    const TextStyle titleStyle = TitleStyle(titleFont);
    const TextStyle hintStyle = DescriptionStyle(bodyFont);
    const float titleH = g.Measure(title, titleStyle, width).h;
    const float hintH = g.Measure(description, hintStyle, width).h;
    constexpr float kArrowH = 40.f;
    const float h = 16.f + 8.f + titleH + 16.f + hintH + 28.f + kArrowH;
    float y = std::max(area.y + 24.f, area.CY() - h * 0.5f);

    g.FillRound({ x, y + 7.f, 22.f, 2.f }, 1.f, palette.accent);
    g.Text(eyebrow, { x + 32.f, y, width - 32.f, 16.f }, palette.accent, EyebrowStyle());
    y += 16.f + 8.f;

    const Rect titleRect{ x, y, width, titleH + 4.f };
    if (seasonal) g.GlowText(title, titleRect, titleStyle, palette.title, 26.f, 0.7f);
    g.Text(title, titleRect, seasonal ? palette.title : Rgba(240, 246, 252), titleStyle);
    y += titleH + 16.f;

    g.Text(description, { x, y, width, hintH + 4.f }, Lerp(palette.muted, Rgba(200, 215, 228), 0.45f), hintStyle);
    y += hintH + 28.f;

    // Down arrow in a ring, bobbing (rests while a game is running)
    const float bob = main.Resting() ? 0.f : Anim::EaseInOutSine(Anim::PingPong(ui.Time(), 1.1)) * 6.f;
    if (!main.Resting()) ui.KeepAnimatingThrottled();
    const Rect ring{ x, y, kArrowH, kArrowH };
    g.GlowRound(ring, kArrowH * 0.5f, palette.accent, 16.f, 0.25f);
    g.Border(ring, kArrowH * 0.5f, palette.accent.WithAlpha(0.1f), palette.accent.WithAlpha(0.5f), 1.f);
    UI::Glyph(ui, UI::Glyphs::ChevronDown, ring.Offset(0.f, bob - 2.f), 14.f, palette.accent);
}

// "Coming soon": darkened with the lock in the middle
void ChapterInfoPanel::DrawLock(UI::Ui& ui, const Rect& r) {
    if (state != State::ComingSoon) return;
    auto& g = ui.g;
    const auto& palette = Seasonal::Current();
    g.FillRect(r, Rgba(5, 10, 18, 170));
    if (const ImagePtr lock = Images::Load(Paths::Asset(L"Images\\BLOQUEADO.png"))) {
        const Rect box = r.Center(120.f, 120.f);
        g.GlowRound(box, 10.f, palette.accent, 20.f, 0.8f);
        g.Border(box, 10.f, Rgba(9, 20, 30, 160), palette.accent, 2.f);
        g.DrawImage(*lock, box.Center(90.f, 90.f));
    }
}

// The developers' screenshots, one after another: each holds kSlideSeconds with a slow zoom and
// then crossfades into the next. Under the frame one dot per screenshot; the current one is a
// wider pill that fills up while it's shown. Hovering the frame pauses it; clicking a dot jumps.
void ChapterInfoPanel::DrawCarousel(UI::Ui& ui, const Rect& frame, float enter) {
    auto& g = ui.g;
    const auto& palette = Seasonal::Current();
    const bool seasonal = Seasonal::IsSeasonal();
    const int count = static_cast<int>(screenshots.size());
    const double now = ui.Time();
    slide = std::clamp(slide, 0, count - 1);

    const auto goTo = [&](int index) {
        if (index == slide) return;
        previousSlide = slide;
        slide = index;
        slideStartedAt = now;
        slideChangedAt = now;
    };

    // Paused while hovered or while the launcher rests (a game is running): the progress holds
    const bool paused = ui.MouseOver(frame) || main.Resting();
    if (paused) slideStartedAt += ui.Dt();
    float progress = static_cast<float>((now - slideStartedAt) / kSlideSeconds);
    if (count > 1 && progress >= 1.f) {
        goTo((slide + 1) % count);
        progress = 0.f;
    }
    progress = std::clamp(progress, 0.f, 1.f);
    const float fade = static_cast<float>((now - slideChangedAt) / kFadeSeconds);
    const bool fading = previousSlide >= 0 && fade < 1.f;
    if (fading) ui.KeepAnimating();
    else if (count > 1 && !main.Resting()) ui.KeepAnimatingThrottled();

    // Each slide zooms in slowly from the moment it starts showing (incl. its crossfade)
    const auto drawSlide = [&](int index, const Rect& r, float opacity, double startedAt) {
        const ImagePtr image = Images::Load(screenshots[static_cast<size_t>(index)]);
        if (!image || opacity <= 0.f) return;
        const float t = std::clamp(static_cast<float>((now - startedAt) / (kSlideSeconds + kFadeSeconds)), 0.f, 1.f);
        const float zoom = 1.f + 0.05f * t;
        const Rect z{ r.CX() - r.w * zoom * 0.5f, r.CY() - r.h * zoom * 0.5f, r.w * zoom, r.h * zoom };
        g.DrawImage(*image, z, opacity, Stretch::UniformToFill);
    };

    const Rect r = frame.Offset((1.f - enter) * 28.f, 0.f);
    g.PushOpacity(enter);
    g.GlowRound(r, 14.f, palette.accent, 40.f, seasonal ? 0.55f : 0.35f);
    g.Shadow(r, 14.f, 30.f, Rgba(0, 0, 0, 160));
    g.PushRoundClip(r, 14.f);
    g.FillRect(r, palette.bar);
    if (fading) {
        drawSlide(previousSlide, r, 1.f, slideChangedAt - kSlideSeconds);
        drawSlide(slide, r, Anim::EaseInOutSine(fade), slideStartedAt);
    } else {
        drawSlide(slide, r, 1.f, slideStartedAt);
    }
    Images::Load(screenshots[static_cast<size_t>((slide + 1) % count)]);   // the next one ready before its turn
    // A soft sheen at the top and a dark foot
    g.FillLinear(r, 0.f, { { 0.f, Rgba(255, 255, 255, 14) }, { 0.2f, Rgba(255, 255, 255, 0) }, { 0.7f, Rgba(0, 0, 0, 0) }, { 1.f, Rgba(0, 0, 0, 110) } },
                 { 0.f, 0.f }, { 0.f, 1.f });
    DrawLock(ui, r);
    g.PopLayer();
    g.StrokeRound(r, 14.f, palette.accent.WithAlpha(0.55f), 1.5f);
    if (seasonal) Seasonal::CardArt::Draw(g, r, variant, 1.f);

    // ── Dots ──
    if (count > 1) {
        float widths[kMaxScreenshots];
        float total = 0.f;
        for (int i = 0; i < count; ++i) {
            widths[i] = ui.Animate(ui.MakeId("carousel-dot-w", i), i == slide ? kPillW : kDot, 14.f);
            total += widths[i] + (i ? kDotGap : 0.f);
        }
        float x = r.CX() - total * 0.5f;
        const float cy = r.B() + kDotsH * 0.5f + 2.f;
        for (int i = 0; i < count; ++i) {
            const Rect dot{ x, cy - kDot * 0.5f, widths[i], kDot };
            const UI::Interaction it = ui.Interact(ui.MakeId("carousel-dot", i), dot.Inset(Edges{ -kDotGap * 0.5f, -8.f }));
            if (it.hovered && i != slide) ui.SetCursor(IDC_HAND);
            if (it.clicked) goTo(i);
            const float hover = ui.Animate(ui.MakeId("carousel-dot-hover", i), it.hovered ? 1.f : 0.f, 16.f);
            if (i == slide) {
                // The pill: a dim track that fills with the accent from the left
                g.FillRound(dot, kDot * 0.5f, palette.accent.WithAlpha(0.22f));
                const float fill = std::max(kDot, dot.w * progress);
                g.GlowRound({ dot.x, dot.y, fill, dot.h }, kDot * 0.5f, palette.accent, 8.f, 0.45f);
                g.FillRound({ dot.x, dot.y, fill, dot.h }, kDot * 0.5f, palette.accent);
            } else {
                g.FillRound(dot, kDot * 0.5f, Rgba(255, 255, 255, static_cast<uint8_t>(60 + 90 * hover)));
            }
            x += widths[i] + kDotGap;
        }
    }
    g.PopLayer();
}

float ChapterInfoPanel::ContentHeight(UI::Ui& ui, float width) {
    auto& g = ui.g;
    const float lineH = g.Measure(L"Ag", DescriptionStyle(bodyFont)).h;
    float h = 16.f + 8.f;                                   // eyebrow
    h += g.Measure(title, TitleStyle(titleFont), width).h + 16.f;    // title
    h += kChipH + 18.f;                                     // chips
    h += std::min(g.Measure(description, DescriptionStyle(bodyFont), width).h, lineH * kDescriptionLines) + 26.f;
    if (!multiplayer) h += kRowH + 22.f;                    // version · platform
    h += kPlayH;                                            // PLAY
    if (!multiplayer) h += 26.f + 14.f + 8.f + kToolH;      // tools
    return h;
}

void ChapterInfoPanel::DrawContent(UI::Ui& ui, float x, float y, float width) {
    auto& g = ui.g;
    auto& l = App::L();
    const auto& palette = Seasonal::Current();
    const bool seasonal = Seasonal::IsSeasonal();

    // ── Eyebrow: "CHAPTER 3" with a short accent line ──
    g.FillRound({ x, y + 7.f, 22.f, 2.f }, 1.f, palette.accent);
    g.Text(eyebrow, { x + 32.f, y, width - 32.f, 16.f }, palette.accent, EyebrowStyle());
    y += 16.f + 8.f;

    // ── Title ──
    const TextStyle titleStyle = TitleStyle(titleFont);
    const float titleH = g.Measure(title, titleStyle, width).h;
    const Rect titleRect{ x, y, width, titleH + 4.f };
    if (seasonal) g.GlowText(title, titleRect, titleStyle, palette.title, 26.f, 0.7f);
    else g.GlowText(title, titleRect, titleStyle, palette.accent, 30.f, 0.22f);
    g.Text(title, titleRect, seasonal ? palette.title : Rgba(240, 246, 252), titleStyle);
    y += titleH + 16.f;

    // ── Chips: status · playtime ──
    {
        const TextStyle chipText = UI::Mono(10.f, true);
        const Color statusColor = state == State::Playing     ? palette.accent
                                  : state == State::Installed ? Hex(0x22CC88)
                                  : state == State::NotFound  ? Hex(0xE8A000)
                                                              : palette.muted;
        float cx = x;
        const float sw = g.Measure(status, chipText).w + 12.f + 10.f + 14.f;
        const Rect s{ cx, y, sw, kChipH };
        g.Border(s, kChipH * 0.5f, statusColor.WithAlpha(0.12f), statusColor.WithAlpha(0.45f), 1.f);
        if (state == State::Playing) {
            const float pulse = Anim::EaseInOutSine(Anim::PingPong(ui.Time(), 1.4));
            g.FillEllipse(s.x + 13.f, s.CY(), 6.f, 6.f, statusColor.WithAlpha(0.3f * pulse));
            if (!main.Resting()) ui.KeepAnimatingThrottled();
        }
        g.FillEllipse(s.x + 13.f, s.CY(), 3.5f, 3.5f, statusColor);
        g.Text(status, { s.x + 22.f, s.y, sw - 22.f, s.h }, statusColor, chipText);
        cx += sw + 8.f;

        const auto seconds = multiplayer ? l.playtime.Seconds(l.SelectedMultiplayer().playtimeId) : l.playtime.Seconds(l.Selected().number);
        const std::wstring played = Services::ChapterPlaytimeStore::Format(seconds);
        const float pw = g.Measure(played, chipText).w + 12.f + 14.f + 6.f + 12.f;
        const Rect p{ cx, y, pw, kChipH };
        g.Border(p, kChipH * 0.5f, Rgba(255, 255, 255, 10), Rgba(255, 255, 255, 30), 1.f);
        UI::Glyph(ui, wchar_t(0xE916), { p.x + 10.f, p.y, 14.f, p.h }, 11.f, palette.muted);   // stopwatch
        g.Text(played, { p.x + 10.f + 14.f + 6.f, p.y, pw, p.h }, Rgba(200, 215, 228), chipText);
    }
    y += kChipH + 18.f;

    // ── Description (at most a few lines) ──
    {
        const TextStyle style = DescriptionStyle(bodyFont);
        const float lineH = g.Measure(L"Ag", style).h;
        const float h = std::min(g.Measure(description, style, width).h, lineH * kDescriptionLines);
        g.PushClip({ x, y, width, h + 2.f });
        g.Text(description, { x, y, width, h + 40.f }, Lerp(palette.muted, Rgba(200, 215, 228), 0.45f), style);
        g.PopClip();
        y += h + 26.f;
    }

    // ── Version picker · Steam / Epic ──
    if (!multiplayer) {
        DrawVersionRow(ui, x, y);
        y += kRowH + 22.f;
    }

    // ── PLAY ──
    DrawPlay(ui, x, y);
    y += kPlayH;

    // ── Tools ──
    if (!multiplayer) {
        y += 26.f;
        g.FillRound({ x, y + 6.f, 12.f, 2.f }, 1.f, palette.muted.WithAlpha(0.7f));
        g.Text(Loc::Get("hero_tools"), { x + 20.f, y, width - 20.f, 14.f }, palette.muted, UI::Mono(10.f, true));
        y += 14.f + 8.f;
        DrawTools(ui, x, y);
    }
}

void ChapterInfoPanel::DrawVersionRow(UI::Ui& ui, float x, float y) {
    auto& g = ui.g;
    auto& l = App::L();
    const auto& palette = Seasonal::Current();
    const TextStyle labelStyle = UI::Mono(9.f, true);
    const TextStyle valueStyle = UI::Mono(12.f, true);

    // Version: "VERSION  Patch 1.3  ⌄" (chapters that aren't out yet have none to pick)
    if (l.Selected().isAvailable) {
        const std::wstring label = WithoutSymbol(Loc::Get("version_prefix"));
        const std::wstring cleanLabel = label.size() && label.back() == L':' ? label.substr(0, label.size() - 1) : label;
        const float lw = g.Measure(cleanLabel, labelStyle).w, vw = g.Measure(version, valueStyle).w;
        const float w = 14.f + lw + 10.f + vw + 10.f + 12.f + 14.f;
        UI::ButtonScope b(ui, ui.MakeId("versionBtn"), { x, y, w, kRowH },
                          { Rgba(9, 20, 30, 190), palette.accent.WithAlpha(0.35f), 1.f, kRowH * 0.5f, { 0.f }, 0.f, kRowH });
        float tx = x + 14.f;
        g.Text(cleanLabel, { tx, y, lw + 2.f, kRowH }, palette.muted, labelStyle);
        tx += lw + 10.f;
        g.Text(version, { tx, y, vw + 2.f, kRowH }, palette.accent, valueStyle);
        tx += vw + 10.f;
        UI::Glyph(ui, UI::Glyphs::ChevronDown, { tx, y, 12.f, kRowH }, 10.f, palette.accent.WithAlpha(0.7f));
        if (b.Clicked()) main.OpenVersions();
        x += w + 10.f;
    }

    // Steam | Epic segmented switch
    const bool epic = l.epic.IsEnabled();
    const TextStyle segStyle = UI::Mono(10.f, true).Center();
    const float segW = 64.f;
    const Rect frame{ x, y, segW * 2.f + 6.f, kRowH };
    g.Border(frame, kRowH * 0.5f, Rgba(9, 20, 30, 190), Rgba(26, 58, 85, 200), 1.f);
    const float knobX = ui.Animate(ui.MakeId("platform-knob"), epic ? 1.f : 0.f, 16.f);
    const Color knobColor = Lerp(Rgba(42, 110, 160), Rgba(232, 120, 0), knobX);
    const Rect knob{ frame.x + 3.f + knobX * segW, frame.y + 3.f, segW, kRowH - 6.f };
    g.FillRound(knob, knob.h * 0.5f, knobColor.WithAlpha(0.28f));
    g.StrokeRound(knob, knob.h * 0.5f, knobColor.WithAlpha(0.8f), 1.f);
    const wchar_t* names[2] = { L"STEAM", L"EPIC" };
    for (int i = 0; i < 2; ++i) {
        const Rect seg{ frame.x + 3.f + i * segW, frame.y + 3.f, segW, kRowH - 6.f };
        const UI::Interaction it = ui.Interact(ui.MakeId("platform-seg", i), seg);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        const bool on = (i == 1) == epic;
        if (it.clicked && !on) main.TogglePlatform();
        const Color text = on ? Rgba(235, 242, 250) : it.hovered ? Rgba(150, 175, 195) : palette.muted;
        g.Text(names[i], seg, text, segStyle);
    }
}

void ChapterInfoPanel::DrawPlay(UI::Ui& ui, float x, float y) {
    auto& g = ui.g;
    const auto& palette = Seasonal::Current();
    const std::wstring text = running ? status : Loc::Get(multiplayer ? "play_button_mp" : "play_button");
    const TextStyle style = UI::Mono(17.f, true);
    const float contentW = (running ? 0.f : 18.f + 12.f) + g.Measure(text, style).w;
    const float w = std::max(300.f, contentW + 64.f);
    const Rect play{ x, y, w, kPlayH };
    const bool enabled = canPlay && !running;

    if (Seasonal::IsSeasonal()) {
        const float t = Anim::EaseInOutSine(Anim::PingPong(Seasonal::LastAnimationTime(), 3.2));
        g.GlowRound(play, 10.f, palette.playBackground, 30.f, 0.3f + 0.55f * t);
        if (!Seasonal::AnimationStopped()) ui.KeepAnimatingThrottled();
    } else if (enabled) {
        g.GlowRound(play, 10.f, palette.playBackground, 24.f, 0.35f);
    }
    if (!canPlay) g.PushOpacity(0.35f);
    {
        UI::ButtonScope b(ui, ui.MakeId("play"), play, { palette.playBackground, Colors::Transparent, 0.f, 10.f, { 0.f }, 300.f, kPlayH }, enabled);
        // Light sweep across the top half
        g.FillLinear({ play.x, play.y, play.w, play.h * 0.5f }, 0.f, { { 0.f, Rgba(255, 255, 255, 28) }, { 1.f, Rgba(255, 255, 255, 0) } },
                     { 0.f, 0.f }, { 0.f, 1.f });
        if (enabled && !Seasonal::IsSeasonal() && !main.Resting()) {
            constexpr double kEvery = 4.5, kSweep = 0.9;
            const float t = static_cast<float>(std::fmod(ui.Time(), kEvery) / kSweep);
            if (t < 1.f) {
                constexpr float kBand = 90.f;
                const float bx = play.x - kBand + (play.w + kBand) * Anim::EaseInOutSine(t);
                g.PushRoundClip(play, 10.f);
                g.FillLinear({ bx, play.y, kBand, play.h }, 0.f,
                             { { 0.f, Rgba(255, 255, 255, 0) }, { 0.5f, Rgba(255, 255, 255, 50) }, { 1.f, Rgba(255, 255, 255, 0) } },
                             { 0.f, 0.f }, { 1.f, 0.f });
                g.PopLayer();
            }
            ui.KeepAnimatingThrottled();
        }
        float cx = play.CX() - contentW * 0.5f;
        if (!running) {
            UI::Glyph(ui, UI::Glyphs::Play, { cx, y, 18.f, kPlayH }, 18.f, palette.playForeground);
            cx += 18.f + 12.f;
        }
        g.Text(text, { cx, y, contentW + 4.f, kPlayH }, palette.playForeground, style);
        if (b.Clicked()) main.PlaySelected();
    }
    if (!canPlay) g.PopLayer();
}

void ChapterInfoPanel::DrawTools(UI::Ui& ui, float x, float y) {
    auto& g = ui.g;
    auto& l = App::L();
    const auto& palette = Seasonal::Current();
    const size_t index = static_cast<size_t>(l.selected);
    const int number = l.chapters[index].number;
    const auto& mods = l.ModState(index);
    const TextStyle text = UI::Mono(10.f, true);

    // Icon + label chip; a lit dot when the mod is installed, dimmed when it isn't
    enum class Icon { None, Save, Manip };
    const auto tool = [&](const char* id, const std::wstring& label, Icon icon, bool installed, bool showDot) {
        const float iconW = icon == Icon::None ? 0.f : 14.f + 7.f;
        const float dotW = showDot ? 6.f + 8.f : 0.f;
        const float w = 14.f + iconW + g.Measure(label, text).w + dotW + 14.f;
        if (!installed) g.PushOpacity(0.45f);
        bool clicked = false;
        {
            UI::ButtonScope b(ui, ui.MakeId(id), { x, y, w, kToolH },
                              { Rgba(9, 20, 30, 200), palette.accent.WithAlpha(0.4f), 1.f, 8.f, { 0.f }, 0.f, kToolH });
            float tx = x + 14.f;
            if (icon == Icon::Save) UI::Glyph(ui, UI::Glyphs::Save, { tx, y, 14.f, kToolH }, 12.f, palette.accent);
            if (icon == Icon::Manip) UI::Icons::LoadManip(g, { tx, y + kToolH * 0.5f - 7.f, 14.f, 14.f });
            tx += iconW;
            const float lw = g.Measure(label, text).w;
            g.Text(label, { tx, y, lw + 2.f, kToolH }, Rgba(215, 228, 238), text);
            tx += lw + 8.f;
            if (showDot) g.FillEllipse(tx + 3.f, y + kToolH * 0.5f, 3.f, 3.f, installed ? Hex(0x22CC88) : palette.muted);
            clicked = b.Clicked();
        }
        if (!installed) g.PopLayer();
        x += w + 8.f;
        return clicked;
    };
    if (tool("save", Loc::Get("hero_saves"), Icon::Save, true, false)) main.OpenSaveCard(number);
    if (tool("ue4ss", L"UE4SS", Icon::None, mods.ue4ss, true)) main.OpenUe4ss(number);
    const bool hasManip = number == 1 || number == 4 || number == 5;
    if (hasManip && tool("manip", L"LOAD MANIP", Icon::Manip, mods.loadManip, true)) main.OpenLoadManip(number);
    if (hasManip && tool("fullbright", L"FULLBRIGHT", Icon::None, mods.fullBright, true)) main.OpenFullBright(number);
}

} // namespace SpeedrunLauncher::Main
