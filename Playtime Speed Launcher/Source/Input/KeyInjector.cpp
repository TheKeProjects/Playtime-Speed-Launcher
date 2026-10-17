#include "Input/KeyInjector.h"
#include <windows.h>
#include "Core/Async.h"

namespace SpeedrunLauncher::Input::Injector {

namespace {

void Send(unsigned vk, unsigned short scan, bool up) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = static_cast<WORD>(vk);
    input.ki.wScan = scan;
    input.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
    SendInput(1, &input, sizeof(INPUT));
}

} // namespace

void Tap(unsigned vk) {
    INPUT inputs[2]{};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = static_cast<WORD>(vk);
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = static_cast<WORD>(vk);
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

void HeldPress(unsigned vk) {
    const auto scan = static_cast<unsigned short>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    Send(vk, scan, false);
    Async::After(60, [vk, scan] { Send(vk, scan, true); });
}

} // namespace SpeedrunLauncher::Input::Injector
