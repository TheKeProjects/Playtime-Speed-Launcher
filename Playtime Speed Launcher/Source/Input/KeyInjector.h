#pragma once

namespace SpeedrunLauncher::Input::Injector {

// Down + up in one SendInput call (F11 remap).
void Tap(unsigned vk);
// Down, held ~60 ms, then up, with the hardware scan code — some mods poll key state once per frame
// and miss same-tick presses / scancode-less input (Chapter 4 remap).
void HeldPress(unsigned vk);

} // namespace SpeedrunLauncher::Input::Injector
