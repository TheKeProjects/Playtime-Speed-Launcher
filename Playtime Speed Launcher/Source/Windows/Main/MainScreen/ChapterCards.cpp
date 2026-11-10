#include "Windows/Main/MainScreen/ChapterCards.h"
#include <algorithm>
#include <map>
#include <vector>
#include "App/Launcher.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Seasonal/SeasonalTheme.h"
#include "Services/Audio/SoundEffects.h"
#include "UI/Widgets/Button.h"
#include "UI/Style.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/MainScreen/MainLayout.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

namespace {

using Section = App::Launcher::Section;

constexpr float kCardW = 168.f, kCardH = 236.f, kGap = 16.f;
constexpr float kHeaderH = 54.f, kTabH = 32.f;
constexpr float kSidePad = MainLayout::ContentPad;

} // namespace

const std::wstring& ChapterCards::BannerPath(int chapter) {
    static std::map<int, std::wstring> paths;
    auto it = paths.find(chapter);
    if (it != paths.end()) return it->second;
    const std::wstring jpg = Paths::Asset(L"Banners\\Chapter " + std::to_wstring(chapter) + L".jpg");
    return paths[chapter] = Files::Exists(jpg) ? jpg : Paths::Asset(L"Banners\\Chapter " + std::to_wstring(chapter) + L".png");
}

void ChapterCards::Draw(UI::Ui& ui, const Rect& area) {
    auto& g = ui.g;
    const auto& palette = Seasonal::Current();
    // The shelf darkens towards the bottom so the cards read over any scenery
    g.FillLinear(area, 0.f, { { 0.f, palette.background.WithAlpha(0.f) }, { 0.35f, palette.background.WithAlpha(0.55f) },
                              { 1.f, palette.background.WithAlpha(0.8f) } },
                 { 0.f, 0.f }, { 0.f, 1.f });
    g.FillRect({ area.x + kSidePad, area.y + kHeaderH - 1.f, area.w - kSidePad * 2.f, 1.f }, palette.barBorder.WithAlpha(0.6f));

    DrawTabs(ui, area.Top(kHeaderH));

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
        g.PushOpacity(enter);
        ui.PushTranslate((1.f - enter) * from, 0.f);
        ui.KeepAnimating();
    }
    DrawCards(ui, area.CutTop(kHeaderH));
    if (entering) {
        ui.PopTransform();
        g.PopLayer();
    }
}

// The two tabs as underlined labels with a count; the underline slides to the selected one.
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
        { Section::Chapters, wchar_t(0xE8F1), Loc::Get("section_chapters"), l.chapters.size() },              // library
        { Section::Multiplayers, wchar_t(0xE716), Loc::Get("section_multiplayers"), l.multiplayers.size() },  // people
    };
    const TextStyle label = UI::Mono(11.f, true);
    const TextStyle chip = UI::Mono(9.f, true).Center();
    constexpr float kChipW = 22.f;

    Rect slots[2];
    float textW[2];
    float x = area.x + kSidePad - 10.f;
    const float y = area.B() - 8.f - kTabH;
    for (int i = 0; i < 2; ++i) {
        textW[i] = g.Measure(tabs[i].text, label).w;
        const float w = 10.f + 14.f + 8.f + textW[i] + 8.f + kChipW + 10.f;
        slots[i] = { x, y, w, kTabH };
        x += w + 8.f;
    }
    const int current = l.InMultiplayers() ? 1 : 0;
    const float indX = ui.Animate(ui.MakeId("section-ind-x"), slots[current].x + 10.f, 16.f);
    const float indW = ui.Animate(ui.MakeId("section-ind-w"), slots[current].w - 20.f, 16.f);
    const Rect underline{ indX, area.B() - 3.f, indW, 3.f };
    g.GlowRound(underline, 1.5f, palette.accent, 10.f, 0.5f);
    g.FillRound(underline, 1.5f, palette.accent);

    for (int i = 0; i < 2; ++i) {
        const Tab& tab = tabs[i];
        const Rect r = slots[i];
        const UI::Id id = ui.MakeId("section-tab", i);
        const UI::Interaction it = ui.Interact(id, r);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        if (it.clicked) main.SelectSection(tab.section);
        const float sel = ui.Animate(ui.MakeId("section-tab-sel", i), i == current ? 1.f : 0.f, 14.f);
        const float hover = ui.Animate(ui.MakeId("section-tab-hover", i), it.hovered ? 1.f : 0.f, 16.f);
        if (hover > 0.01f && i != current) g.FillRound(r, 6.f, palette.accent.WithAlpha(0.06f * hover));
        const Color text = Lerp(Lerp(palette.muted, Rgba(214, 228, 238), hover * 0.6f), Rgba(235, 242, 250), sel);

        float tx = r.x + 10.f;
        g.Text(std::wstring(1, tab.glyph), { tx, r.y, 14.f, r.h }, Lerp(text, palette.accent, sel), UI::IconFont(13.f));
        tx += 14.f + 8.f;
        g.Text(tab.text, { tx, r.y, textW[i] + 2.f, r.h }, text, label);
        tx += textW[i] + 8.f;
        const Rect count{ tx, r.CY() - 9.f, kChipW, 18.f };
        g.FillRound(count, 9.f, Lerp(Rgba(13, 37, 53, 200), palette.accent.WithAlpha(0.25f), sel));
        g.Text(std::to_wstring(tab.count), count, Lerp(text, palette.accent, sel), chip);
    }

    // On the right: the tab's total playtime
    double total = 0.0;
    if (current == 1)
        for (const auto& game : l.multiplayers) total += l.playtime.Seconds(game.playtimeId);
    else
        for (const auto& chapter : l.chapters)
            if (chapter.isAvailable) total += l.playtime.Seconds(chapter.number);
    const std::wstring caption = Loc::Get("shelf_total");
    const std::wstring value = Services::ChapterPlaytimeStore::Format(total);
    const TextStyle captionStyle = UI::Mono(9.f, true);
    const TextStyle valueStyle = UI::Mono(13.f, true);
    const float vw = g.Measure(value, valueStyle).w, cw = g.Measure(caption, captionStyle).w;
    const float right = area.R() - kSidePad;
    const Rect valueRect{ right - vw - 2.f, y, vw + 2.f, kTabH };
    if (valueRect.x - 14.f - cw - 20.f > x) {   // only when it doesn't run into the tabs
        g.Text(value, valueRect, Rgba(225, 236, 245), valueStyle);
        UI::Glyph(ui, wchar_t(0xE916), { valueRect.x - 20.f, y, 14.f, kTabH }, 11.f, palette.accent);   // stopwatch
        g.Text(caption, { valueRect.x - 20.f - 10.f - cw - 2.f, y, cw + 2.f, kTabH }, palette.muted, captionStyle);
    }
}

void ChapterCards::DrawCards(UI::Ui& ui, const Rect& area) {
    auto& l = App::L();
    const bool multiplayer = l.InMultiplayers();

    std::vector<Card> cards;
    if (multiplayer) {
        for (size_t i = 0; i < l.multiplayers.size(); ++i) {
            const auto& game = l.multiplayers[i];
            Card c;
            c.banner = Paths::Asset(L"Banners\\" + game.banner);
            c.title = Loc::Get("mp_" + game.key + "_title");
            c.running = i < l.multiplayerRunning.size() && l.multiplayerRunning[i];
            const double seconds = l.playtime.Seconds(game.playtimeId);
            c.subtitle = c.running                             ? Loc::Get("status_playing")
                         : game.IsInstalled() || seconds > 0.0 ? Services::ChapterPlaytimeStore::Format(seconds)
                                                               : Loc::Get("status_not_found");
            c.available = game.isAvailable;
            c.selected = l.multiplayerPicked && static_cast<int>(i) == l.selectedMultiplayer;
            cards.push_back(std::move(c));
        }
    } else {
        for (size_t i = 0; i < l.chapters.size(); ++i) {
            const auto& chapter = l.chapters[i];
            Card c;
            c.banner = BannerPath(chapter.number);
            c.title = Loc::Get("ch" + std::to_string(chapter.number) + "_title");
            c.running = i < l.gameRunning.size() && l.gameRunning[i];
            c.subtitle = c.running              ? Loc::Get("status_playing")
                         : !chapter.isAvailable ? L""
                                                : Services::ChapterPlaytimeStore::Format(l.playtime.Seconds(chapter.number));
            c.available = chapter.isAvailable;
            c.selected = l.chapterPicked && static_cast<int>(i) == l.selected;
            c.font = Font::Poppy;
            cards.push_back(std::move(c));
        }
    }
    const size_t count = cards.size();
    // Nothing picked yet: no card is dimmed, they all invite a click
    if (!l.HasSelection())
        for (Card& c : cards) c.dimmed = false;
    const int64_t idBase = multiplayer ? 1000 : 0;   // each tab keeps its own card animations

    // Horizontal row, left aligned; room above the cards for the hover lift
    const float contentW = count ? count * kCardW + (count - 1) * kGap : 0.f;
    const Rect viewportArea = area.Inset(Edges{ kSidePad - 8.f, 0.f });
    const float viewH = kCardH + 22.f;
    const Rect viewport{ viewportArea.x, area.CY() - viewH * 0.5f, viewportArea.w, viewH };
    UI::ScrollArea scroll(ui, ui.MakeId(multiplayer ? "multiplayerScroll" : "cardsScroll"), viewport, true, 120.f);
    scroll.SetContentSize(contentW + 16.f);

    float x = viewport.x + 8.f;
    const float y = viewport.y + 10.f;
    for (size_t i = 0; i < count; ++i) {
        const Card& card = cards[i];
        const Rect r{ x, y, kCardW, kCardH };
        x += kCardW + kGap;

        ui.PushId(idBase + static_cast<int64_t>(i));
        const UI::Interaction it = ui.Interact(ui.MakeId("card"), r, true, false);   // hit area stays put while lifting
        if (it.hovered) ui.SetCursor(IDC_HAND);
        if (it.entered) Services::Sfx::Play(Services::Sfx::Hover, true);
        if (it.hovered && ui.In().pressed[0]) {
            if (multiplayer) main.SelectMultiplayer(static_cast<int>(i));
            else main.SelectChapter(static_cast<int>(i));
        }
        const float hover = ui.Animate(ui.MakeId("card-hover"), it.hovered ? 1.f : 0.f, 16.f);
        const float lift = ui.Animate(ui.MakeId("card-lift"), card.selected ? -6.f : it.hovered ? -4.f : 0.f, 16.f);
        DrawCard(ui, card, r.Offset(0.f, lift), hover);
        ui.PopId();
    }
}

void ChapterCards::DrawCard(UI::Ui& ui, const Card& card, const Rect& r, float hover) {
    auto& g = ui.g;
    const auto& palette = Seasonal::Current();
    const float sel = ui.Animate(ui.MakeId("card-sel"), card.selected ? 1.f : 0.f, 14.f);

    if (sel > 0.01f) g.GlowRound(r, 8.f, palette.accent, 22.f, 0.55f * sel);
    g.PushRoundClip(r, 8.f);
    g.FillRect(r, palette.bar);
    if (auto banner = Images::Load(card.banner)) g.DrawImage(*banner, r, 1.f, Stretch::UniformToFill);
    g.FillLinear(r, 0.f, { { 0.40f, Rgba(0, 0, 0, 0) }, { 1.f, Rgba(4, 8, 14, 235) } }, { 0.f, 0.f }, { 0.f, 1.f });

    // Title + playtime / status at the bottom
    TextStyle titleStyle = UI::Mono(card.font == Font::Mono ? 11.f : 14.f, card.font == Font::Mono);
    titleStyle.font = card.font;
    titleStyle.wrap = true;
    titleStyle.valign = VAlign::Bottom;
    const TextStyle subtitleStyle = UI::Mono(10.f);
    const Rect sub{ r.x + 12.f, r.B() - 10.f - 14.f, r.w - 24.f, 14.f };
    g.Text(card.title, { r.x + 12.f, r.y, r.w - 24.f, sub.y - r.y - 2.f }, Rgba(235, 242, 250), titleStyle);
    g.Text(card.subtitle, sub, card.running ? palette.accent : palette.accent.WithAlpha(0.6f), subtitleStyle);

    // "Coming soon": darkened, with an accent wash from the bottom
    if (!card.available) {
        g.FillRect(r, Rgba(5, 10, 18, 150));
        g.FillLinear(r, 0.f, { { 0.f, palette.accent.WithAlpha(0.f) }, { 1.f, palette.accent.WithAlpha(0.10f) } }, { 0.f, 0.f }, { 0.f, 1.f });
    }
    // Not selected: dimmed a little, less while hovered
    const float dim = card.dimmed ? (1.f - sel) * (0.42f - 0.32f * hover) : 0.f;
    if (dim > 0.01f) g.FillRect(r, palette.background.WithAlpha(dim));
    // The lock goes over the dimming so it still reads on an unselected card
    if (!card.available) {
        const Rect ring = r.Center(64.f, 64.f).Offset(0.f, -18.f);
        g.Border(ring, 32.f, Rgba(9, 20, 30, 170), palette.accent.WithAlpha(0.35f + 0.35f * hover), 1.5f);
        if (const ImagePtr image = Images::Load(Paths::Asset(L"Images\\BLOQUEADO.png"))) g.DrawImage(*image, ring.Center(38.f, 38.f), 0.9f);
    }
    g.PopLayer();

    if (sel > 0.01f) g.StrokeRound(r, 8.f, palette.accent.WithAlpha(sel), 2.f);
    else g.StrokeRound(r, 8.f, Lerp(Rgba(13, 42, 59, 120), palette.accent.WithAlpha(0.5f), hover), 1.f);
    // A dot under the selected card
    if (sel > 0.01f) g.FillEllipse(r.CX(), r.B() + 8.f, 2.5f * sel, 2.5f * sel, palette.accent);
    // Playing: a pulsing dot in the corner
    if (card.running) {
        g.FillEllipse(r.R() - 12.f, r.y + 12.f, 7.f, 7.f, palette.accent.WithAlpha(0.25f));
        g.FillEllipse(r.R() - 12.f, r.y + 12.f, 4.f, 4.f, palette.accent);
    }
}

} // namespace SpeedrunLauncher::Main
