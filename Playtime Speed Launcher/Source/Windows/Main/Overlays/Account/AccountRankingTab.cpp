#include <algorithm>
#include <cmath>
#include "App/Config.h"
#include "App/Launcher.h"
#include "App/Paths.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Core/Text.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/Account/AccountService.h"
#include "Services/Account/FriendsService.h"
#include "Services/Chapters/ChapterPlaytimeStore.h"
#include "UI/Layout.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Main/Common/ProfileDraw.h"
#include "Windows/Main/Overlays/Account/AccountKit.h"
#include "Windows/Main/Overlays/Account/AccountOverlay.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
using namespace AccountKit;
using Services::Account::AccountService;
using Services::Account::Profile;
namespace Community = Services::Account::Community;
namespace SC = Services::SpeedrunCom;

namespace {

constexpr float kTabsH = 66.f, kCategoryH = 34.f, kSideW = 300.f, kRowH = 56.f;
constexpr Color kGold = Hex(0xFFC83D), kSilver = Hex(0xC8D2DC), kBronze = Hex(0xD08A4A);

Color Medal(size_t place) { return place == 0 ? kGold : place == 1 ? kSilver : place == 2 ? kBronze : kSub; }

int ChapterOfGame(const std::wstring& gameId) {
    for (const auto& [chapter, id] : Config::SpeedrunCom::ChapterGameIds())
        if (id == gameId) return chapter;
    return 0;
}

std::wstring BannerOf(int chapter) {
    const std::wstring jpg = Paths::Asset(L"Banners\\Chapter " + std::to_wstring(chapter) + L".jpg");
    return Files::Exists(jpg) ? jpg : Paths::Asset(L"Banners\\Chapter " + std::to_wstring(chapter) + L".png");
}

// Where the character's face is on each chapter's poster (fraction of its height), so the wide
// tab shows the face instead of the middle of the picture
float FaceY(int chapter) {
    switch (chapter) {
        case 1: return 0.22f;
        case 2: return 0.40f;
        case 3: return 0.27f;
        case 4: return 0.18f;
        case 5: return 0.22f;
        default: return 0.3f;
    }
}

// The poster filling `r` (rounded), cropped around the face
void DrawArt(Ui& ui, const Image& art, const Rect& r, float radius, float opacity, int chapter) {
    auto& g = ui.g;
    const float scale = std::max(r.w / art.w, r.h / art.h);
    const float sw = r.w / scale, sh = r.h / scale;
    const float sx = (art.w - sw) * 0.5f;
    const float sy = std::clamp(art.h * FaceY(chapter) - sh * 0.5f, 0.f, art.h - sh);
    const D2D1_RECT_F source{ sx, sy, sx + sw, sy + sh };
    g.PushRoundClip(r, radius);
    g.DrawBitmap(art.bitmap.Get(), r, opacity, &source);
    g.PopLayer();
}

std::wstring NameOf(const Profile& p) { return p.ShownName(p.speedrunName.empty() ? Loc::Get("acc_default_name") : p.speedrunName); }

// A small rounded label; returns its width
float Chip(Ui& ui, float x, float cy, const std::wstring& text, Color color, wchar_t glyph = 0) {
    auto& g = ui.g;
    const TextStyle st = Mono(9.5f, true);
    const float tw = g.Measure(text, st).w;
    const float w = tw + 16.f + (glyph ? 16.f : 0.f);
    const Rect r{ x, cy - 10.f, w, 20.f };
    g.Border(r, 10.f, color.WithAlpha(0.12f), color.WithAlpha(0.35f), 1.f);
    float tx = r.x + 8.f;
    if (glyph) {
        UI::Glyph(ui, glyph, { tx - 2.f, r.y, 14.f, r.h }, 9.f, color);
        tx += 16.f;
    }
    g.Text(text, { tx, r.y, tw + 2.f, r.h }, color, st);
    return w;
}

struct Line {
    const Profile* profile;
    const Community::PersonalBest* pb;
    int runs;
};

} // namespace

// ── Data ────────────────────────────────────────────────────────────────────

void AccountOverlay::LoadRanking(bool force) {
    if (membersLoading || (membersLoaded && !force)) return;
    membersLoading = true;
    membersFailed = false;
    if (force) {
        runners.clear();
        runTypes.clear();
    }
    const unsigned gen = ++rankingGeneration;
    auto token = alive;
    Async::Run([] { return Community::FetchMembers(); }, [this, token, gen](std::optional<std::vector<Profile>> result) {
        if (!*token || gen != rankingGeneration) return;
        membersLoading = false;
        if (!result) {
            membersFailed = true;
            return;
        }
        members = std::move(*result);
        membersLoaded = true;
        // Each linked runner's personal bests and runs, one after another (speedrun.com rate limits)
        std::vector<std::wstring> ids;
        for (const auto& m : members)
            if (m.SpeedrunLinked() && !runners.count(m.speedrunId)) ids.push_back(m.speedrunId);
        runnersPending = static_cast<int>(ids.size());
        if (ids.empty()) return;
        Async::Run([this, token, gen, ids] {
            for (const auto& id : ids) {
                if (!*token) return;
                auto data = std::make_shared<Community::RunnerData>(Community::FetchRunner(id));
                Async::Post([this, token, gen, id, data] {
                    if (!*token || gen != rankingGeneration) return;
                    runners[id] = std::move(*data);
                    runnersPending = std::max(0, runnersPending - 1);
                });
            }
        });
    });
}

void AccountOverlay::RequestRunTypes(int chapter) {
    if (runTypes.count(chapter) || loadingTypes.count(chapter)) return;
    loadingTypes.insert(chapter);
    auto token = alive;
    Async::Run([chapter] { return SC::GetRunTypes(chapter); }, [this, token, chapter](std::vector<SC::RunType> types) {
        if (!*token) return;
        loadingTypes.erase(chapter);
        runTypes[chapter] = std::move(types);
    });
}

// ── Page ────────────────────────────────────────────────────────────────────
//   [ chapter cards with their art ......................................... ⟳ ]
//   [ Any% · 100% ...........................................  selectors ]
//   [ podium (top 3)                                  ] [ YOUR POSITION   ]
//   [ the rest: rank · player · time · gap · ...      ] [ THIS CHAPTER    ]
void AccountOverlay::DrawRanking(Ui& ui, const Rect& body) {
    auto& g = ui.g;
    LoadRanking(false);
    RequestRunTypes(rankChapter);

    {
        StaggerItem in(ui, tabChangedAt, 0, 0.f, -8.f);
        constexpr float kRefreshW = 40.f;
        const Rect tabs = body.Top(kTabsH);
        DrawRankTabs(ui, tabs.CutRight(kRefreshW + 10.f));
        const Rect refresh{ tabs.R() - kRefreshW, tabs.y, kRefreshW, tabs.h };
        const bool busy = membersLoading || runnersPending > 0;
        if (MenuKit::FooterButton(ui, ui.MakeId("acc-rank-refresh"), refresh, L"", MenuKit::Kind::Secondary, !busy, Glyphs::Refresh, kAccent))
            LoadRanking(true);
        ui.Tooltip(ui.MakeId("acc-rank-refresh"), Loc::Get("acc_rank_refresh"));
    }

    const Rect categoryRow{ body.x, body.y + kTabsH + 12.f, body.w, kCategoryH };
    const Rect content{ body.x, categoryRow.B() + 14.f, body.w, body.B() - categoryRow.B() - 14.f };
    auto types = runTypes.find(rankChapter);
    if (types == runTypes.end()) {
        Form::Spinner(ui, categoryRow.x + 10.f, categoryRow.CY(), 7.f, kAccent);
        g.Text(Loc::Get("lb_loading_categories"), categoryRow.CutLeft(28.f), kSub, Mono(11.f, true));
        ui.KeepAnimatingThrottled();
        return;
    }
    if (types->second.empty()) {
        g.Text(Loc::Get("lb_load_failed"), content.Top(60.f), Palette::DangerText, Mono(11.f, true).Center());
        return;
    }
    {
        StaggerItem in(ui, tabChangedAt, 1, 0.f, -8.f);
        DrawRankCategories(ui, categoryRow, types->second);
    }
    const int index = std::clamp(rankCategory[rankChapter], 0, static_cast<int>(types->second.size()) - 1);
    DrawRankTable(ui, content, types->second[static_cast<size_t>(index)]);
}

// One card per chapter with its art, number and name; the selected one lights up
void AccountOverlay::DrawRankTabs(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    const auto& chapters = Config::SpeedrunCom::ChapterGameIds();
    const float gap = 10.f;
    const float w = (r.w - gap * (chapters.size() - 1)) / chapters.size();
    size_t i = 0;
    for (const auto& [number, gameId] : chapters) {
        const Rect t{ r.x + i * (w + gap), r.y, w, r.h };
        const Id id = ui.MakeId("acc-rank-ch", number);
        const Interaction it = ui.Interact(id, t);
        const bool sel = number == rankChapter;
        const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 16.f);
        const float on = ui.Animate(HashId(id, 2), sel ? 1.f : 0.f, 12.f);
        const Rect card = t.Offset(0.f, -2.f * hover);
        if (on > 0.01f) g.GlowRound(card, 10.f, kAccent, 16.f, 0.35f * on);
        {
            NoColorFilter keep(g);
            g.FillRound(card, 10.f, Hex(0x05080C));
            if (auto art = Images::Load(BannerOf(number), 512)) DrawArt(ui, *art, card, 10.f, 0.35f + 0.45f * std::max(on, hover * 0.6f), number);
            g.FillLinear(card, 10.f, { { 0.f, Rgba(0, 0, 0, 40) }, { 1.f, Rgba(0, 0, 0, 210) } }, { 0.f, 0.f }, { 0.f, 1.f });
        }
        g.StrokeRound(card, 10.f, Lerp(Lerp(kCardBorder, kHoverBorder, hover), kAccent, on), on > 0.5f ? 1.5f : 1.f);
        g.Text(Loc::Fmt("rk_chapter_short", number), { card.x + 12.f, card.y + 10.f, card.w - 24.f, 16.f }, Lerp(kSub, kAccent, on), Mono(9.5f, true));
        g.Text(Loc::Get("ch" + std::to_string(number) + "_title"), { card.x + 12.f, card.B() - 30.f, card.w - 24.f, 22.f },
               Lerp(Rgba(225, 232, 240, 200), Colors::White, std::max(on, hover)), Mono(11.f, true).Ellipsis());
        if (on > 0.01f) g.FillRound({ card.x + 12.f, card.B() - 6.f, (card.w - 24.f) * on, 3.f }, 1.5f, kAccent);
        if (it.hovered && !sel) ui.SetCursor(IDC_HAND);
        if (it.clicked && !sel) {
            rankChapter = number;
            rankChangedAt = Async::Now();
        }
        ++i;
    }
}

void AccountOverlay::DrawRankCategories(Ui& ui, const Rect& r, std::vector<RunType>& types) {
    auto& g = ui.g;
    int& current = rankCategory[rankChapter];
    current = std::clamp(current, 0, static_cast<int>(types.size()) - 1);
    float x = r.x;
    float selX = r.x, selW = 0.f;
    for (size_t i = 0; i < types.size(); ++i) {
        const float w = g.Measure(types[i].label, Mono(11.f, true)).w + 32.f;
        const Rect t{ x, r.y, w, r.h };
        const Id id = ui.MakeId("acc-rank-cat", static_cast<int64_t>(i));
        const Interaction it = ui.Interact(id, t);
        const bool sel = static_cast<int>(i) == current;
        const float hover = ui.Animate(HashId(id, 1), it.hovered && !sel ? 1.f : 0.f, 16.f);
        g.Border(t, r.h * 0.5f, Lerp(kCard, kRowHover, hover), Lerp(kCardBorder, kHoverBorder, hover), 1.f);
        g.Text(types[i].label, t, sel ? kAccent : Lerp(kSub, kText, hover), Mono(11.f, true).Center());
        if (sel) {
            selX = t.x;
            selW = t.w;
        }
        if (it.hovered && !sel) ui.SetCursor(IDC_HAND);
        if (it.clicked && !sel) {
            current = static_cast<int>(i);
            rankChangedAt = Async::Now();
        }
        x += w + 8.f;
    }
    // Capsule gliding to the selected category
    const float cx = ui.Animate(ui.MakeId("acc-rank-cat-cap"), selX, 14.f, selX);
    const float cw = ui.Animate(ui.MakeId("acc-rank-cat-capw"), selW, 14.f, selW);
    g.Border({ cx, r.y, cw, r.h }, r.h * 0.5f, Rgba(0, 204, 170, 24), kAccent, 1.f);

    // Subcategory selectors (Route, Restriction, ...) on the right
    auto& type = types[static_cast<size_t>(current)];
    float right = r.R();
    for (auto it = type.secondaryVariables.rbegin(); it != type.secondaryVariables.rend(); ++it) {
        auto& variable = *it;
        std::vector<std::wstring> labels;
        int selected = 0;
        for (size_t k = 0; k < variable.values.size(); ++k) {
            labels.push_back(variable.values[k].label);
            if (variable.values[k].id == variable.selectedValueId) selected = static_cast<int>(k);
        }
        float w = 120.f;
        for (const auto& label : labels) w = std::max(w, g.Measure(label, Mono(10.f)).w + 46.f);
        w = std::min(w, 200.f);
        right -= w;
        if (right < x + 10.f) break;
        if (ComboBox(ui, ui.MakeId("acc-rank-var", static_cast<int64_t>(std::hash<std::wstring>{}(variable.id))), { right, r.y + 3.f, w, r.h - 6.f }, selected,
                     labels)) {
            variable.selectedValueId = variable.values[static_cast<size_t>(selected)].id;
            rankChangedAt = Async::Now();
        }
        right -= 8.f;
    }
}

void AccountOverlay::DrawRankTable(Ui& ui, const Rect& r, const RunType& runType) {
    auto& g = ui.g;
    auto& account = AccountService::Get();
    auto& friends = Services::Account::FriendsService::Get();
    const int chapter = ChapterOfGame(runType.gameId);
    const std::wstring& myId = account.UserId();
    const double shown = std::max(rankChangedAt, tabChangedAt);

    // Who has a run here, fastest first
    std::vector<Line> lines;
    int linked = 0, withoutRun = 0, verifiedRuns = 0;
    double communityHours = 0.0;
    for (const auto& m : members) {
        if (auto h = m.playtime.find(chapter); h != m.playtime.end()) communityHours += h->second;
        if (!m.SpeedrunLinked()) continue;
        ++linked;
        auto data = runners.find(m.speedrunId);
        if (data == runners.end() || !data->second.ok) continue;
        auto runs = data->second.verifiedRuns.find(runType.gameId);
        const int runCount = runs == data->second.verifiedRuns.end() ? 0 : runs->second;
        verifiedRuns += runCount;
        if (const Community::PersonalBest* pb = Community::BestFor(data->second, runType)) lines.push_back({ &m, pb, runCount });
        else ++withoutRun;
    }
    std::sort(lines.begin(), lines.end(), [](const Line& a, const Line& b) { return a.pb->time < b.pb->time; });
    const double best = lines.empty() ? 0.0 : lines.front().pb->time;
    const auto open = [&](const Line& line) { OpenViewer(*line.profile, line.pb->weblink, runType.label + L"  \x00B7  " + SC::FormatTime(line.pb->time)); };

    const Rect mainArea = r.CutRight(kSideW + 18.f);
    const Rect side = r.Right(kSideW);

    // ── Side: your position + this chapter ──
    {
        StaggerItem in(ui, shown, 0, 20.f);
        Column col(side.x, side.y, side.w);
        const Rect you = col.Next(150.f, 14.f);
        Card(ui, you, kAccent.WithAlpha(0.4f));
        const Rect c = you.Inset(16.f, 14.f);
        Caption(ui, c.x, c.y, c.w, Loc::Get("rk_you"), kAccent);
        size_t myIndex = lines.size();
        for (size_t i = 0; i < lines.size(); ++i)
            if (lines[i].profile->userId == myId) myIndex = i;
        if (myIndex < lines.size()) {
            const Line& myLine = lines[myIndex];
            g.Text(L"#" + std::to_wstring(myIndex + 1), { c.x, c.y + 22.f, 90.f, 44.f }, Medal(myIndex), Mono(30.f, true));
            g.Text(Loc::Fmt("rk_of", lines.size()), { c.x + 92.f, c.y + 38.f, c.w - 92.f, 20.f }, kSub, Mono(10.f, true));
            g.Text(SC::FormatTime(myLine.pb->time), { c.x, c.y + 70.f, c.w, 22.f }, kText, Mono(15.f, true));
            const std::wstring note = myIndex == 0 ? Loc::Get("rk_first") : Loc::Fmt("rk_gap_first", SC::FormatTime(myLine.pb->time - best));
            g.Text(note, { c.x, c.y + 96.f, c.w, 18.f }, myIndex == 0 ? kGold : kSub, Mono(9.5f, true).Ellipsis());
            if (myLine.pb->place > 0) Chip(ui, c.R() - 90.f, c.y + 8.f, Loc::Fmt("rk_global", myLine.pb->place), kSpeedrun);
        } else if (account.Me().SpeedrunLinked()) {
            TextStyle st = Mono(10.5f, true);
            st.wrap = true;
            g.Text(Loc::Get("rk_no_run"), { c.x, c.y + 28.f, c.w, 60.f }, kSub, st);
        } else {
            TextStyle st = Mono(10.f);
            st.wrap = true;
            g.Text(Loc::Get("rk_link"), { c.x, c.y + 26.f, c.w, 44.f }, kSub, st);
            const std::wstring label = Loc::Get("rk_link_btn");
            const float bw = MenuKit::FooterButtonWidth(ui, label, wchar_t(0xE71B), 110.f);
            if (MenuKit::FooterButton(ui, ui.MakeId("rk-go-link"), { c.x, c.B() - 34.f, bw, 34.f }, label, MenuKit::Kind::Primary, true, wchar_t(0xE71B), kSpeedrun)) {
                tab = 0;
                tabChangedAt = Async::Now();
            }
        }

        const Rect stats = col.Next(std::min(178.f, side.B() - col.y), 0.f);
        Card(ui, stats);
        const Rect s = stats.Inset(16.f, 14.f);
        Caption(ui, s.x, s.y, s.w, Loc::Fmt("rk_stats", chapter));
        const std::pair<const char*, std::wstring> rows[] = {
            { "rk_stat_linked", std::to_wstring(linked) },
            { "rk_stat_with_run", std::to_wstring(lines.size()) },
            { "rk_stat_runs", std::to_wstring(verifiedRuns) },
            { "rk_stat_hours", Services::ChapterPlaytimeStore::Format(communityHours) },
        };
        float y = s.y + 26.f;
        for (const auto& [key, value] : rows) {
            if (y + 26.f > s.B() + 2.f) break;
            g.Text(Loc::Get(key), { s.x, y, s.w - 70.f, 24.f }, kSub, Mono(9.5f).Ellipsis());
            g.Text(value, { s.R() - 70.f, y, 70.f, 24.f }, kText, Mono(11.f, true).Right());
            g.FillRect({ s.x, y + 25.f, s.w, 1.f }, Palette::Divider);
            y += 28.f;
        }
    }

    // Status of the data (bottom of the main column)
    std::wstring note;
    const bool busy = membersLoading || runnersPending > 0;
    if (membersLoading) note = Loc::Get("acc_rank_loading");
    else if (membersFailed) note = Loc::Get("acc_rank_failed");
    else if (runnersPending > 0) note = Loc::Fmt("acc_rank_loading_runs", runnersPending);
    else if (withoutRun > 0) note = Loc::Fmt("acc_rank_without_run", withoutRun);
    const Rect footer = mainArea.Bottom(20.f);
    if (!note.empty()) {
        if (busy) {
            Form::Spinner(ui, footer.x + 8.f, footer.CY(), 6.f, kAccent);
            ui.KeepAnimatingThrottled();
        }
        g.Text(note, footer.CutLeft(busy ? 22.f : 0.f), kSub, Mono(9.5f).Ellipsis());
    }
    const Rect area = mainArea.CutBottom(28.f);

    if (lines.empty()) {
        Card(ui, area);
        UI::Glyph(ui, wchar_t(0xE9F9), area.CenterV(40.f).Offset(0.f, -22.f), 30.f, kCardBorder);
        if (!busy) g.Text(Loc::Get(linked == 0 ? "acc_rank_nobody" : "acc_rank_empty"), area.CenterV(20.f).Offset(0.f, 22.f), kSub, Mono(11.f, true).Center());
        return;
    }

    // ── The table: # · player · time · speedrun.com place · date · runs · hours ──
    Card(ui, area);
    const Rect inner = area.Inset(14.f, 10.f);
    // Compact columns so the player keeps room for the name; the date goes when the table is narrow
    const bool showDate = inner.w >= 820.f;
    const float colTime = 96.f, colPlace = 104.f, colDate = showDate ? 92.f : 0.f, colRuns = 52.f, colHours = 70.f, kGapX = 12.f;
    const auto cells = [&](const Rect& row) {
        std::vector<Rect> c;
        float x = row.R();
        const float widths[] = { colHours, colRuns, colDate, colPlace, colTime };
        for (float w : widths) {
            x -= w;
            c.insert(c.begin(), Rect{ x, row.y, w, row.h });
            if (w > 0.f) x -= kGapX;
        }
        c.insert(c.begin(), Rect{ row.x + 46.f + kGapX, row.y, x - row.x - 46.f - kGapX, row.h });
        c.insert(c.begin(), Rect{ row.x, row.y, 46.f, row.h });
        return c;
    };
    const HAlign aligns[] = { HAlign::Center, HAlign::Left, HAlign::Right, HAlign::Center, HAlign::Center, HAlign::Center, HAlign::Right };
    const Rect header{ inner.x + 10.f, inner.y, inner.w - 30.f, 30.f };
    const auto hc = cells(header);
    const char* heads[] = { "acc_rank_col_pos", "acc_rank_col_player", "acc_rank_col_time", "acc_rank_col_place", "acc_rank_col_date", "acc_rank_col_runs",
                            "acc_rank_col_hours" };
    for (size_t c = 0; c < hc.size(); ++c) {
        if (hc[c].w <= 0.f) continue;
        TextStyle st = Mono(9.5f, true);
        st.align = aligns[c];
        g.Text(Loc::Get(heads[c]), hc[c], kSub, st);
    }
    g.FillRect({ inner.x, header.B(), inner.w, 1.f }, kCardBorder);

    const Rect list{ inner.x, header.B() + 6.f, inner.w, inner.B() - header.B() - 6.f };
    ScrollArea scroll(ui, ui.MakeId("acc-rank-list", rankChapter), list);
    for (size_t i = 0; i < lines.size(); ++i) {
        const Line& line = lines[i];
        StaggerItem in(ui, shown, static_cast<int>(i), 0.f, 10.f, 0.035, 0.32);
        const Rect row{ list.x, list.y + i * (kRowH + 4.f), list.w - 10.f, kRowH };
        const Id id = ui.MakeId("acc-rank-row", static_cast<int64_t>(i));
        const Interaction it = ui.Interact(id, row);
        const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 16.f);
        const bool mine = line.profile->userId == myId;
        const bool friend_ = !mine && friends.IsFriend(line.profile->userId);
        const Color medal = Medal(i);
        g.Border(row, 8.f, Lerp(mine ? Rgba(0, 204, 170, 16) : kRow, kRowHover, hover), mine ? kAccent.WithAlpha(0.6f) : Lerp(kCardBorder.WithAlpha(0.5f), kHoverBorder, hover),
                 1.f);
        ProfileDraw::Nameplate(ui, row, 8.f, *line.profile);
        const bool plate = ProfileDraw::HasNameplate(*line.profile);
        const auto c = cells(row.Inset(10.f, 0.f));
        if (i < 3) g.FillRound({ row.x, row.y + 8.f, 3.f, row.h - 16.f }, 1.5f, medal);

        ProfileDraw::RowText(ui, L"#" + std::to_wstring(i + 1), c[0], medal, Mono(i < 3 ? 15.f : 13.f, true).Center(), plate);
        {
            const Rect cell = c[1];
            const Rect pic{ cell.x, cell.CY() - 19.f, 38.f, 38.f };
            ProfileDraw::Avatar(ui, line.profile->AvatarUrl(), pic, *line.profile, NameOf(*line.profile), i < 3 ? 2.f : 0.f, medal);
            const float tw = cell.w - 48.f;
            ui.PushClip({ pic.R() + 10.f, cell.y, tw, cell.h });
            ProfileDraw::Name(ui, NameOf(*line.profile), { pic.R() + 10.f, cell.y + 5.f, tw, 26.f }, *line.profile, 15.f);
            ui.PopClip();
            float chipX = pic.R() + 10.f;
            if (mine) chipX += Chip(ui, chipX, cell.y + 40.f, Loc::Get("rk_you_chip"), kAccent) + 6.f;
            else if (friend_) {
                // Friends: just the icon
                const Rect mark{ chipX - 1.f, cell.y + 31.f, 16.f, 16.f };
                UI::Glyph(ui, wchar_t(0xE716), mark, 10.f, kAccent);
                if (ui.MouseOver(mark)) ui.Tooltip(ui.MakeId("rk-friend-mark", static_cast<int64_t>(i)), Loc::Get("fr_is_friend"));
                chipX += 20.f;
            }
            ProfileDraw::RowText(ui, L"speedrun.com/" + line.profile->speedrunName, { chipX, cell.y + 31.f, std::max(0.f, cell.R() - chipX), 16.f },
                                 plate ? kSpeedrun : kSpeedrun.WithAlpha(0.8f), Mono(9.5f, plate).Ellipsis(), plate);
        }
        // On a nameplate the columns get a dark halo (and the grey ones turn white) so they read on it
        const Color sub = plate ? kText : kSub;
        ProfileDraw::RowText(ui, SC::FormatTime(line.pb->time), c[2], i == 0 ? kGold : kText, Mono(13.f, true).Right(), plate);
        // The real place on speedrun.com's leaderboard
        ProfileDraw::RowText(ui, line.pb->place > 0 ? L"#" + std::to_wstring(line.pb->place) : L"\x2014", c[3], kSpeedrun, Mono(12.f, true).Center(), plate);
        if (showDate) ProfileDraw::RowText(ui, line.pb->date.empty() ? L"\x2014" : line.pb->date, c[4], sub, Mono(10.f, plate).Center(), plate);
        ProfileDraw::RowText(ui, std::to_wstring(line.runs), c[5], kText, Mono(11.f, true).Center(), plate);
        auto hours = line.profile->playtime.find(chapter);
        ProfileDraw::RowText(ui, hours == line.profile->playtime.end() ? L"\x2014" : Services::ChapterPlaytimeStore::Format(hours->second), c[6], sub,
                             Mono(10.5f, true).Right(), plate);
        if (it.hovered) {
            ui.SetCursor(IDC_HAND);
            ui.Tooltip(id, Loc::Get("acc_rank_open_profile"));
        }
        if (it.clicked) open(line);
    }
    scroll.SetContentSize(lines.size() * (kRowH + 4.f));
}

} // namespace SpeedrunLauncher::Main
