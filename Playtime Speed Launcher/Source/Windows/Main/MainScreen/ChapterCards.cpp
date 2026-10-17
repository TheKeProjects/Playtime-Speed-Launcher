#include "Windows/Main/MainScreen/ChapterCards.h"
#include <algorithm>
#include <vector>
#include "App/Launcher.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Localization/Localization.h"
#include "Seasonal/SeasonalCardArt.h"
#include "Seasonal/SeasonalTheme.h"
#include "Services/Audio/SoundEffects.h"
#include "UI/Icons.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

namespace {

constexpr float kCardW = 340.f, kCardH = 520.f, kSelectedW = 400.f, kSelectedH = 610.f;
constexpr Color kCardBorder = Rgba(13, 42, 59, 50);
constexpr float kButtonPadX = 10.f, kButtonPadY = 5.f;

Color Accent(int alpha) { return Seasonal::Current().accent.WithAlpha(alpha / 255.f); }

std::wstring BannerPath(int chapter) {
    const std::wstring jpg = Paths::Asset(L"Banners\\Chapter " + std::to_wstring(chapter) + L".jpg");
    return Files::Exists(jpg) ? jpg : Paths::Asset(L"Banners\\Chapter " + std::to_wstring(chapter) + L".png");
}

UI::ButtonStyle CardButtonStyle(float height) {
    return { Rgba(9, 20, 30, 180), Accent(140), 1.f, 3.f, { kButtonPadX, kButtonPadY }, 0.f, height };
}

} // namespace

void ChapterCards::Draw(UI::Ui& ui, const Rect& area) {
    auto& l = App::L();
    const size_t count = l.chapters.size();
    const bool seasonal = Seasonal::IsSeasonal();
    // Room above / below the cards for decorations that sit on their top edge
    const float margin = seasonal ? 56.f : 8.f;
    // Selecting a card grows it (and shrinks the previous one) smoothly
    std::vector<float> widths(count), heights(count);
    float contentW = 0.f;
    for (size_t i = 0; i < count; ++i) {
        const bool selected = static_cast<int>(i) == l.selected;
        widths[i] = ui.Animate(ui.MakeId("card-w", static_cast<int64_t>(i)), selected ? kSelectedW : kCardW, 13.f);
        heights[i] = ui.Animate(ui.MakeId("card-h", static_cast<int64_t>(i)), selected ? kSelectedH : kCardH, 13.f);
        contentW += widths[i];
    }
    const float panelH = kSelectedH + margin * 2.f;

    // ScrollViewer Margin="24,0": horizontal only, the 6 px thumb sits below the cards
    const Rect viewportArea = area.Inset(Edges{ 24.f, 0.f });
    const bool scrollable = contentW > viewportArea.w;
    const float viewH = panelH + (scrollable ? 8.f : 0.f);
    const Rect viewport{ viewportArea.x, area.CY() - viewH * 0.5f, viewportArea.w, viewH };
    const UI::Id scrollId = ui.MakeId("cardsScroll");
    UI::ScrollArea scroll(ui, scrollId, viewport, true, 120.f);
    scroll.SetContentSize(contentW);

    float x = viewport.x + (scrollable ? 0.f : (viewport.w - contentW) * 0.5f);
    const float centerY = viewport.y + panelH * 0.5f;
    for (size_t i = 0; i < count; ++i) {
        const bool selected = static_cast<int>(i) == l.selected;
        const float w = widths[i], h = heights[i];
        const Rect r{ x, centerY - h * 0.5f, w, h };
        x += w;

        const auto& chapter = l.chapters[i];
        ui.PushId(static_cast<int64_t>(i));
        const UI::Interaction it = ui.Interact(ui.MakeId("card"), r, true, false);   // hit area stays put while lifting
        if (it.entered) Services::Sfx::Play(Services::Sfx::Hover, true);
        if (it.hovered && ui.In().pressed[0]) main.SelectChapter(static_cast<int>(i));

        // Hovered cards brighten and lift a little; both ease in / out
        float target = selected ? 1.f : chapter.isAvailable ? 0.55f : 0.28f;
        if (!selected && it.hovered) target = 0.85f;
        const float opacity = ui.Animate(ui.MakeId("card-opacity"), target, 16.f);
        const float lift = ui.Animate(ui.MakeId("card-lift"), !selected && it.hovered ? -6.f : 0.f, 16.f);
        DrawCard(ui, i, r.Offset(0.f, lift), selected, opacity);
        ui.PopId();
    }
}

void ChapterCards::DrawCard(UI::Ui& ui, size_t index, const Rect& r, bool selected, float opacity) {
    auto& g = ui.g;
    auto& l = App::L();
    const auto& chapter = l.chapters[index];
    const bool seasonal = Seasonal::IsSeasonal();
    const auto& palette = Seasonal::Current();

    // Seasonal themes: the selected card glows; dimming uses a background-coloured layer instead of
    // opacity so the scenery doesn't show through the card art.
    if (selected && seasonal) g.GlowRound(r, 6.f, palette.accent, 34.f, 0.55f);
    const bool fade = !seasonal && opacity < 1.f;
    if (fade) g.PushOpacity(opacity);

    g.PushRoundClip(r, 6.f);
    if (auto banner = Images::Load(BannerPath(chapter.number))) g.DrawImage(*banner, r, 1.f, Stretch::UniformToFill);
    g.FillLinear(r, 0.f, { { 0.30f, Rgba(0, 0, 0, 0) }, { 0.85f, Rgba(5, 10, 18, 210) } }, { 0.f, 0.f }, { 0.f, 1.f });

    // Bottom stack: title, playtime, action buttons (Margin="12,0,12,12")
    const float innerW = r.w - 24.f;
    TextStyle titleStyle = UI::Mono(12.f);
    titleStyle.weight = DWRITE_FONT_WEIGHT_SEMI_BOLD;
    titleStyle.wrap = true;
    titleStyle.valign = VAlign::Top;
    const std::wstring title = Loc::Get("ch" + std::to_string(chapter.number) + "_title");
    const float titleH = g.Measure(title, titleStyle, innerW).h;
    const std::wstring hours = Services::ChapterPlaytimeStore::Format(l.playtime.Seconds(chapter.number));
    const TextStyle hoursStyle = UI::Mono(10.f);
    const float hoursH = g.Measure(hours, hoursStyle).h;
    const float buttonH = g.Measure(L"+", UI::Mono(12.f, true)).h + kButtonPadY * 2.f + 2.f;
    float y = r.B() - 12.f - buttonH - 8.f - hoursH - 2.f - titleH;
    g.Text(title, { r.x + 12.f, y, innerW, titleH + 2.f }, Rgba(230, 240, 255, 230), titleStyle);
    y += titleH + 2.f;
    g.Text(hours, { r.x + 12.f, y, innerW, hoursH }, Accent(120), hoursStyle);
    y += hoursH + 8.f;
    DrawCardButtons(ui, index, r.x + 12.f, y);

    // "Coming soon" lock
    if (!chapter.isAvailable) {
        g.FillRect(r, Rgba(5, 10, 18, 170));
        const std::wstring locked = Paths::Asset(L"Images\\BLOQUEADO.png");
        const TextStyle soon = UI::Mono(9.f).Center().Wrap();
        const float soonH = g.Measure(Loc::Get("coming_soon"), soon, r.w - 24.f).h;
        const bool hasImage = Files::Exists(locked);
        const float stackH = (hasImage ? 120.f + 10.f : 0.f) + soonH;
        float sy = r.CY() - stackH * 0.5f;
        if (hasImage) {
            const Rect box{ r.CX() - 60.f, sy, 120.f, 120.f };
            g.GlowRound(box, 8.f, Accent(255), 20.f, 0.8f);
            g.Border(box, 8.f, Rgba(9, 20, 30, 160), Accent(255), 2.f);
            if (auto image = Images::Load(locked)) g.DrawImage(*image, box.Center(90.f, 90.f));
            sy += 130.f;
        }
        g.Text(Loc::Get("coming_soon"), { r.x + 12.f, sy, r.w - 24.f, soonH + 2.f }, Accent(120), soon);
    }
    if (seasonal && opacity < 1.f) g.FillRect(r, palette.background.WithAlpha(1.f - opacity));
    g.PopLayer();

    // Border on top (2 px accent when selected)
    if (selected) g.StrokeRound(r, 6.f, palette.accent, 2.f);
    else g.StrokeRound(r, 6.f, kCardBorder, 1.f);
    if (fade) g.PopLayer();

    // Seasonal decorations on the card's top edge (follow the card's opacity)
    if (seasonal) {
        const float decoOpacity = seasonal ? 1.f : opacity;
        Seasonal::CardArt::Draw(g, r, static_cast<int>(index), decoOpacity);
    }
}

void ChapterCards::DrawCardButtons(UI::Ui& ui, size_t index, float x, float y) {
    auto& g = ui.g;
    auto& l = App::L();
    const int number = l.chapters[index].number;
    const auto& mods = l.ModState(index);
    const float buttonH = g.Measure(L"+", UI::Mono(12.f, true)).h + kButtonPadY * 2.f + 2.f;
    const TextStyle smallText = UI::Mono(9.f, true);

    // Save checkpoint button: save glyph + "+"
    {
        const float w = 11.f + 4.f + g.Measure(L"+", UI::Mono(12.f, true)).w + kButtonPadX * 2.f + 2.f;
        UI::ButtonScope b(ui, ui.MakeId("save"), { x, y, w, buttonH }, CardButtonStyle(buttonH));
        UI::Glyph(ui, UI::Glyphs::Save, { b.content.x, b.content.y, 11.f, b.content.h }, 11.f, Accent(200));
        g.Text(L"+", { b.content.x + 15.f, b.content.y, 12.f, b.content.h }, Accent(200), UI::Mono(12.f, true));
        if (b.Clicked()) main.OpenSaveCard(number);
        x += w + 6.f;
    }
    const auto textButton = [&](const char* id, const std::wstring& text, bool installed, bool withIcon) {
        const float iconW = withIcon ? 12.f + 5.f : 0.f;
        const float w = iconW + g.Measure(text, smallText).w + kButtonPadX * 2.f + 2.f;
        if (!installed) g.PushOpacity(0.3f);
        bool clicked = false;
        {
            UI::ButtonScope b(ui, ui.MakeId(id), { x, y, w, buttonH }, CardButtonStyle(buttonH));
            if (withIcon) UI::Icons::LoadManip(g, { b.content.x, b.content.CY() - 6.f, 12.f, 12.f });
            g.Text(text, b.content.CutLeft(iconW), Accent(200), smallText);
            clicked = b.Clicked();
        }
        if (!installed) g.PopLayer();
        x += w + 6.f;
        return clicked;
    };
    if (textButton("ue4ss", L"UE4SS", mods.ue4ss, false)) main.OpenUe4ss(number);
    if ((number == 1 || number == 4 || number == 5) && textButton("manip", L"MANIP", mods.loadManip, true)) main.OpenLoadManip(number);
    if ((number == 1 || number == 5) && textButton("fullbright", L"FULLBRIGHT", mods.fullBright, false)) main.OpenFullBright(number);
}

} // namespace SpeedrunLauncher::Main
