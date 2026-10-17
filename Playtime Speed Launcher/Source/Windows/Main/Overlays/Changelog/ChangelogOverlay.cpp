#include "Windows/Main/Overlays/Changelog/ChangelogOverlay.h"
#include <algorithm>
#include <cmath>
#include "Core/Async.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/App/AppVersion.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Main/Common/MentionText.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

using namespace UI;

namespace {

// ── Metrics ──
constexpr float kPadX = 20.f;            // list margin inside the panel
constexpr float kRail = 18.f;            // timeline line x, from the list's left edge
constexpr float kCardLeft = 40.f;        // card x, from the list's left edge
constexpr float kCardPadX = 18.f, kCardPadTop = 14.f, kCardPadBottom = 16.f;
constexpr float kCardGap = 14.f;
constexpr float kHeaderH = 30.f;
constexpr float kTagH = 22.f;
constexpr float kBulletIndent = 18.f;
constexpr float kLineGap = 6.f;
constexpr float kSectionGap = 14.f;
constexpr float kSummaryH = 34.f;
constexpr float kTextSize = 12.f;

TextStyle LineStyle() { return Mono(kTextSize).Wrap().Top(); }

struct SectionLook {
    Color color;
    wchar_t glyph;   // Segoe MDL2 Assets
};

SectionLook Look(int kind) {
    switch (kind) {
        case 0: return { Rgba(46, 204, 140), 0xE710 };    // Additions: + (Add)
        case 1: return { Rgba(255, 152, 72), 0xE90F };    // Bug Fixes: wrench (Repair)
        case 2: return { Rgba(110, 168, 240), 0xE946 };   // Other: (i) (Info)
        default: return { Hex(0x00CCAA), 0xE73E };        // anything else: check mark
    }
}

} // namespace

void ChangelogOverlay::Open() {
    resetScroll = true;
    openedAt = Async::Now();
    popup.Open();
}

// Measures everything once: card heights and the position of every section / line inside them
void ChangelogOverlay::BuildLayout(Ui& ui, float width) {
    cards.clear();
    const float textW = width - kCardLeft - kCardPadX * 2.f - kBulletIndent;
    float y = kSummaryH;
    for (const auto& entry : Services::Changelog::Entries()) {
        Card card;
        card.entry = &entry;
        card.current = entry.version == AppVersion::CurrentVersion;
        float cy = kCardPadTop + kHeaderH + 8.f;
        for (const auto& change : entry.changes) {
            if (change.rfind(L"## ", 0) == 0) {
                Section s;
                s.title = change.substr(3);
                s.kind = s.title == L"Additions" ? SectionKind::Additions
                         : s.title == L"Bug Fixes" ? SectionKind::Fixes
                         : s.title == L"Other" ? SectionKind::Other
                                               : SectionKind::Generic;
                if (!card.sections.empty()) cy += kSectionGap - kLineGap;
                s.y = cy;
                cy += kTagH + 8.f;
                card.sections.push_back(std::move(s));
                continue;
            }
            if (card.sections.empty()) card.sections.push_back({});   // lines before any header
            Line line;
            line.text = change;
            line.mentions = change.find(L'@') != std::wstring::npos;
            line.y = cy;
            line.h = line.mentions ? MentionText(ui, 0, 0.f, 0.f, textW, change, kTextSize, Palette::Label, false)
                                   : ui.g.Measure(change, LineStyle(), textW).h;
            cy += line.h + kLineGap;
            card.sections.back().lines.push_back(std::move(line));
        }
        card.h = cy - kLineGap + kCardPadBottom;
        card.y = y;
        y += card.h + kCardGap;
        cards.push_back(std::move(card));
    }
    listHeight = y;
    layoutWidth = width;
    layoutLang = Loc::Revision();
}

void ChangelogOverlay::DrawCard(Ui& ui, const Card& card, float x, float y, float w, int index) {
    auto& g = ui.g;
    const Rect r{ x, y, w, card.h };
    // Hover: the card lifts its border / background a little (eased)
    const float hover = ui.Animate(ui.MakeId("changelog-hover", index), ui.MouseOver(r) ? 1.f : 0.f, 14.f);
    const Color accent = card.current ? Hex(0x00CCAA) : Hex(0x3A6A8A);
    const Color bg = card.current ? Lerp(Rgba(0, 38, 34), Rgba(0, 48, 42), hover) : Lerp(Rgba(9, 20, 30), Rgba(13, 28, 42), hover);
    const Color border = card.current ? Rgba(0, 204, 170, static_cast<int>(110 + 70 * hover))
                                      : Lerp(Rgba(19, 44, 64), Rgba(40, 80, 110), hover);
    if (card.current) g.GlowRound(r, 8.f, Hex(0x00CCAA), 22.f, 0.25f + 0.15f * hover);
    g.Border(r, 8.f, bg, border, 1.f);

    // ── Header: version badge · current pill ······ date ──
    const Rect header{ r.x + kCardPadX, r.y + kCardPadTop, r.w - kCardPadX * 2.f, kHeaderH };
    const std::wstring version = L"v" + card.entry->version;
    const TextStyle versionStyle = Mono(16.f, true);
    const float versionW = g.Measure(version, versionStyle).w + 2.f;
    g.Text(version, { header.x, header.y, versionW, header.h }, card.current ? Hex(0x00CCAA) : Rgba(200, 220, 235), versionStyle);
    float px = header.x + versionW + 12.f;
    if (card.current) {
        const std::wstring pill = Loc::Get("changelog_current");
        const TextStyle pillStyle = Mono(9.f, true);
        const float pw = g.Measure(pill, pillStyle).w + 16.f;
        const Rect pr{ px, header.CY() - 10.f, pw, 20.f };
        g.Border(pr, 10.f, Rgba(0, 204, 170, 40), Rgba(0, 204, 170, 140), 1.f);
        g.Text(pill, pr, Hex(0x00CCAA), TextStyle(pillStyle).Center());
    }
    const TextStyle dateStyle = Mono(11.f);
    const float dateW = g.Measure(card.entry->date, dateStyle).w + 2.f;
    g.Text(card.entry->date, { header.R() - dateW, header.y, dateW, header.h }, Rgba(90, 130, 160), dateStyle);
    Glyph(ui, 0xE787, { header.R() - dateW - 22.f, header.y, 16.f, header.h }, 12.f, Rgba(90, 130, 160));   // calendar
    g.FillRect({ header.x, header.B() + 3.f, header.w, 1.f }, accent.WithAlpha(0.18f));

    // ── Sections ──
    const float textX = r.x + kCardPadX + kBulletIndent;
    const float textW = r.w - kCardPadX * 2.f - kBulletIndent;
    for (size_t s = 0; s < card.sections.size(); ++s) {
        const auto& section = card.sections[s];
        const SectionLook look = Look(static_cast<int>(section.kind));
        if (!section.title.empty()) {
            const TextStyle tagStyle = Mono(9.5f, true);
            std::wstring label = section.title;
            std::transform(label.begin(), label.end(), label.begin(), ::towupper);
            const float tw = g.Measure(label, tagStyle).w;
            const Rect tag{ r.x + kCardPadX, r.y + section.y, tw + 34.f, kTagH };
            g.Border(tag, 5.f, look.color.WithAlpha(0.14f), look.color.WithAlpha(0.45f), 1.f);
            Glyph(ui, look.glyph, { tag.x + 8.f, tag.y, 14.f, tag.h }, 10.f, look.color);
            g.Text(label, { tag.x + 25.f, tag.y, tw + 4.f, tag.h }, look.color, tagStyle);
        }
        for (size_t i = 0; i < section.lines.size(); ++i) {
            const auto& line = section.lines[i];
            const float ly = r.y + line.y;
            const float lineH = g.Measure(L"Ag", Mono(kTextSize)).h;
            g.FillEllipse(r.x + kCardPadX + 5.f, ly + lineH * 0.5f + 0.5f, 2.6f, 2.6f, look.color.WithAlpha(0.85f));
            if (line.mentions)
                MentionText(ui, HashId(static_cast<int64_t>(s * 1000 + i), ui.MakeId("changelog-line", index)), textX, ly, textW, line.text,
                            kTextSize, Palette::Label);
            else
                g.Text(line.text, { textX, ly, textW, line.h + 2.f }, Palette::Label, LineStyle());
        }
    }
}

void ChangelogOverlay::Draw(Ui& ui, const Rect& screen) {
    auto& g = ui.g;
    PanelStyle style;
    style.title = Loc::Get("changelog_header");
    OverlayFrame frame(ui, ui.MakeId("changelog"), screen, 850.f, 680.f, style, popup.Opacity(), popup.Scale());
    const Id scrollId = ui.MakeId("changelog-scroll");
    if (resetScroll) {
        ScrollArea::ScrollTo(ui, scrollId, 0.f);
        resetScroll = false;
    }
    {
        ScrollArea scroll(ui, scrollId, frame.body, false, 64.f, ScrollArea::Bar::Classic);
        const float listX = frame.body.x + kPadX;
        const float listW = scroll.ContentWidth() - kPadX * 2.f;
        if (std::abs(listW - layoutWidth) > 0.5f || layoutLang != Loc::Revision()) BuildLayout(ui, listW);
        const float top = frame.body.y + 16.f;

        // Summary line
        const auto& entries = Services::Changelog::Entries();
        if (!entries.empty()) {
            const std::wstring summary = std::to_wstring(entries.size()) + L" versions  \x00B7  latest v" + entries.front().version;
            StaggerItem in(ui, openedAt, 0, 0.f, -10.f);
            g.Text(summary, { listX + 4.f, top, listW, 20.f }, Rgba(90, 130, 160), Mono(11.f));
        }

        // Only the cards inside the viewport are drawn (the layout covers them all)
        const float viewTop = frame.body.y + scroll.Offset() - 40.f;
        const float viewBottom = frame.body.y + scroll.Offset() + frame.body.h + 40.f;
        const float railX = listX + kRail;
        const double now = ui.Time();
        int shown = 0;   // order among the visible cards, for the one-by-one entrance
        for (size_t i = 0; i < cards.size(); ++i) {
            const Card& card = cards[i];
            const float y = top + card.y;
            if (y + card.h + kCardGap < viewTop) continue;
            if (y > viewBottom) break;

            StaggerItem in(ui, openedAt + 0.08, shown++, 46.f);
            // Timeline: the rail runs through every card; each version has a node beside its header
            const float nodeY = y + kCardPadTop + kHeaderH * 0.5f;
            const bool last = i + 1 == cards.size();
            g.FillRect({ railX - 1.f, i == 0 ? nodeY : y - kCardGap, 2.f, (last ? nodeY : y + card.h) - (i == 0 ? nodeY : y - kCardGap) },
                       Rgba(26, 58, 85, 200));
            if (card.current) {
                // Current version: glowing, gently pulsing node
                const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(now * 2.4));
                g.FillEllipse(railX, nodeY, 11.f + 3.f * pulse, 11.f + 3.f * pulse, Rgba(0, 204, 170, static_cast<int>(30 + 30 * pulse)));
                g.FillEllipse(railX, nodeY, 7.f, 7.f, Hex(0x00CCAA));
                g.FillEllipse(railX, nodeY, 3.f, 3.f, Rgba(220, 255, 245));
                ui.KeepAnimatingThrottled();
            } else {
                g.FillEllipse(railX, nodeY, 6.f, 6.f, Rgba(9, 20, 30));
                g.StrokeEllipse(railX, nodeY, 5.f, 5.f, Rgba(58, 106, 138), 2.f);
            }
            // Connector from the node to the card
            g.FillRect({ railX + 7.f, nodeY - 0.5f, kCardLeft - kRail - 7.f, 1.f }, Rgba(26, 58, 85, 200));
            DrawCard(ui, card, listX + kCardLeft, y, listW - kCardLeft, static_cast<int>(i));
        }
        scroll.SetContentSize(listHeight + 28.f);
    }

    const std::wstring back = Loc::Get("back");
    const TextStyle text = Mono(11.f, true).Center();
    const float w = ButtonWidth(ui, back, text, Styles::Footer());
    if (TextButton(ui, ui.MakeId("changelog-close"), { frame.footer.R() - 14.f - w, frame.footer.CY() - 18.f, w, 36.f }, back, text, Palette::Dim,
                   Styles::Footer()))
        popup.Close();
}

} // namespace SpeedrunLauncher::Main
