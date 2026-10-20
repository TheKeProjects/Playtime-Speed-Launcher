#include "Features/InputRemaps.h"
#include <windows.h>
#include <mmsystem.h>
#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include "App/Launcher.h"
#include "Core/Async.h"
#include "Features/EasterEggs.h"
#include "Input/KeyInjector.h"
#include "Input/LowLevelHooks.h"
#include "Settings/CoresSettings.h"
#include "Settings/KeyClickSettings.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Toasts/ToastWindow.h"

namespace SpeedrunLauncher::Features {

namespace {

// Mouse button down / up at the cursor (0 left, 1 right, 2 middle)
void SendMouse(int button, bool up) {
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = button == 1   ? (up ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_RIGHTDOWN)
                       : button == 2 ? (up ? MOUSEEVENTF_MIDDLEUP : MOUSEEVENTF_MIDDLEDOWN)
                                     : (up ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_LEFTDOWN);
    SendInput(1, &input, sizeof(INPUT));
}

// Left / right Shift, Ctrl and Alt count as the same key
unsigned GenericVk(unsigned vk) {
    switch (vk) {
        case VK_LSHIFT: case VK_RSHIFT: return VK_SHIFT;
        case VK_LCONTROL: case VK_RCONTROL: return VK_CONTROL;
        case VK_LMENU: case VK_RMENU: return VK_MENU;
        default: return vk;
    }
}

// A click right now would land in the game: it has focus AND the cursor is over one of its windows
// (not over another app on a second screen, or over a launcher window)
bool ClickWouldHitGame() {
    if (!App::L().IsGameForeground()) return false;
    HWND foreground = GetForegroundWindow();
    POINT cursor{};
    if (!foreground || !GetCursorPos(&cursor)) return false;
    HWND under = WindowFromPoint(cursor);
    if (!under) return false;
    DWORD foregroundPid = 0, underPid = 0;
    GetWindowThreadProcessId(foreground, &foregroundPid);
    GetWindowThreadProcessId(GetAncestor(under, GA_ROOT), &underPid);
    return foregroundPid != 0 && foregroundPid == underPid;
}

// The launcher's own windows have focus (typing / configuring keys there must stay normal)
bool LauncherForeground() {
    DWORD pid = 0;
    if (HWND foreground = GetForegroundWindow()) GetWindowThreadProcessId(foreground, &pid);
    return pid == GetCurrentProcessId();
}

// Where the key acts: in the game only, or (everywhere) in any app but the launcher
bool KeyClickActive(bool everywhere) { return everywhere ? !LauncherForeground() : App::L().IsGameForeground(); }

// A click may go out now: in-game it must land in the game; everywhere, just not in the launcher
bool ClickAllowed(bool everywhere) { return everywhere ? !LauncherForeground() : ClickWouldHitGame(); }

} // namespace

// Key → click state: the bindings (a copy), which bound keys are held, and the stop flag of each
// running spammer. Touched by the hook thread and the spam threads, hence the mutex.
struct KeyClickState {
    std::vector<Settings::KeyClickBinding> bindings;
    bool everywhere = false;
    std::mutex mutex;
    std::set<unsigned> held;
    std::set<unsigned> buttonDown;   // plain clicks whose mouse button we actually pressed (so the release goes out too)
    std::map<unsigned, std::shared_ptr<std::atomic<bool>>> spamming;   // vk → keep running
};

void InputRemaps::Initialize() {
    RefreshCores();
    RefreshKeyClicks();
    auto& f11 = App::L().f11;
    f11.onRawKeyDown = [this](unsigned vk, unsigned time) { FeedEasterEgg(vk, time); };
    f11.Load();
    f11.Refresh();
}

void InputRemaps::Shutdown() {
    StopKeyClicks();
    for (int* id : { &coresKeyboard, &eggKeyboard })
        if (*id) Input::Hooks::RemoveKeyboard(*id), *id = 0;
}

bool InputRemaps::AnyKeyboardHookActive() const { return Input::Hooks::KeyboardHookActive(); }

void InputRemaps::RefreshCores() {
    if (coresKeyboard) Input::Hooks::RemoveKeyboard(coresKeyboard);
    coresKeyboard = 0;
    const auto& settings = Settings::Cores::Current();
    if (!settings.enabled) return;
    // Runs on the input hook thread: keys copied now (RefreshCores runs again when they change)
    const unsigned freezeVk = settings.freezeVk, slowerVk = settings.slowerVk, normalVk = settings.normalVk;
    coresKeyboard = Input::Hooks::AddKeyboard([this, freezeVk, slowerVk, normalVk](const Input::Hooks::KeyEvent& e) {
        if (e.down && !e.injected) FeedEasterEgg(e.vk, e.time);
        if (e.down && !e.up) {
            if ((e.vk == freezeVk || e.vk == slowerVk || e.vk == normalVk) && App::L().IsGameForeground()) {
                const auto mode = e.vk == freezeVk ? Services::CoresMode::Freeze
                                  : e.vk == slowerVk ? Services::CoresMode::Slower
                                                     : Services::CoresMode::Normal;
                Async::Post([this, mode] { HandleCoresHotkey(mode); });
            }
        }
        return false;
    });
}

void InputRemaps::StopKeyClicks() {
    if (keyClickKeyboard) Input::Hooks::RemoveKeyboard(keyClickKeyboard);
    keyClickKeyboard = 0;
    if (!keyClickState) return;
    // Keep the state alive past the lock: dropping the last reference while its mutex is locked
    // destroyed a busy mutex (abort() in Debug)
    const std::shared_ptr<KeyClickState> state = std::move(keyClickState);
    std::lock_guard lock(state->mutex);
    for (auto& [vk, run] : state->spamming) *run = false;
    state->spamming.clear();
    // A plain click whose button we pressed: let it go
    for (unsigned vk : state->buttonDown)
        for (const auto& b : state->bindings)
            if (b.vk == vk && !b.spam) SendMouse(b.button, true);
    state->buttonDown.clear();
    state->held.clear();
}

void InputRemaps::RefreshKeyClicks() {
    StopKeyClicks();
    const auto& settings = Settings::KeyClick::Current();
    if (!settings.enabled || settings.bindings.empty()) return;
    auto state = std::make_shared<KeyClickState>();
    state->bindings = settings.bindings;
    state->everywhere = settings.everywhere;
    keyClickState = state;
    const bool everywhere = settings.everywhere;

    // Hook thread: while a game has focus (or anywhere but the launcher, with "everywhere"); the
    // bound key is swallowed and becomes the click
    keyClickKeyboard = Input::Hooks::AddKeyboard([state, everywhere](const Input::Hooks::KeyEvent& e) {
        if (e.injected) return false;
        const Settings::KeyClickBinding* binding = nullptr;
        for (const auto& b : state->bindings)
            if (GenericVk(b.vk) == GenericVk(e.vk)) binding = &b;
        if (!binding) return false;
        std::lock_guard lock(state->mutex);
        const bool wasHeld = state->held.count(e.vk) != 0;
        if (!KeyClickActive(everywhere) && !wasHeld) return false;   // elsewhere the key works as usual
        const int button = binding->button;
        if (e.down) {
            if (wasHeld) return true;   // keyboard auto-repeat
            state->held.insert(e.vk);
            if (!binding->spam) {
                // Deferred (injecting from inside the hook can get dropped), and checked again when
                // it goes out: only if the click would still land in the game
                const unsigned vk = e.vk;
                Async::Post([state, vk, button, everywhere] {
                    if (!ClickAllowed(everywhere)) return;
                    SendMouse(button, false);
                    std::lock_guard inner(state->mutex);
                    state->buttonDown.insert(vk);
                });
                return true;
            }
            // Spam: clicks at the chosen rate until the key goes up (or the game loses focus)
            auto run = std::make_shared<std::atomic<bool>>(true);
            state->spamming[e.vk] = run;
            const double interval = 1.0 / std::max(1, binding->clicksPerSecond);
            std::thread([run, button, interval, everywhere] {
                using Clock = std::chrono::steady_clock;
                timeBeginPeriod(1);   // accurate sleeps
                const auto step = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(interval));
                // Each click is held a moment (games polling once per frame miss instant ones)
                const auto hold = std::min(step / 2, std::chrono::duration_cast<Clock::duration>(std::chrono::milliseconds(30)));
                auto next = Clock::now();
                while (*run && KeyClickActive(everywhere)) {
                    // Only clicks allowed right now (in-game: cursor over the game; skipped otherwise)
                    if (ClickAllowed(everywhere)) {
                        SendMouse(button, false);
                        std::this_thread::sleep_for(hold);
                        SendMouse(button, true);
                    }
                    next += step;
                    std::this_thread::sleep_until(next);
                }
                timeEndPeriod(1);
            }).detach();
            return true;
        }
        if (e.up && wasHeld) {
            state->held.erase(e.vk);
            if (auto it = state->spamming.find(e.vk); it != state->spamming.end()) {
                *it->second = false;
                state->spamming.erase(it);
            } else {
                // Release only what was pressed (wherever focus is now, so it never stays stuck)
                const unsigned vk = e.vk;
                Async::Post([state, vk, button] {
                    std::lock_guard inner(state->mutex);
                    if (state->buttonDown.erase(vk)) SendMouse(button, true);
                });
            }
            return true;
        }
        return false;
    });
}

void InputRemaps::RefreshEasterEggHook() {
    // A dedicated hook so easter eggs can also be typed in-game; only while a chapter runs
    const bool anyRunning = App::L().AnyGameRunning();
    if (anyRunning && !eggKeyboard) {
        eggKeyboard = Input::Hooks::AddKeyboard([this](const Input::Hooks::KeyEvent& e) {
            if (e.down && !e.injected) return FeedEasterEgg(e.vk, e.time);
            return false;
        });
    } else if (!anyRunning && eggKeyboard) {
        Input::Hooks::RemoveKeyboard(eggKeyboard);
        eggKeyboard = 0;
    }
}

bool InputRemaps::FeedEasterEgg(unsigned vk, unsigned time) {
    const bool inGame = App::L().IsGameForeground();
    if (vk == VK_ESCAPE && inGame && main.Eggs().PlayingInGame()) {
        Async::Post([this] { main.Eggs().Hide(); });
        return true;
    }
    // Several hooks see the same press: drop duplicates
    if (vk == lastEggVk && time == lastEggTime) return false;
    lastEggVk = vk;
    lastEggTime = time;
    // Only keys typed into the launcher itself or into the running game count
    if (!inGame && GetForegroundWindow() != main.Hwnd()) return false;
    Async::Post([this, vk] { main.Eggs().HandleKey(vk); });
    return false;
}

void InputRemaps::HandleCoresHotkey(Services::CoresMode mode) {
    auto& l = App::L();
    const auto exePath = [&l](const Models::Chapter& c) { return l.ActiveExePath(c); };
    const auto shipping = [&l](const std::wstring& exe) { return l.ShippingExeName(exe); };
    auto found = l.cores.FindGameProcess(l.chapters, exePath, shipping);
    if (!found && l.cores.CurrentMode() == Services::CoresMode::Normal) return;
    const unsigned long priority = Settings::Cores::Current().priorityHigh ? HIGH_PRIORITY_CLASS : IDLE_PRIORITY_CLASS;
    auto name = l.cores.ApplyMode(mode, l.chapters, exePath, shipping, priority);
    if (!name) return;
    ShowCoresToast(mode, *name, l.cores.DetectedChapter());
}

void InputRemaps::ShowCoresToast(Services::CoresMode mode, const std::wstring& processName, int chapter) {
    const std::wstring label = mode == Services::CoresMode::Slower ? L"1 CORE" : mode == Services::CoresMode::Freeze ? L"0 CORES" : L"ALL CORES";
    const Color color = mode == Services::CoresMode::Slower ? Rgba(230, 180, 40) : mode == Services::CoresMode::Freeze ? Rgba(220, 60, 60)
                                                                                                                      : Rgba(0, 204, 170);
    Toasts::Spec spec;
    spec.width = 340.f;
    spec.padding = { 14.f, 10.f, 14.f, 10.f };
    spec.duration = 5.0;
    spec.fadeIn = 0.2;
    spec.fadeOut = 0.4;
    spec.accent = color;
    spec.anchor = Toasts::Spec::Anchor::TopLeft;
    spec.lines = { { label, 16.f, true, color, 0.f },
                   { L"Ch." + std::to_wstring(chapter) + L"  \x2014  " + processName, 10.f, false, Rgba(160, 190, 210, 120), 4.f } };
    main.CoresToast().Show(spec);
}

} // namespace SpeedrunLauncher::Features
