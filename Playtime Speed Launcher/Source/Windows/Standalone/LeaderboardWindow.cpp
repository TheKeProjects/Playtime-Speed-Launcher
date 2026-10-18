#include "Windows/Standalone/LeaderboardWindow.h"
#include <algorithm>
#include <cmath>
#include "App/Launcher.h"
#include "Core/Async.h"
#include "Core/Process.h"
#include "Core/Text.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Seasonal/SeasonalCardArt.h"
#include "Seasonal/SeasonalTheme.h"
#include "UI/Layout.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Panel.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Standalone/Common/RemoteImages.h"

namespace SpeedrunLauncher::Windows {

using namespace UI;
namespace SC = Services::SpeedrunCom;

namespace {

constexpr float kRadius = 12.f, kHeaderH = 64.f;
constexpr float kListRowH = 50.f, kListGap = 6.f;
constexpr double kPopIn = 0.34, kPopOut = 0.2;
constexpr Color kPanelBg = Argb(0xF207101A);
constexpr Color kPanelBorder = Hex(0x0D3020);
constexpr Color kDim = Rgba(58, 106, 138);
constexpr Color kText = Hex(0xD2DCE6);
constexpr Color kSub = Hex(0x5A8AA0);
constexpr Color kFaint = Hex(0x2A5A7A);
constexpr Color kCardBg = Rgba(255, 255, 255, 8);
constexpr Color kCardBorder = Hex(0x0D2535);
constexpr Color kGold = Rgba(255, 210, 77), kSilver = Rgba(200, 210, 220), kBronze = Rgba(205, 140, 95);
constexpr Color kVideo = Rgba(230, 70, 70);

std::wstring RunnersKey(const SC::RunType& r) { return r.gameId + L"|" + r.categoryId + L"|" + SC::BuildVarQuery(r); }

Color Medal(int place) { return place == 1 ? kGold : place == 2 ? kSilver : place == 3 ? kBronze : kDim; }

std::wstring ChapterSubtitle(const Models::Chapter& ch) {
    const size_t colon = ch.subTitle.find(L':');
    return colon != std::wstring::npos ? Text::Trim(ch.subTitle.substr(colon + 1)) : ch.subTitle;
}

// Where a non-YouTube video opens ("Twitch", or "the browser" for unknown sites)
std::wstring VideoSite(const SC::LeaderboardEntry& e) { return e.videoSite.empty() ? Loc::Get("lb_browser") : e.videoSite; }

std::wstring Ago(int days) {
    if (days < 0) return L"";
    if (days == 0) return Loc::Get("lb_today");
    if (days == 1) return Loc::Get("lb_yesterday");
    if (days < 31) return Loc::Fmt("lb_days_ago", days);
    if (days < 365) return days / 30 == 1 ? Loc::Get("lb_month_ago") : Loc::Fmt("lb_months_ago", days / 30);
    return days / 365 == 1 ? Loc::Get("lb_year_ago") : Loc::Fmt("lb_years_ago", days / 365);
}

// Initials for the podium avatars ("Duck1ng" → "D", "Foo & Bar" → "FB")
std::wstring Initials(const std::wstring& name) {
    std::wstring out;
    bool next = true;
    for (wchar_t c : name) {
        if (c == L' ' || c == L'&') {
            next = true;
            continue;
        }
        if (next && iswalnum(c)) {
            out += static_cast<wchar_t>(towupper(c));
            next = false;
            if (out.size() == 2) break;
        }
    }
    return out.empty() ? L"?" : out;
}

// Round icon button (▶ video / ↗ run). Returns true when clicked.
bool IconButton(Ui& ui, Id id, const Rect& r, wchar_t glyph, Color accent, const std::wstring& tooltip, bool enabled = true) {
    auto& g = ui.g;
    const Interaction it = ui.Interact(id, r);
    const float hover = ui.Animate(HashId(1, id), enabled && it.hovered ? 1.f : 0.f, 18.f);
    if (hover > 0.01f) g.GlowRound(r, r.h * 0.5f, accent, 10.f, 0.4f * hover);
    g.Border(r, r.h * 0.5f, Lerp(Rgba(255, 255, 255, 10), accent.WithAlpha(0.2f), hover), Lerp(kCardBorder, accent.WithAlpha(0.8f), hover), 1.f);
    g.Text(std::wstring(1, glyph), r, enabled ? Lerp(accent.WithAlpha(0.75f), accent, hover) : kFaint, IconFont(11.f));
    if (enabled && it.hovered) ui.SetCursor(IDC_HAND);
    ui.Tooltip(id, tooltip);
    return enabled && it.clicked;
}

// Wide button with a glyph (details panel). Returns true when clicked.
bool ActionButton(Ui& ui, Id id, const Rect& r, wchar_t glyph, const std::wstring& text, Color accent, bool filled, bool enabled = true) {
    auto& g = ui.g;
    const Interaction it = ui.Interact(id, r);
    const float hover = ui.Animate(HashId(1, id), enabled && it.hovered ? 1.f : 0.f, 18.f);
    const float press = ui.Animate(HashId(2, id), enabled && it.held ? 1.f : 0.f, 30.f);
    const Rect b = r.Inset(press, press * 0.5f);
    if (enabled && (filled || hover > 0.01f)) g.GlowRound(r, 8.f, accent, 12.f, (filled ? 0.25f : 0.f) + 0.3f * hover);
    const float alpha = enabled ? 1.f : 0.35f;
    g.Border(b, 8.f, accent.WithAlpha((filled ? 0.22f : 0.06f) + 0.14f * hover), accent.WithAlpha((0.45f + 0.4f * hover) * alpha), 1.f);
    const TextStyle style = Mono(9.f, true);
    const float tw = g.Measure(text, style).w + 1.f;
    const float x = b.CX() - (16.f + 8.f + tw) * 0.5f;
    g.Text(std::wstring(1, glyph), { x, b.y, 16.f, b.h }, accent.WithAlpha(alpha), IconFont(10.f));
    g.Text(text, { x + 24.f, b.y, tw, b.h }, accent.WithAlpha(alpha), style);
    if (enabled && it.hovered) ui.SetCursor(IDC_HAND);
    return enabled && it.clicked;
}

void Shimmer(Graphics& g, const Rect& r, float radius, double time) {
    const float sweep = static_cast<float>(std::fmod(time * 0.8, 1.3)) - 0.15f;
    g.PushRoundClip(r, radius);
    g.FillLinear({ r.x + r.w * sweep - 80.f, r.y, 160.f, r.h }, 0.f,
                 { { 0.f, Rgba(255, 255, 255, 0) }, { 0.5f, Rgba(255, 255, 255, 16) }, { 1.f, Rgba(255, 255, 255, 0) } }, { 0.f, 0.f }, { 1.f, 0.f });
    g.PopLayer();
}

} // namespace

LeaderboardWindow::LeaderboardWindow() = default;
LeaderboardWindow::~LeaderboardWindow() {
    *alive = false;
    player.Destroy();
}

bool LeaderboardWindow::Open() {
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), &mi);
    UI::Window::Options o;
    o.title = L"Leaderboards";
    o.style = WS_POPUP | WS_CLIPCHILDREN;   // the video player is a child window
    o.exStyle = WS_EX_TOOLWINDOW;
    o.transparent = true;
    o.topmost = true;
    o.showCommand = SW_HIDE;
    if (!Create(o)) return false;
    g.SetColorFilter(&Seasonal::Filter);   // follows the seasonal theme like the main window
    if (Seasonal::IsSeasonal()) {          // seasonal particles: idle frames at 30 fps
        SetContinuous(true, true);
        SetIdleFps(30.0);
    }
    SetWindowPos(hwnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top, 0);
    player.Create(hwnd, { true, Loc::Get("lb_loading_video"), Rgba(200, 80, 60, 200) });
    player.onFailed = [this] { player.ShowError(Loc::Get("lb_video_failed")); };
    openedAt = Async::Now();
    Show(SW_SHOW);
    Activate();
    if (!App::L().chapters.empty()) SelectChapter(App::L().chapters.front().number);
    return true;
}

// Esc / the hotkey again / the owner closing it: pop out first
void LeaderboardWindow::OnClosing(bool& cancel) {
    if (finished) return;
    cancel = true;
    BeginClose();
    Invalidate();
}

void LeaderboardWindow::BeginClose() {
    if (closingAt >= 0.0) return;
    closingAt = Async::Now();
    if (video.open) player.Stop();
}

void LeaderboardWindow::SelectChapter(int number) {
    chapter = number;
    openSelector.clear();
    selectedRunner = 0;
    categoryChangedAt = Async::Now();
    if (runTypes.count(number) || loadingChapters.count(number)) return;
    loadingChapters.insert(number);
    auto guard = alive;
    Async::Run([number] { return SC::GetRunTypes(number); }, [this, guard, number](std::vector<SC::RunType> list) {
        if (!*guard) return;
        loadingChapters.erase(number);
        runTypes[number] = std::move(list);
        categoryChangedAt = Async::Now();
        Invalidate();
    });
}

void LeaderboardWindow::SelectCategory(int index) {
    if (category[chapter] == index) return;
    category[chapter] = index;
    openSelector.clear();
    selectedRunner = 0;
    categoryChangedAt = Async::Now();
}

void LeaderboardWindow::RequestRunners(const SC::RunType& runType) {
    const std::wstring key = RunnersKey(runType);
    if (runners.count(key) || loadingRunners.count(key)) return;
    loadingRunners.insert(key);
    auto guard = alive;
    Async::Run([runType] { return SC::GetTopRunners(runType); }, [this, guard, key](std::vector<SC::LeaderboardEntry> list) {
        if (!*guard) return;
        loadingRunners.erase(key);
        runners[key] = std::move(list);
        runnersLoadedAt[key] = Async::Now();
        Invalidate();
    });
}

// ── Video: YouTube plays here (like the tutorials), anything else opens in the browser ──

void LeaderboardWindow::PlayVideo(const Entry& entry) {
    if (entry.youtubeId.empty()) {
        Process::OpenUrl(entry.videoUrl);
        return;
    }
    video = {};
    video.open = true;
    video.title = entry.playerName;
    const auto& types = runTypes[chapter];
    const int cat = category[chapter];
    video.subtitle = L"#" + std::to_wstring(entry.place) + L"  ·  " + SC::FormatTime(entry.timeSeconds) +
                     (cat < static_cast<int>(types.size()) ? L"  ·  " + types[static_cast<size_t>(cat)].label : L"") +
                     L"  ·  " + Loc::Fmt("lb_chapter", chapter);
    video.openedAt = Async::Now();
    player.Load(entry.youtubeId);
}

void LeaderboardWindow::CloseVideo() {
    if (!video.open || video.closingAt >= 0.0) return;
    video.closingAt = Async::Now();
    player.Stop();   // hides the player window right away (the panel fades after it)
}

void LeaderboardWindow::DrawVideo(const Rect& screen) {
    const double now = Async::Now();
    float closeT = 0.f;
    if (video.closingAt >= 0.0) {
        closeT = Anim::Clamp01(static_cast<float>((now - video.closingAt) / 0.18));
        ui.KeepAnimating();
    }
    const float openT = Anim::Clamp01(static_cast<float>((now - video.openedAt) / 0.3));
    if (openT < 1.f) ui.KeepAnimating();
    const float fade = Anim::EaseOutCubic(std::min(1.f, openT * 1.5f)) * (1.f - Anim::EaseInCubic(closeT));
    const bool layered = fade < 0.999f;   // a layer only while it fades
    if (layered) g.PushOpacity(fade);
    g.FillRect(screen, Palette::BackdropDark);
    const Interaction backdrop = ui.Interact(ui.MakeId("video-backdrop"), screen, true, false);

    // 16:9 video + seek row + controls, sized to the screen
    const float videoW = std::min(screen.w - 120.f, (screen.h - 220.f) * 16.f / 9.f);
    const float videoH = videoW * 9.f / 16.f;
    const Rect card = screen.Center(videoW + 2.f, 58.f + videoH + 22.f + 52.f + 2.f).Offset(0.f, (1.f - Anim::EaseOutCubic(openT)) * 16.f);
    ui.Block(card);
    PanelGlow(ui, card, kRadius, kVideo, static_cast<float>(now - video.openedAt));
    g.Border(card, kRadius, kPanelBg, kPanelBorder, 1.f);
    const Rect header{ card.x + 1.f, card.y + 1.f, card.w - 2.f, 58.f };
    g.FillRound(header, kRadius - 1.f, kRadius - 1.f, 0.f, 0.f, Hex(0x05090F));
    g.FillRect({ header.x, header.B() - 1.f, header.w, 1.f }, Palette::Divider);
    {
        NoColorFilter keep(g);
        g.FillEllipse(header.x + 30.f, header.CY(), 15.f, 15.f, kVideo.WithAlpha(0.18f));
        g.StrokeEllipse(header.x + 30.f, header.CY(), 15.f, 15.f, kVideo.WithAlpha(0.7f), 1.f);
        g.Text(L"\xE768", { header.x + 17.f, header.y, 28.f, header.h }, kVideo, IconFont(11.f));
    }
    g.Text(video.title, { header.x + 56.f, header.y + 11.f, header.w - 120.f, 20.f }, kText, Mono(13.f, true).Ellipsis());
    g.Text(video.subtitle, { header.x + 56.f, header.y + 31.f, header.w - 120.f, 16.f }, kSub, Mono(9.f, true).Ellipsis());
    const ButtonStyle round{ Palette::ButtonBg, Palette::ButtonBorder, 1.f, 16.f, { 0.f }, 0.f, 32.f };
    if (TextButton(ui, ui.MakeId("video-close"), { header.R() - 16.f - 32.f, header.CY() - 16.f, 32.f, 32.f }, L"\x2715",
                   Mono(12.f, true).WithFont(Font::Ui).Center(), Palette::Dim, round))
        CloseVideo();
    player.Draw(ui, { card.x + 1.f, header.B(), videoW, videoH + 22.f + 52.f }, DpiScale(), true, true, false);
    if (layered) g.PopLayer();
    if (backdrop.held && ui.In().pressed[0] && !ui.MouseOver(card)) CloseVideo();
    if (closeT >= 1.f) video = {};
}

// ── Runner name with its flag in front ──────────────────────────────────────

float LeaderboardWindow::DrawRunnerName(const Entry& e, float x, float y, float maxW, float h, const TextStyle& style, float hover, bool center) {
    const float flagW = e.country.empty() ? 0.f : 20.f;
    const float gap = flagW > 0.f ? 7.f : 0.f;
    const float nameW = std::min(g.Measure(e.playerName, style).w + 1.f, maxW - flagW - gap);
    const float total = flagW + gap + nameW;
    float cx = center ? x + (maxW - total) * 0.5f : x;
    NoColorFilter keep(g);   // real flags and the runner's own colour
    if (flagW > 0.f) {
        const Rect flag{ cx, y + h * 0.5f - 7.f, 20.f, 14.f };
        if (auto img = RemoteImages::Get(e.flagUrl)) {
            g.DrawImageRounded(*img, flag, 2.f, 1.f, Stretch::UniformToFill);
        } else {
            // Not downloaded yet: the code in a little box meanwhile
            g.Border(flag, 2.f, Rgba(255, 255, 255, 12), kCardBorder, 1.f);
            g.Text(Text::Upper(e.country.substr(0, 2)), flag, kSub, Mono(7.f, true).Center());
            if (Async::Now() - openedAt < 20.0) ui.KeepAnimatingThrottled();   // a flag that failed to download isn't waited for forever
        }
        cx += flagW + gap;
    }
    const Color nameColor = e.nameColor ? Hex(e.nameColor) : kText;
    TextStyle s = style;
    s.ellipsis = true;
    g.Text(e.playerName, { cx, y, nameW, h }, Lerp(nameColor, Colors::White, 0.25f * hover), s);
    return total;
}

// ── Tabs ────────────────────────────────────────────────────────────────────

void LeaderboardWindow::DrawChapterTabs(const Rect& r) {
    const auto& chapters = App::L().chapters;
    if (chapters.empty()) return;
    const float gap = 8.f;
    const float w = (r.w - gap * (chapters.size() - 1)) / chapters.size();
    float selX = -1.f;
    for (size_t i = 0; i < chapters.size(); ++i) {
        const auto& ch = chapters[i];
        StaggerItem in(ui, openedAt + 0.08, static_cast<int>(i), 0.f, -10.f, 0.04, 0.34);
        const Rect tab{ r.x + i * (w + gap), r.y, w, r.h };
        const Interaction it = ui.Interact(ui.MakeId("chapter-tab", ch.number), tab);
        const bool selected = ch.number == chapter;
        const float hover = ui.Animate(ui.MakeId("chapter-tab-hover", ch.number), it.hovered && !selected ? 1.f : 0.f, 16.f);
        const float sel = ui.Animate(ui.MakeId("chapter-tab-sel", ch.number), selected ? 1.f : 0.f, 12.f);
        g.Border(tab, r.h * 0.5f, Lerp(kCardBg, Rgba(255, 255, 255, 18), hover), Lerp(kCardBorder, Rgba(40, 90, 120, 160), hover), 1.f);
        if (selected) selX = tab.x;
        const std::wstring digits = (ch.number < 10 ? L"0" : L"") + std::to_wstring(ch.number);
        const float dw = g.Measure(digits, Mono(13.f, true)).w + 1.f;
        g.Text(digits, { tab.x + 16.f, tab.y, dw, tab.h }, Lerp(Palette::Muted, Palette::Teal, std::max(sel, hover)), Mono(13.f, true));
        g.Text(ChapterSubtitle(ch), { tab.x + 16.f + dw + 10.f, tab.y, tab.w - dw - 42.f, tab.h }, Lerp(kSub, kText, std::max(sel, hover)),
               Mono(10.f).SemiBold().Ellipsis());
        if (it.hovered && !selected) ui.SetCursor(IDC_HAND);
        if (it.clicked && !selected) SelectChapter(ch.number);
    }
    // Capsule gliding to the selected tab (drawn as an outline over it)
    if (selX >= 0.f) {
        const float x = ui.Animate(ui.MakeId("chapter-capsule"), selX, 14.f, selX);
        const Rect cap{ x, r.y, w, r.h };
        g.GlowRound(cap, r.h * 0.5f, Palette::Teal, 14.f, 0.3f);
        g.Border(cap, r.h * 0.5f, Rgba(0, 204, 170, 26), Palette::Teal, 1.f);
    }
}

void LeaderboardWindow::DrawCategoryRow(const Rect& r) {
    auto it = runTypes.find(chapter);
    if (it == runTypes.end() || it->second.empty()) return;
    auto& types = it->second;
    int& current = category[chapter];
    current = std::clamp(current, 0, static_cast<int>(types.size()) - 1);
    float x = r.x;
    float selX = 0.f, selW = 0.f;
    for (size_t i = 0; i < types.size(); ++i) {
        const std::wstring& label = types[i].label;
        const float w = g.Measure(label, Mono(12.f, true)).w + 28.f;
        const Rect tab{ x, r.y, w, r.h };
        const Interaction hit = ui.Interact(ui.MakeId("category-tab", static_cast<int64_t>(i)), tab);
        const bool selected = static_cast<int>(i) == current;
        const float hover = ui.Animate(ui.MakeId("category-tab-hover", static_cast<int64_t>(i)), hit.hovered ? 1.f : 0.f, 16.f);
        const float sel = ui.Animate(ui.MakeId("category-tab-sel", static_cast<int64_t>(i)), selected ? 1.f : 0.f, 12.f);
        g.Text(label, tab.Offset(0.f, -2.f), Lerp(Lerp(kSub, kText, hover), Palette::Teal, sel), Mono(12.f, true).Center());
        if (selected) {
            selX = tab.x;
            selW = tab.w;
        }
        if (hit.hovered && !selected) ui.SetCursor(IDC_HAND);
        if (hit.clicked) SelectCategory(static_cast<int>(i));
        x += w + 6.f;
    }
    // Underline sliding (and stretching) to the selected tab
    g.FillRect({ r.x, r.B() - 1.f, r.w, 1.f }, Palette::Divider);
    const float ux = ui.Animate(ui.MakeId("category-underline-x"), selX, 14.f, selX);
    const float uw = ui.Animate(ui.MakeId("category-underline-w"), selW, 14.f, selW);
    g.FillRound({ ux + 10.f, r.B() - 3.f, uw - 20.f, 3.f }, 1.5f, Palette::Teal);
    g.GlowRound({ ux + 10.f, r.B() - 3.f, uw - 20.f, 3.f }, 1.5f, Palette::Teal, 8.f, 0.5f);

    DrawSelectors(types[static_cast<size_t>(current)], r.R(), r.CY() - 2.f);
}

// Subcategory selectors (Route, Restriction...) as pills right-aligned in the category row
void LeaderboardWindow::DrawSelectors(SC::RunType& runType, float right, float cy) {
    float x = right;
    for (size_t i = runType.secondaryVariables.size(); i-- > 0;) {
        auto& v = runType.secondaryVariables[i];
        const std::wstring label = Text::Upper(v.name);
        std::wstring selected = v.values.empty() ? L"" : v.values.front().label;
        for (const auto& value : v.values)
            if (value.id == v.selectedValueId) selected = value.label;
        const float lw = g.Measure(label, Mono(8.f, true)).w + 1.f, tw = g.Measure(selected, Mono(10.f).SemiBold()).w + 1.f;
        const float w = 12.f + lw + 8.f + tw + 9.f + 10.f + 12.f;
        x -= w;
        const Rect r{ x, cy - 14.f, w, 28.f };
        x -= 8.f;
        const bool isOpen = openSelector == v.id;
        const Interaction it = ui.Interact(ui.MakeId("selector", static_cast<int64_t>(i)), r);
        const float hover = ui.Animate(ui.MakeId("selector-hover", static_cast<int64_t>(i)), it.hovered || isOpen ? 1.f : 0.f, 18.f);
        g.Border(r, 14.f, Lerp(Rgba(10, 24, 37), Rgba(14, 34, 52), hover), Lerp(Rgba(26, 58, 85), Palette::Teal, hover * 0.8f), 1.f);
        g.Text(label, { r.x + 12.f, r.y, lw, r.h }, kDim, Mono(8.f, true));
        g.Text(selected, { r.x + 12.f + lw + 8.f, r.y, tw, r.h }, Lerp(Palette::Label, kText, hover), Mono(10.f).SemiBold());
        const float turn = ui.Animate(ui.MakeId("selector-turn", static_cast<int64_t>(i)), isOpen ? 1.f : 0.f, 16.f);
        const float chx = r.x + 12.f + lw + 8.f + tw + 9.f, chy = r.CY();
        g.PushTransform(D2D1::Matrix3x2F::Rotation(180.f * turn, D2D1::Point2F(chx, chy)));
        g.Text(L"\xE70D", { chx - 5.f, chy - 6.f, 10.f, 12.f }, Lerp(Rgba(42, 90, 122), Palette::Teal, hover), IconFont(7.f));
        g.PopTransform();
        if (it.hovered) ui.SetCursor(IDC_HAND);
        if (it.clicked) openSelector = isOpen ? L"" : v.id;
        if (!isOpen) continue;
        SC::RunType* type = &runType;
        SC::Variable* variable = &v;
        ui.Defer([this, r, type, variable] {
            auto& gr = ui.g;
            const float itemH = 30.f;
            float w = std::max(r.w, 150.f);
            for (const auto& value : variable->values) w = std::max(w, gr.Measure(value.label, Mono(11.f)).w + 40.f);
            const Rect list{ r.R() - w, r.B() + 6.f, w, itemH * variable->values.size() + 8.f };
            // Drops open from the top
            float& since = ui.Slot(ui.MakeId("selector-opened"), -1.f);
            const float t = static_cast<float>(Async::Now());
            if (since < 0.f) since = t;
            const float p = Anim::EaseOutCubic(Anim::Clamp01((t - since) / 0.18f));
            if (p < 1.f) ui.KeepAnimating();
            ui.Block(list);
            gr.PushOpacity(p);
            gr.PushRoundClip({ list.x - 20.f, list.y - 2.f, list.w + 40.f, (list.h + 22.f) * p }, 10.f);
            gr.Shadow(list.Offset(0.f, 6.f), 8.f, 16.f, Argb(0x90000000));
            gr.Border(list, 8.f, Rgba(9, 20, 30), Rgba(21, 48, 72), 1.f);
            float iy = list.y + 4.f;
            int n = 0;
            for (const auto& value : variable->values) {
                const Rect item{ list.x + 4.f, iy, list.w - 8.f, itemH };
                const Interaction hit = ui.Interact(ui.MakeId("selector-option", n), item);
                const float h2 = ui.Animate(ui.MakeId("selector-option-hover", n), hit.hovered ? 1.f : 0.f, 20.f);
                const bool chosen = value.id == variable->selectedValueId;
                gr.FillRound(item, 6.f, Lerp(chosen ? Rgba(0, 60, 50, 160) : Rgba(0, 0, 0, 0), Rgba(255, 255, 255, 22), h2));
                if (chosen) gr.Text(std::wstring(1, Glyphs::CheckMark), { item.x + 8.f, item.y, 14.f, item.h }, Palette::Teal, IconFont(9.f));
                gr.Text(value.label, { item.x + 28.f + 3.f * h2, item.y, item.w - 32.f, item.h }, chosen ? Palette::Teal : Palette::Label, Mono(11.f));
                if (hit.hovered) ui.SetCursor(IDC_HAND);
                if (hit.clicked) {
                    variable->selectedValueId = value.id;
                    openSelector.clear();
                    selectedRunner = 0;
                    categoryChangedAt = Async::Now();
                    RequestRunners(*type);
                }
                iy += itemH;
                ++n;
            }
            gr.PopLayer();
            gr.PopLayer();
            if (ui.In().pressed[0] && !ui.MouseOver(list) && !ui.MouseOver(r)) openSelector.clear();
            if (openSelector.empty()) since = -1.f;
        });
    }
}

// ── Podium: 2nd · 1st · 3rd rising from the bottom ──────────────────────────

void LeaderboardWindow::DrawPodium(const Rect& r, const std::vector<Entry>* list, double loadedAt) {
    const double now = Async::Now();
    g.Border(r, 10.f, Rgba(9, 20, 30, 150), Rgba(17, 34, 51, 140), 1.f);
    // Spotlight behind the winner
    g.FillRadial({ r.CX() - 160.f, r.y - 40.f, 320.f, r.h }, { { 0.f, kGold.WithAlpha(0.06f) }, { 1.f, kGold.WithAlpha(0.f) } });
    const int order[3] = { 2, 1, 3 };
    const float heights[3] = { 72.f, 104.f, 52.f };
    const float colW = (r.w - 40.f) / 3.f;
    for (int c = 0; c < 3; ++c) {
        const int place = order[c];
        const size_t idx = static_cast<size_t>(place - 1);
        const bool has = list && idx < list->size();
        const Rect col{ r.x + 20.f + c * colW, r.y + 14.f, colW, r.h - 14.f };
        // Pedestals rise one after another after a load: 3rd, 2nd, then the winner
        const float delay = place == 1 ? 0.24f : place == 2 ? 0.12f : 0.f;
        const float rise = list ? Anim::EaseOutBack(Anim::Clamp01(static_cast<float>((now - loadedAt - delay) / 0.5)), 0.6f) : 0.f;
        if (list && rise < 1.f) ui.KeepAnimating();
        const Color medal = Medal(place);
        const float ph = heights[c] * rise;
        const Rect pedestal{ col.x + 8.f, col.B() - ph, col.w - 16.f, ph };
        const Interaction it = has ? ui.Interact(ui.MakeId("podium", place), { col.x, col.y, col.w, col.h }) : Interaction{};
        const float hover = ui.Animate(ui.MakeId("podium-hover", place), it.hovered ? 1.f : 0.f, 16.f);
        const bool chosen = has && selectedRunner == static_cast<int>(idx);
        {
            NoColorFilter keep(g);
            if (ph > 1.f) {
                g.FillRound(pedestal, 8.f, 8.f, 0.f, 0.f, medal.WithAlpha(0.14f + 0.08f * hover));
                g.FillRect({ pedestal.x, pedestal.y, pedestal.w, 2.f }, medal.WithAlpha(0.8f));
                g.Text(std::to_wstring(place), pedestal.Offset(0.f, 4.f), medal.WithAlpha(0.55f), Mono(place == 1 ? 34.f : 26.f, true).Center());
            }
        }
        if (!has) {
            if (!list) Shimmer(g, pedestal.h > 1.f ? pedestal : Rect{ col.x + 8.f, col.B() - heights[c], col.w - 16.f, heights[c] }, 8.f, now);
            continue;
        }
        const Entry& e = (*list)[idx];
        // Avatar (speedrun.com picture, initials meanwhile / without one), name with flag and time above
        // the pedestal; they drop in after it
        const float appear = Anim::Clamp01(static_cast<float>((now - loadedAt - delay - 0.25) / 0.3));
        if (appear < 1.f) ui.KeepAnimating();
        const bool appearing = appear < 1.f;
        if (appearing) g.PushOpacity(appear);
        const float lift = -3.f * hover + (1.f - Anim::EaseOutCubic(appear)) * -10.f;
        const float ar = place == 1 ? 30.f : 25.f;
        const float ax = col.CX(), ay = pedestal.y - 64.f - ar + lift;
        {
            NoColorFilter keep(g);
            if (place == 1) {
                // Crown bobbing over the record holder
                const float bob = std::sin(static_cast<float>(now) * 2.4f) * 2.f * std::clamp(4.f - static_cast<float>(now - loadedAt), 0.f, 1.f);
                g.Text(L"\U0001F451", { ax - 16.f, ay - ar - 28.f + bob, 32.f, 26.f }, Colors::White, Mono(16.f).WithFont(Font::Ui).Center());
            }
            g.GlowRound({ ax - ar, ay - ar, ar * 2.f, ar * 2.f }, ar, medal, 14.f, 0.35f + 0.3f * std::max(hover, chosen ? 1.f : 0.f));
            const Color fill = e.nameColor ? Hex(e.nameColor) : Palette::Teal;
            const Rect avatar{ ax - ar, ay - ar, ar * 2.f, ar * 2.f };
            const ImagePtr picture = e.avatarUrl.empty() ? nullptr : RemoteImages::Get(e.avatarUrl, 128);
            g.FillEllipse(ax, ay, ar, ar, Lerp(Rgba(12, 22, 32), fill, 0.35f));
            if (picture) {
                g.DrawImageRounded(*picture, avatar, ar, 1.f, Stretch::UniformToFill);
            } else {
                g.Text(Initials(e.playerName), avatar, Colors::White, Mono(place == 1 ? 17.f : 14.f, true).Center());
                if (!e.avatarUrl.empty() && now - openedAt < 20.0) ui.KeepAnimatingThrottled();   // until it arrives (not forever)
            }
            g.StrokeEllipse(ax, ay, ar, ar, medal, chosen ? 3.f : 2.f);
        }
        DrawRunnerName(e, col.x + 4.f, ay + ar + 8.f, col.w - 8.f, 20.f, Mono(11.f, true), hover, true);
        g.Text(SC::FormatTime(e.timeSeconds), { col.x, ay + ar + 30.f, col.w, 18.f }, place == 1 ? Palette::Teal : kText, Mono(12.f).SemiBold().Center());
        if (appearing) g.PopLayer();
        if (it.hovered) ui.SetCursor(IDC_HAND);
        if (it.clicked && selectedRunner != static_cast<int>(idx)) {
            selectedRunner = static_cast<int>(idx);
            runnerChangedAt = now;
        }
    }
}

// ── Details of the selected runner ──────────────────────────────────────────

void LeaderboardWindow::DrawDetails(const Rect& r, const std::vector<Entry>* list) {
    const double now = Async::Now();
    g.Border(r, 10.f, Rgba(9, 20, 30, 150), Rgba(17, 34, 51, 140), 1.f);
    if (!list) {
        Shimmer(g, r, 10.f, now);
        return;
    }
    if (list->empty()) {
        g.Text(std::wstring(1, Glyphs::Info), r.Offset(0.f, -14.f), Palette::Muted, IconFont(22.f));
        g.Text(Loc::Get("lb_no_runs"), r.Offset(0.f, 22.f), kSub, Mono(11.f, true).Center());
        return;
    }
    selectedRunner = std::clamp(selectedRunner, 0, static_cast<int>(list->size()) - 1);
    const Entry& e = (*list)[static_cast<size_t>(selectedRunner)];
    const double record = list->front().timeSeconds;

    // The content slides in when another runner is picked
    const float t = Anim::Clamp01(static_cast<float>((now - runnerChangedAt) / 0.3));
    const bool sliding = t < 1.f;
    if (sliding) {
        ui.KeepAnimating();
        g.PushOpacity(Anim::EaseOutCubic(t));
        ui.PushTranslate((1.f - Anim::EaseOutCubic(t)) * 14.f, 0.f);
    }
    const Rect in = r.Inset(18.f, 16.f);
    const Color medal = Medal(e.place);
    {
        NoColorFilter keep(g);
        g.FillEllipse(in.x + 14.f, in.y + 14.f, 14.f, 14.f, e.place <= 3 ? medal : Rgba(255, 255, 255, 14));
        if (e.place > 3) g.StrokeEllipse(in.x + 14.f, in.y + 14.f, 14.f, 14.f, kDim, 1.f);
        g.Text(std::to_wstring(e.place), { in.x, in.y, 28.f, 28.f }, e.place <= 3 ? Rgba(20, 20, 24) : kSub, Mono(11.f, true).Center());
    }
    DrawRunnerName(e, in.x + 40.f, in.y, in.w - 200.f, 28.f, Mono(14.f, true));
    const std::wstring time = SC::FormatTime(e.timeSeconds);
    g.Text(time, { in.R() - 160.f, in.y - 2.f, 160.f, 24.f }, Palette::Teal, Mono(17.f, true).Right());
    g.Text(e.place == 1 ? Loc::Get("lb_world_record") : Loc::Fmt("lb_behind_record", SC::FormatTime(e.timeSeconds - record)),
           { in.R() - 200.f, in.y + 22.f, 200.f, 14.f }, e.place == 1 ? kGold : kSub, Mono(8.f, true).Right());

    // Facts: date / country
    float y = in.y + 46.f;
    const struct {
        std::wstring label;
        std::wstring value;
    } facts[] = { { Loc::Get("lb_date"), e.date.empty() ? L"—" : e.date + L"  (" + Ago(e.daysAgo) + L")" },
                  { Loc::Get("lb_country"), e.countryName.empty() ? L"—" : e.countryName } };
    float fx = in.x;
    for (const auto& f : facts) {
        const float fw = (in.w - 10.f) * 0.5f;
        g.Border({ fx, y, fw, 42.f }, 8.f, Rgba(255, 255, 255, 6), kCardBorder, 1.f);
        g.Text(f.label, { fx + 12.f, y + 6.f, fw - 24.f, 12.f }, kFaint, Mono(8.f, true));
        g.Text(f.value, { fx + 12.f, y + 19.f, fw - 24.f, 18.f }, kText, Mono(10.f, true).Ellipsis());
        fx += fw + 10.f;
    }
    y += 42.f + 12.f;

    // Comment (as much as fits), leaving room for its pictures under it
    const float buttonsY = in.B() - 34.f;
    const bool pictures = !e.commentImages.empty();
    constexpr float kPictureMinH = 60.f, kPictureMaxH = 130.f;
    if (!e.comment.empty() && buttonsY - y > 30.f) {
        const float room = buttonsY - 12.f - y - (pictures ? kPictureMinH + 10.f : 0.f);
        const float h = std::min(g.Measure(e.comment, Mono(10.f).Wrap().Top(), in.w - 14.f).h, std::max(16.f, room));
        g.FillRect({ in.x, y, 2.f, h }, Palette::Teal.WithAlpha(0.5f));
        g.PushClip({ in.x, y, in.w, h });
        g.Text(e.comment, { in.x + 12.f, y, in.w - 14.f, h + 40.f }, Rgba(160, 185, 205), Mono(10.f).Wrap().Top());
        g.PopClip();
        y += h + 10.f;
    } else if (e.comment.empty() && !pictures) {
        g.Text(Loc::Get("lb_no_comment"), { in.x, y, in.w, 16.f }, kFaint, Mono(9.f).Italic());
    }
    // Pictures in a row (shimmer while downloading); a click opens the full picture in the browser
    const float pictureH = std::min(kPictureMaxH, buttonsY - 12.f - y);
    if (pictures && pictureH >= 40.f) {
        NoColorFilter keep(g);   // real colours
        float px = in.x;
        for (size_t i = 0; i < e.commentImages.size(); ++i) {
            const std::wstring& url = e.commentImages[i];
            const ImagePtr img = RemoteImages::Get(url);
            const float pw = std::min(img && img->h > 0.f ? pictureH * img->w / img->h : pictureH * 16.f / 9.f, in.R() - px);
            if (pw < 40.f) break;
            const Rect pic{ px, y, pw, pictureH };
            const Interaction it = ui.Interact(ui.MakeId("comment-picture", static_cast<int64_t>(i)), pic);
            if (it.hovered) ui.SetCursor(IDC_HAND);
            if (it.clicked) Process::OpenUrl(url);
            if (img) {
                g.DrawImageRounded(*img, pic, 6.f, 1.f, Stretch::UniformToFill);
            } else {
                g.FillRound(pic, 6.f, Rgba(255, 255, 255, 6));
                Shimmer(g, pic, 6.f, now);
                if (Async::Now() - openedAt < 20.0) ui.KeepAnimatingThrottled();   // a picture that failed isn't waited for forever
            }
            g.StrokeRound(pic, 6.f, it.hovered ? Palette::Teal.WithAlpha(0.7f) : kCardBorder, 1.f);
            px += pw + 8.f;
        }
    }

    // Links: video (plays here), run, profile
    const float bw = (in.w - 16.f) / 3.f;
    const bool youtube = !e.youtubeId.empty();
    {
        NoColorFilter keep(g);
        const std::wstring videoText = e.videoUrl.empty() ? Loc::Get("lb_no_video_btn") : youtube ? Loc::Get("lb_watch_video_btn") : Loc::Fmt("lb_open_in_btn", Text::Upper(VideoSite(e)));
        if (ActionButton(ui, ui.MakeId("detail-video"), { in.x, buttonsY, bw, 34.f }, 0xE768, videoText, kVideo, true, !e.videoUrl.empty())) PlayVideo(e);
    }
    if (ActionButton(ui, ui.MakeId("detail-run"), { in.x + bw + 8.f, buttonsY, bw, 34.f }, 0xE8A7, L"RUN", Palette::Teal, false, !e.runWeblink.empty()))
        Process::OpenUrl(e.runWeblink);
    if (ActionButton(ui, ui.MakeId("detail-profile"), { in.x + 2.f * (bw + 8.f), buttonsY, bw, 34.f }, Glyphs::People, Loc::Get("lb_profile"), Palette::Hint, false,
                     !e.playerWeblink.empty()))
        Process::OpenUrl(e.playerWeblink);
    if (sliding) {
        ui.PopTransform();
        g.PopLayer();
    }
}

// ── Top 10 list ─────────────────────────────────────────────────────────────

void LeaderboardWindow::DrawList(const Rect& r, const std::vector<Entry>* list, double loadedAt) {
    const double now = Async::Now();
    g.Text(L"TOP 10", { r.x + 2.f, r.y, 100.f, 16.f }, kSub, Mono(9.f, true));
    const Rect view{ r.x, r.y + 24.f, r.w, r.h - 24.f };
    if (!list) {
        for (int i = 0; i < 7; ++i) {
            const Rect row{ view.x, view.y + i * (kListRowH + kListGap), view.w, kListRowH };
            g.FillRound(row, 8.f, Rgba(255, 255, 255, 6));
            g.FillEllipse(row.x + 24.f, row.CY(), 12.f, 12.f, Rgba(255, 255, 255, 12));
            g.FillRound({ row.x + 48.f, row.CY() - 5.f, row.w * 0.3f, 10.f }, 5.f, Rgba(255, 255, 255, 14));
            Shimmer(g, row, 8.f, now + i * 0.08);
        }
        ui.KeepAnimatingThrottled();
        return;
    }
    if (list->empty()) return;
    const double record = list->front().timeSeconds;
    ScrollArea scroll(ui, ui.MakeId("top-list", chapter), view, false, 56.f, ScrollArea::Bar::Thin);
    const float w = scroll.ContentWidth() - 4.f;
    for (size_t i = 0; i < list->size(); ++i) {
        const Entry& e = (*list)[i];
        StaggerItem in(ui, loadedAt, static_cast<int>(i), 22.f, 0.f, 0.035, 0.32);
        const Rect row{ view.x, view.y + i * (kListRowH + kListGap), w, kListRowH };
        const Id rowId = ui.MakeId("top-row", static_cast<int64_t>(i));
        const Interaction it = ui.Interact(rowId, row);
        const bool chosen = selectedRunner == static_cast<int>(i);
        const float hover = ui.Animate(HashId(1, rowId), it.hovered ? 1.f : 0.f, 16.f);
        const float sel = ui.Animate(HashId(2, rowId), chosen ? 1.f : 0.f, 14.f);
        const Color medal = Medal(e.place);
        const bool podium = e.place <= 3;
        {
            NoColorFilter keep(g);
            Color bg = podium ? medal.WithAlpha(0.05f) : Rgba(255, 255, 255, 5);
            bg = Lerp(Lerp(bg, Rgba(255, 255, 255, 16), hover), Rgba(0, 204, 170, 30), sel);
            g.Border(row, 8.f, bg, Lerp(Lerp(podium ? medal.WithAlpha(0.22f) : Rgba(17, 34, 51, 100), Rgba(40, 90, 120, 160), hover), Palette::Teal, sel), 1.f);
            if (sel > 0.01f) {
                const float bh = (row.h - 16.f) * Anim::EaseOutCubic(sel);
                g.FillRound({ row.x + 3.f, row.CY() - bh * 0.5f, 3.f, bh }, 1.5f, Palette::Teal);
            }
            // Place
            const float mx = row.x + 26.f, my = row.CY();
            if (podium) {
                g.FillEllipse(mx, my, 12.f, 12.f, medal);
                g.Text(std::to_wstring(e.place), { mx - 12.f, my - 12.f, 24.f, 24.f }, Rgba(20, 20, 24), Mono(10.f, true).Center());
            } else {
                g.StrokeEllipse(mx, my, 11.f, 11.f, kDim.WithAlpha(0.7f), 1.f);
                g.Text(std::to_wstring(e.place), { mx - 12.f, my - 12.f, 24.f, 24.f }, kSub, Mono(9.f, true).Center());
            }
        }
        // Right: ▶ then time / gap
        const Rect videoR{ row.R() - 10.f - 28.f, row.CY() - 14.f, 28.f, 28.f };
        bool overButton = false;
        {
            NoColorFilter keep(g);
            const bool hasVideo = !e.videoUrl.empty();
            const std::wstring tip = !hasVideo ? Loc::Get("lb_no_video_tip") : !e.youtubeId.empty() ? Loc::Get("lb_watch_video") : Loc::Fmt("lb_open_in", VideoSite(e));
            if (IconButton(ui, ui.MakeId("top-video", static_cast<int64_t>(i)), videoR, 0xE768, kVideo, tip, hasVideo)) PlayVideo(e);
            overButton = ui.MouseOver(videoR);
        }
        const std::wstring time = SC::FormatTime(e.timeSeconds);
        const float timeW = g.Measure(time, Mono(12.f).SemiBold()).w + 1.f;
        const float tr = videoR.x - 12.f;
        g.Text(time, { tr - timeW, row.y + (e.place > 1 ? 8.f : 0.f), timeW, e.place > 1 ? 18.f : row.h }, e.place == 1 ? Palette::Teal : kText,
               Mono(12.f).SemiBold());
        if (e.place > 1) {
            const std::wstring gap = L"+" + SC::FormatTime(e.timeSeconds - record);
            const float gw = g.Measure(gap, Mono(8.f)).w + 1.f;
            g.Text(gap, { tr - gw, row.y + 27.f, gw, 14.f }, kFaint, Mono(8.f));
        }
        // Flag + name, and how long ago under it
        const float nx = row.x + 48.f + 3.f * hover;
        const float nameRight = tr - timeW - 14.f;
        DrawRunnerName(e, nx, row.y + 7.f, nameRight - nx, 20.f, Mono(11.f, true), hover);
        const std::wstring ago = Ago(e.daysAgo);
        if (!ago.empty()) g.Text(ago, { nx, row.y + 27.f, nameRight - nx, 14.f }, kFaint, Mono(8.f).Ellipsis());

        if (it.hovered && !overButton) ui.SetCursor(IDC_HAND);
        if (it.clicked && !overButton && !chosen) {
            selectedRunner = static_cast<int>(i);
            runnerChangedAt = now;
        }
    }
    scroll.SetContentSize(list->size() * (kListRowH + kListGap));
}

// ── Frame ───────────────────────────────────────────────────────────────────

void LeaderboardWindow::OnFrame() {
    const Rect screen{ 0.f, 0.f, g.Width(), g.Height() };
    const double now = Async::Now();

    float closeT = 0.f;
    if (closingAt >= 0.0) {
        closeT = Anim::Clamp01(static_cast<float>((now - closingAt) / kPopOut));
        if (closeT >= 1.f) {
            finished = true;
            Close();
            return;
        }
        ui.KeepAnimating();
    }
    const bool interactive = closingAt < 0.0;
    if (interactive && ui.KeyPressed(VK_ESCAPE)) {
        if (video.open) CloseVideo();
        else if (!openSelector.empty()) openSelector.clear();
        else BeginClose();
    }
    if (video.open && video.closingAt < 0.0 && ui.Focus() == 0 && ui.KeyPressed(VK_SPACE)) player.TogglePlay();

    // Pop-in / pop-out
    const float sinceOpen = static_cast<float>(now - openedAt);
    const float openT = Anim::Clamp01(sinceOpen / static_cast<float>(kPopIn));
    const float fade = Anim::EaseOutCubic(std::min(1.f, openT * 1.4f)) * (1.f - Anim::EaseInCubic(closeT));
    const float scale = Anim::Lerp(0.94f, 1.f, Anim::EaseOutBack(openT, 0.7f)) * Anim::Lerp(1.f, 0.96f, Anim::EaseInCubic(closeT));
    if (openT < 1.f) ui.KeepAnimating();
    const bool layered = fade < 0.999f;
    if (layered) g.PushOpacity(fade);

    g.FillRect(screen, Palette::Backdrop);
    const Interaction backdrop = ui.Interact(ui.MakeId("backdrop"), screen, true, false);
    const Rect panel = screen.Center(std::min(1180.f, screen.w - 40.f), std::min(800.f, screen.h - 40.f));
    PanelBackdropGlow(ui, panel, Palette::Teal, sinceOpen);
    ui.PushScale(scale, panel.CX(), panel.CY());
    ui.Block(panel);
    PanelGlow(ui, panel, kRadius, Palette::Teal, sinceOpen);
    g.Border(panel, kRadius, kPanelBg, kPanelBorder, 1.f);
    g.FillLinear(panel.Inset(1.f).Top(220.f), kRadius - 1.f, { { 0.f, Palette::Teal.WithAlpha(0.06f) }, { 1.f, Palette::Teal.WithAlpha(0.f) } },
                 { 0.f, 0.f }, { 0.f, 1.f });

    // ── Header: 🏆 LEADERBOARDS / speedrun.com · PC ................... [✕] ──
    const Rect header{ panel.x + 1.f, panel.y + 1.f, panel.w - 2.f, kHeaderH };
    g.FillRound(header, kRadius - 1.f, kRadius - 1.f, 0.f, 0.f, Hex(0x05090F, 0.85f));
    g.FillRect({ header.x, header.B() - 1.f, header.w, 1.f }, Palette::Divider);
    {
        StaggerItem in(ui, openedAt + 0.06, 0, -14.f, 0.f, 0.0, 0.36);
        const Rect badge{ header.x + 18.f, header.CY() - 19.f, 38.f, 38.f };
        const float shine = Anim::Clamp01(1.f - std::fabs(static_cast<float>(std::fmod(now, 4.0)) - 0.6f) / 0.6f);
        {
            NoColorFilter keep(g);
            g.GlowRound(badge, 10.f, kGold, 16.f, 0.15f + 0.35f * shine);
            g.Border(badge, 10.f, kGold.WithAlpha(0.12f), kGold.WithAlpha(0.5f + 0.3f * shine), 1.f);
        }
        g.Text(L"\U0001F3C6", badge, Colors::White, Mono(18.f).WithFont(Font::Ui).Center());
        g.Text(Loc::Get("lb_title"), { badge.R() + 14.f, header.y + 14.f, 400.f, 20.f }, Palette::Teal, Mono(14.f, true));
        g.Text(L"speedrun.com  ·  PC  ·  " + Loc::Get("lb_subtitle"), { badge.R() + 14.f, header.y + 35.f, 400.f, 16.f }, kSub, Mono(9.f, true));
    }
    const ButtonStyle smallButton{ Palette::ButtonBg, Palette::ButtonBorder, 1.f, 16.f, { 0.f }, 0.f, 32.f };
    if (TextButton(ui, ui.MakeId("close"), { header.R() - 16.f - 32.f, header.CY() - 16.f, 32.f, 32.f }, L"\x2715",
                   Mono(12.f, true).WithFont(Font::Ui).Center(), Palette::Dim, smallButton) &&
        interactive)
        BeginClose();

    // ── Tabs ────────────────────────────────────────────────────────────────
    const Rect content{ panel.x + 20.f, header.B() + 16.f, panel.w - 40.f, panel.B() - header.B() - 36.f };
    DrawChapterTabs({ content.x, content.y, content.w, 38.f });
    const Rect categoryRow{ content.x, content.y + 38.f + 12.f, content.w, 40.f };
    const Rect body{ content.x, categoryRow.B() + 16.f, content.w, content.B() - categoryRow.B() - 16.f };

    auto types = runTypes.find(chapter);
    if (types == runTypes.end()) {
        g.Text(Loc::Get("lb_loading_categories"), categoryRow, kDim, Mono(11.f, true));
        ui.KeepAnimatingThrottled();
    } else if (types->second.empty()) {
        g.Text(std::wstring(1, Glyphs::Warning), body.Offset(0.f, -20.f), Palette::DangerText, IconFont(26.f));
        g.Text(Loc::Get("lb_load_failed"), body.Offset(0.f, 24.f), Palette::DangerText, Mono(11.f, true).Center());
    } else {
        DrawCategoryRow(categoryRow);
    }

    // ── Podium + details (left) · top 10 (right) ────────────────────────────
    const std::vector<Entry>* list = nullptr;
    double loadedAt = now;
    if (types != runTypes.end() && !types->second.empty()) {
        const SC::RunType& runType = types->second[static_cast<size_t>(std::clamp(category[chapter], 0, static_cast<int>(types->second.size()) - 1))];
        RequestRunners(runType);
        const std::wstring key = RunnersKey(runType);
        if (auto found = runners.find(key); found != runners.end()) {
            list = &found->second;
            loadedAt = std::max({ runnersLoadedAt[key], categoryChangedAt, openedAt + 0.15 });
        }
    }
    if (types == runTypes.end() || !types->second.empty()) {
        // The whole body fades / rises in again after a chapter or category change
        const float change = Anim::Clamp01(static_cast<float>((now - categoryChangedAt) / 0.3));
        const bool changing = change < 1.f;
        if (changing) {
            ui.KeepAnimating();
            g.PushOpacity(Anim::EaseOutCubic(change));
            ui.PushTranslate(0.f, (1.f - Anim::EaseOutCubic(change)) * 10.f);
        }
        const float leftW = std::min(470.f, body.w * 0.46f);
        const Rect podium{ body.x, body.y, leftW, 286.f };
        DrawPodium(podium, list, loadedAt);
        DrawDetails({ body.x, podium.B() + 14.f, leftW, body.B() - podium.B() - 14.f }, list);
        DrawList({ body.x + leftW + 20.f, body.y, body.w - leftW - 20.f, body.h }, list, loadedAt);
        if (changing) {
            ui.PopTransform();
            g.PopLayer();
        }
    }
    if (!loadingRunners.empty() || !loadingChapters.empty()) ui.KeepAnimatingThrottled();

    // Seasonal theme: particles over the content, decorations resting on the top edge
    Seasonal::CardArt::DrawPanelParticles(g, panel, kRadius, now, 0.45f);
    PanelDecor(ui, panel, kRadius, Palette::Teal, sinceOpen, header.B());
    Seasonal::CardArt::DrawPanelEdge(g, panel, now);
    if (PanelEntranceRunning(sinceOpen)) ui.KeepAnimating();

    ui.PopTransform();
    if (layered) g.PopLayer();
    if (interactive && !video.open && backdrop.held && ui.In().pressed[0] && !ui.MouseOver(panel)) BeginClose();

    // The video sits over everything (its player is a child window on top of it)
    if (video.open) DrawVideo(screen);
}

} // namespace SpeedrunLauncher::Windows
