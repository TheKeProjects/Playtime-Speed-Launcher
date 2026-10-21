#include "Windows/Standalone/ManualUpdateWindow.h"
#include <algorithm>
#include "Core/Async.h"
#include "Core/Process.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Seasonal/SeasonalCardArt.h"
#include "Seasonal/SeasonalTheme.h"
#include "Services/App/AppVersion.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/Panel.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Popups/TutorialWindows.h"

namespace SpeedrunLauncher::Windows {

using namespace UI;
namespace U = Services::Updates;

namespace {

constexpr float kPanelW = 520.f, kPanelH = 430.f, kRadius = 10.f;
constexpr float kMargin = 26.f;   // transparent border holding the glow and the corner marks
constexpr float kDecorH = 30.f;   // room above the panel for the seasonal decorations

} // namespace

bool ManualUpdateWindow::Open(HWND owner) {
    ownerWindow = owner;
    UI::Window::Options o;
    o.title = L"Manual Update";
    o.style = WS_POPUP;
    o.owner = owner;
    o.transparent = true;
    decor = Seasonal::IsSeasonal() ? kDecorH : 0.f;
    o.width = static_cast<int>(kPanelW + kMargin * 2.f);
    o.height = static_cast<int>(kPanelH + kMargin * 2.f + decor);
    o.showCommand = SW_HIDE;
    if (!Create(o)) return false;
    g.SetColorFilter(&Seasonal::Filter);   // follows the seasonal theme like the main window
    SetContinuous(true, true);              // spinner, glow and the season's particles
    SetIdleFps(30.0);
    CenterOnOwner(*this, owner, kPanelW + kMargin * 2.f, kPanelH + kMargin * 2.f + decor);
    if (owner) EnableWindow(owner, FALSE);   // ShowDialog
    Show(SW_SHOW);
    Activate();
    auto guard = alive;
    Async::Run([] { return U::LatestReleaseInfo(); }, [this, guard](U::UpdateInfo info) {
        if (!*guard) return;
        checking = false;
        if (!Text::Trim(info.downloadUrl).empty()) release = info;
        else failed = true;
        Invalidate();
    });
    return true;
}

void ManualUpdateWindow::OnClosing(bool& cancel) {
    cancel = false;
    if (ownerWindow) {
        EnableWindow(ownerWindow, TRUE);
        SetForegroundWindow(ownerWindow);
    }
}

// Panel with the launcher's menu look (glow, accent lines, corner marks, seasonal dressing): a
// badge heading, the intro, a status card (checking spinner / ready / error), the download bar and
// DOWNLOAD (main) / OPEN IN BROWSER
void ManualUpdateWindow::OnFrame() {
    const double now = ui.Time();
    if (openedAt < 0.0) openedAt = now;
    const float sinceOpen = static_cast<float>(now - openedAt);
    const Rect panel{ kMargin, kMargin + decor, kPanelW, kPanelH };
    PanelGlow(ui, panel, kRadius, Palette::Teal, sinceOpen);
    PanelRise rise(ui, sinceOpen);
    g.Border(panel, kRadius, Argb(0xF207101A), Hex(0x153048), 1.f);
    g.FillLinear(panel.Inset(1.f).Top(160.f), kRadius - 1.f, { { 0.f, Palette::Teal.WithAlpha(0.07f) }, { 1.f, Palette::Teal.WithAlpha(0.f) } }, { 0.f, 0.f },
                 { 0.f, 1.f });

    // Header: badge + title, ✕
    const Rect header{ panel.x + 1.f, panel.y + 1.f, panel.w - 2.f, 60.f };
    g.FillRound(header, kRadius - 1.f, kRadius - 1.f, 0.f, 0.f, Hex(0x05090F));
    MenuKit::GlyphBadge(ui, { header.x + 16.f, header.CY() - 17.f, 34.f, 34.f }, Glyphs::Sync, Palette::Teal, 15.f);
    g.Text(Loc::Get("manual_update_header"), { header.x + 64.f, header.y, header.w - 120.f, header.h }, Palette::Teal, Mono(14.f, true));
    if (MenuKit::FooterButton(ui, ui.MakeId("close"), { header.R() - 16.f - 32.f, header.CY() - 16.f, 32.f, 32.f }, L"", MenuKit::Kind::Secondary, true,
                              Glyphs::Cancel) ||
        (ui.KeyPressed(VK_ESCAPE) && !downloading)) {
        Close();
        return;
    }

    // Content
    const float x = panel.x + 22.f, w = panel.w - 44.f;
    float y = header.B() + 18.f;
    {
        StaggerItem reveal(ui, openedAt + 0.12, 0, 0.f, 10.f);
        y += Paragraph(ui, x, y, w, Loc::Get("manual_update_intro"), Mono(11.f), Hex(0x8AABBD)) + 16.f;
    }

    // Status card: amber spinner while checking, green when ready, red on errors
    const Color accent = failed ? MenuKit::kRed : release ? MenuKit::kGreen : MenuKit::kAmber;
    std::wstring title = Loc::Get("manual_update_checking"), message;
    if (failed) title = Loc::Get("manual_update_error");
    else if (release) {
        title = Loc::Fmt("manual_update_ready", L"v" + release->latestVersion);
        message = release->fileName + L"  |  " + Text::FileSize(release->fileSize);
    }
    {
        StaggerItem reveal(ui, openedAt + 0.12, 1, 0.f, 10.f);
        TextStyle titleStyle = Mono(12.f, true);
        titleStyle.wrap = true;
        titleStyle.valign = VAlign::Top;
        TextStyle body = Mono(10.f);
        body.wrap = true;
        body.valign = VAlign::Top;
        const float textX = x + 70.f, textW = x + w - 16.f - textX;
        const float titleH = g.Measure(title, titleStyle, textW).h, messageH = message.empty() ? 0.f : g.Measure(message, body, textW).h + 4.f;
        const Rect box{ x, y, w, std::max(titleH + messageH, 44.f) + 28.f };
        g.GlowRound(box, 10.f, accent, 14.f, 0.18f);
        g.Border(box, 10.f, accent.WithAlpha(16.f / 255.f), accent.WithAlpha(80.f / 255.f), 1.f);
        const Rect icon{ x + 14.f, box.CY() - 22.f, 44.f, 44.f };
        if (!release && !failed) {
            g.Border(icon, 12.f, accent.WithAlpha(20.f / 255.f), accent.WithAlpha(90.f / 255.f), 1.f);
            Form::Spinner(ui, icon.CX(), icon.CY(), 11.f, accent);
        } else {
            MenuKit::GlyphBadge(ui, icon, failed ? Glyphs::Warning : Glyphs::Download, accent, 18.f);
        }
        const float top = box.CY() - (titleH + messageH) * 0.5f;
        g.Text(title, { textX, top, textW, titleH }, Lerp(accent, Palette::Bright, 0.3f), titleStyle);
        if (!message.empty()) g.Text(message, { textX, top + titleH + 4.f, textW, messageH }, Hex(0x8AABBD).WithAlpha(0.9f), body);
        y = box.B();
    }
    if (downloading) {
        y += 16.f;
        g.Text(Loc::Fmt("updates_downloading", percent), { x, y, w, 16.f }, Palette::Teal, Mono(10.f, true));
        MenuKit::GlowProgress(ui, ui.MakeId("progress"), { x, y + 22.f, w, 8.f }, percent / 100.f);
    }

    // Footer: DOWNLOAD (main), OPEN IN BROWSER
    const Rect footer{ panel.x + 1.f, panel.B() - 63.f, panel.w - 2.f, 62.f };
    g.FillRound(footer, 0.f, 0.f, kRadius - 1.f, kRadius - 1.f, Palette::HeaderBg);
    g.FillRect({ footer.x, footer.y, footer.w, 1.f }, Palette::Divider);
    const std::wstring download = Loc::Get("manual_update_download_btn"), browser = Loc::Get("manual_update_browser_btn");
    const float dw = MenuKit::FooterButtonWidth(ui, download, Glyphs::Download, 200.f);
    if (MenuKit::FooterButton(ui, ui.MakeId("download"), { footer.x + 16.f, footer.CY() - 18.f, dw, 36.f }, download, MenuKit::Kind::Primary,
                              release && !downloading, Glyphs::Download)) {
        downloading = true;
        percent = 0;
        auto guard = alive;
        Async::Run([this, guard] {
            return U::DownloadAndInstallLauncherExe([this, guard](int p) {
                Async::Post([this, guard, p] {
                    if (*guard) percent = p;
                });
            });
        },
                   [this, guard](bool ok) {
                       if (ok || !*guard) return;   // on success the updater restarts the launcher
                       downloading = false;
                       failed = true;
                       release.reset();
                   });
    }
    const float bw = std::min(MenuKit::FooterButtonWidth(ui, browser, Glyphs::Forward, 140.f), footer.R() - 16.f - (footer.x + 16.f + dw + 10.f));
    if (MenuKit::FooterButton(ui, ui.MakeId("browser"), { footer.R() - 16.f - bw, footer.CY() - 18.f, bw, 36.f }, browser, MenuKit::Kind::Secondary,
                              !downloading, Glyphs::Forward)) {
        const std::wstring url = release && !Text::Trim(release->releaseUrl).empty() ? release->releaseUrl : AppVersion::GitHubRepoUrl() + L"/releases/latest";
        Process::OpenUrl(url);
    }

    // Seasonal theme: particles in the body, decorations on the top edge
    Seasonal::CardArt::DrawPanelParticles(g, panel, kRadius, now, 0.5f);
    PanelDecor(ui, panel, kRadius, Palette::Teal, sinceOpen, header.B(), footer.y);
    Seasonal::CardArt::DrawPanelEdge(g, panel, now);
    if (PanelEntranceRunning(sinceOpen)) ui.KeepAnimating();
}

} // namespace SpeedrunLauncher::Windows
