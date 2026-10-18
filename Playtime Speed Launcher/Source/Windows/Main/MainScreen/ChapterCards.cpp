#include "Windows/Main/MainScreen/ChapterCards.h"
#include <algorithm>
#include <map>
#include <vector>
#include "App/Launcher.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Graphics/Animation.h"
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

using Section = App::Launcher::Section;

constexpr float kCardW = 340.f, kCardH = 520.f, kSelectedW = 400.f, kSelectedH = 610.f;
constexpr Color kCardBorder = Rgba(13, 42, 59, 50);
constexpr float kButtonPadX = 10.f, kButtonPadY = 5.f;
constexpr float kTabsH = 56.f, kTabH = 38.f;

Color Accent(int alpha) { return Seasonal::Current().accent.WithAlpha(alpha / 255.f); }

// Resolved once per chapter: the assets don't change while the launcher runs, and this is asked every frame
const std::wstring& BannerPath(int chapter) {
    static std::map<int, std::wstring> paths;
    auto it = paths.find(chapter);
    if (it != paths.end()) return it->second;
    const std::wstring jpg = Paths::Asset(L"Banners\\Chapter " + std::to_wstring(chapter) + L".jpg");
    return paths[chapter] = Files::Exists(jpg) ? jpg : Paths::Asset(L"Banners\\Chapter " + std::to_wstring(chapter) + L".png");
}

UI::ButtonStyle CardButtonStyle(float height) {
    return { Rgba(9, 20, 30, 180), Accent(140), 1.f, 3.f, { kButtonPadX, kButtonPadY }, 0.f, height };
}

} // namespace

void ChapterCards::Draw(UI::Ui& ui, const Rect& area) {
    DrawTabs(ui, area.Top(kTabsH));

    // Switching tabs: the new cards fade in sliding from the side of the tab that was picked
    auto& l = App::L();
    if (l.section != shownSection) {
        shownSection = l.section;
        sectionChangedAt = ui.Time();
    }
    const float enter = Anim::EaseOutCubic(static_cast<float>((ui.Time() - sectionChangedAt) / 0.38));
    const bool entering = enter < 1.f;
    if (entering) {
        const float from = l.InMultiplayers() ? 60.f : -60.f;
        ui.g.PushOpacity(enter);
        ui.PushTranslate((1.f - enter) * from, 0.f);
        ui.KeepAnimating();
    }
    DrawCards(ui, area.CutTop(kTabsH));
    if (entering) {
        ui.PopTransform();
        ui.g.PopLayer();
    }
}

// Pill with the two tabs; a glowing indicator slides under the selected one.
void ChapterCards::DrawTabs(UI::Ui& ui, const Rect& area) {
    auto& g = ui.g;
    auto& l = App::L();
    const auto& palette = Seasonal::Current();
    struct Tab {
        Section section;
        wchar_t glyph;
        std::wstring text;
        size_t count;
    };
    const Tab tabs[] = {
        { Section::Chapters, wchar_t(0xE8F1), Loc::Get("section_chapters"), l.chapters.size() },        // library
        { Section::Multiplayers, wchar_t(0xE716), Loc::Get("section_multiplayers"), l.multiplayers.size() },   // people
    };
    const TextStyle label = UI::Mono(11.f, true);
    const TextStyle chip = UI::Mono(9.f, true).Center();
    constexpr float kPad = 4.f, kGap = 4.f, kChipW = 22.f;
    float textW[2], widths[2];
    float total = kPad * 2.f + kGap;
    for (int i = 0; i < 2; ++i) {
        textW[i] = g.Measure(tabs[i].text, label).w;
        widths[i] = 16.f + 14.f + 8.f + textW[i] + 10.f + kChipW + 14.f;
        total += widths[i];
    }
    const Rect bar{ area.CX() - total * 0.5f, area.y + 12.f + (area.h - 12.f - kTabH - kPad * 2.f) * 0.5f, total, kTabH + kPad * 2.f };
    g.Border(bar, bar.h * 0.5f, palette.bar.WithAlpha(0.88f), palette.barBorder, 1.f);

    Rect slots[2];
    float x = bar.x + kPad;
    for (int i = 0; i < 2; ++i) {
        slots[i] = { x, bar.y + kPad, widths[i], kTabH };
        x += widths[i] + kGap;
    }
    // Sliding indicator
    const int current = l.InMultiplayers() ? 1 : 0;
    const float indX = ui.Animate(ui.MakeId("section-ind-x"), slots[current].x - bar.x, 16.f);
    const float indW = ui.Animate(ui.MakeId("section-ind-w"), slots[current].w, 16.f);
    const Rect ind{ bar.x + indX, slots[current].y, indW, kTabH };
    g.GlowRound(ind, kTabH * 0.5f, palette.accent, 14.f, 0.3f);
    g.Border(ind, kTabH * 0.5f, palette.accent.WithAlpha(0.16f), palette.accent.WithAlpha(0.7f), 1.f);

    for (int i = 0; i < 2; ++i) {
        const Tab& tab = tabs[i];
        const Rect r = slots[i];
        const UI::Id id = ui.MakeId("section-tab", i);
        const UI::Interaction it = ui.Interact(id, r);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        if (it.clicked) main.SelectSection(tab.section);
        const float sel = ui.Animate(ui.MakeId("section-tab-sel", i), i == current ? 1.f : 0.f, 14.f);
        const float hover = ui.Animate(ui.MakeId("section-tab-hover", i), it.hovered ? 1.f : 0.f, 16.f);
        if (hover > 0.01f && i != current) g.FillRound(r, kTabH * 0.5f, palette.accent.WithAlpha(0.06f * hover));
        const Color text = Lerp(Lerp(palette.muted, Rgba(214, 228, 238), hover * 0.6f), palette.accent, sel);

        float tx = r.x + 16.f;
        g.Text(std::wstring(1, tab.glyph), { tx, r.y, 14.f, r.h }, text, UI::IconFont(13.f));
        tx += 14.f + 8.f;
        g.Text(tab.text, { tx, r.y, textW[i] + 2.f, r.h }, text, label);
        tx += textW[i] + 10.f;
        const Rect count{ tx, r.CY() - 9.f, kChipW, 18.f };
        g.FillRound(count, 9.f, Lerp(Rgba(13, 37, 53, 200), palette.accent.WithAlpha(0.25f), sel));
        g.Text(std::to_wstring(tab.count), count, text, chip);
    }
}

void ChapterCards::DrawCards(UI::Ui& ui, const Rect& area) {
    auto& l = App::L();
    const bool multiplayer = l.InMultiplayers();

    // What each card of the current tab shows
    std::vector<Card> cards;
    if (multiplayer) {
        for (size_t i = 0; i < l.multiplayers.size(); ++i) {
            const auto& game = l.multiplayers[i];
            Card c;
            c.banner = Paths::Asset(L"Banners\\" + game.banner);
            c.title = Loc::Get("mp_" + game.key + "_title");
            c.subtitle = l.multiplayerRunning[i] ? Loc::Get("status_playing")
                         : game.IsInstalled()     ? Loc::Get("status_installed")
                                                  : Loc::Get("status_not_found");
            c.available = game.isAvailable;
            c.selected = static_cast<int>(i) == l.selectedMultiplayer;
            cards.push_back(std::move(c));
        }
    } else {
        for (size_t i = 0; i < l.chapters.size(); ++i) {
            const auto& chapter = l.chapters[i];
            Card c;
            c.banner = BannerPath(chapter.number);
            c.title = Loc::Get("ch" + std::to_string(chapter.number) + "_title");
            c.subtitle = Services::ChapterPlaytimeStore::Format(l.playtime.Seconds(chapter.number));
            c.available = chapter.isAvailable;
            c.selected = static_cast<int>(i) == l.selected;
            c.chapterIndex = static_cast<int>(i);
            cards.push_back(std::move(c));
        }
    }
    const size_t count = cards.size();
    const int64_t idBase = multiplayer ? 1000 : 0;   // each tab keeps its own card animations

    const bool seasonal = Seasonal::IsSeasonal();
    // Room above / below the cards for decorations that sit on their top edge
    const float margin = seasonal ? 56.f : 8.f;
    // Selecting a card grows it (and shrinks the previous one) smoothly
    std::vector<float> widths(count), heights(count);
    float contentW = 0.f;
    for (size_t i = 0; i < count; ++i) {
        widths[i] = ui.Animate(ui.MakeId("card-w", idBase + static_cast<int64_t>(i)), cards[i].selected ? kSelectedW : kCardW, 13.f);
        heights[i] = ui.Animate(ui.MakeId("card-h", idBase + static_cast<int64_t>(i)), cards[i].selected ? kSelectedH : kCardH, 13.f);
        contentW += widths[i];
    }
    const float panelH = kSelectedH + margin * 2.f;

    // ScrollViewer Margin="24,0": horizontal only, the 6 px thumb sits below the cards
    const Rect viewportArea = area.Inset(Edges{ 24.f, 0.f });
    const bool scrollable = contentW > viewportArea.w;
    const float viewH = panelH + (scrollable ? 8.f : 0.f);
    const Rect viewport{ viewportArea.x, area.CY() - viewH * 0.5f, viewportArea.w, viewH };
    const UI::Id scrollId = ui.MakeId(multiplayer ? "multiplayerScroll" : "cardsScroll");
    UI::ScrollArea scroll(ui, scrollId, viewport, true, 120.f);
    scroll.SetContentSize(contentW);

    float x = viewport.x + (scrollable ? 0.f : (viewport.w - contentW) * 0.5f);
    const float centerY = viewport.y + panelH * 0.5f;
    for (size_t i = 0; i < count; ++i) {
        const Card& card = cards[i];
        const float w = widths[i], h = heights[i];
        const Rect r{ x, centerY - h * 0.5f, w, h };
        x += w;

        ui.PushId(idBase + static_cast<int64_t>(i));
        const UI::Interaction it = ui.Interact(ui.MakeId("card"), r, true, false);   // hit area stays put while lifting
        if (it.entered) Services::Sfx::Play(Services::Sfx::Hover, true);
        if (it.hovered && ui.In().pressed[0]) {
            if (multiplayer) main.SelectMultiplayer(static_cast<int>(i));
            else main.SelectChapter(static_cast<int>(i));
        }

        // Hovered cards brighten and lift a little; both ease in / out
        float target = card.selected ? 1.f : card.available ? 0.55f : 0.28f;
        if (!card.selected && it.hovered) target = 0.85f;
        const float opacity = ui.Animate(ui.MakeId("card-opacity"), target, 16.f);
        const float lift = ui.Animate(ui.MakeId("card-lift"), !card.selected && it.hovered ? -6.f : 0.f, 16.f);
        DrawCard(ui, card, static_cast<int>(i), r.Offset(0.f, lift), opacity);
        ui.PopId();
    }
}

void ChapterCards::DrawCard(UI::Ui& ui, const Card& card, int index, const Rect& r, float opacity) {
    auto& g = ui.g;
    const bool selected = card.selected;
    const bool seasonal = Seasonal::IsSeasonal();
    const auto& palette = Seasonal::Current();

    // Seasonal themes: the selected card glows; dimming uses a background-coloured layer instead of
    // opacity so the scenery doesn't show through the card art.
    if (selected && seasonal) g.GlowRound(r, 6.f, palette.accent, 34.f, 0.55f);
    const bool fade = !seasonal && opacity < 1.f;
    if (fade) g.PushOpacity(opacity);

    g.PushRoundClip(r, 6.f);
    if (auto banner = Images::Load(card.banner)) g.DrawImage(*banner, r, 1.f, Stretch::UniformToFill);
    g.FillLinear(r, 0.f, { { 0.30f, Rgba(0, 0, 0, 0) }, { 0.85f, Rgba(5, 10, 18, 210) } }, { 0.f, 0.f }, { 0.f, 1.f });

    // Bottom stack: title, playtime / status, action buttons (Margin="12,0,12,12")
    const float innerW = r.w - 24.f;
    TextStyle titleStyle = UI::Mono(12.f);
    titleStyle.weight = DWRITE_FONT_WEIGHT_SEMI_BOLD;
    titleStyle.wrap = true;
    titleStyle.valign = VAlign::Top;
    const float titleH = g.Measure(card.title, titleStyle, innerW).h;
    const TextStyle subtitleStyle = UI::Mono(10.f);
    const float subtitleH = g.Measure(card.subtitle, subtitleStyle).h;
    const bool buttons = card.chapterIndex >= 0;
    const float buttonH = buttons ? g.Measure(L"+", UI::Mono(12.f, true)).h + kButtonPadY * 2.f + 2.f + 8.f : 0.f;
    float y = r.B() - 12.f - buttonH - subtitleH - 2.f - titleH;
    g.Text(card.title, { r.x + 12.f, y, innerW, titleH + 2.f }, Rgba(230, 240, 255, 230), titleStyle);
    y += titleH + 2.f;
    g.Text(card.subtitle, { r.x + 12.f, y, innerW, subtitleH }, Accent(120), subtitleStyle);
    y += subtitleH + 8.f;
    if (buttons) DrawCardButtons(ui, static_cast<size_t>(card.chapterIndex), r.x + 12.f, y);

    // "Coming soon" lock
    if (!card.available) {
        g.FillRect(r, Rgba(5, 10, 18, 170));
        // Images::Load caches a missing file too: no disk check every frame
        const ImagePtr image = Images::Load(Paths::Asset(L"Images\\BLOQUEADO.png"));
        const TextStyle soon = UI::Mono(9.f).Center().Wrap();
        const std::wstring soonText = Loc::Get("coming_soon");
        const float soonH = g.Measure(soonText, soon, r.w - 24.f).h;
        const float stackH = (image ? 120.f + 10.f : 0.f) + soonH;
        float sy = r.CY() - stackH * 0.5f;
        if (image) {
            const Rect box{ r.CX() - 60.f, sy, 120.f, 120.f };
            g.GlowRound(box, 8.f, Accent(255), 20.f, 0.8f);
            g.Border(box, 8.f, Rgba(9, 20, 30, 160), Accent(255), 2.f);
            g.DrawImage(*image, box.Center(90.f, 90.f));
            sy += 130.f;
        }
        g.Text(soonText, { r.x + 12.f, sy, r.w - 24.f, soonH + 2.f }, Accent(120), soon);
    }
    if (seasonal && opacity < 1.f) g.FillRect(r, palette.background.WithAlpha(1.f - opacity));
    g.PopLayer();

    // Border on top (2 px accent when selected)
    if (selected) g.StrokeRound(r, 6.f, palette.accent, 2.f);
    else g.StrokeRound(r, 6.f, kCardBorder, 1.f);
    if (fade) g.PopLayer();

    // Seasonal decorations on the card's top edge
    if (seasonal) Seasonal::CardArt::Draw(g, r, index, 1.f);
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
