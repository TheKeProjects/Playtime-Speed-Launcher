#include "Windows/Main/Overlays/LiveSplit/LiveSplitOverlay.h"
#include <algorithm>
#include <memory>
#include "App/Paths.h"
#include "Core/Async.h"
#include "Graphics/Images.h"
#include "Localization/Localization.h"
#include "UI/Icons.h"
#include "UI/Layout.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "Windows/Dialogs/DialogParts.h"
#include "Windows/Dialogs/DialogWindow.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
namespace LS = Services::LiveSplit;
namespace P = Dialogs::Parts;

void LiveSplitOverlay::StartupCheck() {
    Async::Run([] { return LS::Check(); }, [this](LS::Info result) {
        info = result;
        if (result.isUpdateAvailable) main.SetLiveSplitUpdateAlert(true);
    });
}

void LiveSplitOverlay::Act() {
    if (downloading || !info) return;
    if (info->isInstalled && !info->isUpdateAvailable) {
        if (info->installPath) LS::Launch(*info->installPath);
        popup.Close();
        return;
    }
    downloading = true;
    percent = 0;
    const LS::Info current = *info;
    Async::Run([this, current] {
        const bool ok = LS::DownloadAndInstall(current, LS::DefaultInstallDir(), [this](int p) { Async::Post([this, p] { percent = p; }); });
        return ok ? std::optional<LS::Info>(LS::Check()) : std::nullopt;
    }, [this](std::optional<LS::Info> fresh) {
        downloading = false;
        if (!fresh) return;
        info = fresh;
        main.SetLiveSplitUpdateAlert(false);
    });
}

void LiveSplitOverlay::Draw(Ui& ui, const Rect& screen) {
    auto& g = ui.g;
    PanelStyle style;
    style.title = L"[ LIVESPLIT ]";
    OverlayFrame frame(ui, ui.MakeId("livesplit"), screen, 500.f, 0.f, style, popup.Opacity(), popup.Scale());
    Column col(frame.body.x + 28.f, frame.body.y + 22.f, frame.body.w - 56.f);

    // Logo + title
    {
        const float titleH = g.Measure(L"Ag", Mono(15.f, true)).h, subH = g.Measure(L"Ag", Mono(11.f)).h;
        const Rect row = col.Next(std::max(44.f, titleH + 4.f + subH), 18.f);
        Icons::LiveSplit(g, { row.x, row.CY() - 22.f, 44.f, 44.f });
        const float x = row.x + 44.f + 16.f, top = row.CY() - (titleH + 4.f + subH) * 0.5f;
        g.Text(L"LiveSplit", { x, top, row.R() - x, titleH }, Palette::Teal, Mono(15.f, true));
        const char* subtitle = !info ? "livesplit_checking" : !info->isInstalled ? "livesplit_not_installed"
                             : info->isUpdateAvailable ? "livesplit_update_available" : "livesplit_up_to_date";
        g.Text(Loc::Get(subtitle), { x, top + titleH + 4.f, row.R() - x, subH }, Palette::Dim, Mono(11.f));
    }

    // Installed / latest
    {
        const float labelH = g.Measure(L"Ag", Mono(10.f)).h, valueH = g.Measure(L"Ag", Mono(12.f, true)).h;
        const Rect box = col.Next(1.f + 10.f + labelH + 3.f + valueH + 10.f + 1.f, 10.f);
        g.Border(box, 3.f, Palette::HeaderBg, Palette::ButtonBorder, 1.f);
        const Rect inner = box.Inset(15.f, 11.f);
        const std::wstring installed = info && info->isInstalled ? (info->installedVersion.empty() ? L"\x2014" : info->installedVersion)
                                                                 : Loc::Get("livesplit_none");
        const std::wstring latest = info && !info->latestVersion.empty() ? info->latestVersion : L"\x2014";
        const std::pair<const char*, std::wstring> columns[] = { { "livesplit_installed_version", installed }, { "livesplit_latest_version", latest } };
        for (int i = 0; i < 2; ++i) {
            const Rect cell = GridCell(inner, 2, i, 0.f);
            g.Text(Loc::Get(columns[i].first), { cell.x, cell.y, cell.w, labelH }, Hex(0x2A4A6A), Mono(10.f));
            g.Text(columns[i].second, { cell.x, cell.y + labelH + 3.f, cell.w, valueH }, Palette::Label, Mono(12.f, true));
        }
    }

    // Install path
    if (info && info->isInstalled && info->installPath && !info->installPath->empty()) {
        const float h = g.Measure(L"Ag", Mono(10.f)).h;
        const Rect box = col.Next(1.f + 8.f + h + 8.f + 1.f, 12.f);
        g.Border(box, 3.f, Palette::HeaderBg, Palette::ButtonBorder, 1.f);
        g.Text(*info->installPath, box.Inset(15.f, 9.f), Palette::Muted, Mono(10.f).Ellipsis());
    }

    // Action / progress
    if (downloading) {
        col.Gap(4.f);
        const float h = g.Measure(L"Ag", Mono(11.f)).h;
        g.Text(Loc::Fmt("livesplit_downloading", percent), col.Next(h, 8.f), Palette::Teal, Mono(11.f));
        ProgressBar(ui, col.Next(6.f), percent / 100.f);
        ui.KeepAnimatingThrottled();
    } else if (info) {
        col.Gap(4.f);
        const char* key = !info->isInstalled ? "livesplit_install_btn" : info->isUpdateAvailable ? "livesplit_update_btn" : "livesplit_launch_btn";
        ButtonStyle s{ Palette::ConfirmBg, Palette::ConfirmBorder, 1.f, 3.f, { 0.f }, 0.f, 40.f };
        if (TextButton(ui, ui.MakeId("livesplit-action"), col.Next(40.f), Loc::Get(key), Mono(11.f, true).Center(), Palette::Teal, s)) Act();
    }
    frame.SetContentHeight(22.f + col.Height() + 18.f);

    const std::wstring back = Loc::Get("back");
    const TextStyle text = Mono(11.f, true).Center();
    const float w = ButtonWidth(ui, back, text, Styles::Footer());
    if (TextButton(ui, ui.MakeId("livesplit-close"), { frame.footer.R() - 14.f - w, frame.footer.CY() - 18.f, w, 36.f }, back, text, Palette::Dim,
                   Styles::Footer()))
        popup.Close();
}

void LiveSplitOverlay::ShowTcpNotice() {
    struct Page {
        const char* step;
        const wchar_t* image;
        const char* footer;
    };
    static constexpr Page kPages[] = { { "livesplit_tcp_step1", L"1.png", nullptr }, { "livesplit_tcp_step2", L"2.png", "livesplit_tcp_reminder" } };
    struct State {
        int page = 0;
        int dialog = 0;
    };
    auto state = std::make_shared<State>();
    Dialogs::Options o;
    o.title = Loc::Get("livesplit_tcp_title");
    o.contentWidth = 370.f;
    state->dialog = Dialogs::Show(main.Hwnd(), o, [state](Ui& ui, const Rect& area) {
        auto& g = ui.g;
        const Page& page = kPages[state->page];
        const int last = static_cast<int>(std::size(kPages)) - 1;
        Column col(area);
        P::Label(ui, col, Loc::Get(page.step), 11.f, Palette::Teal, false, 10.f);
        if (auto img = Images::Load(Paths::Asset(std::wstring(L"Images\\Tutorial Live Split\\") + page.image))) {
            const float w = std::min({ 340.f, col.w, img->w });
            const float h = w * img->h / std::max(1.f, img->w);
            g.DrawImage(*img, col.Next(h).CutRight(col.w - w));
        }
        if (page.footer) {
            col.Gap(12.f);
            P::Label(ui, col, Loc::Get(page.footer), 10.f, Rgba(120, 160, 190, 150));
        }
        // Prev · n / N · Next / Done
        col.Gap(16.f);
        const Rect nav = col.Next(28.f);
        const std::wstring prev = Loc::Get("steam_tutorial_prev");
        const std::wstring next = Loc::Get(state->page == last ? "steam_tutorial_done" : "steam_tutorial_next");
        const float pw = P::SmallButtonWidth(ui, prev), nw = P::SmallButtonWidth(ui, next);
        if (P::SmallButton(ui, ui.MakeId("tcp-prev"), { nav.x, nav.y, pw, 28.f }, prev, Palette::Label, state->page > 0)) --state->page;
        g.Text(std::to_wstring(state->page + 1) + L" / " + std::to_wstring(last + 1), nav, Palette::Label, Mono(10.f).Center());
        if (P::SmallButton(ui, ui.MakeId("tcp-next"), { nav.R() - nw, nav.y, nw, 28.f }, next, Palette::Teal)) {
            if (state->page < last) ++state->page;
            else Dialogs::Close(state->dialog);
        }
        return col.Height();
    });
}

} // namespace SpeedrunLauncher::Main
