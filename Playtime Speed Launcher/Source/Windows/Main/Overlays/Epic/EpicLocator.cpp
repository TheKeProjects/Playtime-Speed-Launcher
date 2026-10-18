#include "Windows/Main/Overlays/Epic/EpicLocator.h"
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include "App/Launcher.h"
#include "App/Paths.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/System.h"
#include "Graphics/Animation.h"
#include "Graphics/Images.h"
#include "Localization/Localization.h"
#include "UI/Style.h"
#include "Windows/Dialogs/DialogParts.h"
#include "Windows/Dialogs/DialogWindow.h"

namespace SpeedrunLauncher::Main::EpicLocator {

using namespace UI;
namespace P = Dialogs::Parts;
using Stage = Services::EpicGamesService::DetectStage;

namespace {

constexpr Color kTeal = Rgba(0, 204, 170);
constexpr Color kGreen = Hex(0x00CC88);
constexpr Color kAmber = Rgba(255, 176, 32);
constexpr Color kDim = Rgba(120, 160, 190, 160);
constexpr float kWidth = 420.f;
constexpr double kMinSearch = 4.5;        // the search always stays on screen at least this long
constexpr unsigned kFoundHoldMs = 1400;   // the success burst plays before the dialog closes

// Written by the search worker, read by the dialog every frame
struct SearchState {
    Async::Cancel cancel;
    std::atomic<int> stage{ 0 };      // Stage
    std::atomic<int> scanned{ 0 };    // folders read so far
    double startedAt = 0.0;
    double foundAt = -1.0;            // UI thread: when the folder turned up
    std::mutex mutex;
    std::wstring folder;              // the folder being read right now (the found one at the end)

    void Report(Stage s, const std::wstring& dir) {
        stage = static_cast<int>(s);
        ++scanned;
        SetFolder(dir);
    }
    void SetFolder(const std::wstring& dir) {
        std::lock_guard lock(mutex);
        folder = dir;
    }
    std::wstring Folder() {
        std::lock_guard lock(mutex);
        return folder;
    }
};

std::wstring Elapsed(double seconds) {
    const int s = static_cast<int>(seconds);
    wchar_t buf[16];
    swprintf_s(buf, L"%02d:%02d", s / 60, s % 60);
    return buf;
}

void DrawEpicLogo(Ui& ui, const Rect& r, float radius, Color accent) {
    auto& g = ui.g;
    if (auto logo = Images::Load(Paths::Asset(L"Images\\Epic.png"), 128)) {
        g.DrawImageRounded(*logo, r, radius);
        g.StrokeRound(r.Inset(0.5f), radius - 0.5f, accent.WithAlpha(0.6f), 1.f);
    } else {
        g.Border(r, radius, accent.WithAlpha(0.1f), accent.WithAlpha(0.5f), 1.f);
        g.Text(L"E", r, accent, Mono(22.f, true).Center());
    }
}

// Header of the "not found" dialog: the Epic logo in a glowing badge, a heading and a subtitle.
void Hero(Ui& ui, Column& col, const std::wstring& heading, const std::wstring& subtitle, Color accent) {
    auto& g = ui.g;
    const Rect r = col.Next(56.f, 18.f);
    const Rect badge{ r.x + 4.f, r.y + 4.f, 48.f, 48.f };
    const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(ui.Time() * 2.4));
    g.GlowRound(badge, 12.f, accent, 18.f, 0.12f + 0.18f * pulse);
    DrawEpicLogo(ui, badge, 12.f, accent);
    const float tx = badge.R() + 22.f;
    g.Text(heading, { tx, r.y + 9.f, r.R() - tx, 20.f }, accent, Mono(14.f, true));
    g.Text(subtitle, { tx, r.y + 31.f, r.R() - tx, 16.f }, kDim, Mono(10.f).Ellipsis());
    ui.KeepAnimatingThrottled();
}

// Same ring as the hand-mods install: a track with an arc filling from the top and a comet at its
// head, a dashed orbit turning inside and the Epic logo bobbing in the middle. There's no real
// total, so the arc creeps towards 90 % while searching and closes once the folder turns up, with
// a burst of sparks and a ✓ on the logo.
float DrawSearching(Ui& ui, const Rect& area, SearchState& state) {
    auto& g = ui.g;
    const double now = Async::Now();
    const bool found = state.foundAt >= 0.0;
    const float since = found ? static_cast<float>(now - state.foundAt) : 0.f;
    const float okT = ui.Animate(ui.MakeId("epic-search-ok"), found ? 1.f : 0.f, 8.f, 0.f);
    const Color accent = Lerp(kTeal, kGreen, okT);
    const float elapsed = static_cast<float>(now - state.startedAt);
    const float target = found ? 1.f : 0.9f * (1.f - std::exp(-elapsed / 2.2f));
    const float shown = ui.Animate(ui.MakeId("epic-search-progress"), target, 6.f, 0.f);

    Column col(area);
    constexpr float kRing = 54.f;
    const Rect ringRow = col.Next(kRing * 2.f + 36.f, 14.f);
    const float cx = ringRow.CX(), cy = ringRow.y + 18.f + kRing;

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
    if (!found) {
        // Comet at the head of the arc
        if (shown > 0.001f) {
            const float a = -1.5707963f + shown * 6.2831853f;
            g.FillEllipse(cx + std::cos(a) * kRing, cy + std::sin(a) * kRing, 7.f, 7.f, accent.WithAlpha(0.25f));
            g.FillEllipse(cx + std::cos(a) * kRing, cy + std::sin(a) * kRing, 3.5f, 3.5f, Colors::White.WithAlpha(0.85f));
        }
        // A dashed orbit turning slowly inside the ring
        const float spin = static_cast<float>(std::fmod(now * 0.9, 6.2831853));
        for (int i = 0; i < 12; ++i) {
            const float a = spin + i * 0.5235988f;
            g.FillEllipse(cx + std::cos(a) * (kRing - 13.f), cy + std::sin(a) * (kRing - 13.f), 1.3f, 1.3f, accent.WithAlpha(0.25f + 0.05f * (i % 3)));
        }
    } else {
        // A burst of sparks and an expanding ring
        const float burst = Anim::Clamp01(since / 0.8f);
        if (burst < 1.f) {
            for (int i = 0; i < 14; ++i) {
                const float a = i * 0.4487989f + 0.2f;
                const float d = kRing + 6.f + 40.f * Anim::EaseOutCubic(burst);
                const float r = 3.f * (1.f - burst);
                g.FillEllipse(cx + std::cos(a) * d, cy + std::sin(a) * d, r, r, (i % 2 ? kGreen : kTeal).WithAlpha(1.f - burst));
            }
            const float grow = kRing + 30.f * Anim::EaseOutCubic(burst);
            g.StrokeEllipse(cx, cy, grow, grow, kGreen.WithAlpha(0.5f * (1.f - burst)), 2.f);
        }
    }

    // Centre: the Epic logo bobbing and tilting while it searches, settling (with a small pop) once found
    const float calm = 1.f - okT;
    const float bob = std::sin(static_cast<float>(now) * 3.2f) * 4.f * calm;
    const float tilt = std::sin(static_cast<float>(now) * 1.6f) * 8.f * calm;
    const float pop = found ? 1.f + 0.12f * std::sin(Anim::Clamp01(since / 0.35f) * 3.1415927f) : 1.f;
    const float half = 24.f * pop;
    const Rect logo{ cx - half, cy - half + bob, half * 2.f, half * 2.f };
    g.PushTransform(D2D1::Matrix3x2F::Rotation(tilt, D2D1::Point2F(cx, cy)));
    g.GlowRound(logo, 11.f, accent, 14.f, 0.25f + 0.1f * static_cast<float>(std::sin(now * 2.4)));
    DrawEpicLogo(ui, logo, 11.f, accent);
    g.PopTransform();
    // ✓ badge popping onto the logo's corner
    if (found) {
        const float r = 10.f * Anim::EaseOutBack(since / 0.45f, 1.6f);
        if (r > 1.5f) {
            const float bx = logo.R() - 2.f, by = logo.B() - 2.f;
            g.FillEllipse(bx, by, r, r, Hex(0x09141E));
            g.FillEllipse(bx, by, r - 1.5f, r - 1.5f, kGreen);
            g.Text(std::wstring(1, Glyphs::CheckMark), { bx - r, by - r, r * 2.f, r * 2.f }, Hex(0x03110D), IconFont(9.f * r / 10.f));
        }
    }

    // Heading and what it's doing right now
    g.Text(Loc::Get(found ? "epic_search_found" : "epic_search_heading"), col.Next(20.f, 4.f), accent, Mono(14.f, true).Center());
    const bool drives = state.stage.load() == static_cast<int>(Stage::Drives);
    const std::wstring folder = state.Folder();
    std::wstring status = Loc::Get(found ? "epic_search_found_sub" : drives ? "epic_search_step_drives" : "epic_search_step_manifests");
    if (!found && drives && folder.size() >= 3) status += L"  \x00B7  " + folder.substr(0, 3);
    g.Text(status, col.Next(16.f, 16.f), kDim, Mono(10.f).Center().Ellipsis());

    // Folder being read right now (the found folder at the end)
    g.Text(Loc::Get(found ? "epic_search_found_label" : "epic_search_current"), col.Next(14.f, 6.f), Rgba(58, 106, 138, 220), Mono(9.f, true));
    const Rect box = col.Next(30.f, 10.f);
    g.Border(box, 3.f, Rgba(6, 15, 24), found ? kGreen.WithAlpha(0.4f) : Rgba(13, 37, 53), 1.f);
    g.Text(std::wstring(1, Glyphs::Folder), { box.x + 6.f, box.y, 22.f, box.h }, accent.WithAlpha(0.7f), IconFont(11.f));
    g.Text(folder.empty() ? L"\x2026" : folder, { box.x + 30.f, box.y, box.w - 40.f, box.h }, Rgba(160, 190, 220, 220), Mono(10.f).Ellipsis());

    // Folders checked · elapsed time
    const Rect foot = col.Next(14.f);
    g.Text(std::to_wstring(state.scanned.load()) + L" " + Loc::Get("epic_search_folders"), foot, kDim, Mono(9.f));
    g.Text(Elapsed((found ? state.foundAt : now) - state.startedAt), foot, kDim, Mono(9.f).Right());
    ui.KeepAnimatingThrottled();
    return col.Height();
}

float DrawNotFound(Ui& ui, const Rect& area) {
    Column col(area);
    Hero(ui, col, Loc::Get("epic_not_found_heading"), Loc::Get("epic_not_found_sub"), kAmber);
    P::Label(ui, col, Loc::Get("epic_not_found_content"), 11.f, Rgba(160, 190, 220, 220), false, 14.f);
    P::Label(ui, col, Loc::Get("epic_not_found_pick"), 9.f, Rgba(58, 106, 138, 220), true, 6.f);
    P::Box(ui, col, L"\x2026\\Epic Games\\PoppyPlaytimeChapterOne", 10.f, Rgba(0, 204, 170, 14), Rgba(0, 204, 170, 70), kTeal, { 10.f, 8.f },
           8.f, true);
    P::Label(ui, col, Loc::Get("epic_not_found_hint"), 10.f, kDim);
    return col.Height();
}

void Search(HWND owner, std::function<void()> located);

void PickFolder(HWND owner, const std::function<void()>& located) {
    const std::wstring picked = System::PickFolder(owner, L"");
    if (picked.empty()) return;
    // Accept PoppyPlaytimeChapterOne itself or its parent folder
    const std::wstring sub = Files::Combine(picked, L"PoppyPlaytimeChapterOne");
    App::L().epic.SetBasePath(Files::DirectoryExists(sub) ? sub : picked);
    if (located) located();
}

void ShowNotFound(HWND owner, std::function<void()> located) {
    Dialogs::Options options;
    options.title = L"Epic Games";
    options.primary = Loc::Get("select_folder");
    options.secondary = Loc::Get("epic_search_again");
    options.close = Loc::Get("cancel");
    options.contentWidth = kWidth;
    options.seasonal = true;
    Dialogs::Show(owner, options, [](Ui& ui, const Rect& area) { return DrawNotFound(ui, area); },
                  [owner, located](Dialogs::Result result) {
                      if (result == Dialogs::Result::Primary) PickFolder(owner, located);
                      else if (result == Dialogs::Result::Secondary) Search(owner, located);
                  });
}

void Search(HWND owner, std::function<void()> located) {
    auto state = std::make_shared<SearchState>();
    state->startedAt = Async::Now();
    Dialogs::Options options;
    options.title = L"Epic Games";
    options.secondary = Loc::Get("select_folder_manually");
    options.close = Loc::Get("cancel");
    options.contentWidth = kWidth;
    options.seasonal = true;
    options.live = true;
    // Cancel / Esc / "select manually" stop the search; the worker notices and finishes quietly
    const int dialog = Dialogs::Show(owner, options, [state](Ui& ui, const Rect& area) { return DrawSearching(ui, area, *state); },
                                     [owner, state, located](Dialogs::Result result) {
                                         state->cancel.Request();
                                         if (result == Dialogs::Result::Secondary) PickFolder(owner, located);
                                     });
    Async::Run([state] {
        return App::L().epic.TryAutoDetect(state->cancel, [&state](Stage stage, const std::wstring& dir) { state->Report(stage, dir); });
    }, [owner, dialog, state, located](std::optional<std::wstring> found) {
        // Keep the search on screen for a moment even when it was instant
        const double left = kMinSearch - (Async::Now() - state->startedAt);
        Async::After(left > 0.0 ? static_cast<unsigned>(left * 1000.0) : 0u, [owner, dialog, state, located, found] {
            if (state->cancel.Requested()) return;
            if (!found) {
                Dialogs::Close(dialog);
                ShowNotFound(owner, located);
                return;
            }
            // Show where it was, play the success burst, then switch to Epic
            state->SetFolder(*found);
            state->foundAt = Async::Now();
            App::L().epic.SetBasePath(found);
            Async::After(kFoundHoldMs, [dialog, state, located] {
                if (state->cancel.Requested()) return;
                Dialogs::Close(dialog);
                if (located) located();
            });
        });
    });
}

} // namespace

void Start(HWND owner, std::function<void()> located) { Search(owner, std::move(located)); }

} // namespace SpeedrunLauncher::Main::EpicLocator
