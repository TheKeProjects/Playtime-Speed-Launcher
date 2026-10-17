// Hand Mods → install screen: download (padded to a 1.5 s minimum so the bar reads as progress),
// hand-colour conflict check against every installed mod, install, then a short hold.
#include <algorithm>
#include <cmath>
#include "Core/Async.h"
#include "Core/Text.h"
#include "Graphics/Animation.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "Windows/Dialogs/DialogWindow.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/HandMods/HandModsKit.h"
#include "Windows/Main/Overlays/HandMods/HandModsOverlay.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
using namespace HandKit;
namespace HM = Services::HandMods;

namespace {

constexpr double kDownloadFloor = 1.5;     // seconds
constexpr unsigned kInstallHoldMs = 700;   // extraction is near-instant: keep the text readable

std::wstring Downloading(int percent) { return L"Downloading\x2026 " + std::to_wstring(percent) + L"%"; }

} // namespace

void HandModsOverlay::StartInstall(const HM::Mod& mod) {
    if (!paks) return;
    GoTo(Screen::Installing);
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
            install.doneAt = Async::Now();
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
            ++listRevision;   // installed flags are re-read
            Async::After(kInstallHoldMs, [this, mod, ok] {
                install.progress = 100.f;
                install.error = !ok;
                install.text = ok ? L"\x2714 " + mod.name + L" installed successfully." : L"Error installing mod:\nCould not extract the files.";
                install.done = true;
                install.doneAt = Async::Now();
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
    AskConflict(mod.name, affected, names, [this, doInstall](bool proceed) {
        if (proceed) doInstall();
        else GoTo(Screen::Mods, -1);
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
    const double now = Async::Now();
    const bool success = install.done && !install.error;
    const float since = install.doneAt >= 0.0 ? static_cast<float>(now - install.doneAt) : 0.f;
    // Ring colour: teal while working, green once installed, red on an error (cross-faded)
    const float okT = ui.Animate(ui.MakeId("hand-install-ok"), success ? 1.f : 0.f, 8.f, 0.f);
    const float errT = ui.Animate(ui.MakeId("hand-install-err"), install.error ? 1.f : 0.f, 8.f, 0.f);
    const Color accent = Lerp(Lerp(Palette::Teal, kGreen, okT), Palette::DangerText, errT);
    const float shown = ui.Animate(ui.MakeId("hand-install-progress"), progress / 100.f, 10.f, 0.f);

    // Centred stack: ring (hand inside), name, status, Done
    TextStyle status = Mono(11.f);
    status.align = HAlign::Center;
    status.wrap = true;
    status.valign = VAlign::Top;
    const float statusH = g.Measure(install.text, status, 340.f).h;
    constexpr float kRing = 54.f;
    const float total = kRing * 2.f + 22.f + 20.f + 8.f + statusH + (install.done ? 22.f + 34.f : 0.f);
    float y = body.CY() - total * 0.5f;
    // An error shakes the ring for half a second
    const float shake = install.error ? std::sin(since * 42.f) * 7.f * std::max(0.f, 1.f - since / 0.5f) : 0.f;
    const float cx = body.CX() + shake, cy = y + kRing;

    // Soft glow and track
    g.FillEllipse(cx, cy, kRing + 14.f, kRing + 14.f, accent.WithAlpha(0.05f + 0.04f * okT));
    g.StrokeEllipse(cx, cy, kRing, kRing, Palette::ButtonBorder, 5.f);
    // Progress arc (segments with round caps), starting at the top
    constexpr int kSegments = 96;
    const int lit = static_cast<int>(std::round(shown * kSegments));
    const float step = 6.2831853f / kSegments;
    for (int i = 0; i < lit; ++i) {
        const float a0 = -1.5707963f + i * step, a1 = a0 + step;
        g.Line(cx + std::cos(a0) * kRing, cy + std::sin(a0) * kRing, cx + std::cos(a1) * kRing, cy + std::sin(a1) * kRing, accent, 5.f, true);
    }
    // Comet at the head of the arc while it's still filling
    if (!install.done && shown > 0.001f) {
        const float a = -1.5707963f + shown * 6.2831853f;
        g.FillEllipse(cx + std::cos(a) * kRing, cy + std::sin(a) * kRing, 7.f, 7.f, accent.WithAlpha(0.25f));
        g.FillEllipse(cx + std::cos(a) * kRing, cy + std::sin(a) * kRing, 3.5f, 3.5f, Colors::White.WithAlpha(0.85f));
    }
    // Waiting: a dashed orbit turning slowly inside the ring
    if (!install.done) {
        const float spin = static_cast<float>(std::fmod(now * 0.9, 6.2831853));
        for (int i = 0; i < 12; ++i) {
            const float a = spin + i * 0.5235988f;
            g.FillEllipse(cx + std::cos(a) * (kRing - 13.f), cy + std::sin(a) * (kRing - 13.f), 1.3f, 1.3f, accent.WithAlpha(0.25f + 0.05f * (i % 3)));
        }
    }

    // Centre: the hand bobbing and the percentage; on success a ✓ pops in with a burst of sparks
    if (success) {
        const float pop = Anim::EaseOutBack(since / 0.45f, 1.6f);
        const float burst = Anim::Clamp01(since / 0.8f);
        if (burst < 1.f) {
            for (int i = 0; i < 14; ++i) {
                const float a = i * 0.4487989f + 0.2f;
                const float d = kRing + 6.f + 40.f * Anim::EaseOutCubic(burst);
                const float r = 3.f * (1.f - burst);
                g.FillEllipse(cx + std::cos(a) * d, cy + std::sin(a) * d, r, r, (i % 2 ? kGreen : Palette::Teal).WithAlpha(1.f - burst));
            }
            g.StrokeEllipse(cx, cy, kRing + 30.f * Anim::EaseOutCubic(burst), kRing + 30.f * Anim::EaseOutCubic(burst), kGreen.WithAlpha(0.5f * (1.f - burst)),
                            2.f);
        }
        const float r = 30.f * pop;
        if (r > 0.5f) {
            g.FillEllipse(cx, cy, r, r, kGreen.WithAlpha(0.2f));
            g.StrokeEllipse(cx, cy, r, r, kGreen, 1.5f);
            g.Text(std::wstring(1, Glyphs::CheckMark), { cx - r, cy - r, r * 2.f, r * 2.f }, kGreen, IconFont(20.f * pop));
        }
    } else {
        const float bob = install.error ? 0.f : std::sin(static_cast<float>(now) * 3.2f) * 4.f;
        const float tilt = install.error ? 0.f : std::sin(static_cast<float>(now) * 1.6f) * 8.f;
        g.PushTransform(D2D1::Matrix3x2F::Rotation(tilt, D2D1::Point2F(cx, cy + 6.f)));
        g.Text(install.error ? std::wstring(1, Glyphs::Warning) : std::wstring(L"\x270B"), { cx - 30.f, cy - 34.f + bob, 60.f, 44.f }, accent,
               install.error ? IconFont(24.f) : Mono(28.f).WithFont(Font::Ui).Center());
        g.PopTransform();
        if (!install.error)
            g.Text(std::to_wstring(static_cast<int>(std::round(shown * 100.f))) + L"%", { cx - 40.f, cy + 12.f, 80.f, 18.f }, accent,
                   Mono(12.f, true).Center());
    }
    y += kRing * 2.f + 22.f;

    g.Text(install.modName, { body.x + 20.f, y, body.w - 40.f, 20.f }, Palette::Bright, Mono(14.f, true).Center().Ellipsis());
    y += 20.f + 8.f;
    g.Text(install.text, { body.CX() - 170.f, y, 340.f, statusH }, install.error ? Palette::DangerText : accent, status);
    y += statusH;
    if (install.done) {
        // Done rises in after the result
        const float rise = Anim::EaseOutBack(Anim::Clamp01((since - 0.25f) / 0.4f), 0.8f);
        y += 22.f + (1.f - rise) * 12.f;
        g.PushOpacity(Anim::Clamp01((since - 0.25f) / 0.25f));
        const ButtonStyle s{ Palette::ConfirmBg, Palette::ConfirmBorder, 1.f, 6.f, { 0.f }, 130.f, 34.f };
        const float w = ButtonWidth(ui, L"Done", Mono(11.f, true), s);
        const Rect b{ body.CX() - w * 0.5f, y, w, 34.f };
        if (success) g.GlowRound(b, 6.f, kGreen, 14.f, 0.25f + 0.15f * Anim::PingPong(now, 1.6) * Calm(since, 2.0));
        const bool clicked = TextButton(ui, ui.MakeId("hand-done"), b, L"Done", Mono(11.f, true).Center(), Palette::Teal, s);
        g.PopLayer();
        if (clicked) GoTo(Screen::Mods, -1);
    }
    // Orbit / bob while working; after the result only until the burst and the Done glow settle
    if (!install.done || since < 3.f) ui.KeepAnimatingThrottled();
}

} // namespace SpeedrunLauncher::Main
