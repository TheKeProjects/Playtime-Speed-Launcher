#include "Windows/Main/Overlays/LiveSplit/LiveSplitOverlay.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include "App/Paths.h"
#include "Core/Async.h"
#include "Graphics/Images.h"
#include "Localization/Localization.h"
#include "UI/Icons.h"
#include "UI/Layout.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Dialogs/DialogParts.h"
#include "Windows/Dialogs/DialogWindow.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
namespace LS = Services::LiveSplit;
namespace P = Dialogs::Parts;
namespace K = UI::MenuKit;

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

// The LiveSplit logo in a glowing badge with the state as a chip, the installed / latest versions
// as two cards (the latest lights up green when it's newer), the install folder, and the action
// (install / update / launch) as the footer's main button; a glowing bar while downloading
void LiveSplitOverlay::Draw(Ui& ui, const Rect& screen) {
    auto& g = ui.g;
    PanelStyle style;
    style.title = L"[ LIVESPLIT ]";
    OverlayFrame frame(ui, ui.MakeId("livesplit"), screen, 520.f, 0.f, style, popup.Opacity(), popup.Scale());
    const double start = frame.OpenedAt() + 0.15;
    Column col(frame.body.x + 26.f, frame.body.y + 22.f, frame.body.w - 52.f);
    const bool update = info && info->isUpdateAvailable;
    const Color stateColor = !info ? K::kDim : !info->isInstalled ? K::kAmber : update ? K::kGreen : Palette::Teal;
    int row = 0;

    // Logo + title + state
    {
        StaggerItem reveal(ui, start, row++, 0.f, 10.f);
        const Rect r = col.Next(56.f, 18.f);
        const Rect badge{ r.x, r.y, 56.f, 56.f };
        const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(ui.Time() * 2.4));
        g.GlowRound(badge, 14.f, stateColor, 18.f, 0.15f + 0.2f * pulse);
        g.Border(badge, 14.f, stateColor.WithAlpha(22.f / 255.f), stateColor.WithAlpha(120.f / 255.f), 1.f);
        {
            NoColorFilter keep(g);   // the logo keeps its own colours
            Icons::LiveSplit(g, badge.Inset(9.f).Offset(0.f, 1.5f * static_cast<float>(std::sin(ui.Time() * 2.4))));
        }
        ui.KeepAnimatingThrottled();
        const float x = badge.R() + 18.f;
        g.Text(L"LiveSplit", { x, r.y + 6.f, r.R() - x, 22.f }, K::kTitle, Mono(15.f, true));
        const char* subtitle = !info ? "livesplit_checking" : !info->isInstalled ? "livesplit_not_installed"
                             : update ? "livesplit_update_available" : "livesplit_up_to_date";
        const float cw = K::Chip(ui, x, r.y + 40.f, Loc::Get(subtitle), stateColor);
        if (!info) Form::Spinner(ui, x + cw + 14.f, r.y + 40.f, 6.f, K::kDim);
    }

    // Installed / latest
    {
        StaggerItem reveal(ui, start, row++, 0.f, 10.f);
        const Rect cards = col.Next(62.f, 12.f);
        const std::wstring installed = info && info->isInstalled ? (info->installedVersion.empty() ? L"\x2014" : info->installedVersion)
                                                                 : Loc::Get("livesplit_none");
        const std::wstring latest = info && !info->latestVersion.empty() ? info->latestVersion : L"\x2014";
        struct Card {
            const char* label;
            std::wstring value;
            wchar_t glyph;
            Color accent;
        };
        const Card list[] = { { "livesplit_installed_version", installed, Glyphs::Install, info && info->isInstalled ? Palette::Teal : K::kDim },
                              { "livesplit_latest_version", latest, Glyphs::Sync, update ? K::kGreen : Palette::Teal } };
        for (int i = 0; i < 2; ++i) {
            const Rect c = GridCell(cards, 2, i, 12.f);
            const Card& card = list[i];
            const bool lit = i == 1 && update;
            if (lit) g.GlowRound(c, 8.f, K::kGreen, 14.f, 0.2f + 0.15f * static_cast<float>(std::sin(ui.Time() * 2.4)));
            g.Border(c, 8.f, lit ? K::kGreen.WithAlpha(20.f / 255.f) : Palette::HeaderBg, lit ? K::kGreen.WithAlpha(0.6f) : Palette::ButtonBorder, 1.f);
            const Rect icon{ c.x + 12.f, c.CY() - 15.f, 30.f, 30.f };
            g.Border(icon, 8.f, card.accent.WithAlpha(20.f / 255.f), card.accent.WithAlpha(80.f / 255.f), 1.f);
            Glyph(ui, card.glyph, icon, 12.f, card.accent);
            const float tx = icon.R() + 12.f;
            g.Text(Loc::Get(card.label), { tx, c.CY() - 18.f, c.R() - tx - 8.f, 16.f }, K::kDim, Mono(9.5f).Ellipsis());
            g.Text(card.value, { tx, c.CY() + 1.f, c.R() - tx - 8.f, 18.f }, lit ? K::kGreen : K::kTitle, Mono(12.f, true).Ellipsis());
        }
    }

    // Install folder
    if (info && info->isInstalled && info->installPath && !info->installPath->empty()) {
        StaggerItem reveal(ui, start, row++, 0.f, 10.f);
        const Rect box = col.Next(34.f, 12.f);
        g.Border(box, 6.f, Palette::HeaderBg, Palette::ButtonBorder, 1.f);
        Glyph(ui, Glyphs::Folder, { box.x + 10.f, box.y, 18.f, box.h }, 11.f, Palette::Dim);
        g.Text(*info->installPath, box.Inset(Edges{ 36.f, 0.f, 12.f, 0.f }), Palette::Muted, Mono(10.f).Ellipsis());
    }

    // Download progress
    if (downloading) {
        StaggerItem reveal(ui, start, row++, 0.f, 10.f);
        col.Gap(2.f);
        const Rect line = col.Next(16.f, 8.f);
        g.Text(Loc::Fmt("livesplit_downloading", percent), line, Palette::Teal, Mono(10.f, true));
        K::GlowProgress(ui, ui.MakeId("livesplit-progress"), col.Next(8.f, 4.f), percent / 100.f);
    }
    frame.SetContentHeight(22.f + col.Height() + 12.f);

    // Footer: BACK on the left, the action on the right
    const std::wstring back = Loc::Get("back");
    const float bw = K::FooterButtonWidth(ui, back, Glyphs::Back, 110.f);
    if (K::FooterButton(ui, ui.MakeId("livesplit-close"), { frame.footer.x + 14.f, frame.footer.CY() - 18.f, bw, 36.f }, back, K::Kind::Secondary, true,
                        Glyphs::Back) ||
        (popup.IsOpen() && ui.KeyPressed(VK_ESCAPE)))
        popup.Close();
    if (info) {
        const char* key = !info->isInstalled ? "livesplit_install_btn" : update ? "livesplit_update_btn" : "livesplit_launch_btn";
        const wchar_t glyph = !info->isInstalled ? Glyphs::Download : update ? Glyphs::Sync : Glyphs::Play;
        const std::wstring action = Loc::Get(key);
        const float aw = K::FooterButtonWidth(ui, action, glyph, 180.f);
        if (K::FooterButton(ui, ui.MakeId("livesplit-action"), { frame.footer.R() - 14.f - aw, frame.footer.CY() - 18.f, aw, 36.f }, action,
                            K::Kind::Primary, !downloading, glyph, update ? K::kGreen : Palette::Teal))
            Act();
    }
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
    o.seasonal = true;
    state->dialog = Dialogs::Show(main.Hwnd(), o, [state](Ui& ui, const Rect& area) {
        auto& g = ui.g;
        const Page& page = kPages[state->page];
        const int last = static_cast<int>(std::size(kPages)) - 1;
        Column col(area);
        // Step dots, then the instruction
        {
            const Rect dots = col.Next(8.f, 12.f);
            for (int i = 0; i <= last; ++i) {
                const float on = ui.Animate(ui.MakeId("tcp-dot", i), i == state->page ? 1.f : 0.f, 14.f);
                g.FillRound({ dots.x + i * 30.f, dots.y, 8.f + 16.f * on, 8.f }, 4.f,
                            Lerp(Palette::ButtonBorder, Palette::Teal, std::max(on, i < state->page ? 0.6f : 0.f)));
            }
        }
        P::Label(ui, col, Loc::Get(page.step), 11.f, Palette::Teal, true, 10.f);
        if (auto img = Images::Load(Paths::Asset(std::wstring(L"Images\\Tutorial Live Split\\") + page.image))) {
            const float w = std::min({ 340.f, col.w, img->w });
            const float h = w * img->h / std::max(1.f, img->w);
            const Rect r = col.Next(h).CutRight(col.w - w);
            g.GlowRound(r, 8.f, Palette::Teal, 14.f, 0.2f);
            g.DrawImageRounded(*img, r, 8.f, 1.f, Stretch::Uniform);
            g.StrokeRound(r.Inset(0.5f), 7.5f, Palette::Teal.WithAlpha(0.4f), 1.f);
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
