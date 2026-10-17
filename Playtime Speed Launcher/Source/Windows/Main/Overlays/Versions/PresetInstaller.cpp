#include "Windows/Main/Overlays/Versions/PresetInstaller.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <set>
#include "App/Launcher.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Graphics/Images.h"
#include "Localization/Localization.h"
#include "Services/Discord/DiscordPresenceService.h"
#include "Services/Platforms/SteamCmdRunner.h"
#include "Services/Platforms/SteamCredentialStore.h"
#include "Services/Platforms/SteamDetector.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/Form.h"
#include "UI/Window.h"
#include "Windows/Dialogs/DialogParts.h"
#include "Windows/Dialogs/DialogWindow.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
namespace P = Dialogs::Parts;
namespace SteamCmd = Services::SteamCmd;

namespace {

constexpr Color kTeal = Rgba(0, 204, 170);
constexpr Color kDim = Rgba(120, 160, 190, 160);

std::wstring Elapsed(double seconds) {
    const int s = static_cast<int>(seconds);
    wchar_t buf[16];
    swprintf_s(buf, L"%02d:%02d", s / 60, s % 60);
    return buf;
}

// Two-line description card (install mode AUTO / MANUAL).
void ModeCard(Ui& ui, Column& col, const std::wstring& title, const std::wstring& desc, Color accent, Color titleColor, float marginBottom) {
    TextStyle body = Mono(10.f);
    body.wrap = true;
    body.valign = VAlign::Top;
    const float inner = col.w - 22.f;
    const float titleH = ui.g.Measure(title, Mono(11.f, true)).h;
    const float descH = ui.g.Measure(desc, body, inner).h;
    const Rect r = col.Next(1.f + 8.f + titleH + 3.f + descH + 8.f + 1.f, marginBottom);
    ui.g.Border(r, 3.f, accent.WithAlpha(20.f / 255.f), accent.WithAlpha(50.f / 255.f), 1.f);
    ui.g.Text(title, { r.x + 11.f, r.y + 9.f, inner, titleH }, titleColor, Mono(11.f, true));
    ui.g.Text(desc, { r.x + 11.f, r.y + 9.f + titleH + 3.f, inner, descH }, kDim, body);
}

// SteamCMD dialogs header: glowing badge with a glyph, "STEAMCMD" and a subtitle.
void SteamCmdHero(Ui& ui, Column& col, wchar_t glyph, const std::wstring& subtitle, float marginBottom) {
    auto& g = ui.g;
    const Rect r = col.Next(48.f, marginBottom);
    const Rect badge{ r.x, r.y, 48.f, 48.f };
    const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(ui.Time() * 2.4));
    g.GlowRound(badge, 12.f, kTeal, 18.f, 0.15f + 0.2f * pulse);
    g.Border(badge, 12.f, Rgba(0, 204, 170, 28), Rgba(0, 204, 170, 130), 1.f);
    // The glyph bobs gently
    g.Text(std::wstring(1, glyph), badge.Offset(0.f, 2.f * static_cast<float>(std::sin(ui.Time() * 2.4))), kTeal, IconFont(20.f));
    g.Text(L"STEAMCMD", { badge.R() + 14.f, r.y + 5.f, r.w - 62.f, 20.f }, Rgba(214, 228, 238), Mono(14.f, true));
    g.Text(subtitle, { badge.R() + 14.f, r.y + 26.f, r.w - 62.f, 16.f }, kDim, Mono(10.f));
    ui.KeepAnimatingThrottled();
}

// Row of small pills ("~4 MB", "Official from Valve"...)
void InfoChips(Ui& ui, Column& col, const std::vector<std::wstring>& chips, float marginBottom) {
    const TextStyle style = Mono(9.f, true).Center();
    const Rect r = col.Next(22.f, marginBottom);
    float x = r.x;
    for (const auto& chip : chips) {
        const float w = ui.g.Measure(chip, style).w + 18.f;
        if (x + w > r.R()) break;
        ui.g.Border({ x, r.y, w, r.h }, 11.f, Rgba(0, 204, 170, 16), Rgba(0, 204, 170, 60), 1.f);
        ui.g.Text(chip, { x, r.y, w, r.h }, Rgba(120, 210, 190), style);
        x += w + 6.f;
    }
}

// Written by the download worker, read by the dialog every frame
struct SteamCmdDownloadState {
    std::atomic<int> stage{ 0 };   // SteamCmd::DownloadStage
    std::atomic<uint64_t> received{ 0 }, total{ 0 };
    Async::Cancel cancel;
    double startedAt = 0.0;
};

std::wstring Megabytes(uint64_t bytes) {
    wchar_t buf[32];
    swprintf_s(buf, L"%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    return buf;
}

float DrawSteamCmdDownload(Ui& ui, const Rect& area, const SteamCmdDownloadState& state) {
    auto& g = ui.g;
    Column col(area);
    SteamCmdHero(ui, col, Glyphs::Download, Loc::Get("steamcmd_hero_sub"), 18.f);

    const int stage = state.stage.load();
    const uint64_t received = state.received.load(), total = state.total.load();
    // Steps: done (✓), running (spinner), waiting (ring)
    const std::wstring steps[] = { Loc::Get("steamcmd_step_download"), Loc::Get("steamcmd_step_extract"), Loc::Get("steamcmd_step_ready") };
    for (int i = 0; i < 3; ++i) {
        const Rect row = col.Next(24.f, 4.f);
        const float cx = row.x + 10.f, cy = row.CY();
        const bool done = i < stage, running = i == stage;
        const float lit = ui.Animate(ui.MakeId("steamcmd-step", i), done || running ? 1.f : 0.f, 10.f);
        if (done) {
            g.FillEllipse(cx, cy, 9.f, 9.f, Rgba(0, 204, 170, 40));
            g.StrokeEllipse(cx, cy, 9.f, 9.f, kTeal, 1.f);
            g.Text(std::wstring(1, Glyphs::CheckMark), { cx - 9.f, cy - 9.f, 18.f, 18.f }, kTeal, IconFont(9.f));
        } else if (running) {
            Form::Spinner(ui, cx, cy, 8.f, kTeal);
        } else {
            g.StrokeEllipse(cx, cy, 8.f, 8.f, Rgba(58, 106, 138, 140), 1.f);
        }
        const Color text = Lerp(Rgba(90, 120, 145), done ? Rgba(120, 200, 180) : Rgba(214, 228, 238), lit);
        g.Text(steps[i], { row.x + 30.f, row.y, row.w - 30.f, row.h }, text, Mono(11.f, running));
        if (i < 2) g.FillRect({ cx - 0.5f, cy + 10.f, 1.f, 8.f }, done ? Rgba(0, 204, 170, 120) : Rgba(58, 106, 138, 70));
    }
    col.Gap(12.f);

    // Progress bar: determinate while the size is known, marquee otherwise
    const Rect bar = col.Next(8.f, 8.f);
    const bool known = stage == 0 && total > 0;
    if (known) {
        const float target = static_cast<float>(static_cast<double>(received) / static_cast<double>(total));
        const float shown = ui.Animate(ui.MakeId("steamcmd-progress"), target, 10.f, 0.f);
        g.FillRound(bar, 4.f, Hex(0x0D2535));
        if (shown > 0.f) {
            const Rect fill{ bar.x, bar.y, std::max(8.f, bar.w * shown), bar.h };
            g.GlowRound(fill, 4.f, kTeal, 10.f, 0.35f);
            g.FillLinear(fill, 4.f, { { 0.f, Rgba(0, 150, 125) }, { 1.f, kTeal } }, { 0.f, 0.f }, { 1.f, 0.f });
            // Light sweeping along the filled part
            const float sweep = static_cast<float>(std::fmod(ui.Time() * 0.9, 1.3)) - 0.15f;
            g.PushRoundClip(fill, 4.f);
            g.FillLinear({ fill.x + fill.w * sweep - 40.f, fill.y, 80.f, fill.h }, 0.f,
                         { { 0.f, Rgba(255, 255, 255, 0) }, { 0.5f, Rgba(255, 255, 255, 70) }, { 1.f, Rgba(255, 255, 255, 0) } }, { 0.f, 0.f }, { 1.f, 0.f });
            g.PopLayer();
        }
    } else {
        ProgressBarIndeterminate(ui, bar, kTeal);
    }

    // "1.8 / 4.0 MB · 45%"  ............  "00:04"
    const Rect status = col.Next(16.f);
    std::wstring line;
    if (stage == 0) {
        if (total > 0) line = Megabytes(received) + L" / " + Megabytes(total) + L"  \x00B7  " +
                              std::to_wstring(static_cast<int>(100.0 * static_cast<double>(received) / static_cast<double>(total))) + L"%";
        else if (received > 0) line = Megabytes(received);
        else line = Loc::Get("starting");
    } else {
        line = Loc::Get("steamcmd_step_extract") + L"\x2026";
    }
    g.Text(line, status, kTeal, Mono(10.f, true));
    g.Text(Elapsed(Async::Now() - state.startedAt), status, kDim, Mono(10.f).Right());
    ui.KeepAnimating();
    return col.Height();
}

std::wstring SafeFileName(const std::wstring& name) {
    std::wstring out = name;
    for (auto& c : out)
        if (c < 32 || wcschr(L"<>:\"/\\|?*", c)) c = L'_';
    return out;
}

// File / folder entries under `root` (relative paths, folders marked).
std::set<std::wstring> Snapshot(const std::wstring& root) {
    std::set<std::wstring> entries;
    std::vector<std::wstring> pending{ root };
    while (!pending.empty()) {
        const std::wstring dir = pending.back();
        pending.pop_back();
        for (const auto& sub : Files::ListDirectories(dir)) {
            entries.insert(L"D" + Files::RelativePath(root, sub));
            pending.push_back(sub);
        }
        for (const auto& file : Files::ListFiles(dir)) entries.insert(L"F" + Files::RelativePath(root, file));
    }
    return entries;
}

} // namespace

HWND PresetInstaller::Owner() const { return window.Hwnd(); }

std::wstring TranslatePresetName(const std::wstring& name) { return Text::Replace(name, L"Parche", Loc::Get("patch")); }

void PresetInstaller::ShowError(const std::wstring& message) {
    Dialogs::Options o;
    o.title = Loc::Get("error_title");
    o.close = Loc::Get("close");
    Dialogs::ShowText(Owner(), o, message, Rgba(160, 180, 200));
}

std::vector<std::wstring> PresetInstaller::LogTail(const std::wstring& manifestId, size_t count) const {
    auto it = downloads.find(manifestId);
    if (it == downloads.end()) return {};
    std::vector<std::wstring> lines = it->second->lines;
    if (!lines.empty()) lines[0] = L"[ " + Elapsed(Async::Now() - it->second->startedAt) + L" ]  " + Loc::Get("steamcmd_in_progress");
    if (lines.size() > count) lines.erase(lines.begin(), lines.end() - static_cast<long long>(count));
    return lines;
}

void PresetInstaller::CancelAll() {
    for (auto& [id, d] : downloads) d->cancel.Request();
}

// ── Mode choice ─────────────────────────────────────────────────────────────

void PresetInstaller::Install(const Models::ChapterPreset& preset, int chapterNumber) {
    Dialogs::Options o;
    o.title = Loc::Fmt("install_dialog_title", TranslatePresetName(preset.name));
    o.primary = Loc::Get("install_mode_auto_btn");
    o.secondary = Loc::Get("install_mode_manual_btn");
    o.close = Loc::Get("cancel");
    o.contentWidth = 420.f;
    const std::wstring command = preset.Command();
    Dialogs::Show(Owner(), o, [command](Ui& ui, const Rect& area) {
        Column col(area);
        P::Label(ui, col, Loc::Get("credentials_version_label"), 11.f, kDim, false, 6.f);
        P::Box(ui, col, command, 10.f, Rgba(0, 204, 170, 50), Colors::Transparent, kTeal, { 10.f, 6.f }, 16.f);
        P::Label(ui, col, Loc::Get("install_mode_label"), 11.f, kDim, false, 10.f);
        ModeCard(ui, col, Loc::Get("install_mode_auto_title"), Loc::Get("install_mode_auto_desc"), kTeal, kTeal, 6.f);
        ModeCard(ui, col, Loc::Get("install_mode_manual_title"), Loc::Get("install_mode_manual_desc"), Rgba(80, 130, 200), Rgba(80, 160, 230), 0.f);
        return col.Height();
    }, [this, preset, chapterNumber](Dialogs::Result r) {
        if (r == Dialogs::Result::Primary) StartAuto(preset, chapterNumber);
        else if (r == Dialogs::Result::Secondary) StartManual(preset, chapterNumber);
    });
}

// ── MANUAL: Steam console instructions ──────────────────────────────────────

std::wstring PresetInstaller::ManualDepotFolder(const Models::ChapterPreset& preset) {
    // The Steam console downloads into the Steam install's own steamapps\content
    const std::wstring steamDir = SteamCmd::SteamInstallPath().value_or(Files::Combine(Files::ProgramFilesX86(), L"Steam"));
    const std::wstring depotFolder = Files::Combine(steamDir, L"steamapps", L"content", L"app_" + std::to_wstring(preset.appId),
                                                    L"depot_" + std::to_wstring(preset.depotId));
    Files::CreateDirectories(depotFolder);
    return depotFolder;
}

void PresetInstaller::OpenManualConsole(const Models::ChapterPreset& preset) {
    // Folder first, then the console on top of it (ready to paste)
    System::SetClipboardText(preset.Command());
    Process::Open(L"explorer.exe", Process::QuoteArgument(ManualDepotFolder(preset)));
    Process::OpenUrl(L"steam://open/console");
}

void PresetInstaller::StartManual(const Models::ChapterPreset& preset, int chapterNumber) {
    const std::wstring depotFolder = ManualDepotFolder(preset);
    const std::wstring command = preset.Command();
    const Color text = Rgba(160, 200, 230, 200);

    auto accepted = std::make_shared<bool>(false);
    Dialogs::Options o;
    o.title = Loc::Get("install_manual_title");
    o.primary = Loc::Get("install_manual_open_btn");
    o.primaryMinWidth = 160.f;
    o.close = Loc::Get("cancel");
    o.contentWidth = 480.f;
    o.primaryEnabled = [accepted] { return *accepted; };
    Dialogs::Show(Owner(), o, [=](Ui& ui, const Rect& area) {
        Column col(area);
        const auto step = [&](const wchar_t* number, const std::wstring& body, const std::function<void(Column&)>& extra) {
            ui.g.Text(number, { col.x, col.y, 22.f, 16.f }, kTeal, Mono(11.f, true).Top());
            Column inner(col.x + 22.f, col.y, col.w - 22.f);
            P::Label(ui, inner, body, 11.f, text);
            if (extra) extra(inner);
            col.Gap(inner.Height() + 10.f);
        };
        step(L"1.", Loc::Get("install_manual_step1"), nullptr);
        step(L"2.", Loc::Get("install_manual_step2"), [&](Column& c) {
            c.Gap(6.f);
            P::Box(ui, c, command, 11.f, Rgba(0, 204, 170, 40), Rgba(0, 204, 170, 60), kTeal);
        });
        step(L"3.", Loc::Get("install_manual_step3"), [&](Column& c) {
            c.Gap(6.f);
            P::Box(ui, c, depotFolder, 9.f, Rgba(80, 130, 200, 25), Rgba(80, 130, 200, 50), Rgba(80, 160, 230));
        });
        step(L"4.", Loc::Get("install_manual_step4"), nullptr);
        step(L"5.", Loc::Get("install_manual_step5"), nullptr);
        col.Gap(6.f);
        P::CheckBox(ui, ui.MakeId("manual-confirm"), col, Loc::Get("install_manual_confirm"), *accepted, 11.f, text);
        return col.Height();
    }, [=](Dialogs::Result r) {
        if (r != Dialogs::Result::Primary || !*accepted) return;
        OpenManualConsole(preset);   // command on the clipboard, Steam console and the depot folder open

        Dialogs::Options wait;
        wait.title = Loc::Get("install_manual_title");
        wait.primary = Loc::Get("steamguard_phone_continue");
        wait.close = Loc::Get("cancel");
        wait.contentWidth = 360.f;
        Dialogs::ShowText(Owner(), wait, Loc::Get("install_manual_waiting"), text, [=](Dialogs::Result w) {
            if (w == Dialogs::Result::Primary) LocateDepot(preset, chapterNumber, std::nullopt);
        });
    });
}

// ── AUTO: SteamCMD ──────────────────────────────────────────────────────────

void PresetInstaller::AcquireSteamCmd(std::function<void(std::optional<std::wstring>)> then) {
    Dialogs::Options o;
    o.title = Loc::Get("steamcmd_not_found_title");
    o.primary = Loc::Get("steamcmd_download_auto");
    o.primaryMinWidth = 150.f;
    o.secondary = Loc::Get("steamcmd_find");
    o.close = Loc::Get("cancel");
    o.contentWidth = 440.f;
    Dialogs::Show(Owner(), o, [](Ui& ui, const Rect& area) {
        Column col(area);
        SteamCmdHero(ui, col, Glyphs::Download, Loc::Get("steamcmd_hero_sub"), 14.f);
        P::Label(ui, col, Loc::Get("steamcmd_not_found_desc"), 11.f, Rgba(160, 190, 210), false, 10.f);
        InfoChips(ui, col, { Loc::Get("steamcmd_chip_size"), Loc::Get("steamcmd_chip_official"), Loc::Get("steamcmd_chip_local") }, 16.f);
        ModeCard(ui, col, Loc::Get("steamcmd_option_auto_title"), Loc::Get("steamcmd_option_auto_desc"), kTeal, kTeal, 6.f);
        ModeCard(ui, col, Loc::Get("steamcmd_option_find_title"), Loc::Get("steamcmd_option_find_desc"), Rgba(80, 130, 200), Rgba(80, 160, 230), 0.f);
        return col.Height();
    }, [this, then](Dialogs::Result r) {
        if (r == Dialogs::Result::Secondary) {
            const std::wstring picked = System::PickFile(Owner(), L"steamcmd.exe", { { L"steamcmd.exe", L"steamcmd.exe" }, { L"Executables", L"*.exe" } });
            if (then) then(picked.empty() ? std::nullopt : std::optional<std::wstring>(picked));
            return;
        }
        if (r != Dialogs::Result::Primary) {
            if (then) then(std::nullopt);
            return;
        }
        // Download: steps (download → extract → ready) with a live progress bar; Cancel stops it
        auto state = std::make_shared<SteamCmdDownloadState>();
        state->startedAt = Async::Now();
        Dialogs::Options p;
        p.title = Loc::Get("downloading_steamcmd_title");
        p.close = Loc::Get("cancel");
        p.contentWidth = 400.f;
        p.live = true;
        const int progress = Dialogs::Show(Owner(), p, [state](Ui& ui, const Rect& area) { return DrawSteamCmdDownload(ui, area, *state); },
                                           [state](Dialogs::Result) { state->cancel.Request(); });
        Async::Run([state] {
            std::wstring error;
            auto path = SteamCmd::Download([state](SteamCmd::DownloadStage stage, uint64_t received, uint64_t total) {
                state->stage = static_cast<int>(stage);
                state->received = received;
                state->total = total;
            }, &error, state->cancel);
            return std::make_pair(path, error);
        }, [this, progress, state, then](std::pair<std::optional<std::wstring>, std::wstring> result) {
            Dialogs::Close(progress);
            if (!result.first && !state->cancel.Requested()) ShowError(Loc::Get("error_download_steamcmd") + L"\n" + result.second);
            if (then) then(result.first);
        });
    });
}

void PresetInstaller::StartAuto(const Models::ChapterPreset& preset, int chapterNumber) {
    const auto withSteamCmd = [this, preset, chapterNumber](const std::wstring& steamCmd) {
        PromptCredentials(preset, [this, preset, chapterNumber, steamCmd](std::wstring user, std::optional<std::wstring> pass) {
            App::L().store.SetSteamUsername(user);
            SteamCmd::CopyCredentials(steamCmd);
            RunSteamCmd(preset, chapterNumber, steamCmd, user, pass);
        });
    };
    if (auto found = SteamCmd::Find()) {
        withSteamCmd(*found);
        return;
    }
    AcquireSteamCmd([withSteamCmd](std::optional<std::wstring> path) {
        if (path) withSteamCmd(*path);
    });
}

void PresetInstaller::StartAutoWith(const Models::ChapterPreset& preset, int chapterNumber, const std::wstring& user,
                                    const std::optional<std::wstring>& password) {
    const auto run = [this, preset, chapterNumber, user, password](const std::wstring& steamCmd) {
        App::L().store.SetSteamUsername(user);
        SteamCmd::CopyCredentials(steamCmd);
        RunSteamCmd(preset, chapterNumber, steamCmd, user, password);
    };
    if (auto found = SteamCmd::Find()) {
        run(*found);
        return;
    }
    AcquireSteamCmd([run](std::optional<std::wstring> path) {
        if (path) run(*path);
    });
}

void PresetInstaller::PromptCredentials(const Models::ChapterPreset& preset,
                                        std::function<void(std::wstring, std::optional<std::wstring>)> then) {
    struct State {
        std::wstring user, pass;
        bool save = false;
        int selected = -1;   // saved account index, -1 = manual entry
        std::vector<Services::SteamCredentials::Account> accounts;
    };
    auto state = std::make_shared<State>();
    const std::wstring suggested = SteamCmd::LoggedInUsername().value_or(App::L().store.SteamUsername().value_or(L""));
    state->user = suggested;
    state->accounts = Services::SteamCredentials::LoadAll();
    if (!state->accounts.empty()) state->selected = 0;   // pre-select the first saved account
    const std::wstring command = preset.Command();

    Dialogs::Options o;
    o.title = Loc::Fmt("install_dialog_title", TranslatePresetName(preset.name));
    o.primary = Loc::Get("start_download");
    o.close = Loc::Get("cancel");
    o.contentWidth = 440.f;
    Dialogs::Show(Owner(), o, [state, command, suggested](Ui& ui, const Rect& area) {
        Column col(area);
        P::Label(ui, col, Loc::Get("credentials_version_label"), 11.f, kDim, false, 6.f);
        P::Box(ui, col, command, 10.f, Rgba(0, 204, 170, 50), Colors::Transparent, kTeal, { 10.f, 5.f }, 12.f);

        if (!state->accounts.empty()) {
            P::Label(ui, col, Loc::Get("saved_accounts"), 8.f, Rgba(0, 204, 170, 140), true, 6.f);
            for (size_t i = 0; i < state->accounts.size(); ++i) {
                const float h = 8.f + std::max(ui.g.Measure(L"\xE77B", IconFont(14.f)).h, ui.g.Measure(L"Ag", Mono(11.f, true)).h) + 8.f + 2.f;
                const Rect r = col.Next(h, 4.f);
                const Interaction it = ui.Interact(ui.MakeId("account", static_cast<int64_t>(i)), r);
                ui.SetCursor(IDC_HAND);
                const bool active = state->selected == static_cast<int>(i);
                ui.g.Border(r, 4.f, active ? Rgba(0, 204, 170, 40) : it.hovered ? Rgba(0, 80, 100, 50) : Rgba(10, 40, 60, 30),
                            active ? Rgba(0, 204, 170, 220) : Rgba(30, 60, 90, 80), 1.f);
                ui.g.Text(L"\xE77B", { r.x + 13.f, r.y, 16.f, r.h }, Rgba(0, 204, 170, 120), IconFont(14.f));
                ui.g.Text(state->accounts[i].username, { r.x + 13.f + 14.f + 10.f, r.y, r.w - 40.f, r.h }, Rgba(180, 210, 230), Mono(11.f, true));
                if (it.held && ui.In().pressed[0]) state->selected = static_cast<int>(i);
            }
            col.Gap(10.f - 4.f);
            // "Use another account"
            const std::wstring other = Loc::Get("use_another_account");
            ButtonStyle s{ Rgba(255, 255, 255, 20), Rgba(100, 140, 160, 50), 1.f, 3.f, { 10.f, 4.f }, 0.f, 0.f };
            const float bh = ui.g.Measure(other, Mono(9.f)).h + 8.f + 2.f;
            const Rect br = col.Next(bh, 12.f);
            if (TextButton(ui, ui.MakeId("use-another"), { br.x, br.y, ButtonWidth(ui, other, Mono(9.f), s), bh }, other, Mono(9.f).Center(),
                           Rgba(100, 160, 200, 160), s)) {
                state->selected = -1;
                state->user = suggested;
                state->pass.clear();
            }
        }

        if (state->selected < 0) {
            P::Label(ui, col, Loc::Get("username_placeholder"), 9.f, Rgba(150, 180, 200, 120), false, 2.f);
            P::Input(ui, ui.MakeId("cred-user"), col, state->user);
            col.Gap(8.f);
            P::Label(ui, col, Loc::Get("password_placeholder"), 9.f, Rgba(150, 180, 200, 120), false, 2.f);
            P::Input(ui, ui.MakeId("cred-pass"), col, state->pass, true);
            col.Gap(10.f);
            P::CheckBox(ui, ui.MakeId("cred-save"), col, Loc::Get("save_password"), state->save, 10.f, Rgba(150, 180, 200, 180));
        }
        return col.Height();
    }, [state, then](Dialogs::Result r) {
        if (r != Dialogs::Result::Primary) return;
        if (state->selected >= 0) {
            const auto& account = state->accounts[static_cast<size_t>(state->selected)];
            then(account.username, Services::SteamCredentials::Decrypt(account));
            return;
        }
        const std::wstring user = Text::Trim(state->user);
        if (user.empty()) return;
        std::optional<std::wstring> pass;
        if (!state->pass.empty()) pass = state->pass;
        if (state->save && pass) Services::SteamCredentials::Save(user, *pass);
        then(user, pass);
    });
}

void PresetInstaller::RunSteamCmd(const Models::ChapterPreset& preset, int chapterNumber, const std::wstring& steamCmd, const std::wstring& user,
                                  const std::optional<std::wstring>& password) {
    auto download = std::make_shared<Download>();
    download->startedAt = Async::Now();
    download->lines.push_back(L"[ 00:00 ]  " + Loc::Get("steamcmd_in_progress"));
    downloads[preset.manifestId] = download;
    auto& l = App::L();
    if (auto* ch = l.ChapterByNumber(chapterNumber)) l.discord->SetInstalling(*ch, preset.name);

    // Log every file / folder SteamCMD creates under its steamapps\content (FileSystemWatcher)
    const std::wstring content = Files::Combine(Files::LocalAppData(), L"SpeedrunLauncher", L"steamcmd", L"steamapps", L"content");
    Files::CreateDirectories(content);
    Async::Run([download, content] {
        std::set<std::wstring> seen = Snapshot(content);
        while (!download->finished && !download->cancel.Requested()) {
            Sleep(500);
            for (const auto& entry : Snapshot(content)) {
                if (!seen.insert(entry).second) continue;
                const std::wstring line = (entry[0] == L'D' ? L"[DIR]   " : L"[FILE]  ") + entry.substr(1);
                Async::Post([download, line] { download->lines.push_back(line); });
            }
        }
    });

    // Steam Guard code prompt (asked from the SteamCMD worker thread)
    HWND dialogOwner = Owner();
    const auto prompt = [dialogOwner](const std::wstring&) -> std::optional<std::wstring> {
        auto code = std::make_shared<std::wstring>();
        Dialogs::Options o;
        o.title = Loc::Get("steamguard_title");
        o.primary = Loc::Get("understood");
        o.close = Loc::Get("cancel");
        o.contentWidth = 300.f;
        const auto result = Dialogs::ShowBlocking(dialogOwner, o, [code](Ui& ui, const Rect& area) {
            Column col(area);
            P::Label(ui, col, Loc::Get("steamguard_code_label"), 11.f, Rgba(160, 190, 220, 200), false, 8.f);
            P::Input(ui, ui.MakeId("guard-code"), col, *code);
            return col.Height();
        });
        if (result != Dialogs::Result::Primary) return std::nullopt;
        return Text::Trim(*code);
    };

    const Models::ChapterPreset p = preset;
    Async::Run([=] {
        std::wstring error;
        try {
            SteamCmd::Run(steamCmd, user, password, p.appId, p.depotId, p.manifestId, [](const std::wstring&) {}, prompt, download->cancel);
        } catch (const std::exception& e) {
            error = Text::Widen(e.what());
        }
        return error;
    }, [this, download, p, chapterNumber, steamCmd](std::wstring error) {
        download->finished = true;
        downloads.erase(p.manifestId);
        auto& launcher = App::L();
        if (auto* ch = launcher.ChapterByNumber(chapterNumber)) launcher.discord->SetChapterSelected(*ch, launcher.VersionLabel(*ch));
        if (!error.empty()) ShowError(Loc::Get("error_steamcmd_msg") + L"\n" + error);
        if (download->cancel.Requested()) return;
        LocateDepot(p, chapterNumber, steamCmd);
    });

    // SteamCMD may now be waiting for the phone approval (shown on the store's product page)
    if (password && !password->empty()) download->phoneCheck = true;
}

// ── After the download ──────────────────────────────────────────────────────

void PresetInstaller::LocateDepot(const Models::ChapterPreset& preset, int chapterNumber, const std::optional<std::wstring>& steamCmd) {
    if (auto depot = Services::SteamDetector::FindDepotDownloadPath(preset.appId, preset.depotId, steamCmd)) {
        notFound.erase(preset.manifestId);
        SetReady(preset, chapterNumber, *depot);
        return;
    }
    // Shown on the store's product page (pick the folder / look again / cancel)
    notFound[preset.manifestId] = NotFound{ preset, chapterNumber, steamCmd };
    if (onNotFound) onNotFound(chapterNumber, preset.manifestId);
}

void PresetInstaller::UseFolder(const std::wstring& manifestId, const std::wstring& folder) {
    auto it = notFound.find(manifestId);
    if (it == notFound.end() || folder.empty()) return;
    const NotFound n = it->second;
    notFound.erase(it);
    SetReady(n.preset, n.chapterNumber, folder);
}

void PresetInstaller::RetryLocate(const std::wstring& manifestId) {
    auto it = notFound.find(manifestId);
    if (it == notFound.end()) return;
    const NotFound n = it->second;
    LocateDepot(n.preset, n.chapterNumber, n.steamCmd);
}

bool PresetInstaller::PhoneCheck(const std::wstring& manifestId) const {
    auto it = downloads.find(manifestId);
    return it != downloads.end() && it->second->phoneCheck;
}

void PresetInstaller::DismissPhoneCheck(const std::wstring& manifestId) {
    if (auto it = downloads.find(manifestId); it != downloads.end()) {
        it->second->phoneCheck = false;
        it->second->phoneWaiting = true;
    }
}

bool PresetInstaller::PhoneWaiting(const std::wstring& manifestId) const {
    auto it = downloads.find(manifestId);
    // lines[0] is the "in progress" line: anything after it means files are arriving (approved)
    return it != downloads.end() && it->second->phoneWaiting && it->second->lines.size() <= 1;
}

void PresetInstaller::SkipPhoneWaiting(const std::wstring& manifestId) {
    if (auto it = downloads.find(manifestId); it != downloads.end()) it->second->phoneWaiting = false;
}

void PresetInstaller::SetReady(const Models::ChapterPreset& preset, int chapterNumber, const std::wstring& depotPath) {
    ready[preset.manifestId] = Ready{ preset, chapterNumber, depotPath };
    if (onReady) onReady(chapterNumber, preset.manifestId);
}

const PresetInstaller::Ready* PresetInstaller::ReadyFor(const std::wstring& manifestId) const {
    auto it = ready.find(manifestId);
    return it == ready.end() ? nullptr : &it->second;
}

std::optional<std::wstring> PresetInstaller::MovingStatus(const std::wstring& manifestId) const {
    auto it = moving.find(manifestId);
    if (it == moving.end()) return std::nullopt;
    return *it->second;
}

void PresetInstaller::Finish(const std::wstring& manifestId, std::wstring name, const std::optional<std::wstring>& icon, const std::wstring& folder) {
    auto it = ready.find(manifestId);
    if (it == ready.end() || folder.empty()) return;
    const Ready r = it->second;
    ready.erase(it);
    name = Text::Trim(name);
    if (name.empty()) name = TranslatePresetName(r.preset.name);
    App::L().store.SetPreferredPath(r.chapterNumber, folder);
    const std::wstring dest = Files::Combine(folder, SafeFileName(name));
    const std::wstring depotPath = r.depotPath;

    auto status = std::make_shared<std::wstring>(Loc::Get("moving_files"));
    moving[manifestId] = status;
    Async::Run([status, depotPath, dest] {
        const auto set = [status](const char* key) {
            const std::wstring text = Loc::Get(key);
            Async::Post([status, text] { *status = text; });
        };
        set("moving_files_dest");
        // Copy everything, then delete the source (works across drives)
        bool ok = Files::CopyDirectory(depotPath, dest, true);
        if (ok) Files::DeleteDirectory(depotPath);
        set("cleaning_temp");
        const std::wstring appDir = Files::DirectoryName(depotPath);
        if (!appDir.empty() && Files::DirectoryExists(appDir)) Files::DeleteDirectory(appDir);
        set("registering");
        return ok;
    }, [this, manifestId, r, name, icon, dest](bool ok) {
        moving.erase(manifestId);
        if (!ok) {
            ShowError(Loc::Get("error_move") + L"\n" + dest);
            return;
        }
        auto exe = Services::SteamDetector::FindGameExe(dest);
        if (!exe) {
            ShowError(Loc::Get("error_no_exe") + L"\n" + dest);
            return;
        }
        auto& store = App::L().store;
        store.AddCustom(r.chapterNumber, name, *exe);
        store.MarkManifestInstalled(r.preset.manifestId);
        if (icon) store.UpdateCustom(r.chapterNumber, *exe, name, icon);
        if (onInstalled) onInstalled();
        if (onFinished) onFinished(r.chapterNumber);
    });
}

} // namespace SpeedrunLauncher::Main
