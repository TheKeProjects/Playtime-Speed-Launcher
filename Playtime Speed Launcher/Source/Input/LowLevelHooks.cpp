#include "Input/LowLevelHooks.h"
#include <windows.h>
#include <atomic>
#include <map>
#include <mutex>

namespace SpeedrunLauncher::Input::Hooks {

namespace {

// The hooks live on their own thread with nothing else to do. Windows waits for a low-level hook
// on every key press / click system-wide and silently removes it when it answers too slowly, so
// it must never share a thread with rendering or other UI work.
constexpr UINT WM_SYNC_HOOKS = WM_APP + 1;

HHOOK keyboardHook = nullptr;   // owned by the hook thread
HHOOK mouseHook = nullptr;
std::recursive_mutex handlersMutex;   // held while handlers run, so Remove* never returns mid-call
std::map<int, KeyHandler> keyHandlers;
std::map<int, MouseHandler> mouseHandlers;
int nextId = 1;
std::atomic_bool keyboardActive{ false };
DWORD hookThreadId = 0;

LRESULT CALLBACK KeyboardProc(int code, WPARAM wp, LPARAM lp) {
    if (code >= 0) {
        const auto* data = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lp);
        KeyEvent e;
        e.vk = data->vkCode;
        e.scanCode = data->scanCode;
        e.time = data->time;
        e.down = wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN;
        e.up = wp == WM_KEYUP || wp == WM_SYSKEYUP;
        e.injected = (data->flags & LLKHF_INJECTED) != 0;
        bool swallow = false;
        {
            std::lock_guard lock(handlersMutex);
            for (const auto& [id, handler] : keyHandlers)
                if (handler && handler(e)) swallow = true;
        }
        if (swallow) return 1;
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

LRESULT CALLBACK MouseProc(int code, WPARAM wp, LPARAM lp) {
    if (code >= 0 && wp != WM_MOUSEMOVE) {
        const auto* data = reinterpret_cast<const MSLLHOOKSTRUCT*>(lp);
        MouseEvent e;
        e.message = static_cast<unsigned>(wp);
        e.xButton = HIWORD(data->mouseData);
        e.wheelDelta = static_cast<short>(HIWORD(data->mouseData));
        e.injected = (data->flags & LLMHF_INJECTED) != 0;
        bool swallow = false;
        {
            std::lock_guard lock(handlersMutex);
            for (const auto& [id, handler] : mouseHandlers)
                if (handler && handler(e)) swallow = true;
        }
        if (swallow) return 1;
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

// Hook thread: installs / removes the hooks to match the registered handlers
void SyncOnHookThread() {
    bool wantKeyboard, wantMouse;
    {
        std::lock_guard lock(handlersMutex);
        wantKeyboard = !keyHandlers.empty();
        wantMouse = !mouseHandlers.empty();
    }
    if (wantKeyboard && !keyboardHook) keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandleW(nullptr), 0);
    if (!wantKeyboard && keyboardHook) {
        UnhookWindowsHookEx(keyboardHook);
        keyboardHook = nullptr;
    }
    // A global mouse hook adds latency to every mouse move system-wide: only while needed
    if (wantMouse && !mouseHook) mouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, GetModuleHandleW(nullptr), 0);
    if (!wantMouse && mouseHook) {
        UnhookWindowsHookEx(mouseHook);
        mouseHook = nullptr;
    }
    keyboardActive = keyboardHook != nullptr;
}

DWORD WINAPI HookThread(LPVOID ready) {
    MSG msg;
    PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);   // create the message queue
    SetEvent(static_cast<HANDLE>(ready));
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_SYNC_HOOKS) SyncOnHookThread();
        // Hook callbacks are delivered while this thread waits in GetMessage
    }
    return 0;
}

void RequestSync() {
    static std::once_flag started;
    std::call_once(started, [] {
        HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        HANDLE thread = CreateThread(nullptr, 0, HookThread, ready, 0, &hookThreadId);
        if (thread) {
            SetThreadPriority(thread, THREAD_PRIORITY_HIGHEST);
            WaitForSingleObject(ready, 2000);
            CloseHandle(thread);   // runs for the lifetime of the app
        }
        CloseHandle(ready);
    });
    if (hookThreadId) PostThreadMessageW(hookThreadId, WM_SYNC_HOOKS, 0, 0);
}

} // namespace

int AddKeyboard(KeyHandler handler) {
    int id;
    {
        std::lock_guard lock(handlersMutex);
        id = nextId++;
        keyHandlers[id] = std::move(handler);
    }
    RequestSync();
    return id;
}

void RemoveKeyboard(int id) {
    {
        std::lock_guard lock(handlersMutex);
        keyHandlers.erase(id);
    }
    RequestSync();
}

int AddMouse(MouseHandler handler) {
    int id;
    {
        std::lock_guard lock(handlersMutex);
        id = nextId++;
        mouseHandlers[id] = std::move(handler);
    }
    RequestSync();
    return id;
}

void RemoveMouse(int id) {
    {
        std::lock_guard lock(handlersMutex);
        mouseHandlers.erase(id);
    }
    RequestSync();
}

bool KeyboardHookActive() {
    std::lock_guard lock(handlersMutex);
    return !keyHandlers.empty() || keyboardActive;
}

void RemoveAll() {
    {
        std::lock_guard lock(handlersMutex);
        keyHandlers.clear();
        mouseHandlers.clear();
    }
    RequestSync();
}

} // namespace SpeedrunLauncher::Input::Hooks
