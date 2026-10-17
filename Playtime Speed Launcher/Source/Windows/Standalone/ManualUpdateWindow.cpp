#include "Windows/Standalone/ManualUpdateWindow.h"
#include <algorithm>
#include "Core/Async.h"
#include "Core/Process.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Seasonal/SeasonalTheme.h"
#include "Services/App/AppVersion.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "Windows/Popups/TutorialWindows.h"

namespace SpeedrunLauncher::Windows {

using namespace UI;
namespace U = Services::Updates;

bool ManualUpdateWindow::Open(HWND owner) {
    ownerWindow = owner;
    UI::Window::Options o;
    o.title = L"Manual Update";
    o.style = WS_POPUP;
    o.owner = owner;
    o.transparent = true;
    o.width = 500;
    o.height = 420;
    o.showCommand = SW_HIDE;
    if (!Create(o)) return false;
    CenterOnOwner(*this, owner, 500.f, 420.f);
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

void ManualUpdateWindow::OnFrame() {
    const Rect all{ 0.f, 0.f, g.Width(), g.Height() };
    g.Border(all, 8.f, Argb(0xEF07101A), Hex(0x153048), 1.f);

    // Header: dot + title, ✕
    const Rect header{ 1.f, 1.f, all.w - 2.f, 47.f };
    g.FillRound(header, 7.f, 7.f, 0.f, 0.f, Hex(0x05090F));
    g.FillRect({ header.x, header.B(), header.w, 1.f }, Palette::Divider);
    g.FillRound({ header.x + 16.f, header.CY() - 4.f, 8.f, 8.f }, 4.f, Palette::Teal);
    g.Text(Loc::Get("manual_update_header"), header.CutLeft(16.f + 18.f), Palette::Teal, Mono(13.f, true));
    if (TextButton(ui, ui.MakeId("close"), { header.R() - 16.f - 28.f, header.CY() - 14.f, 28.f, 28.f }, L"\x2715", Mono(12.f, true).WithFont(Font::Ui).Center(),
                   Palette::Dim, { Palette::ButtonBg, Palette::ButtonBorder, 1.f, 4.f, { 0.f }, 0.f, 28.f }) ||
        ui.KeyPressed(VK_ESCAPE)) {
        Close();
        return;
    }

    // Content
    float y = header.B() + 1.f + 16.f;
    const float x = 20.f, w = all.w - 40.f;
    y += Paragraph(ui, x, y, w, Loc::Get("manual_update_intro"), Mono(11.f), Hex(0x8AABBD)) + 14.f;

    const Color statusColor = failed ? Rgba(0xE6, 0x51, 0x00) : Palette::Teal;
    std::wstring title = Loc::Get("manual_update_checking"), message;
    if (failed) title = Loc::Get("manual_update_error");
    else if (release) {
        title = Loc::Fmt("manual_update_ready", L"v" + release->latestVersion);
        message = release->fileName + L"  |  " + Text::FileSize(release->fileSize);
    }
    TextStyle body = Mono(11.f);
    body.wrap = true;
    body.valign = VAlign::Top;
    const float textX = x + 17.f + 22.f + 14.f, textW = x + w - 17.f - textX;
    const float titleH = g.Measure(title, Mono(14.f, true)).h, messageH = message.empty() ? 0.f : g.Measure(message, body, textW).h;
    const Rect box{ x, y, w, 1.f + 12.f + std::max(titleH + messageH, 30.f) + 12.f + 1.f };
    g.Border(box, 4.f, Palette::HeaderBg, Palette::ButtonBorder, 1.f);
    g.Text(L"\xE895", { x + 17.f, box.y, 22.f, box.h }, statusColor, IconFont(22.f));
    const float top = box.CY() - (titleH + messageH) * 0.5f;
    g.Text(title, { textX, top, textW, titleH }, statusColor, Mono(14.f, true));
    if (!message.empty()) g.Text(message, { textX, top + titleH, textW, messageH }, Hex(0x8AABBD).WithAlpha(0.9f), body);
    y = box.B();
    if (downloading) {
        y += 14.f;
        const std::wstring text = Loc::Fmt("updates_downloading", percent);
        const float tw = g.Measure(text, Mono(11.f)).w + 1.f;
        const float lineH = g.Measure(L"Ag", Mono(11.f)).h;
        g.Text(text, { x + 2.f, y, tw, lineH }, Palette::Teal, Mono(11.f));
        ProgressBar(ui, { x + 2.f + tw + 12.f, y + lineH * 0.5f - 3.f, 220.f, 6.f }, percent / 100.f);
        ui.KeepAnimating();
    }

    // Footer: DOWNLOAD (teal), OPEN IN BROWSER
    const Rect footer{ 1.f, all.h - 61.f, all.w - 2.f, 60.f };
    g.FillRound(footer, 0.f, 0.f, 7.f, 7.f, Palette::HeaderBg);
    g.FillRect({ footer.x, footer.y, footer.w, 1.f }, Palette::Divider);
    const TextStyle text = Mono(11.f, true).Center();
    const std::wstring download = Loc::Get("manual_update_download_btn"), browser = Loc::Get("manual_update_browser_btn");
    const ButtonStyle dl{ Palette::Teal, Colors::Transparent, 0.f, 3.f, { 0.f }, 220.f, 36.f };
    const float dw = ButtonWidth(ui, download, text, dl);
    if (TextButton(ui, ui.MakeId("download"), { footer.x + 14.f, footer.CY() - 18.f, dw, 36.f }, download, text, Hex(0x060C12), dl,
                   release && !downloading)) {
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
    ButtonStyle bs = Styles::Footer();
    bs.minWidth = 140.f;
    const float bw = ButtonWidth(ui, browser, text, bs);
    if (TextButton(ui, ui.MakeId("browser"), { footer.x + 14.f + dw + 8.f, footer.CY() - 18.f, bw, 36.f }, browser, text, Palette::Dim, bs, !downloading)) {
        const std::wstring url = release && !Text::Trim(release->releaseUrl).empty() ? release->releaseUrl : AppVersion::GitHubRepoUrl() + L"/releases/latest";
        Process::OpenUrl(url);
    }
}

} // namespace SpeedrunLauncher::Windows
