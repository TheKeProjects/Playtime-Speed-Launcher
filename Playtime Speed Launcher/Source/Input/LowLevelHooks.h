#pragma once
#include <functional>

// One WH_KEYBOARD_LL / WH_MOUSE_LL hook shared by every feature that needs global input
// (F11 remap, Cores hotkeys, Chapter 4 remap, easter eggs typed in-game). Each feature registers
// a handler; the hook is installed while at least one handler is registered.
// The hooks run on a dedicated thread (Windows waits for them on every key press system-wide and
// silently removes hooks that answer too slowly). Handlers therefore run on that thread: keep them
// quick, only read data that is thread-safe or captured at registration, and hand real work to
// the UI thread with Async::Post. Remove* waits for a handler that is running to finish.
namespace SpeedrunLauncher::Input::Hooks {

struct KeyEvent {
    unsigned vk = 0;
    unsigned scanCode = 0;
    unsigned time = 0;
    bool down = false;       // WM_KEYDOWN / WM_SYSKEYDOWN
    bool up = false;         // WM_KEYUP / WM_SYSKEYUP
    bool injected = false;   // SendInput (ours or someone else's)
};

struct MouseEvent {
    unsigned message = 0;    // WM_MBUTTONDOWN, WM_XBUTTONDOWN, ...
    int xButton = 0;         // 1 or 2 for WM_XBUTTON*
    short wheelDelta = 0;    // WM_MOUSEWHEEL
    bool injected = false;
};

// Return true to swallow the event (it never reaches other applications).
using KeyHandler = std::function<bool(const KeyEvent&)>;
using MouseHandler = std::function<bool(const MouseEvent&)>;

int AddKeyboard(KeyHandler handler);
void RemoveKeyboard(int id);
int AddMouse(MouseHandler handler);
void RemoveMouse(int id);
bool KeyboardHookActive();
void RemoveAll();

} // namespace SpeedrunLauncher::Input::Hooks
