#include "Services/Hotkeys/F11RemapService.h"
#include <windows.h>
#include <algorithm>
#include <memory>
#include "Core/Text.h"
#include "Input/KeyInjector.h"
#include "Input/LowLevelHooks.h"
#include "Settings/ConfigFile.h"

namespace SpeedrunLauncher::Services {

namespace {
constexpr const wchar_t* kFile = L"f11remap.cfg";
}

void F11RemapService::Load() {
    auto text = Config::ReadText(kFile);
    if (!text) return;
    const auto parts = Text::Split(*text, L',');
    if (parts.empty()) return;
    enabled = parts[0] == L"1";
    bindings.clear();
    // Legacy single-binding format: "enabled,K|M,value"
    if (parts.size() == 3 && parts[1].size() == 1) {
        const auto value = Text::ToInt(parts[2]);
        if (value && parts[1] == L"K") bindings.push_back(Input::Binding::Key(static_cast<unsigned>(*value)));
        else if (value && parts[1] == L"M") bindings.push_back(Input::Binding::Mouse(static_cast<int>(*value)));
        return;
    }
    for (size_t i = 1; i < parts.size(); ++i)
        if (auto b = Input::Binding::Parse(parts[i])) bindings.push_back(*b);
}

void F11RemapService::Save() const {
    std::wstring out = enabled ? L"1" : L"0";
    for (const auto& b : bindings)
        if (b.type != Input::BindingType::None) out += L"," + b.Token();
    Config::WriteText(kFile, out);
}

void F11RemapService::SetEnabled(bool on) {
    enabled = on;
    Save();
    Refresh();
}

void F11RemapService::AddBinding(const Input::Binding& binding) {
    if (std::find(bindings.begin(), bindings.end(), binding) != bindings.end()) return;
    bindings.push_back(binding);
    Save();
    Refresh();
}

void F11RemapService::ReplaceBinding(size_t index, const Input::Binding& binding) {
    if (index >= bindings.size()) return;
    bindings[index] = binding;
    Save();
    Refresh();
}

void F11RemapService::RemoveBinding(size_t index) {
    if (index >= bindings.size()) return;
    bindings.erase(bindings.begin() + static_cast<long long>(index));
    Save();
    Refresh();
}

void F11RemapService::Uninstall() {
    if (keyboardId) Input::Hooks::RemoveKeyboard(keyboardId);
    if (mouseId) Input::Hooks::RemoveMouse(mouseId);
    keyboardId = mouseId = 0;
}

void F11RemapService::Refresh() {
    Uninstall();
    if (!enabled || bindings.empty()) return;
    // The handlers run on the input hook thread: they get their own copy of the bindings and of
    // the held-keys set, so editing the bindings here never races with them.
    const auto triggers = bindings;
    const auto rawKeyDown = onRawKeyDown;
    auto heldKeys = std::make_shared<std::set<unsigned>>();
    keyboardId = Input::Hooks::AddKeyboard([triggers, rawKeyDown, heldKeys](const Input::Hooks::KeyEvent& e) {
        if (e.down && !e.injected && rawKeyDown) rawKeyDown(e.vk, e.time);
        // Swallow the real F11 (our own synthetic one is reported as injected)
        if (e.vk == VK_F11 && !e.injected) return true;
        if (!e.injected) {
            const bool isTrigger = std::any_of(triggers.begin(), triggers.end(), [&](const Input::Binding& b) {
                return b.type == Input::BindingType::Keyboard && b.keyVk == e.vk;
            });
            if (isTrigger) {
                if (e.down && heldKeys->insert(e.vk).second) Input::Injector::Tap(VK_F11);   // debounced while held
                else if (e.up) heldKeys->erase(e.vk);
            }
        }
        return false;
    });
    const bool anyMouse = std::any_of(triggers.begin(), triggers.end(), [](const auto& b) { return b.type == Input::BindingType::Mouse; });
    if (anyMouse) {
        mouseId = Input::Hooks::AddMouse([triggers](const Input::Hooks::MouseEvent& e) {
            int pressed = 0;
            if (e.message == WM_MBUTTONDOWN) pressed = Input::MouseButtons::Middle;
            else if (e.message == WM_XBUTTONDOWN) pressed = e.xButton == XBUTTON1 ? Input::MouseButtons::XButton1
                                                           : e.xButton == XBUTTON2 ? Input::MouseButtons::XButton2 : 0;
            if (pressed && std::any_of(triggers.begin(), triggers.end(), [&](const Input::Binding& b) {
                    return b.type == Input::BindingType::Mouse && b.mouseButton == pressed;
                }))
                Input::Injector::Tap(VK_F11);
            return false;
        });
    }
}

} // namespace SpeedrunLauncher::Services
