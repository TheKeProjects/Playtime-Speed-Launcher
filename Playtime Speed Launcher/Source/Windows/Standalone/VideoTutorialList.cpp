// Skips & glitches → sidebar: search box, filter pills and the (grouped) video list.
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include "App/Paths.h"
#include "Core/Text.h"
#include "Graphics/Images.h"
#include "UI/Style.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "UI/Widgets/TextBox.h"
#include "Windows/Standalone/Common/Pills.h"
#include "Windows/Standalone/VideoTutorialWindow.h"
#include "Localization/Localization.h"

namespace SpeedrunLauncher::Windows {

using namespace UI;
namespace VT = Services::VideoTutorials;

namespace {

bool IEquals(const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) == 0; }
bool IContains(const std::wstring& haystack, const std::wstring& needle) { return Text::Lower(haystack).find(Text::Lower(needle)) != std::wstring::npos; }

std::vector<std::wstring> SplitList(const std::wstring& s) {
    std::vector<std::wstring> out;
    for (const auto& part : Text::Split(s, L','))
        if (auto t = Text::Trim(part); !t.empty()) out.push_back(t);
    return out;
}

bool ListContains(const std::vector<std::wstring>& list, const std::wstring& value) {
    return std::any_of(list.begin(), list.end(), [&](const std::wstring& v) { return IEquals(v, value); });
}

// Case-insensitive distinct + ordinal sort (OrderBy)
std::vector<std::wstring> Distinct(std::vector<std::wstring> values, bool versionOrder = false) {
    std::vector<std::wstring> out;
    for (auto& v : values)
        if (!ListContains(out, v)) out.push_back(v);
    std::sort(out.begin(), out.end(), [versionOrder](const std::wstring& a, const std::wstring& b) {
        // Versions: "<1.3" style entries first
        const std::wstring ka = versionOrder && !a.empty() && a[0] == L'<' ? L"0" + a : a;
        const std::wstring kb = versionOrder && !b.empty() && b[0] == L'<' ? L"0" + b : b;
        return ka < kb;
    });
    return out;
}

// Description run categories ("Any%, Glitchless — notes")
std::vector<std::wstring> ParseRunCategories(const std::wstring& description) {
    std::wstring main = description;
    for (const wchar_t* sep : { L" \x2014 ", L" \x2013 " })
        if (auto p = main.find(sep); p != std::wstring::npos) main = main.substr(0, p);
    std::vector<std::wstring> out;
    for (const auto& s : SplitList(main))
        if (Text::Lower(s).rfind(L"all categor", 0) != 0) out.push_back(s);
    return out;
}

void FilterLabel(Ui& ui, Column& col, const std::wstring& text) {
    col.Gap(6.f);
    const float h = ui.g.Measure(L"Ag", Mono(8.f, true)).h;
    ui.g.Text(text, col.Next(h), Rgba(0, 204, 170, 100), Mono(8.f, true));
}

// "All" + options; returns true when the selection changed.
bool FilterRow(Ui& ui, Column& col, std::string_view id, const std::vector<std::wstring>& options, std::optional<std::wstring>& value,
               float marginBottom = 4.f) {
    col.Gap(3.f);
    std::vector<std::wstring> labels{ Loc::Get("vt_all") };
    labels.insert(labels.end(), options.begin(), options.end());
    const int clicked = Pills::Wrap(ui, col, id, labels, [&](size_t i) { return i == 0 ? !value.has_value() : value && *value == labels[i]; });
    col.Gap(marginBottom);   // WrapPanel bottom margin (the pills' own 4 px are already in)
    if (clicked < 0) return false;
    if (clicked == 0) value.reset();
    else value = labels[static_cast<size_t>(clicked)];
    return true;
}

} // namespace

// ── Filters ─────────────────────────────────────────────────────────────────

void VideoTutorialWindow::RefreshFilterLists() {
    if (lists.built && lists.chapter == chapterFilter) return;
    const auto& videos = VT::All();
    FilterLists l;
    std::vector<std::wstring> chapters;
    for (const auto& v : videos)
        if (!v.chapter.empty()) chapters.push_back(v.chapter);
    l.chapters = Distinct(chapters);
    std::vector<const Video*> scoped;
    for (const auto& v : videos)
        if (!chapterFilter || v.chapter == *chapterFilter) scoped.push_back(&v);
    l.structured = chapterFilter && std::any_of(scoped.begin(), scoped.end(), [](const Video* v) { return !v->runCategories.empty(); });
    if (l.structured) {
        static const std::set<std::wstring> kGroups{ L"major skips", L"smallButton tricks", L"legacy", L"general" };
        for (const Video* v : scoped) {
            l.runCats.insert(l.runCats.end(), v->runCategories.begin(), v->runCategories.end());
            for (const auto& s : SplitList(v->version)) l.versions.push_back(s);
            l.routes.insert(l.routes.end(), v->routes.begin(), v->routes.end());
            if (!v->category.empty() && !kGroups.count(Text::Lower(v->category))) l.special.push_back(v->category);
            for (const auto& s : SplitList(v->restrictions)) l.restrictions.push_back(s);
        }
        l.runCats = Distinct(l.runCats);
        l.versions = Distinct(l.versions, true);
        l.routes = Distinct(l.routes);
        l.special = Distinct(l.special);
        l.restrictions = Distinct(l.restrictions);
    } else {
        for (const Video* v : scoped) {
            if (!v->category.empty()) l.categories.push_back(v->category);
            if (!VT::FlatList)
                for (const auto& c : ParseRunCategories(v->description)) l.runCats.push_back(c);
        }
        l.categories = Distinct(l.categories);
        l.runCats = Distinct(l.runCats);
    }
    l.chapter = chapterFilter;
    l.built = true;
    lists = std::move(l);
}

void VideoTutorialWindow::DrawFilters(Column& col) {
    RefreshFilterLists();
    const auto chapters = lists.chapters;   // copies: picking a chapter rebuilds the lists
    if (!chapters.empty()) {
        FilterLabel(ui, col, Loc::Get("vt_chapter"));
        if (FilterRow(ui, col, "f-chapter", chapters, chapterFilter, 8.f)) {
            categoryFilter.reset();
            runCategoryFilter.reset();
            versionFilter.reset();
            routeFilter.reset();
            restrictionsFilter.reset();
        }
    }

    RefreshFilterLists();
    const FilterLists l = lists;
    if (l.structured) {
        const auto& special = l.special;
        const auto& runCats = l.runCats;
        const auto& versions = l.versions;
        const auto& routes = l.routes;
        const auto& restrictions = l.restrictions;
        if (!special.empty()) {
            FilterLabel(ui, col, Loc::Get("vt_category"));
            if (FilterRow(ui, col, "f-category", special, categoryFilter) && categoryFilter) runCategoryFilter.reset();
        }
        if (runCats.size() > 1) {
            FilterLabel(ui, col, special.empty() ? Loc::Get("vt_category") : Loc::Get("vt_run_categories"));
            if (FilterRow(ui, col, "f-runcat", runCats, runCategoryFilter) && runCategoryFilter) categoryFilter.reset();
        }
        if (versions.size() > 1) {
            FilterLabel(ui, col, Loc::Get("vt_version"));
            FilterRow(ui, col, "f-version", versions, versionFilter);
        }
        if (routes.size() > 1) {
            FilterLabel(ui, col, Loc::Get("vt_route"));
            FilterRow(ui, col, "f-route", routes, routeFilter);
        }
        if (!restrictions.empty()) {
            FilterLabel(ui, col, Loc::Get("vt_restrictions"));
            FilterRow(ui, col, "f-restrictions", restrictions, restrictionsFilter);
        }
    } else {
        const auto& categories = l.categories;
        const auto& runCats = l.runCats;
        if (!VT::FlatList && categories.size() > 1) {
            FilterLabel(ui, col, Loc::Get("vt_category"));
            FilterRow(ui, col, "f-category", categories, categoryFilter);
        }
        if (!runCats.empty()) {
            FilterLabel(ui, col, Loc::Get("vt_run_category"));
            FilterRow(ui, col, "f-runcat", runCats, runCategoryFilter);
        }
    }
}

const std::vector<const VideoTutorialWindow::Video*>& VideoTutorialWindow::Filtered() {
    // Recomputed only when a filter or the search text changes
    const auto part = [](const std::optional<std::wstring>& v) { return v ? *v : std::wstring(L"\x1E"); };
    const std::wstring key = search + L"\x1F" + part(chapterFilter) + L"\x1F" + part(categoryFilter) + L"\x1F" + part(runCategoryFilter) + L"\x1F" +
                             part(versionFilter) + L"\x1F" + part(routeFilter) + L"\x1F" + part(restrictionsFilter);
    if (key == filteredKey) return filteredVideos;
    filteredKey = key;
    listShownAt = ui.Time() + (openedAt < 0.0 || ui.Time() - openedAt < 0.2 ? 0.15 : 0.0);   // first time: after the panel
    const std::wstring raw = Text::Trim(search);
    const bool authorOnly = !raw.empty() && raw[0] == L'@';
    const std::wstring term = authorOnly ? raw.substr(1) : raw;
    std::vector<const Video*> out;
    for (const auto& v : VT::All()) {
        if (chapterFilter && v.chapter != *chapterFilter) continue;
        if (categoryFilter && v.category != *categoryFilter) continue;
        if (runCategoryFilter) {
            const bool ok = !v.runCategories.empty() ? ListContains(v.runCategories, *runCategoryFilter)
                                                     : IContains(v.description, *runCategoryFilter) || IContains(v.description, L"All Categor");
            if (!ok) continue;
        }
        if (versionFilter && !v.version.empty() && !ListContains(SplitList(v.version), *versionFilter)) continue;
        if (routeFilter && !v.routes.empty() && !ListContains(v.routes, *routeFilter)) continue;
        if (restrictionsFilter && !v.restrictions.empty() && !ListContains(SplitList(v.restrictions), *restrictionsFilter)) continue;
        if (!term.empty()) {
            const bool match = authorOnly ? IContains(v.author, term)
                                          : IContains(v.title, term) || IContains(v.author, term) || IContains(v.chapter, term) ||
                                                IContains(v.category, term) || IContains(v.description, term);
            if (!match) continue;
        }
        out.push_back(&v);
    }
    filteredVideos = std::move(out);
    return filteredVideos;
}

// ── List ────────────────────────────────────────────────────────────────────

void VideoTutorialWindow::DrawVideoItem(Column& col, const Video& video, int index) {
    const bool isLoading = loadingId == video.id;
    const bool isCurrent = current && current->id == video.id;
    const bool isSelected = selectedId == video.id;
    const bool proac = IEquals(video.author, L"proac");

    // Measure: dot + title (wrapped) + tags + "Loading..."
    TextStyle title = Mono(11.f, true);
    title.wrap = true;
    title.valign = VAlign::Top;
    const float avatar = proac ? 28.f + 8.f : 0.f;
    const float textX = col.x + 8.f + 6.f + 8.f;
    const float textW = col.w - 16.f - 14.f - avatar;
    const float titleH = g.Measure(video.title, title, textW).h;
    const float tagH = g.Measure(L"Ag", Mono(8.f, true)).h + 2.f + 2.f;
    std::vector<std::pair<std::wstring, bool>> tags;   // text, version tag
    if (!video.version.empty()) tags.emplace_back(video.version, true);
    for (const auto& r : video.routes) tags.emplace_back(r, false);
    float tagsH = 0.f;
    std::vector<Rect> tagRects;
    if (!tags.empty()) {
        float x = textX, y = 0.f;
        for (const auto& [text, _] : tags) {
            const float w = g.Measure(text, Mono(8.f, true)).w + 10.f + 2.f;
            if (x > textX && x + w > textX + textW) {
                x = textX;
                y += tagH;
            }
            tagRects.push_back({ x, y, w, tagH });
            x += w + 4.f;
        }
        tagsH = 3.f + y + tagH;
    }
    const float loadingH = isLoading ? 2.f + g.Measure(L"Ag", Mono(9.f)).h : 0.f;
    const float contentH = std::max(titleH + tagsH + loadingH, proac ? 28.f : 0.f);
    col.Gap(2.f);
    const Rect r = col.Next(8.f + contentH + 8.f, 2.f);
    if (r.B() < visibleTop || r.y > visibleBottom) return;   // off screen: only its height matters
    StaggerItem in(ui, listShownAt, index, -22.f, 0.f, 0.03, 0.34);

    const Id id = ui.MakeId("video", index);
    const Interaction it = ui.Interact(id, r, !isLoading);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    if (isLoading && ui.MouseOver(r)) ui.SetCursor(IDC_WAIT);
    // Hover / selection fade in (instead of jumping) and the row nudges right while hovered
    const float hoverK = ui.Animate(HashId(id, 0x1D01), it.hovered ? 1.f : 0.f, 16.f, 0.f);
    const float selK = ui.Animate(HashId(id, 0x1D02), isSelected ? 1.f : 0.f, 14.f, 0.f);
    const Color normal = proac ? Rgba(218, 165, 32, 18) : Rgba(255, 255, 255, 12);
    const Color hover = proac ? Rgba(218, 165, 32, 35) : Rgba(255, 255, 255, 28);
    const Color bg = Lerp(Lerp(normal, hover, hoverK), Rgba(0, 204, 170, 50), selK);
    if (selK > 0.01f) g.GlowRound(r, 4.f, Palette::Teal, 10.f, 0.25f * selK);
    if (proac) g.Border(r, 4.f, bg, Rgba(218, 165, 32, 90), 1.f);
    else g.FillRound(r, 4.f, bg);
    const float lit = std::max(selK, isCurrent ? 1.f : 0.f);
    if (lit > 0.01f) g.FillRound({ r.x, r.y + 6.f, 3.f * lit, r.h - 12.f }, 1.5f, Palette::Teal);
    ui.PushTranslate(3.f * hoverK, 0.f);

    const float top = r.y + 8.f;
    const Color dot = isLoading ? Rgba(255, 170, 0) : isCurrent ? Palette::Teal : Rgba(100, 130, 150, 60);
    if (isLoading || isCurrent) {
        // The playing / loading video's dot breathes
        const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(ui.Time() * (isLoading ? 6.0 : 2.4)));
        g.FillEllipse(r.x + 11.f, top + 7.f, 3.f + 3.f * pulse, 3.f + 3.f * pulse, dot.WithAlpha(0.25f * (1.f - pulse)));
        ui.KeepAnimatingThrottled();
    }
    g.FillRound({ r.x + 8.f, top + 4.f, 6.f, 6.f }, 3.f, dot);
    g.Text(video.title, { textX, top, textW, titleH },
           isLoading || isCurrent ? Rgba(200, 216, 228) : Lerp(Rgba(160, 180, 200, 130), Rgba(200, 216, 228), hoverK), title);
    for (size_t i = 0; i < tags.size(); ++i) {
        const Rect t = tagRects[i].Offset(0.f, top + titleH + 3.f);
        const Color fg = tags[i].second ? Rgba(0, 204, 170, 180) : Rgba(160, 180, 200, 140);
        const Color tbg = tags[i].second ? Rgba(0, 204, 170, 30) : Rgba(160, 180, 200, 15);
        g.Border(t, 3.f, tbg, fg.WithAlpha(fg.a / 3.f), 1.f);
        g.Text(tags[i].first, t, fg, Mono(8.f, true).Center());
    }
    if (isLoading)
        g.Text(Loc::Get("vt_loading"), { textX, top + titleH + tagsH + 2.f, textW, loadingH - 2.f }, Rgba(255, 170, 0, 200), Mono(9.f));
    if (proac) {
        const Rect a{ r.R() - 8.f - 28.f, r.CY() - 14.f, 28.f, 28.f };
        if (auto img = Images::Load(Paths::Asset(L"Images\\proacventure.png"))) g.DrawImageRounded(*img, a, 14.f);
        g.Border(a, 14.f, Colors::Transparent, Rgba(218, 165, 32, 180), 1.5f);
    }
    ui.PopTransform();
    if (it.held && ui.In().pressed[0] && loadingId.empty()) Select(video);
}

void VideoTutorialWindow::DrawVideoList(const Rect& area) {
    // Smooth wheel (Delta * 0.5 per notch)
    ScrollArea scroll(ui, ui.MakeId("video-list"), area, false, 60.f);
    Column col(area.x + 10.f, area.y, area.w - 10.f - 4.f - 6.f);
    visibleTop = area.y + scroll.Offset() - 20.f;
    visibleBottom = area.y + scroll.Offset() + area.h + 20.f;
    const auto& filtered = Filtered();
    if (filtered.empty()) {
        col.Gap(8.f);
        col.Gap(Paragraph(ui, col.x, col.y, col.w, VT::All().empty() ? Loc::Get("vt_no_videos") : Loc::Get("vt_no_results"), Mono(10.f), Rgba(100, 140, 160, 120)));
        scroll.SetContentSize(col.Height() + 10.f);
        return;
    }
    std::set<std::wstring> chapterSet;
    for (const Video* v : filtered) chapterSet.insert(v->chapter);
    const bool multiChapter = !chapterFilter && chapterSet.size() > 1;

    int index = 0;
    const auto groups = [&](const std::vector<const Video*>& list, bool indented) {
        // GroupBy(Category).OrderBy(Key)
        std::map<std::wstring, std::vector<const Video*>> byCategory;
        for (const Video* v : list) byCategory[v->category].push_back(v);
        bool first = true;
        for (const auto& [category, items] : byCategory) {
            if (!first) {
                const float m = indented ? 6.f : 8.f;
                col.Gap(m);
                g.FillRect({ col.x, col.y, col.w, 1.f }, Rgba(13, 32, 48, 50));
                col.Gap(1.f + m);
            }
            first = false;
            const float h = g.Measure(L"Ag", Mono(9.f, true)).h;
            col.Gap(indented ? 2.f : 4.f);
            g.Text(Text::Upper(category), { col.x + (indented ? 8.f : 0.f), col.y, col.w, h }, Rgba(0, 204, 170, 180), Mono(9.f, true));
            col.Gap(h + (indented ? 5.f : 6.f));
            for (const Video* v : items) DrawVideoItem(col, *v, index++);
        }
    };
    if (multiChapter) {
        std::map<std::wstring, std::vector<const Video*>> byChapter;
        for (const Video* v : filtered) byChapter[v->chapter].push_back(v);
        bool first = true;
        for (const auto& [chapter, items] : byChapter) {
            if (!first) {
                col.Gap(10.f);
                g.FillRect({ col.x, col.y, col.w, 1.f }, Rgba(13, 32, 48, 50));
                col.Gap(1.f + 10.f);
            }
            first = false;
            const float h = g.Measure(L"Ag", Mono(10.f, true)).h;
            col.Gap(4.f);
            g.Text(Text::Upper(chapter), { col.x, col.y, col.w, h }, Rgba(255, 170, 0), Mono(10.f, true));
            col.Gap(h + 6.f);
            if (VT::FlatList)
                for (const Video* v : items) DrawVideoItem(col, *v, index++);
            else
                groups(items, true);
        }
    } else if (VT::FlatList) {
        for (const Video* v : filtered) DrawVideoItem(col, *v, index++);
    } else {
        groups(filtered, false);
    }
    scroll.SetContentSize(col.Height() + 10.f);
}

void VideoTutorialWindow::DrawSidebar(const Rect& area) {
    g.FillRect(area, Hex(0x04080E));
    g.FillRect({ area.R() - 1.f, area.y, 1.f, area.h }, Palette::Divider);

    // Search (Margin 10,10,10,8)
    TextBoxStyle s;
    s.text = Mono(11.f);
    s.foreground = Hex(0x8AABBD);
    s.background = Hex(0x060D18);
    s.border = Palette::Divider;
    s.focusBorder = Palette::Divider;
    s.caret = Palette::Teal;
    s.selection = Rgba(0, 204, 170, 100);
    s.padding = { 28.f, 7.f, 8.f, 7.f };
    s.radius = 4.f;
    s.placeholder = Loc::Get("vt_search_placeholder");
    s.placeholderColor = Hex(0x1A3A55);
    const Rect box{ area.x + 10.f, area.y + 10.f, area.w - 21.f, TextBoxHeight(ui, s) };
    TextBox(ui, ui.MakeId("search"), box, search, s);
    g.Text(L"\xE721", { box.x + 9.f, box.y, 12.f, box.h }, Hex(0x1A3A55), IconFont(11.f));

    // Filters (Padding 10,0,10,8, bottom border)
    Column col(area.x + 10.f, box.B() + 8.f, area.w - 21.f);
    DrawFilters(col);
    col.Gap(8.f);
    g.FillRect({ area.x, col.y, area.w - 1.f, 1.f }, Palette::Divider);
    DrawVideoList({ area.x, col.y + 1.f, area.w - 1.f, area.B() - col.y - 1.f });
}

} // namespace SpeedrunLauncher::Windows
