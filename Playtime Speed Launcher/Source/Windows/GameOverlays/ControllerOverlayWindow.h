#pragma once
#include <atomic>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "Graphics/Images.h"
#include "UI/Window.h"

namespace SpeedrunLauncher::Windows {

// Input display overlay for speedrun recordings: a gamepad (Xbox / DualSense) or keyboard + mouse
// skin from Assets\Controllers (OBS input-overlay JSON + sprite sheet), lighting up pressed
// buttons, moving sticks and filling triggers. Click-through, always on top, in a screen corner.
class ControllerOverlayWindow : public UI::Window {
public:
    ControllerOverlayWindow(std::wstring controllerFolder, std::wstring keyboardLayout);
    ~ControllerOverlayWindow() override;
    bool Open(const std::wstring& corner);

protected:
    void OnFrame() override;
    Color ClearColor() const override { return Colors::Transparent; }

private:
    struct Element {
        std::wstring id;
        int type = 0;
        int mapping[4]{};   // x, y, w, h in the sprite sheet
        int pos[2]{};       // position on the canvas
        int code = 0, side = 0, stickRadius = 0, direction = 1;
        int sheet = 0;      // 0 = main / keyboard sheet, 1 = mouse sheet
        int z = 0;
    };
    struct State {
        std::set<int> codes;
        int dpad = 0;       // GameControllerSwitchPosition (0 = centre)
        double lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
        int wheel = 0;      // 0 neutral, 1 middle click, 2 scroll up, 3 scroll down
    };

    bool LoadGamepad();
    bool LoadKeyboardMouse();
    void Poll(State& state);
    void DrawSprite(const Element& el, int sx, int sy, float x, float y, float opacity);

    std::wstring folder, layout;
    std::vector<Element> elements;
    ImagePtr sheets[2];
    int canvasW = 0, canvasH = 0;
    std::map<int, unsigned> keyVk;     // scancode → virtual key
    int mouseHook = 0;
    std::atomic<unsigned long long> wheelFlashUntil{ 0 };   // written by the input hook thread
    std::atomic<int> wheelFlashIndex{ 0 };
};

} // namespace SpeedrunLauncher::Windows
