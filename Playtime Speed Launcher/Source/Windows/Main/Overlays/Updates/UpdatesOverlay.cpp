#include "Windows/Main/Overlays/Updates/UpdatesOverlay.h"
#include <algorithm>
#include <cmath>
#include "Core/Async.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/App/AppVersion.h"
#include "Services/App/VersionCompare.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
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

// Big "icon + title + message" banner button (status, GameBanana).
bool Banner(Ui& ui, Id id, Column& col, wchar_t glyph, const std::wstring& title, const std::wstring& message, Color background,
            bool enabled, bool border = true) {
    auto& g = ui.g;
    TextStyle body = Mono(12.f);
    body.wrap = true;
    body.valign = VAlign::Top;
    const float textX = 16.f + 1.f + 22.f + 14.f;
    const float textW = col.w - textX - 17.f;
    const float titleH = g.Measure(title, Mono(15.f, true)).h, messageH = g.Measure(message, body, textW).h;
    const Rect r = col.Next(1.f + 12.f + std::max(titleH + messageH, 30.f) + 12.f + 1.f, 10.f);
    // The status button keeps WPF's default #707070 border; the GameBanana one sets BorderThickness 0
    ButtonScope b(ui, id, r, { background, border ? Hex(0x707070) : Colors::Transparent, border ? 1.f : 0.f, 4.f, { 16.f, 12.f }, 0.f, r.h }, enabled);
    g.Text(std::wstring(1, glyph), { r.x + 17.f, r.y, 22.f, r.h }, Colors::White, IconFont(22.f));
    const float top = r.CY() - (titleH + messageH) * 0.5f;
    g.Text(title, { r.x + textX, top, textW, titleH }, Colors::White, Mono(15.f, true));
    g.Text(message, { r.x + textX, top + titleH, textW, messageH }, Colors::White.WithAlpha(0.85f), body);
    return b.Clicked();
}

} // namespace

void UpdatesOverlay::Open() {
    installView = false;
    popup.Open();
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

void UpdatesOverlay::DrawCheckView(Ui& ui, Column& col) {
    auto& g = ui.g;
    // Current version banner (click = check again)
    {
        const float labelH = g.Measure(L"Ag", Mono(10.f)).h, versionH = g.Measure(L"Ag", Mono(20.f, true)).h;
        const Rect r = col.Next(1.f + 12.f + labelH + versionH + labelH + 12.f + 1.f, 10.f);
        ButtonScope b(ui, ui.MakeId("updates-check"), r, { Palette::HeaderBg, Palette::ButtonBorder, 1.f, 4.f, { 16.f, 12.f }, 0.f, r.h });
        g.Text(std::wstring(1, Glyphs::Sync), { r.x + 17.f, r.y, 22.f, r.h }, Palette::Teal, IconFont(22.f));
        const float x = r.x + 17.f + 22.f + 14.f, w = r.R() - x - 17.f;
        float y = r.y + 13.f;
        g.Text(Loc::Get("updates_current_version"), { x, y, w, labelH }, Palette::Muted, Mono(10.f));
        y += labelH;
        g.Text(AppVersion::DisplayVersion(), { x, y, w, versionH }, Palette::Teal, Mono(20.f, true));
        y += versionH;
        g.Text(Loc::Get(checking ? "updates_checking" : "updates_check_hint"), { x, y, w, labelH }, Hex(0x1A3A50), Mono(10.f).Italic());
        if (b.Clicked()) Check();
    }
    if (info) {
        // Status: available (green) / up to date (blue) / error (orange)
        if (info->isUpdateAvailable) {
            if (Banner(ui, ui.MakeId("updates-status"), col, 0xE896, Loc::Get("updates_available_title"),
                       Loc::Fmt("updates_available_msg", L"v" + info->latestVersion), Rgba(46, 125, 50), true)) {
                ShowInstall(false);
            }
        } else if (!info->latestVersion.empty() && info->latestVersion == AppVersion::CurrentVersion) {
            Banner(ui, ui.MakeId("updates-status"), col, 0xE930, Loc::Get("updates_up_to_date_title"), Loc::Get("updates_up_to_date_msg"),
                   Rgba(21, 101, 192), false);
        } else {
            Banner(ui, ui.MakeId("updates-status"), col, 0xE814, Loc::Get("updates_error_title"), Loc::Get("updates_error_msg"), Rgba(230, 81, 0),
                   false);
        }
        if (!info->isUpdateAvailable && gb && gb->isUpdateAvailable) {
            if (Banner(ui, ui.MakeId("updates-gb"), col, 0xE774, Loc::Get("updates_gb_title"), Loc::Fmt("updates_gb_msg", L"v" + gb->latestVersion),
                       Hex(0xF4821A), true, false)) {
                ShowInstall(true);
            }
        }
        if (info->isUpdateAvailable) {
            // Release details
            const float labelH = g.Measure(L"Ag", Mono(10.f, true)).h, rowH = g.Measure(L"Ag", Mono(11.f)).h;
            const Rect r = col.Next(1.f + 12.f + labelH + 8.f + rowH * 3.f + 12.f + 1.f);
            g.Border(r, 4.f, Palette::HeaderBg, Palette::ButtonBorder, 1.f);
            float y = r.y + 13.f;
            const float x = r.x + 17.f;
            g.Text(Loc::Get("updates_details_label"), { x, y, r.w - 34.f, labelH }, Palette::Teal, Mono(10.f, true));
            y += labelH + 8.f;
            const std::pair<const char*, std::wstring> rows[] = { { "updates_latest_version", L"v" + info->latestVersion },
                                                                  { "updates_file_name", info->fileName },
                                                                  { "updates_file_size", Text::FileSize(info->fileSize) } };
            for (const auto& [key, value] : rows) {
                g.Text(Loc::Get(key), { x, y, 130.f, rowH }, Palette::Muted, Mono(11.f));
                g.Text(value, { x + 130.f, y, r.R() - x - 147.f, rowH }, Palette::Label, Mono(11.f).Ellipsis());
                y += rowH;
            }
        }
    }

    // "Manual update" link (Margin 2,14,0,0)
    col.Gap(14.f);
    const std::wstring link = Loc::Get("updates_manual_link");
    const TextStyle linkStyle = Mono(10.f);
    const TextSize size = g.Measure(link, linkStyle);
    const Rect r = col.Next(size.h);
    if (LinkButton(ui, ui.MakeId("updates-manual"), { r.x + 2.f, r.y, size.w + 2.f, size.h }, link, linkStyle, Palette::Dim))
        Standalone::OpenManualUpdate();
}

void UpdatesOverlay::DrawInstallView(Ui& ui, const Rect& body) {
    auto& g = ui.g;
    const bool useGb = installGb && gb;
    const std::wstring version = L"v" + (useGb ? gb->latestVersion : info->latestVersion);
    const std::wstring file = useGb ? gb->fileName : info->fileName;
    const long long fileSize = useGb ? gb->fileSize : info->fileSize;
    std::wstring changelog = Text::Trim(useGb ? gb->changelog : info->changelog);
    if (changelog.empty()) changelog = L"\x2014";

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
    if (changelog != newsSource) ScrollArea::ScrollTo(ui, scrollId, 0.f);
    ScrollArea scroll(ui, scrollId, list, false, 48.f, ScrollArea::Bar::Classic);
    const float listW = scroll.ContentWidth() - 4.f;
    if (changelog != newsSource || std::abs(listW - newsWidth) > 0.5f || newsLang != Loc::Revision())
        BuildNews(ui, changelog, useGb ? gb->latestVersion : info->latestVersion, listW);
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
    const TextStyle text = Mono(11.f, true).Center();
    if (downloading) {
        const std::wstring label = Loc::Fmt("updates_downloading", percent);
        const float w = g.Measure(label, Mono(11.f)).w + 1.f;
        g.Text(label, { row.x, row.y, w, row.h }, Palette::Teal, Mono(11.f));
        ProgressBar(ui, { row.x + w + 12.f, row.CY() - 3.f, 200.f, 6.f }, percent / 100.f);
        ui.KeepAnimatingThrottled();
    } else if (installView) {
        const std::wstring accept = Loc::Get("updates_download_btn");
        ButtonStyle acceptStyle{ Palette::Teal, Colors::Transparent, 0.f, 3.f, { 0.f }, 200.f, 36.f };
        const float aw = ButtonWidth(ui, accept, text, acceptStyle);
        if (TextButton(ui, ui.MakeId("updates-accept"), { row.x, row.CY() - 18.f, aw, 36.f }, accept, text, Hex(0x060C12), acceptStyle)) Install();
        const std::wstring cancel = Loc::Get("updates_cancel_btn");
        ButtonStyle cancelStyle = Styles::Footer();
        cancelStyle.minWidth = 100.f;
        const float cw = ButtonWidth(ui, cancel, text, cancelStyle);
        if (TextButton(ui, ui.MakeId("updates-cancel"), { row.x + aw + 8.f, row.CY() - 18.f, cw, 36.f }, cancel, text, Palette::Dim, cancelStyle))
            installView = false;
    }
    const std::wstring close = Loc::Get("updates_close");
    const float w = ButtonWidth(ui, close, text, Styles::Footer());
    if (TextButton(ui, ui.MakeId("updates-close"), { row.R() - w, row.CY() - 18.f, w, 36.f }, close, text, Palette::Dim, Styles::Footer())) {
        popup.Close();
        if (!downloading) installView = false;
    }
}

void UpdatesOverlay::Draw(Ui& ui, const Rect& screen) {
    PanelStyle style;
    style.title = Loc::Get("updates_header");
    OverlayFrame frame(ui, ui.MakeId("updates"), screen, 700.f, 540.f, style, popup.Opacity(), popup.Scale());
    if (installView && (installGb ? gb.has_value() : info.has_value())) {
        DrawInstallView(ui, frame.body);
    } else {
        installView = false;
        ScrollArea scroll(ui, ui.MakeId("updates-scroll"), frame.body, false, 48.f, ScrollArea::Bar::Classic);
        Column col(frame.body.x + 20.f, frame.body.y + 14.f, scroll.ContentWidth() - 40.f);
        DrawCheckView(ui, col);
        scroll.SetContentSize(col.Height() + 28.f);
    }
    DrawFooter(ui, frame.footer);
}

} // namespace SpeedrunLauncher::Main
