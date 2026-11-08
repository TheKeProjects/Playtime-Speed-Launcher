#include "Services/Collectibles/InGameOverlay.h"
#include <windows.h>
#include <memory>
#include <string>
#include "Core/Async.h"
#include "Core/Process.h"
#include "Core/Text.h"
#include "Services/Collectibles/CollectiblesServer.h"
#include "Settings/CollectiblesSettings.h"
#include "Windows/GameOverlays/CollectiblesOverlayWindow.h"

namespace SpeedrunLauncher::Services::Collectibles::InGameOverlay {

namespace {

constexpr unsigned kIntervalMs = 2000;
constexpr ULONGLONG kPreviewMs = 10000;
// The chapters the overlay knows (Services\Collectibles\SaveGameService)
constexpr const wchar_t* kGames[] = { L"Poppy_Playtime-Win64-Shipping", L"Playtime_Prototype4-Win64-Shipping", L"Playtime_Chapter3-Win64-Shipping", L"ch4_pro-Win64-Shipping", L"ch5_pro-Win64-Shipping" };

ULONGLONG previewUntil = 0;
bool moving = false;
Async::TimerId timer = 0;
std::unique_ptr<Windows::CollectiblesOverlayWindow> window;
HWINEVENTHOOK foregroundHook = nullptr, minimizeHook = nullptr;
DWORD gamePid = 0;   // last foreground process found to be the game (saves reopening it on every check)

bool GameRunning() {
    const auto processes = Process::List();
    for (const wchar_t* game : kGames)
        if (!Process::FindByName(processes, game).empty()) return true;
    return false;
}

// The active window is one of the chapters, and not minimized
bool GameInFront() {
    HWND fg = GetForegroundWindow();
    if (!fg || IsIconic(fg)) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (!pid) return false;
    if (pid == gamePid) return true;
    bool game = false;
    if (HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
        wchar_t path[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(process, 0, path, &size)) {
            const wchar_t* name = wcsrchr(path, L'\\');
            name = name ? name + 1 : path;
            for (const wchar_t* exe : kGames)
                if (_wcsicmp(name, (std::wstring(exe) + L".exe").c_str()) == 0) game = true;
        }
        CloseHandle(process);
    }
    if (game) gamePid = pid;
    return game;
}

// Shown while moving / previewing, otherwise only with the game in front
void UpdateVisibility() {
    if (!window) return;
    window->SetShown(moving || GetTickCount64() < previewUntil || GameInFront());
}

void CALLBACK OnWinEvent(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD) { UpdateVisibility(); }

void Refresh() {
    const auto& s = Settings::Collectibles::Current();
    const bool show = moving || (s.inGame && GameRunning()) || GetTickCount64() < previewUntil;
    if (!show) {
        if (window) {
            window->Destroy();
            window.reset();
        }
        return;
    }
    const int port = Server::Ensure();
    if (!port) return;
    // move=1: the page draws a frame and a "drag me" label. ingame=1: nothing that animates forever, and
    // no particles: while anything moves in the overlay, Windows composes it with the game every frame,
    // which makes a borderless game stutter
    const std::wstring url =
        L"http://127.0.0.1:" + std::to_wstring(port) + L"/overlay?" + Text::Widen(s.Query()) + L"&ingame=1" + (moving ? L"&move=1" : L"");
    const Windows::CollectiblesOverlayWindow::Placement placement{ s.right, s.scale, s.inGameOffset };
    if (!window) {
        window = std::make_unique<Windows::CollectiblesOverlayWindow>();
        window->onMoved = [](int offset) {
            auto& settings = Settings::Collectibles::Current();
            settings.inGameOffset = offset;
            settings.Save();
        };
        if (!window->Open(url, placement)) {
            window.reset();
            return;
        }
    } else {
        window->Update(url, placement);
    }
    window->SetMoveMode(moving);
    UpdateVisibility();
}

} // namespace

void Start() {
    if (!timer) timer = Async::Every(kIntervalMs, [] { Refresh(); });
    // Another window activated / something minimized or restored: show or hide right away
    if (!foregroundHook)
        foregroundHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, OnWinEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (!minimizeHook)
        minimizeHook = SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND, nullptr, OnWinEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
    Refresh();
}

void Shutdown() {
    moving = false;
    if (foregroundHook) UnhookWinEvent(foregroundHook);
    if (minimizeHook) UnhookWinEvent(minimizeHook);
    foregroundHook = minimizeHook = nullptr;
    if (timer) Async::CancelTimer(timer);
    timer = 0;
    if (window) window->Destroy();
    window.reset();
}

void Apply() { Refresh(); }

void Preview() {
    previewUntil = GetTickCount64() + kPreviewMs;
    Refresh();
    // Gone again once the preview ends (unless the game is running)
    Async::After(static_cast<unsigned>(kPreviewMs) + 100, [] { Refresh(); });
}

bool Showing() { return window && window->IsOpen(); }

void StartMove() {
    moving = true;
    Refresh();
}

void EndMove() {
    moving = false;
    Refresh();
}

bool Moving() { return moving; }

void ResetPosition() {
    auto& s = Settings::Collectibles::Current();
    s.inGameOffset = 0;
    s.Save();
    Refresh();
}

} // namespace SpeedrunLauncher::Services::Collectibles::InGameOverlay
