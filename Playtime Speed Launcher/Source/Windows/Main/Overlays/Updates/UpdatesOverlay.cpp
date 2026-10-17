#include "Windows/Main/Overlays/Updates/UpdatesOverlay.h"
#include <algorithm>
#include "Core/Async.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/App/AppVersion.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Main/Common/MentionText.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Standalone/StandaloneWindows.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
namespace U = Services::Updates;

namespace {

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
                installView = true;
                installGb = false;
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
                installView = true;
                installGb = true;
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

    // What's new (Margin 12,8,12,4, Padding 12)
    const Rect box{ body.x + 12.f, head.B() + 8.f, body.w - 24.f, body.B() - head.B() - 12.f };
    g.FillRound(box, 4.f, Palette::HeaderBg);
    g.FillRect({ box.x + 4.f, box.B() - 1.f, box.w - 8.f, 1.f }, Palette::Divider);
    const float labelH = g.Measure(L"Ag", Mono(10.f, true)).h;
    g.Text(Loc::Get("updates_whats_new"), { box.x + 12.f, box.y + 12.f, box.w - 24.f, labelH }, Palette::Teal, Mono(10.f, true));
    const Rect text{ box.x + 12.f, box.y + 12.f + labelH + 8.f, box.w - 24.f, box.B() - 12.f - (box.y + 12.f + labelH + 8.f) };
    ScrollArea scroll(ui, ui.MakeId("updates-changelog"), text, false, 48.f, ScrollArea::Bar::Classic);
    scroll.SetContentSize(MentionText(ui, ui.MakeId("updates-mentions"), text.x, text.y, scroll.ContentWidth(), changelog, 12.f, Palette::Label));
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
