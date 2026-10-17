// Hand Mods → install screen: download (padded to a 1.5 s minimum so the bar reads as progress),
// hand-colour conflict check against every installed mod, install, then a short hold.
#include <algorithm>
#include "Core/Async.h"
#include "Core/Text.h"
#include "Graphics/Animation.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "Windows/Dialogs/DialogWindow.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/HandMods/HandModsOverlay.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
namespace HM = Services::HandMods;

namespace {

constexpr double kDownloadFloor = 1.5;     // seconds
constexpr unsigned kInstallHoldMs = 700;   // extraction is near-instant: keep the text readable

std::wstring Downloading(int percent) { return L"Downloading\x2026 " + std::to_wstring(percent) + L"%"; }

} // namespace

void HandModsOverlay::StartInstall(const HM::Mod& mod) {
    if (!paks) return;
    screen = Screen::Installing;
    install = {};
    install.modName = mod.name;
    install.text = Downloading(0);
    const double started = Async::Now();
    const int number = chapter;
    Async::Run([this, mod, number] {
        return HM::Download(mod, number, [this](int p) {
            Async::Post([this, p] {
                if (install.animDuration > 0.0) return;
                install.progress = static_cast<float>(p);
                install.text = Downloading(p);
            });
        });
    }, [this, mod, started](std::optional<std::wstring> zip) {
        if (!zip) {
            install.error = true;
            install.text = L"Error installing mod:\nThe download failed.";
            install.done = true;
            return;
        }
        // Cached / tiny downloads finish instantly: animate the rest of the bar up to the floor
        const double remaining = std::max(0.0, kDownloadFloor - (Async::Now() - started));
        install.animFrom = install.progress;
        install.animStart = Async::Now();
        install.animDuration = std::max(remaining, 0.001);
        const std::wstring zipPath = *zip;
        Async::After(static_cast<unsigned>(remaining * 1000.0), [this, mod, zipPath] {
            install.animDuration = 0.0;
            install.progress = 100.f;
            install.text = Downloading(100);
            InstallDownloaded(mod, zipPath);
        });
    });
}

void HandModsOverlay::InstallDownloaded(const HM::Mod& mod, const std::wstring& zipPath) {
    const std::wstring dir = *paks;
    const std::vector<std::wstring> hands = HM::ReadDeclaredHands(zipPath).value_or(std::vector<std::wstring>{ HM::UnknownHand });

    // Every installed mod's hand marker (also catches installed Extras variants)
    std::vector<std::pair<std::wstring, std::vector<std::wstring>>> conflicting;
    for (const auto& marker : HM::AllInstalledHandMarkers(dir))
        if (_wcsicmp(marker.first.c_str(), mod.baseName.c_str()) != 0 && HM::HandsConflict(hands, marker.second)) conflicting.push_back(marker);

    const auto doInstall = [this, dir, zipPath, mod, conflicting] {
        install.text = L"Installing files\x2026";
        Async::Run([dir, zipPath, mod, conflicting] {
            for (const auto& c : conflicting) HM::UninstallByBaseName(dir, c.first);
            return HM::Install(dir, zipPath, mod.baseName);
        }, [this, mod](bool ok) {
            Async::After(kInstallHoldMs, [this, mod, ok] {
                install.progress = 100.f;
                install.error = !ok;
                install.text = ok ? L"\x2714 " + mod.name + L" installed successfully." : L"Error installing mod:\nCould not extract the files.";
                install.done = true;
            });
        });
    };
    if (conflicting.empty()) {
        doInstall();
        return;
    }

    // "⚠ X changes the Blue hand. Y will be removed first."
    std::vector<std::wstring> names, shared;
    for (const auto& c : conflicting) names.push_back(c.first);
    for (const auto& h : hands) {
        for (const auto& c : conflicting) {
            if (std::any_of(c.second.begin(), c.second.end(), [&](const std::wstring& o) { return _wcsicmp(o.c_str(), h.c_str()) == 0; })) {
                shared.push_back(h);
                break;
            }
        }
    }
    const std::vector<std::wstring>& affected = shared.empty() ? hands : shared;
    Dialogs::Options o;
    o.title = L"Hand Conflict";
    o.primary = L"Continue";
    o.close = L"Cancel";
    Dialogs::ShowText(main.Hwnd(), o,
                      L"\x26A0 " + mod.name + L" changes the " + Text::Join(affected, L", ") + L" hand" + (affected.size() == 1 ? L"" : L"s") + L".\n" +
                          Text::Join(names, L", ") + L" will be removed first.",
                      Rgba(160, 180, 200, 200), [this, doInstall](Dialogs::Result r) {
                          if (r == Dialogs::Result::Primary) doInstall();
                          else screen = Screen::Mods;
                      });
}

void HandModsOverlay::DrawInstalling(Ui& ui, const Rect& body) {
    auto& g = ui.g;
    float progress = install.progress;
    if (install.animDuration > 0.0) {
        const float t = static_cast<float>(std::min(1.0, (Async::Now() - install.animStart) / install.animDuration));
        progress = install.animFrom + (100.f - install.animFrom) * t;
        install.text = Downloading(static_cast<int>(progress));
        ui.KeepAnimatingThrottled();
    }
    // Centred stack: name, 320×8 bar, status, Done
    const float nameH = g.Measure(L"Ag", Mono(14.f, true)).h;
    TextStyle status = Mono(11.f);
    status.align = HAlign::Center;
    status.wrap = true;
    status.valign = VAlign::Top;
    const float statusH = g.Measure(install.text, status, 320.f).h;
    const float total = nameH + 16.f + 8.f + 10.f + statusH + (install.done ? 18.f + 34.f : 0.f);
    float y = body.CY() - total * 0.5f;
    g.Text(install.modName, { body.x, y, body.w, nameH }, Palette::Bright, Mono(14.f, true).Center());
    y += nameH + 16.f;
    ProgressBar(ui, { body.CX() - 160.f, y, 320.f, 8.f }, progress / 100.f);
    y += 8.f + 10.f;
    g.Text(install.text, { body.CX() - 160.f, y, 320.f, statusH }, install.error ? Rgba(204, 51, 51, 220) : Palette::Teal, status);
    y += statusH;
    if (install.done) {
        y += 18.f;
        const ButtonStyle s{ Palette::ConfirmBg, Palette::ConfirmBorder, 1.f, 3.f, { 0.f }, 120.f, 34.f };
        const float w = ButtonWidth(ui, L"Done", Mono(11.f, true), s);
        if (TextButton(ui, ui.MakeId("hand-done"), { body.CX() - w * 0.5f, y, w, 34.f }, L"Done", Mono(11.f, true).Center(), Palette::Teal, s))
            screen = Screen::Mods;
    }
}

} // namespace SpeedrunLauncher::Main
