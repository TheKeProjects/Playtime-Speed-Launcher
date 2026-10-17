#include "Windows/GameOverlays/ControllerOverlayWindow.h"
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>
#include <algorithm>
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Json.h"
#include "Core/Text.h"
#include "Input/LowLevelHooks.h"

#pragma comment(lib, "windowsapp.lib")

namespace SpeedrunLauncher::Windows {

namespace {

namespace wgi = winrt::Windows::Gaming::Input;

enum ElementType { TypeBody = 0, TypeKey = 1, TypeButton = 2, TypeMouseButton = 3, TypeWheel = 4, TypeStick = 5, TypeTrigger = 6,
                   TypeGuide = 7, TypeDpad = 8 };

// DPad strip sprite index per GameControllerSwitchPosition (Center, Up, UpRight, Right, DownRight,
// Down, DownLeft, Left, UpLeft); the sheet order is inactive, Left, Right, Up, Down, UpLeft, UpRight,
// DownLeft, DownRight.
constexpr int kDpadSprite[] = { 0, 3, 6, 2, 8, 4, 7, 1, 5 };
constexpr uint16_t kVendorSony = 0x054C;
// DualSense raw HID button order → OBS / SDL button code
constexpr std::pair<int, int> kPsButtons[] = { { 0, 2 }, { 1, 0 }, { 2, 1 }, { 3, 3 }, { 4, 9 }, { 5, 10 }, { 8, 4 }, { 9, 6 }, { 10, 7 }, { 11, 8 } };
constexpr int kKeyboardMouseGap = 30;

} // namespace

ControllerOverlayWindow::ControllerOverlayWindow(std::wstring controllerFolder, std::wstring keyboardLayout)
    : folder(std::move(controllerFolder)), layout(std::move(keyboardLayout)) {}

ControllerOverlayWindow::~ControllerOverlayWindow() {
    if (mouseHook) Input::Hooks::RemoveMouse(mouseHook);
}

namespace {

void ReadElements(const Json::Value& root, int sheet, int sheetXOffset, int canvasXOffset, int canvasYOffset, auto& out) {
    for (const auto& el : root["elements"].Elements()) {
        typename std::remove_reference_t<decltype(out)>::value_type e;
        e.type = static_cast<int>(el["type"].Int());
        e.id = el["id"].WStr();
        for (int i = 0; i < 4; ++i) e.mapping[i] = static_cast<int>(el["mapping"][static_cast<size_t>(i)].Int());
        for (int i = 0; i < 2; ++i) e.pos[i] = static_cast<int>(el["pos"][static_cast<size_t>(i)].Int());
        e.code = static_cast<int>(el["code"].Int(0));
        e.side = static_cast<int>(el["side"].Int(0));
        e.stickRadius = static_cast<int>(el["stick_radius"].Int(0));
        e.direction = static_cast<int>(el["direction"].Int(1));
        e.z = static_cast<int>(el["z_level"].Int(0));
        e.sheet = sheet;
        e.mapping[0] += sheetXOffset;
        e.pos[0] += canvasXOffset;
        e.pos[1] += canvasYOffset;
        out.push_back(e);
    }
}

} // namespace

bool ControllerOverlayWindow::LoadGamepad() {
    const std::wstring dir = Paths::Asset(L"Controllers\\" + folder);
    auto jsons = Files::ListFiles(dir, L"*.json");
    std::wstring png;
    for (const auto& f : Files::ListFiles(dir, L"*.png")) {
        const std::wstring n = Text::Lower(Files::FileNameWithoutExtension(f));
        if (!Text::EndsWith(n, L"black") && !Text::EndsWith(n, L"white") && !Text::EndsWith(n, L"clear") && !Text::EndsWith(n, L"show-player")) {
            png = f;
            break;
        }
    }
    if (jsons.empty() || png.empty()) return false;
    sheets[0] = Images::Load(png, 8192);
    auto bytes = Files::ReadBytes(jsons.front());
    if (!sheets[0] || !bytes) return false;
    const Json::Value root = Json::Parse(std::string_view(*bytes));
    canvasW = static_cast<int>(root["overlay_width"].Int());
    canvasH = static_cast<int>(root["overlay_height"].Int());
    ReadElements(root, 0, 0, 0, 0, elements);
    return canvasW > 0 && canvasH > 0;
}

bool ControllerOverlayWindow::LoadKeyboardMouse() {
    const std::wstring base = Paths::Asset(L"Controllers\\keyboard");
    const std::wstring kbJson = Files::Combine(base, L"wasd", layout == L"azerty" ? L"wasd-full-azerty.json" : L"wasd-full.json");
    const std::wstring kbPng = Files::Combine(base, L"wasd", L"wasd.png");
    const std::wstring mouseJson = Files::Combine(base, L"mouse", L"mouse-no-movement.json");
    const std::wstring mousePng = Files::Combine(base, L"mouse", L"mouse.png");
    auto kbBytes = Files::ReadBytes(kbJson), mouseBytes = Files::ReadBytes(mouseJson);
    sheets[0] = Images::Load(kbPng, 8192);
    sheets[1] = Images::Load(mousePng, 8192);
    if (!kbBytes || !mouseBytes || !sheets[0] || !sheets[1]) return false;
    const Json::Value kb = Json::Parse(std::string_view(*kbBytes));
    const Json::Value mouse = Json::Parse(std::string_view(*mouseBytes));
    const int kbW = static_cast<int>(kb["overlay_width"].Int()), kbH = static_cast<int>(kb["overlay_height"].Int());
    const int mW = static_cast<int>(mouse["overlay_width"].Int()), mH = static_cast<int>(mouse["overlay_height"].Int());
    canvasW = kbW + kKeyboardMouseGap + mW;
    canvasH = std::max(kbH, mH);
    // The mouse skin sits to the right of the keyboard, both centred vertically
    ReadElements(kb, 0, 0, 0, (canvasH - kbH) / 2, elements);
    ReadElements(mouse, 1, 0, kbW + kKeyboardMouseGap, (canvasH - mH) / 2, elements);
    return canvasW > 0 && canvasH > 0;
}

bool ControllerOverlayWindow::Open(const std::wstring& corner) {
    const bool loaded = folder == L"keyboard" ? LoadKeyboardMouse() : LoadGamepad();
    std::stable_sort(elements.begin(), elements.end(), [](const Element& a, const Element& b) { return a.z < b.z; });
    for (const auto& el : elements)
        if (el.type == TypeKey && !keyVk.count(el.code)) keyVk[el.code] = MapVirtualKeyW(static_cast<UINT>(el.code), MAPVK_VSC_TO_VK);
    if (!loaded) {
        canvasW = 360;
        canvasH = 219;
    }

    const float width = 360.f, height = 360.f * canvasH / std::max(1, canvasW);
    UI::Window::Options o;
    o.title = L"Controller Overlay";
    o.style = WS_POPUP;
    o.exStyle = WS_EX_TOOLWINDOW;
    o.transparent = true;
    o.clickThrough = true;   // the game must stay "the window under the cursor"
    o.topmost = true;
    o.noActivate = true;
    o.width = static_cast<int>(width);
    o.height = static_cast<int>(height);
    o.showCommand = SW_HIDE;
    if (!Create(o)) return false;

    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const float scale = DpiScale();
    const int w = static_cast<int>(width * scale), h = static_cast<int>(height * scale), margin = static_cast<int>(20 * scale);
    const bool right = corner == L"top-right" || corner == L"bottom-right";
    const bool bottom = corner == L"bottom-left" || corner == L"bottom-right";
    SetWindowPos(hwnd, HWND_TOPMOST, right ? work.right - w - margin : work.left + margin, bottom ? work.bottom - h - margin : work.top + margin,
                 w, h, SWP_NOACTIVATE);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    SetContinuous(true);   // polls input every frame

    if (folder == L"keyboard") {
        // Wheel scrolling can't be polled: flash the wheel sprite briefly from a mouse hook
        mouseHook = Input::Hooks::AddMouse([this](const Input::Hooks::MouseEvent& e) {
            if (e.message == WM_MOUSEWHEEL) {
                wheelFlashIndex = e.wheelDelta > 0 ? 2 : 3;
                wheelFlashUntil = GetTickCount64() + 150;
            }
            return false;
        });
    }
    return true;
}

void ControllerOverlayWindow::Poll(State& s) {
    const auto down = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
    if (folder == L"keyboard") {
        for (const auto& [scan, vk] : keyVk)
            if (down(static_cast<int>(vk))) s.codes.insert(scan);
        // Asset convention: 1 left, 2 right, 4 lower side button, 5 upper side button
        if (down(VK_LBUTTON)) s.codes.insert(1);
        if (down(VK_RBUTTON)) s.codes.insert(2);
        if (down(VK_XBUTTON1)) s.codes.insert(4);
        if (down(VK_XBUTTON2)) s.codes.insert(5);
        s.wheel = down(VK_MBUTTON) ? 1 : GetTickCount64() < wheelFlashUntil.load() ? wheelFlashIndex.load() : 0;
        return;
    }
    try {
        // DualSense: raw HID buttons (the Gamepad view mislabels them), analog values via Gamepad
        for (const auto& raw : wgi::RawGameController::RawGameControllers()) {
            if (raw.HardwareVendorId() != kVendorSony) continue;
            const size_t buttonCount = static_cast<size_t>(raw.ButtonCount());
            std::vector<wgi::GameControllerSwitchPosition> switches(static_cast<size_t>(raw.SwitchCount()));
            std::vector<double> axes(static_cast<size_t>(raw.AxisCount()));
            std::unique_ptr<bool[]> b(new bool[buttonCount + 1]{});
            raw.GetCurrentReading(winrt::array_view<bool>(b.get(), b.get() + buttonCount),
                                  winrt::array_view<wgi::GameControllerSwitchPosition>(switches),
                                  winrt::array_view<double>(axes));
            for (const auto& [index, code] : kPsButtons)
                if (static_cast<size_t>(index) < buttonCount && b[static_cast<size_t>(index)]) s.codes.insert(code);
            const int hat = switches.empty() ? 0 : static_cast<int>(switches[0]);
            using P = wgi::GameControllerSwitchPosition;
            const auto is = [&](std::initializer_list<P> list) {
                for (P p : list)
                    if (hat == static_cast<int>(p)) return true;
                return false;
            };
            if (is({ P::Up, P::UpRight, P::UpLeft })) s.codes.insert(11);
            if (is({ P::Down, P::DownRight, P::DownLeft })) s.codes.insert(12);
            if (is({ P::Left, P::UpLeft, P::DownLeft })) s.codes.insert(13);
            if (is({ P::Right, P::UpRight, P::DownRight })) s.codes.insert(14);
            s.dpad = hat;
            if (auto pad = wgi::Gamepad::FromGameController(raw)) {
                const auto r = pad.GetCurrentReading();
                s.lx = r.LeftThumbstickX;
                s.ly = r.LeftThumbstickY;
                s.rx = r.RightThumbstickX;
                s.ry = r.RightThumbstickY;
                s.lt = r.LeftTrigger;
                s.rt = r.RightTrigger;
            }
            return;
        }
        const auto pads = wgi::Gamepad::Gamepads();
        if (pads.Size() == 0) return;
        const auto r = pads.GetAt(0).GetCurrentReading();
        using B = wgi::GamepadButtons;
        const auto has = [&](B button) { return (r.Buttons & button) == button; };
        if (has(B::A)) s.codes.insert(0);
        if (has(B::B)) s.codes.insert(1);
        if (has(B::X)) s.codes.insert(2);
        if (has(B::Y)) s.codes.insert(3);
        if (has(B::View)) s.codes.insert(4);
        if (has(B::Menu)) s.codes.insert(6);
        if (has(B::LeftThumbstick)) s.codes.insert(7);
        if (has(B::RightThumbstick)) s.codes.insert(8);
        if (has(B::LeftShoulder)) s.codes.insert(9);
        if (has(B::RightShoulder)) s.codes.insert(10);
        const bool du = has(B::DPadUp), dd = has(B::DPadDown), dl = has(B::DPadLeft), dr = has(B::DPadRight);
        if (du) s.codes.insert(11);
        if (dd) s.codes.insert(12);
        if (dl) s.codes.insert(13);
        if (dr) s.codes.insert(14);
        using P = wgi::GameControllerSwitchPosition;
        P dpad = P::Center;
        if (du && dr) dpad = P::UpRight;
        else if (dr && dd) dpad = P::DownRight;
        else if (dd && dl) dpad = P::DownLeft;
        else if (dl && du) dpad = P::UpLeft;
        else if (du) dpad = P::Up;
        else if (dr) dpad = P::Right;
        else if (dd) dpad = P::Down;
        else if (dl) dpad = P::Left;
        s.dpad = static_cast<int>(dpad);
        s.lx = r.LeftThumbstickX;
        s.ly = r.LeftThumbstickY;
        s.rx = r.RightThumbstickX;
        s.ry = r.RightThumbstickY;
        s.lt = r.LeftTrigger;
        s.rt = r.RightTrigger;
    } catch (...) {
    }
}

void ControllerOverlayWindow::DrawSprite(const Element& el, int sx, int sy, float x, float y, float opacity) {
    const ImagePtr& sheet = sheets[el.sheet];
    if (!sheet || opacity <= 0.f) return;
    const D2D1_RECT_F src = D2D1::RectF(static_cast<float>(sx), static_cast<float>(sy), static_cast<float>(sx + el.mapping[2]),
                                        static_cast<float>(sy + el.mapping[3]));
    g.DrawBitmap(sheet->bitmap.Get(), { x, y, static_cast<float>(el.mapping[2]), static_cast<float>(el.mapping[3]) }, opacity, &src);
}

void ControllerOverlayWindow::OnFrame() {
    if (canvasW <= 0 || elements.empty()) return;
    State s;
    Poll(s);
    // Viewbox Stretch="Uniform": the canvas scales with the window
    const float scale = std::min(g.Width() / canvasW, g.Height() / canvasH);
    g.PushTransform(D2D1::Matrix3x2F::Scale(scale, scale) *
                    D2D1::Matrix3x2F::Translation((g.Width() - canvasW * scale) * 0.5f, (g.Height() - canvasH * scale) * 0.5f));
    for (const auto& el : elements) {
        const float px = static_cast<float>(el.pos[0]), py = static_cast<float>(el.pos[1]);
        const int mx = el.mapping[0], my = el.mapping[1], mw = el.mapping[2], mh = el.mapping[3];
        switch (el.type) {
            case TypeWheel: {
                const int step = mw + 3;   // 3 px between frames in the strip
                DrawSprite(el, mx + std::clamp(s.wheel, 0, 3) * step, my, px, py, 1.f);
                break;
            }
            case TypeDpad: {
                const int step = mw + 3;
                const int index = s.dpad >= 0 && s.dpad < 9 ? kDpadSprite[s.dpad] : 0;
                DrawSprite(el, mx + index * step, my, px, py, 1.f);
                break;
            }
            case TypeButton:
            case TypeKey:
            case TypeMouseButton:
            case TypeTrigger: {
                // Inactive (white) sprite always, active (cyan) sprite right below it in the sheet
                DrawSprite(el, mx, my, px, py, 1.f);
                float active = s.codes.count(el.code) ? 1.f : 0.f;
                if (el.type == TypeTrigger) active = static_cast<float>(el.side == 0 ? s.lt : s.rt);
                DrawSprite(el, mx, my + mh, px, py, active);
                break;
            }
            case TypeStick: {
                const double nx = el.side == 0 ? s.lx : s.rx, ny = el.side == 0 ? s.ly : s.ry;
                DrawSprite(el, mx, my, px + static_cast<float>(nx * el.stickRadius), py + static_cast<float>(-ny * el.stickRadius), 1.f);
                break;
            }
            default:
                DrawSprite(el, mx, my, px, py, 1.f);
        }
    }
    g.PopTransform();
}

} // namespace SpeedrunLauncher::Windows
