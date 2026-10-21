#include "Windows/Main/Overlays/Updates/UpdatesOverlay.h"
#include <algorithm>
#include <cmath>
#include "Core/Async.h"
#include "Graphics/Animation.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/App/AppVersion.h"
#include "Services/App/VersionCompare.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Main/Common/ChangelogLook.h"
#include "Windows/Main/Common/MentionText.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Standalone/StandaloneWindows.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
namespace U = Services::Updates;

namespace {

// What's new cards
constexpr float kNewsPadX = 16.f, kNewsHeaderH = 46.f, kNewsGap = 10.f;
constexpr float kNewsTagH = 22.f, kNewsIndent = 18.f, kNewsLineGap = 6.f, kNewsSectionGap = 14.f;
constexpr float kNewsText = 12.f;

} // namespace

void UpdatesOverlay::Open(Page startPage) {
    installView = false;
    popup.Open();
    page = startPage == Page::Updates ? Page::Changelog : Page::Updates;   // force the page's entrance
    SelectPage(startPage, Async::Now() + 0.12);   // right behind the panel's own entrance
}

void UpdatesOverlay::SelectPage(Page next, double now) {
    if (next == page) return;
    page = next;
    pageAt = now;
    if (next == Page::Changelog) changelog.Reset(now + 0.1);
}

void UpdatesOverlay::ShowInstall(bool gameBanana) {
    installView = true;
    installGb = gameBanana;
    newsSource.clear();   // rebuilt (and folded back to "newest open") on the next frame
}

void UpdatesOverlay::Check() {
    if (checking) return;
    checking = true;
    info.reset();
    gb.reset();
    Async::Run([] { return std::make_pair(U::CheckForUpdates(), U::CheckGameBanana()); },
               [this](std::pair<U::UpdateInfo, U::GbUpdateInfo> result) {
                   checking = false;
                   info = result.first;
                   gb = result.second;
                   if (info->isUpdateAvailable || gb->isUpdateAvailable) main.SetUpdateAlert(true);
               });
}

void UpdatesOverlay::Install() {
    if (downloading) return;
    downloading = true;
    percent = 0;
    const bool useGb = installGb && gb;
    const auto release = info;
    const auto banana = gb;
    Async::Run([this, useGb, release, banana] {
        const auto progress = [this](int p) { Async::Post([this, p] { percent = p; }); };
        if (useGb) return U::DownloadAndInstallGameBanana(*banana, progress);
        if (release) return U::DownloadAndInstall(*release, progress);
        return false;
    }, [this](bool ok) {
        // On success the updater batch restarts the launcher; we only get here on failure
        if (!ok) {
            downloading = false;
            installView = false;
        }
    });
}

namespace {

constexpr float kBannerH = 96.f, kGap = 12.f, kSideW = 300.f, kCardRadius = 10.f;
constexpr Color kBlue = Rgba(60, 150, 230), kOrange = Hex(0xF4821A);

enum class Status { Unknown, Checking, UpToDate, Available, GameBanana, Error };

// Rounded card with a small caption on top ("YOUR VERSION"...); returns the content area
Rect Card(Ui& ui, const Rect& r, const std::wstring& caption, wchar_t glyph) {
    auto& g = ui.g;
    g.Border(r, kCardRadius, Palette::HeaderBg, Palette::ButtonBorder, 1.f);
    g.FillLinear(r.Inset(1.f).Top(40.f), kCardRadius - 1.f, { { 0.f, Palette::Teal.WithAlpha(0.06f) }, { 1.f, Palette::Teal.WithAlpha(0.f) } }, { 0.f, 0.f },
                 { 0.f, 1.f });
    Glyph(ui, glyph, { r.x + 14.f, r.y + 10.f, 16.f, 18.f }, 11.f, Palette::Teal);
    g.Text(caption, { r.x + 36.f, r.y + 10.f, r.w - 50.f, 18.f }, Palette::Teal, Mono(9.5f, true));
    return { r.x + 14.f, r.y + 32.f, r.w - 28.f, r.h - 42.f };
}

} // namespace

// The two options side by side: a card per page (glyph, name, what's in it); the picked one
// glows and a bar slides under it
void UpdatesOverlay::DrawPages(Ui& ui, const Rect& row) {
    auto& g = ui.g;
    struct Option {
        Page page;
        wchar_t glyph;
        const char* title;
        const char* subtitle;
    };
    const Option options[] = { { Page::Updates, Glyphs::Sync, "updates_tab", "updates_tab_sub" },
                               { Page::Changelog, Glyphs::Changelog, "changelog_tab", "changelog_tab_sub" } };
    const float slide = ui.Animate(ui.MakeId("upd-page-bar"), page == Page::Updates ? 0.f : 1.f, 14.f);
    for (int i = 0; i < 2; ++i) {
        const Option& o = options[i];
        const Rect r = GridCell(row, 2, i, 12.f);
        const Id id = ui.MakeId("upd-page", i);
        const Interaction it = ui.Interact(id, r);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        const bool selected = page == o.page;
        const float hover = ui.Animate(HashId(id, 1), it.hovered && !selected ? 1.f : 0.f, 16.f, 0.f);
        const float sel = ui.Animate(HashId(id, 2), selected ? 1.f : 0.f, 14.f);
        // The update alert shows on the UPDATES option too
        const bool alert = o.page == Page::Updates && main.UpdateAlert();
        const Color accent = alert ? Rgba(220, 60, 40) : Palette::Teal;
        if (sel > 0.01f) g.GlowRound(r, 10.f, accent, 16.f, 0.3f * sel);
        g.Border(r, 10.f, Lerp(Lerp(Palette::OptionBg, Hex(0x0A1A28), hover), accent.WithAlpha(24.f / 255.f), sel),
                 Lerp(Lerp(Palette::ButtonBorder, Palette::ControlBorder, hover), accent.WithAlpha(0.85f), sel), 1.f);
        const Rect badge{ r.x + 12.f, r.CY() - 18.f, 36.f, 36.f };
        g.Border(badge, 9.f, accent.WithAlpha((18.f + 30.f * sel) / 255.f), accent.WithAlpha((60.f + 120.f * sel) / 255.f), 1.f);
        const float spin = o.page == Page::Updates && (checking || alert) ? static_cast<float>(std::fmod(ui.Time() * (checking ? 240.0 : 60.0), 360.0)) : 0.f;
        g.PushTransform(D2D1::Matrix3x2F::Rotation(spin, D2D1::Point2F(badge.CX(), badge.CY())));
        Glyph(ui, o.glyph, badge, 15.f, Lerp(Palette::Dim, accent, std::max(sel, 0.5f * hover)));
        g.PopTransform();
        if (spin != 0.f) ui.KeepAnimatingThrottled();
        const float tx = badge.R() + 12.f + 3.f * hover;
        g.Text(Loc::Get(o.title), { tx, r.CY() - 18.f, r.R() - tx - 12.f, 18.f }, Lerp(Lerp(Palette::Dim, Palette::Label, hover), accent, sel),
               Mono(12.f, true).Ellipsis());
        g.Text(Loc::Get(o.subtitle), { tx, r.CY() + 2.f, r.R() - tx - 12.f, 16.f }, MenuKit::kDim, Mono(9.5f).Ellipsis());
        if (alert) {
            const float pulse = 1.f - 0.45f * Anim::EaseInOutSine(Anim::PingPong(ui.Time(), 1.2));
            g.FillEllipse(badge.R() - 2.f, badge.y + 2.f, 5.f, 5.f, Rgba(220, 60, 40).WithAlpha(pulse));
            ui.KeepAnimatingThrottled();
        }
        if (it.clicked) SelectPage(o.page, ui.Time());
    }
    // Sliding bar under the picked option
    const float cellW = (row.w - 12.f) * 0.5f;
    const Rect bar{ row.x + (cellW + 12.f) * slide + cellW * 0.25f, row.B() + 6.f, cellW * 0.5f, 3.f };
    g.GlowRound(bar, 1.5f, Palette::Teal, 8.f, 0.6f);
    g.FillRound(bar, 1.5f, Palette::Teal);
}

// Banner: a gradient strip with a drifting light, the sync badge floating, the title, a subtitle
// and the installed version as a pill
void UpdatesOverlay::DrawBanner(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    const float t = static_cast<float>(ui.Time());
    g.PushRoundClip(r, kCardRadius);
    g.FillLinear(r, 0.f, { { 0.f, Rgba(0, 60, 52) }, { 0.55f, Hex(0x0A1D2C) }, { 1.f, Hex(0x07121C) } }, { 0.f, 0.f }, { 1.f, 1.f });
    const float lx = r.x + r.w * (0.65f + 0.25f * std::sin(t * 0.6f));
    g.FillRadial({ lx - 200.f, r.y - 90.f, 400.f, 260.f }, { { 0.f, Palette::Teal.WithAlpha(0.18f) }, { 1.f, Palette::Teal.WithAlpha(0.f) } });
    // A few faint rings rippling out from the badge
    const float ring = std::fmod(t, 3.f) / 3.f;
    const float bx = r.x + 60.f, by = r.CY();
    for (int i = 0; i < 2; ++i) {
        const float k = std::fmod(ring + 0.5f * static_cast<float>(i), 1.f);
        g.StrokeEllipse(bx, by, 34.f + 50.f * k, 34.f + 50.f * k, Palette::Teal.WithAlpha(0.25f * (1.f - k)), 1.5f);
    }
    g.PopLayer();
    g.StrokeRound(r.Inset(0.5f), kCardRadius - 0.5f, Palette::Teal.WithAlpha(0.35f), 1.f);
    MenuKit::GlyphBadge(ui, { bx - 30.f, by - 30.f + 3.f * std::sin(t * 2.f), 60.f, 60.f }, Glyphs::Sync, Palette::Teal, 26.f);
    const float tx = r.x + 112.f;
    g.Text(Loc::Get("updates_banner_title"), { tx, r.CY() - 26.f, r.R() - tx - 200.f, 24.f }, MenuKit::kTitle, Mono(17.f, true).Ellipsis());
    g.Text(Loc::Get("updates_banner_sub"), { tx, r.CY() + 2.f, r.R() - tx - 200.f, 18.f }, Rgba(140, 175, 195), Mono(10.5f).Ellipsis());
    const std::wstring pill = Loc::Fmt("updates_installed_pill", AppVersion::DisplayVersion());
    const float pw = ui.g.Measure(pill, Mono(9.f, true)).w + 18.f;
    MenuKit::Chip(ui, r.R() - 20.f - pw, r.CY(), pill, Palette::Teal);
    ui.KeepAnimatingThrottled();
}

// The big status: an orb (spinning arcs while checking, a check when up to date, a bouncing
// download arrow when there's an update), the title, the message and the actions under it
void UpdatesOverlay::DrawStatus(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    const float t = static_cast<float>(ui.Time());
    Status status = Status::Unknown;
    if (checking) status = Status::Checking;
    else if (info && info->isUpdateAvailable) status = Status::Available;
    else if (gb && gb->isUpdateAvailable) status = Status::GameBanana;
    else if (info && !info->latestVersion.empty()) status = Status::UpToDate;
    else if (info) status = Status::Error;
    const Color color = status == Status::Available ? MenuKit::kGreen : status == Status::GameBanana ? kOrange : status == Status::UpToDate ? kBlue
                      : status == Status::Error ? MenuKit::kRed : MenuKit::kAmber;

    // The state pops in whenever it changes
    struct StateAnim {
        int status = -1;
        double since = -10.0;
    };
    auto& anim = ui.Data<StateAnim>(ui.MakeId("upd-status-anim"));
    if (anim.status != static_cast<int>(status)) {
        anim.status = static_cast<int>(status);
        anim.since = ui.Time();
    }
    const float since = static_cast<float>(ui.Time() - anim.since);
    const float pop = Anim::EaseOutBack(Anim::Clamp01(since / 0.5f), 0.8f);
    if (since < 0.6f) ui.KeepAnimating();
    ui.KeepAnimatingThrottled();

    g.Border(r, kCardRadius, Palette::HeaderBg, Palette::ButtonBorder, 1.f);
    g.FillRadial({ r.CX() - r.w * 0.6f, r.y - 120.f, r.w * 1.2f, 360.f }, { { 0.f, color.WithAlpha(0.10f) }, { 1.f, color.WithAlpha(0.f) } });

    // Orb
    const float ocx = r.CX(), ocy = r.y + 98.f, orb = 50.f * (0.85f + 0.15f * pop);
    if (status != Status::Checking && status != Status::Unknown) {
        const float ring = std::fmod(t, 2.f) / 2.f;
        for (int i = 0; i < 2; ++i) {
            const float k = std::fmod(ring + 0.5f * static_cast<float>(i), 1.f);
            g.StrokeEllipse(ocx, ocy, orb + 30.f * k, orb + 30.f * k, color.WithAlpha(0.4f * (1.f - k)), 2.f);
        }
    }
    g.GlowRound({ ocx - orb, ocy - orb, orb * 2.f, orb * 2.f }, orb, color, 24.f, 0.45f);
    g.FillRadial({ ocx - orb, ocy - orb, orb * 2.f, orb * 2.f }, { { 0.f, Lerp(color, Colors::White, 0.3f) }, { 1.f, Lerp(color, Colors::Black, 0.15f) } });
    if (status == Status::Checking) {
        // Two arcs chasing each other around the orb
        const auto arc = [&](float start, float sweep) {
            constexpr int kSegments = 20;
            const float step = sweep / kSegments, rr = orb + 11.f;
            for (int i = 0; i < kSegments; ++i) {
                const float a0 = start + i * step, a1 = a0 + step;
                g.Line(ocx + std::cos(a0) * rr, ocy + std::sin(a0) * rr, ocx + std::cos(a1) * rr, ocy + std::sin(a1) * rr, color, 3.f, true);
            }
        };
        const float spin = std::fmod(t * 5.f, 6.2831853f);
        arc(spin, 1.4f);
        arc(spin + 3.1415927f, 1.4f);
        ui.KeepAnimating();
    }
    wchar_t glyph = Glyphs::Info;
    float bob = 0.f, spin = 0.f;
    switch (status) {
        case Status::Checking: glyph = Glyphs::Sync; spin = std::fmod(t * 200.f, 360.f); break;
        case Status::UpToDate: glyph = Glyphs::CheckMark; break;
        case Status::Available: glyph = Glyphs::Download; bob = 5.f * std::sin(t * 3.f); break;
        case Status::GameBanana: glyph = Glyphs::Download; bob = 5.f * std::sin(t * 3.f); break;
        case Status::Error: glyph = Glyphs::Warning; break;
        default: break;
    }
    g.PushTransform(D2D1::Matrix3x2F::Rotation(spin, D2D1::Point2F(ocx, ocy)) * D2D1::Matrix3x2F::Translation(0.f, bob));
    ui.PushScale(std::max(0.01f, pop), ocx, ocy);
    Glyph(ui, glyph, { ocx - orb, ocy - orb, orb * 2.f, orb * 2.f }, 38.f, Colors::White);
    ui.PopTransform();
    g.PopTransform();

    // Title + message
    std::wstring title, message;
    switch (status) {
        case Status::Checking: title = Loc::Get("updates_hero_checking_title"); message = Loc::Get("updates_hero_checking_msg"); break;
        case Status::UpToDate: title = Loc::Get("updates_up_to_date_title"); message = Loc::Get("updates_up_to_date_msg"); break;
        case Status::Available:
            title = Loc::Get("updates_available_title");
            message = Loc::Fmt("updates_hero_available_msg", L"v" + info->latestVersion);
            break;
        case Status::GameBanana: title = Loc::Get("updates_gb_title"); message = Loc::Fmt("updates_hero_gb_msg", L"v" + gb->latestVersion); break;
        case Status::Error: title = Loc::Get("updates_error_title"); message = Loc::Get("updates_error_msg"); break;
        default: title = Loc::Get("updates_hero_unknown_title"); message = Loc::Get("updates_hero_unknown_msg"); break;
    }
    const float textW = r.w - 60.f;
    TextStyle titleStyle = Mono(16.f, true);
    titleStyle.wrap = true;
    titleStyle.align = HAlign::Center;
    titleStyle.valign = VAlign::Top;
    TextStyle msgStyle = Mono(11.f);
    msgStyle.wrap = true;
    msgStyle.align = HAlign::Center;
    msgStyle.valign = VAlign::Top;
    float y = ocy + orb + 34.f;
    const float textK = Anim::EaseOutCubic(Anim::Clamp01((since - 0.1f) / 0.35f));
    g.PushOpacity(textK);
    ui.PushTranslate(0.f, 8.f * (1.f - textK));
    const float titleH = g.Measure(title, titleStyle, textW).h;
    g.Text(title, { r.x + 30.f, y, textW, titleH }, Lerp(color, Palette::Bright, 0.35f), titleStyle);
    y += titleH + 8.f;
    const float msgH = g.Measure(message, msgStyle, textW).h;
    g.Text(message, { r.x + 30.f, y, textW, msgH }, Rgba(140, 170, 190), msgStyle);
    ui.PopTransform();
    g.PopLayer();

    // Actions at the bottom of the card: the main one (see the update / check again), GameBanana,
    // and the manual update
    const float by = r.B() - 22.f - 38.f;
    std::vector<std::pair<int, float>> buttons;   // which, width
    const std::wstring view = Loc::Get("updates_view_btn"), again = Loc::Get("updates_check_again_btn"), banana = Loc::Get("updates_gb_btn"),
                       manual = Loc::Get("updates_manual_btn");
    const bool available = status == Status::Available, gbAvailable = gb && gb->isUpdateAvailable && !available;
    const std::wstring& mainText = available ? view : again;
    const wchar_t mainGlyph = available ? Glyphs::Forward : Glyphs::Refresh;
    const float mw = MenuKit::FooterButtonWidth(ui, mainText, mainGlyph, 170.f);
    const float gw = gbAvailable ? MenuKit::FooterButtonWidth(ui, banana, Glyphs::Download, 150.f) : 0.f;
    const float total = mw + (gbAvailable ? 10.f + gw : 0.f);
    float x = r.CX() - total * 0.5f;
    if (MenuKit::FooterButton(ui, ui.MakeId("upd-main"), { x, by, mw, 38.f }, mainText, available ? MenuKit::Kind::Primary : MenuKit::Kind::Secondary,
                              !checking && !downloading, mainGlyph, available ? MenuKit::kGreen : Palette::Teal)) {
        if (available) ShowInstall(false);
        else Check();
    }
    x += mw + 10.f;
    if (gbAvailable &&
        MenuKit::FooterButton(ui, ui.MakeId("upd-gb"), { x, by, gw, 38.f }, banana, MenuKit::Kind::Primary, !downloading, Glyphs::Download, kOrange))
        ShowInstall(true);
    // Manual update as a quiet link under the buttons
    const TextStyle linkStyle = Mono(9.5f, true).Center();
    const float lw = g.Measure(manual, linkStyle).w + 24.f;
    const Rect link{ r.CX() - lw * 0.5f, by - 26.f, lw, 18.f };
    const Interaction it = ui.Interact(ui.MakeId("upd-manual"), link);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const float hover = ui.Animate(ui.MakeId("upd-manual-hover"), it.hovered ? 1.f : 0.f, 16.f, 0.f);
    Glyph(ui, Glyphs::Download, { link.x, link.y, 14.f, link.h }, 9.f, Lerp(Palette::Dim, Palette::Teal, hover));
    g.Text(manual, link.CutLeft(14.f), Lerp(Palette::Dim, Palette::Teal, hover), TextStyle(linkStyle).Underline());
    if (it.clicked) Standalone::OpenManualUpdate();
}

void UpdatesOverlay::DrawVersionCard(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    const Rect c = Card(ui, r, Loc::Get("updates_version_card"), Glyphs::Info);
    g.Text(AppVersion::DisplayVersion(), { c.x, c.y - 2.f, c.w, 28.f }, Palette::Teal, Mono(21.f, true));
    const std::wstring latest = checking ? Loc::Get("updates_source_waiting")
                              : info && !info->latestVersion.empty() ? L"v" + info->latestVersion : L"\x2014";
    const bool newer = info && info->isUpdateAvailable;
    // "Latest published ······ v4.1.5 · 3.2 MB"
    const std::wstring value = newer && info->fileSize > 0 ? latest + L"   B7  " + Text::FileSize(info->fileSize) : latest;
    g.Text(Loc::Get("updates_latest_label"), { c.x, c.y + 30.f, c.w, 16.f }, MenuKit::kDim, Mono(9.5f));
    g.Text(value, { c.x, c.y + 30.f, c.w, 16.f }, newer ? MenuKit::kGreen : Palette::Label, Mono(10.f, true).Right());
}

void UpdatesOverlay::DrawSourcesCard(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    const Rect c = Card(ui, r, Loc::Get("updates_sources_card"), Glyphs::Download);
    struct Source {
        const char* name;
        bool known;
        bool reached;
        std::wstring version;
        bool newer;
    };
    const Source sources[] = {
        { "updates_source_github", info.has_value(), info && !info->latestVersion.empty(), info ? info->latestVersion : L"", info && info->isUpdateAvailable },
        { "updates_source_gb", gb.has_value(), gb && !gb->latestVersion.empty(), gb ? gb->latestVersion : L"", gb && gb->isUpdateAvailable },
    };
    float y = c.y;
    for (const auto& s : sources) {
        const Rect row{ c.x, y, c.w, 26.f };
        y += 28.f;
        const Color dot = !s.known ? MenuKit::kAmber : s.newer ? MenuKit::kGreen : s.reached ? Palette::Teal : MenuKit::kRed;
        if (!s.known) Form::Spinner(ui, row.x + 6.f, row.CY(), 5.f, dot);
        else {
            g.FillEllipse(row.x + 6.f, row.CY(), 7.f, 7.f, dot.WithAlpha(0.18f));
            g.FillEllipse(row.x + 6.f, row.CY(), 3.5f, 3.5f, dot);
        }
        g.Text(Loc::Get(s.name), { row.x + 20.f, row.y, row.w - 110.f, row.h }, Palette::Label, Mono(10.5f, true).Ellipsis());
        const std::wstring value = !s.known ? Loc::Get("updates_source_waiting") : s.reached ? L"v" + s.version : Loc::Get("updates_source_none");
        g.Text(value, row, s.newer ? MenuKit::kGreen : MenuKit::kDim, Mono(10.f, true).Right());
    }
}

void UpdatesOverlay::DrawHowCard(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    const Rect c = Card(ui, r, Loc::Get("updates_how_card"), Glyphs::Education);
    TextStyle step = Mono(10.f);
    step.wrap = true;
    step.valign = VAlign::Top;
    float y = c.y;
    const char* keys[] = { "updates_how_1", "updates_how_2", "updates_how_3" };
    for (int i = 0; i < 3; ++i) {
        const std::wstring text = Loc::Get(keys[i]);
        const float h = std::max(20.f, g.Measure(text, step, c.w - 30.f).h);
        if (y + h > c.B() + 6.f) break;
        const Rect n{ c.x, y, 20.f, 20.f };
        g.Border(n, 10.f, Palette::Teal.WithAlpha(0.12f), Palette::Teal.WithAlpha(0.5f), 1.f);
        g.Text(std::to_wstring(i + 1), n, Palette::Teal, Mono(9.f, true).Center());
        if (i < 2) g.FillRect({ n.CX() - 0.5f, n.B() + 2.f, 1.f, h - 14.f }, Palette::Teal.WithAlpha(0.25f));
        g.Text(text, { c.x + 30.f, y + 2.f, c.w - 30.f, h }, Rgba(150, 180, 200), step);
        y += h + 8.f;
    }
}

// Banner on top, the status on the left, the three cards on the right; everything comes in one
// after another
void UpdatesOverlay::DrawOverview(Ui& ui, const Rect& area) {
    {
        StaggerItem reveal(ui, pageAt, 0, 0.f, 12.f);
        DrawBanner(ui, area.Top(kBannerH));
    }
    const float top = area.y + kBannerH + kGap;
    const Rect left{ area.x, top, area.w - kSideW - kGap, area.B() - top };
    const Rect right{ left.R() + kGap, top, kSideW, area.B() - top };
    {
        StaggerItem reveal(ui, pageAt + 0.08, 0, 0.f, 16.f);
        DrawStatus(ui, left);
    }
    const float versionH = 88.f, sourcesH = 96.f;
    {
        StaggerItem reveal(ui, pageAt + 0.14, 0, 26.f);
        DrawVersionCard(ui, right.Top(versionH));
    }
    {
        StaggerItem reveal(ui, pageAt + 0.2, 0, 26.f);
        DrawSourcesCard(ui, { right.x, right.y + versionH + kGap, right.w, sourcesH });
    }
    {
        StaggerItem reveal(ui, pageAt + 0.26, 0, 26.f);
        const float y = right.y + versionH + sourcesH + kGap * 2.f;
        DrawHowCard(ui, { right.x, y, right.w, right.B() - y });
    }
}

void UpdatesOverlay::DrawInstallView(Ui& ui, const Rect& body) {
    auto& g = ui.g;
    const bool useGb = installGb && gb;
    const std::wstring version = L"v" + (useGb ? gb->latestVersion : info->latestVersion);
    const std::wstring file = useGb ? gb->fileName : info->fileName;
    const long long fileSize = useGb ? gb->fileSize : info->fileSize;
    std::wstring notes = Text::Trim(useGb ? gb->changelog : info->changelog);
    if (notes.empty()) notes = L"\x2014";

    // Header strip: install glyph + "vX ready" + file / size
    const float titleH = g.Measure(L"Ag", Mono(14.f, true)).h, subH = g.Measure(L"Ag", Mono(11.f)).h;
    const Rect head{ body.x, body.y, body.w, 12.f + std::max(titleH + subH, 30.f) + 12.f + 1.f };
    g.FillRect(head, Palette::HeaderBg);
    g.FillRect({ head.x, head.B() - 1.f, head.w, 1.f }, Palette::Divider);
    g.Text(std::wstring(1, Glyphs::Install), { head.x + 16.f, head.y, 22.f, head.h - 1.f }, Palette::Teal, IconFont(22.f));
    const float x = head.x + 16.f + 22.f + 14.f, top = head.y + (head.h - 1.f - titleH - subH) * 0.5f;
    g.Text(Loc::Fmt("updates_install_ready", version), { x, top, head.R() - x - 16.f, titleH }, Palette::Teal, Mono(14.f, true));
    g.Text(Loc::Fmt("updates_install_subtitle", file, Text::FileSize(fileSize)), { x, top + titleH, head.R() - x - 16.f, subH }, Palette::Muted,
           Mono(11.f).Ellipsis());

    // What's new: "NOVEDADES" · "3 new versions · v4.0.0 → v4.1.1", then one folding card per version
    const float labelH = g.Measure(L"Ag", Mono(10.f, true)).h;
    const Rect label{ body.x + 16.f, head.B() + 12.f, body.w - 32.f, labelH };
    g.Text(Loc::Get("updates_whats_new"), label, Palette::Teal, Mono(10.f, true));
    const Rect list{ body.x + 12.f, label.B() + 10.f, body.w - 24.f, body.B() - label.B() - 14.f };
    const Id scrollId = ui.MakeId("updates-changelog");
    if (notes != newsSource) ScrollArea::ScrollTo(ui, scrollId, 0.f);
    ScrollArea scroll(ui, scrollId, list, false, 48.f, ScrollArea::Bar::Classic);
    const float listW = scroll.ContentWidth() - 4.f;
    if (notes != newsSource || std::abs(listW - newsWidth) > 0.5f || newsLang != Loc::Revision())
        BuildNews(ui, notes, useGb ? gb->latestVersion : info->latestVersion, listW);
    if (!news.empty()) {
        const std::wstring from = L"v" + AppVersion::CurrentVersion, to = L"v" + news.front().entry.version;
        const std::wstring summary = news.size() > 1 ? Loc::Fmt("updates_news_summary", static_cast<int>(news.size()), from, to)
                                                     : from + L"  \x2192  " + to;
        g.Text(summary, label, Palette::Muted, Mono(10.f).Right());
    }
    float y = list.y;
    for (size_t i = 0; i < news.size(); ++i) {
        StaggerItem in(ui, newsShownAt, static_cast<int>(i), 0.f, 14.f);
        y += DrawNewsCard(ui, i, list.x, y, listW) + kNewsGap;
    }
    scroll.SetContentSize(y - list.y);
}

// Only the versions after the installed one (up to the one being installed). A changelog without
// version lines, or one that doesn't list them, still shows: as the new version's notes.
void UpdatesOverlay::BuildNews(Ui& ui, const std::wstring& text, const std::wstring& latest, float width) {
    const auto all = Services::Changelog::Parse(text, latest);
    std::vector<Services::Changelog::Entry> kept;
    for (const auto& e : all)
        if (Versions::IsNewer(AppVersion::CurrentVersion, e.version) && !Versions::IsNewer(latest, e.version)) kept.push_back(e);
    // Versions the published file doesn't list (an older single-version changelog.txt) come from
    // the changelog built into this launcher, when it has them
    for (const auto& e : Services::Changelog::Entries()) {
        if (!Versions::IsNewer(AppVersion::CurrentVersion, e.version) || Versions::IsNewer(latest, e.version)) continue;
        if (std::none_of(kept.begin(), kept.end(), [&](const auto& k) { return k.version == e.version; })) kept.push_back(e);
    }
    std::sort(kept.begin(), kept.end(), [](const auto& a, const auto& b) { return Versions::IsNewer(b.version, a.version); });
    if (kept.empty())
        for (const auto& e : all)
            if (e.version == latest) kept.push_back(e);
    if (kept.empty() && !all.empty()) kept.push_back(all.front());

    const float textW = width - kNewsPadX * 2.f - kNewsIndent;
    news.clear();
    for (auto& entry : kept) {
        NewsCard card;
        card.entry = std::move(entry);
        float cy = 12.f;
        for (const auto& change : card.entry.changes) {
            if (change.rfind(L"## ", 0) == 0) {
                if (!card.sections.empty()) cy += kNewsSectionGap - kNewsLineGap;
                card.sections.push_back({ change.substr(3), cy, {} });
                cy += kNewsTagH + 8.f;
                continue;
            }
            if (card.sections.empty()) card.sections.push_back({ L"", cy, {} });
            NewsLine line;
            line.text = change;
            line.mentions = change.find(L'@') != std::wstring::npos;
            line.y = cy;
            line.h = line.mentions ? MentionText(ui, 0, 0.f, 0.f, textW, change, kNewsText, Palette::Label, false)
                                   : ui.g.Measure(change, Mono(kNewsText).Wrap().Top(), textW).h;
            cy += line.h + kNewsLineGap;
            card.sections.back().lines.push_back(std::move(line));
        }
        card.bodyH = cy - kNewsLineGap + 16.f;
        news.push_back(std::move(card));
    }
    // A new changelog: only the newest version starts open
    if (text != newsSource) {
        newsOpen.assign(news.size(), false);
        if (!newsOpen.empty()) newsOpen[0] = true;
        newsShownAt = ui.Time();
    }
    newsSource = text;
    newsWidth = width;
    newsLang = Loc::Revision();
}

// Header (chevron · version · NEW · per-section counts ······ date), click to fold / unfold; the
// body grows open with the sections and their changes. Returns the card's current height.
float UpdatesOverlay::DrawNewsCard(Ui& ui, size_t index, float x, float y, float w) {
    auto& g = ui.g;
    const NewsCard& card = news[index];
    const bool newest = index == 0;
    const int id = static_cast<int>(index);
    const float open = ui.Animate(ui.MakeId("upd-news-open", id), newsOpen[index] ? 1.f : 0.f, 12.f);
    const float h = kNewsHeaderH + card.bodyH * open;
    const Rect r{ x, y, w, h };
    const Rect header{ x, y, w, kNewsHeaderH };

    const Interaction it = ui.Interact(ui.MakeId("upd-news-head", id), header);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    if (it.clicked) newsOpen[index] = !newsOpen[index];
    const float hover = ui.Animate(ui.MakeId("upd-news-hover", id), it.hovered ? 1.f : 0.f, 14.f);
    const Color accent = newest ? Hex(0x00CCAA) : Hex(0x3A6A8A);
    const Color bg = newest ? Lerp(Rgba(0, 38, 34), Rgba(0, 48, 42), hover) : Lerp(Rgba(9, 20, 30), Rgba(13, 28, 42), hover);
    const Color border = newest ? Rgba(0, 204, 170, static_cast<int>(110 + 70 * hover)) : Lerp(Rgba(19, 44, 64), Rgba(40, 80, 110), hover);
    if (newest) g.GlowRound(r, 8.f, Hex(0x00CCAA), 18.f, 0.2f + 0.12f * hover);
    g.Border(r, 8.f, bg, border, 1.f);

    // ── Header ──
    const float cy = header.CY();
    {
        const float cx = x + 22.f;
        g.PushTransform(D2D1::Matrix3x2F::Rotation(90.f * open, D2D1::Point2F(cx, cy)));
        Glyph(ui, 0xE76C, { cx - 8.f, cy - 8.f, 16.f, 16.f }, 10.f, Lerp(accent, Colors::White, 0.2f * hover));   // chevron right
        g.PopTransform();
    }
    float px = x + 40.f;
    const std::wstring version = L"v" + card.entry.version;
    const TextStyle versionStyle = Mono(15.f, true);
    const float versionW = g.Measure(version, versionStyle).w + 2.f;
    g.Text(version, { px, header.y, versionW, header.h }, newest ? Hex(0x00CCAA) : Rgba(200, 220, 235), versionStyle);
    px += versionW + 10.f;
    if (newest) {
        const std::wstring pill = Loc::Get("updates_news_latest");
        const TextStyle pillStyle = Mono(9.f, true);
        const float pw = g.Measure(pill, pillStyle).w + 16.f;
        const Rect pr{ px, cy - 10.f, pw, 20.f };
        g.Border(pr, 10.f, Rgba(0, 204, 170, 40), Rgba(0, 204, 170, 140), 1.f);
        g.Text(pill, pr, Hex(0x00CCAA), TextStyle(pillStyle).Center());
        px += pw + 10.f;
    }
    // Date on the right (calendar glyph before it)
    float right = header.R() - kNewsPadX;
    if (!card.entry.date.empty()) {
        const TextStyle dateStyle = Mono(11.f);
        const float dateW = g.Measure(card.entry.date, dateStyle).w + 2.f;
        g.Text(card.entry.date, { right - dateW, header.y, dateW, header.h }, Rgba(90, 130, 160), dateStyle);
        Glyph(ui, 0xE787, { right - dateW - 22.f, header.y, 16.f, header.h }, 12.f, Rgba(90, 130, 160));
        right -= dateW + 30.f;
    }
    // How many changes of each kind (+6  ✎3  🔧2 ...), as far as they fit
    const TextStyle countStyle = Mono(10.f, true);
    for (const auto& section : card.sections) {
        if (section.title.empty() || section.lines.empty()) continue;
        const auto look = ChangelogLook::ForSection(section.title);
        const std::wstring count = std::to_wstring(section.lines.size());
        const float cw = 8.f + 12.f + 4.f + g.Measure(count, countStyle).w + 8.f;
        if (px + cw > right) break;
        const Rect chip{ px, cy - 10.f, cw, 20.f };
        g.Border(chip, 10.f, look.color.WithAlpha(0.1f), look.color.WithAlpha(0.35f), 1.f);
        Glyph(ui, look.glyph, { chip.x + 7.f, chip.y, 12.f, chip.h }, 9.f, look.color);
        g.Text(count, { chip.x + 23.f, chip.y, cw - 23.f, chip.h }, look.color, countStyle);
        ui.Tooltip(ui.MakeId("upd-news-chip", id * 64 + static_cast<int>(&section - card.sections.data())), section.title);
        px += cw + 6.f;
    }

    // ── Body (clipped while it folds) ──
    if (open > 0.01f) {
        const Rect bodyR{ x, header.B(), w, h - kNewsHeaderH };
        g.PushClip(bodyR);
        g.PushOpacity(std::min(1.f, open * 1.4f));
        g.FillRect({ x + kNewsPadX, header.B(), w - kNewsPadX * 2.f, 1.f }, accent.WithAlpha(0.18f));
        const float textX = x + kNewsPadX + kNewsIndent;
        const float textW = w - kNewsPadX * 2.f - kNewsIndent;
        const float lineH = g.Measure(L"Ag", Mono(kNewsText)).h;
        for (size_t s = 0; s < card.sections.size(); ++s) {
            const auto& section = card.sections[s];
            const auto look = ChangelogLook::ForSection(section.title);
            if (!section.title.empty()) {
                const TextStyle tagStyle = Mono(9.5f, true);
                std::wstring tagText = section.title;
                std::transform(tagText.begin(), tagText.end(), tagText.begin(), ::towupper);
                const float tw = g.Measure(tagText, tagStyle).w;
                const Rect tag{ x + kNewsPadX, header.B() + section.y, tw + 34.f, kNewsTagH };
                g.Border(tag, 5.f, look.color.WithAlpha(0.14f), look.color.WithAlpha(0.45f), 1.f);
                Glyph(ui, look.glyph, { tag.x + 8.f, tag.y, 14.f, tag.h }, 10.f, look.color);
                g.Text(tagText, { tag.x + 25.f, tag.y, tw + 4.f, tag.h }, look.color, tagStyle);
            }
            for (size_t i = 0; i < section.lines.size(); ++i) {
                const auto& line = section.lines[i];
                const float ly = header.B() + line.y;
                if (ly > bodyR.B()) break;
                g.FillEllipse(x + kNewsPadX + 5.f, ly + lineH * 0.5f + 0.5f, 2.6f, 2.6f, look.color.WithAlpha(0.85f));
                if (line.mentions)
                    MentionText(ui, HashId(static_cast<int64_t>(s * 1000 + i), ui.MakeId("upd-news-line", id)), textX, ly, textW, line.text,
                                kNewsText, Palette::Label);
                else
                    ChangelogLook::DrawLine(g, line.text, { textX, ly, textW, line.h + 2.f }, kNewsText, look.color);
            }
        }
        g.PopLayer();
        g.PopClip();
    }
    if (open > 0.001f && open < 0.999f) ui.KeepAnimating();
    return h;
}

void UpdatesOverlay::DrawFooter(Ui& ui, const Rect& footer) {
    auto& g = ui.g;
    const Rect row = footer.Inset(14.f, 0.f);
    if (downloading) {
        const std::wstring label = Loc::Fmt("updates_downloading", percent);
        const float w = g.Measure(label, Mono(11.f)).w + 1.f;
        g.Text(label, { row.x, row.y, w, row.h }, Palette::Teal, Mono(11.f));
        MenuKit::GlowProgress(ui, ui.MakeId("updates-progress"), { row.x + w + 12.f, row.CY() - 4.f, 220.f, 8.f }, percent / 100.f);
    } else if (installView) {
        const std::wstring accept = Loc::Get("updates_download_btn");
        const float aw = MenuKit::FooterButtonWidth(ui, accept, Glyphs::Download, 200.f);
        if (MenuKit::FooterButton(ui, ui.MakeId("updates-accept"), { row.x, row.CY() - 18.f, aw, 36.f }, accept, MenuKit::Kind::Primary, true, Glyphs::Download))
            Install();
        const std::wstring cancel = Loc::Get("updates_cancel_btn");
        const float cw = MenuKit::FooterButtonWidth(ui, cancel, Glyphs::Cancel, 100.f);
        if (MenuKit::FooterButton(ui, ui.MakeId("updates-cancel"), { row.x + aw + 8.f, row.CY() - 18.f, cw, 36.f }, cancel, MenuKit::Kind::Secondary, true,
                                  Glyphs::Cancel))
            installView = false;
    }
    const std::wstring close = Loc::Get("updates_close");
    const float w = MenuKit::FooterButtonWidth(ui, close, Glyphs::Back, 120.f);
    if (MenuKit::FooterButton(ui, ui.MakeId("updates-close"), { row.R() - w, row.CY() - 18.f, w, 36.f }, close, MenuKit::Kind::Secondary, true, Glyphs::Back)) {
        popup.Close();
        if (!downloading) installView = false;
    }
}

void UpdatesOverlay::Draw(Ui& ui, const Rect& screen) {
    PanelStyle style;
    style.title = Loc::Get("updates_header");
    OverlayFrame frame(ui, ui.MakeId("updates"), screen, 900.f, 690.f, style, popup.Opacity(), popup.Scale());
    // The two options on top (hidden in the install view, which has its own header)
    const bool install = page == Page::Updates && installView && (installGb ? gb.has_value() : info.has_value());
    if (!install && page == Page::Updates) installView = false;
    Rect content = frame.body;
    if (!install) {
        const Rect row{ frame.body.x + 20.f, frame.body.y + 16.f, frame.body.w - 40.f, 54.f };
        {
            StaggerItem reveal(ui, frame.OpenedAt() + 0.1, 0, 0.f, 10.f);
            DrawPages(ui, row);
        }
        content = { frame.body.x + 20.f, row.B() + 20.f, frame.body.w - 40.f, frame.body.B() - row.B() - 36.f };
    }
    if (page == Page::Changelog) changelog.Draw(ui, { frame.body.x, content.y - 4.f, frame.body.w, frame.body.B() - content.y + 4.f });
    else if (install) DrawInstallView(ui, frame.body);
    else DrawOverview(ui, content);
    DrawFooter(ui, frame.footer);
    if (popup.IsOpen() && !downloading && ui.KeyPressed(VK_ESCAPE)) {
        if (install) installView = false;
        else popup.Close();
    }
}

} // namespace SpeedrunLauncher::Main
