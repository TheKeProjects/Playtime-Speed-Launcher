#include "Windows/Main/Overlays/Mods/CustomNameOverlay.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include "App/Launcher.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/Text.h"
#include "Services/GameFiles/CustomNameFiles.h"
#include "Settings/CustomNameSettings.h"
#include "UI/Layout.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/Stagger.h"
#include "UI/Widgets/TextBox.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
namespace K = UI::MenuKit;
namespace CN = Services::CustomNameFiles;
using Settings::CustomName;

namespace {

constexpr float kWidth = 640.f;
constexpr Color kAccent = Hex(0xFF5FA2);   // the mod's own pink, apart from the teal of the rest

struct Animation {
    const wchar_t* key;     // value in config.ini
    const wchar_t* label;
    bool usesColor2;
};
constexpr Animation kAnimations[] = {
    { L"none", L"NONE", false },     { L"rainbow", L"RAINBOW", false }, { L"gradient", L"GRADIENT", true },
    { L"pulse", L"PULSE", false },   { L"blink", L"BLINK", false },     { L"wave", L"WAVE", false },
    { L"shake", L"SHAKE", false },   { L"wobble", L"WOBBLE", false },   { L"glitch", L"GLITCH", true },
    { L"fire", L"FIRE", false },     { L"neon", L"NEON", false },
};

// The nameplate font (Anonymous Pro) only has Regular, Bold, Italic and BoldItalic
constexpr const wchar_t* kTypefaces[] = { L"Regular", L"Bold" };

constexpr uint32_t kSwatches[] = { 0xFFFFFF, 0xFF3B3B, 0xFF8A00, 0xFFD400, 0x7CFF4F, 0x00E0A0, 0x00CCFF,
                                   0x3B6BFF, 0x9B4BFF, 0xFF4FD8, 0xFF5FA2, 0x9A9A9A, 0x202020 };

Color FromRgb(uint32_t rgb, float a = 1.f) { return Hex(rgb & 0xFFFFFF, a); }

Color Hsv(float h, float s, float v) {
    h = h - std::floor(h);
    const int i = static_cast<int>(h * 6.f) % 6;
    const float f = h * 6.f - std::floor(h * 6.f);
    const float p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
    switch (i) {
        case 0: return { v, t, p };
        case 1: return { q, v, p };
        case 2: return { p, v, t };
        case 3: return { p, q, v };
        case 4: return { t, p, v };
        default: return { v, p, q };
    }
}

// Same random for the whole 33 ms step: the game animates 30 times a second
float StepRandom(double t, int salt) {
    uint32_t x = static_cast<uint32_t>(std::floor(t * 30.0)) * 2654435761u + static_cast<uint32_t>(salt) * 40503u;
    x ^= x >> 13;
    x *= 0x5bd1e995u;
    x ^= x >> 15;
    return (x & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
}

// The look of the name at time t: a copy of Effect() in the Lua mod
struct Fx {
    Color color;
    float alpha = 1.f, dx = 0.f, dy = 0.f, scale = 1.f, angle = 0.f;
};
Fx Effect(const CustomName& c, double time) {
    const float t = static_cast<float>(time), s = c.speed;
    const Color c1 = FromRgb(c.color), c2 = FromRgb(c.color2);
    Fx fx{ c1 };
    const std::wstring& a = c.animation;
    if (a == L"rainbow") {
        fx.color = Hsv(t * 0.25f * s, 0.75f, 1.f);
    } else if (a == L"gradient") {
        fx.color = Lerp(c1, c2, (std::sin(t * 2.2f * s) + 1.f) / 2.f);
    } else if (a == L"pulse") {
        const float k = (std::sin(t * 4.f * s) + 1.f) / 2.f;
        fx.scale = 1.f + 0.10f * k;
        fx.color = Lerp(c1, Colors::White, 0.25f * k);
    } else if (a == L"blink") {
        fx.alpha = 0.3f + 0.7f * std::pow((std::sin(t * 5.f * s) + 1.f) / 2.f, 0.4f);
    } else if (a == L"wave") {
        fx.dy = std::sin(t * 4.f * s) * 4.f;
        fx.angle = std::cos(t * 4.f * s) * 2.f;
    } else if (a == L"shake") {
        const float m = 1.6f * std::min(s, 2.f);
        fx.dx = (StepRandom(time, 1) * 2.f - 1.f) * m;
        fx.dy = (StepRandom(time, 2) * 2.f - 1.f) * m;
        fx.angle = (StepRandom(time, 3) * 2.f - 1.f) * 2.f;
    } else if (a == L"wobble") {
        fx.angle = std::sin(t * 3.f * s) * 4.f;
        fx.scale = 1.f + 0.04f * std::sin(t * 6.f * s);
    } else if (a == L"glitch") {
        if (StepRandom(time, 4) < 0.12f * std::min(s, 2.f)) {
            fx.dx = (StepRandom(time, 5) * 2.f - 1.f) * 5.f;
            fx.color = StepRandom(time, 6) < 0.5f ? c2 : Color{ 1.f, 0.15f, 0.35f };
            fx.alpha = 0.75f;
        }
    } else if (a == L"fire") {
        const float n = (std::sin(t * 9.f * s) + std::sin(t * 13.7f * s + 1.3f) + 2.f) / 4.f;
        fx.color = Lerp(Color{ 1.f, 0.25f, 0.05f }, Color{ 1.f, 0.85f, 0.2f }, n * 0.8f + StepRandom(time, 7) * 0.2f);
        fx.dy = -n * 1.5f;
    } else if (a == L"neon") {
        const float flick = StepRandom(time, 8) < 0.027f ? 0.35f : 1.f;
        fx.alpha = flick * (0.85f + 0.15f * std::sin(t * 8.f * s));
        fx.color = Lerp(c1, Colors::White, 0.15f);
    }
    return fx;
}

std::vector<std::wstring> EfpVersionExes() {
    std::vector<std::wstring> exes;
    for (const auto& game : App::L().multiplayers)
        if (!game.presets.empty())
            for (const auto& custom : App::L().store.Customs(game.playtimeId)) exes.push_back(custom.exePath);
    return exes;
}

void Caption(Ui& ui, Column& col, const std::wstring& text) {
    const Rect r = col.Next(14.f, 6.f);
    ui.g.FillRound({ r.x, r.y + 6.f, 10.f, 2.f }, 1.f, kAccent.WithAlpha(0.8f));
    ui.g.Text(text, r.CutLeft(18.f), K::kDim, Mono(9.5f, true));
}

// Wrapping chips; returns the clicked index (-1 = none)
int Chips(Ui& ui, Column& col, const char* id, const std::vector<std::wstring>& texts, const std::vector<bool>& selected) {
    constexpr float h = 26.f, gap = 6.f;
    float x = col.x, y = col.y;
    int clicked = -1;
    for (size_t i = 0; i < texts.size(); ++i) {
        const float w = ui.g.Measure(texts[i], Mono(10.f, true)).w + 24.f;
        if (x + w > col.x + col.w && x > col.x) {
            x = col.x;
            y += h + gap;
        }
        if (Chip(ui, ui.MakeId(id, static_cast<int64_t>(i)), { x, y, w, h }, texts[i], selected[i], kAccent)) clicked = static_cast<int>(i);
        x += w + gap;
    }
    col.Next(y + h - col.y, 14.f);
    return clicked;
}

// A row of colour dots; returns true when one was picked
bool Swatches(Ui& ui, Column& col, const char* id, uint32_t& value) {
    constexpr float d = 24.f, gap = 8.f;
    const Rect row = col.Next(d, 14.f);
    bool changed = false;
    for (size_t i = 0; i < std::size(kSwatches); ++i) {
        const Rect r{ row.x + i * (d + gap), row.y, d, d };
        const Interaction it = ui.Interact(ui.MakeId(id, static_cast<int64_t>(i)), r);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        const bool on = (value & 0xFFFFFF) == kSwatches[i];
        const float lift = ui.Animate(ui.MakeId(id, static_cast<int64_t>(100 + i)), it.hovered || on ? 1.f : 0.f, 16.f);
        const Color c = FromRgb(kSwatches[i]);
        if (on) ui.g.GlowRound(r, d * 0.5f, c, 10.f, 0.5f);
        ui.g.FillEllipse(r.CX(), r.CY(), d * 0.5f - 3.f + 2.f * lift, d * 0.5f - 3.f + 2.f * lift, c);
        ui.g.StrokeEllipse(r.CX(), r.CY(), d * 0.5f - 3.f + 2.f * lift, d * 0.5f - 3.f + 2.f * lift, Rgba(255, 255, 255, on ? 230 : 40), on ? 2.f : 1.f);
        if (it.clicked && !on) {
            value = kSwatches[i];
            changed = true;
        }
    }
    return changed;
}

} // namespace

void CustomNameOverlay::Open() {
    auto& l = App::L();
    const auto& game = l.SelectedMultiplayer();
    win64.reset();
    versionName.clear();
    if (auto exe = l.ActiveExePath(game); exe && l.UsesOldVersion(game)) {
        win64 = CN::Win64ForExe(*exe);
        versionName = Files::FileNameWithoutExtension(*exe);
        for (const auto& custom : l.store.Customs(game.playtimeId))
            if (custom.exePath == *exe) versionName = custom.name;
    }
    installed = win64 && CN::IsInstalled(*win64);
    message.clear();
    dirty = true;   // syncs once on opening: brings the versions' script up to date after a launcher update
    Flush();
    popup.Open();
}

void CustomNameOverlay::Changed(Ui& ui) {
    dirty = true;
    dirtyAt = ui.Time();
}

void CustomNameOverlay::Flush() {
    if (!dirty) return;
    dirty = false;
    const CustomName& c = CustomName::Current();
    c.Save();
    Async::Run([exes = EfpVersionExes(), config = c.ToGameConfig()] {
        CN::SyncAll(exes, config);
        return true;
    }, [](bool) {});
}

void CustomNameOverlay::Toggle() {
    if (busy || !win64) return;
    Flush();
    busy = true;
    const std::wstring dir = *win64;
    const bool remove = installed;
    const std::wstring config = CustomName::Current().ToGameConfig();
    Async::Run([dir, remove, config] {
        const ULONGLONG begin = GetTickCount64();
        CN::Result r = remove ? CN::Uninstall(dir) : CN::Install(dir, config);
        const ULONGLONG spent = GetTickCount64() - begin;
        if (spent < 600) Sleep(static_cast<DWORD>(600 - spent));   // the spinner doesn't just flash
        return r;
    }, [this](CN::Result r) {
        busy = false;
        installed = win64 && CN::IsInstalled(*win64);
        message = r.message;
        messageOk = r.ok;
        MessageBeep(r.ok ? MB_ICONINFORMATION : MB_ICONERROR);
        main.RefreshInfo();
    });
}

void CustomNameOverlay::DrawPreview(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    const CustomName& c = CustomName::Current();
    g.Border(r, 10.f, Rgba(4, 9, 14), kAccent.WithAlpha(0.35f), 1.f);
    g.PushRoundClip(r, 10.f);
    // Faint scan lines, like a game HUD behind the name
    for (float y = r.y + 3.f; y < r.B(); y += 4.f) g.FillRect({ r.x, y, r.w, 1.f }, Rgba(255, 255, 255, 6));

    const std::wstring before = [] {
        wchar_t buf[MAX_COMPUTERNAME_LENGTH + 1] = {};
        DWORD n = MAX_COMPUTERNAME_LENGTH + 1;
        const std::wstring pc = GetComputerNameW(buf, &n) ? std::wstring(buf, n) : L"DESKTOP";
        return pc + L"-4F2A9C1E7B";
    }();
    g.Text(L"BEFORE", { r.x + 14.f, r.y + 10.f, 80.f, 14.f }, K::kDim, Mono(9.f, true));
    TextStyle strike = Mono(10.f);
    g.Text(before, { r.x + 70.f, r.y + 10.f, r.w - 84.f, 14.f }, Rgba(140, 160, 175, 150), strike.Ellipsis());
    const float bw = std::min(g.Measure(before, strike).w, r.w - 84.f);
    g.FillRect({ r.x + 70.f, r.y + 17.f, bw, 1.f }, Rgba(204, 51, 51, 200));

    const bool placeholder = Text::Trim(c.name).empty();
    std::wstring name = placeholder ? L"Your name" : Text::Trim(c.name);
    if (c.style && c.letterCase == L"upper") name = Text::Upper(name);
    else if (c.style && c.letterCase == L"lower") name = Text::Lower(name);
    if (c.style && c.spacing) {
        std::wstring wide;
        for (size_t i = 0; i < name.size(); ++i) {
            if (i) wide += L'\x200A';   // hair space
            wide += name[i];
        }
        name = wide;
    }
    TextStyle style;
    style.font = Font::Mono;   // the nameplates use a monospaced font (Anonymous Pro)
    style.size = 30.f;
    if (c.style) {
        if (Text::Equals(c.typeface, L"Bold")) style.weight = DWRITE_FONT_WEIGHT_BOLD;
        style.italic = c.italic;
    }
    style.Center();
    Fx fx = c.style ? Effect(c, ui.Time()) : Fx{ Colors::White };
    if (!c.style) fx.color = Rgba(230, 236, 242);
    const float scale = (c.style ? c.size : 1.f) * fx.scale;
    const Rect area{ r.x, r.y + 24.f, r.w, r.h - 24.f };
    const float cx = area.CX(), cy = area.CY();
    g.PushTransform(D2D1::Matrix3x2F::Scale(scale, scale, { cx, cy }) *
                    D2D1::Matrix3x2F::Rotation(fx.angle, { cx, cy }) * D2D1::Matrix3x2F::Translation(fx.dx, fx.dy));
    const Color color = placeholder ? Rgba(120, 140, 155) : fx.color;
    if (c.style && !placeholder) {
        if (c.shadow) g.Text(name, area.Offset(2.f, 2.f), Color{ 0.f, 0.f, 0.f, 0.75f * fx.alpha }, style);
        if (c.outline)
            for (int i = 0; i < 8; ++i) {
                const float a = i * 3.14159265f / 4.f;
                g.Text(name, area.Offset(std::cos(a) * 1.6f, std::sin(a) * 1.6f), Color{ 0.f, 0.f, 0.f, fx.alpha }, style);
            }
    }
    g.Text(name, area, color.WithAlpha(fx.alpha * (placeholder ? 0.6f : 1.f)), style);
    g.PopTransform();
    g.PopLayer();
    if (c.style && c.animation != L"none") ui.KeepAnimating();
}

void CustomNameOverlay::Draw(Ui& ui, const Rect& screen) {
    auto& g = ui.g;
    CustomName& c = CustomName::Current();
    if (dirty && ui.Time() - dirtyAt > 0.5) Flush();
    if (dirty) ui.KeepAnimatingThrottled();   // so the flush above runs without waiting for input

    PanelStyle style;
    style.title = L"[ CUSTOM NAME ]";
    style.titleColor = kAccent;
    OverlayFrame frame(ui, ui.MakeId("customname"), screen, kWidth, 0.f, style, popup.Opacity(), popup.Scale());
    const double start = frame.OpenedAt() + 0.12;
    Column col(frame.body.x + 26.f, frame.body.y + 20.f, frame.body.w - 52.f);
    int row = 0;

    // Hero: badge, title, version and whether the mod is on in it
    {
        StaggerItem in(ui, start, row++, 0.f, 10.f);
        const Rect r = col.Next(K::kHeroH, 14.f);
        const std::wstring sub = win64 ? L"Escape From Playtime \x00B7 " + versionName : L"Pick an old version first";
        K::Hero(ui, r.CutRight(150.f), Glyphs::People, L"Your in-game name", sub, kAccent);
        const std::wstring chip = !win64 ? L"NO VERSION" : installed ? L"ON IN THIS VERSION" : L"OFF IN THIS VERSION";
        const Color chipColor = !win64 ? K::kAmber : installed ? K::kGreen : K::kDim;
        const float cw = g.Measure(chip, Mono(9.f, true)).w + 26.f;
        K::Chip(ui, r.R() - cw, r.CY(), chip, chipColor);
    }

    // Live preview
    {
        StaggerItem in(ui, start, row++, 0.f, 10.f);
        DrawPreview(ui, col.Next(104.f, 16.f));
    }

    // Name
    {
        StaggerItem in(ui, start, row++, 0.f, 10.f);
        Caption(ui, col, L"NAME");
        TextBoxStyle s;
        s.text = Mono(13.f);
        s.foreground = Palette::Bright;
        s.background = Palette::HeaderBg;
        s.border = Palette::ButtonBorder;
        s.caret = kAccent;
        s.padding = { 10.f, 7.f };
        s.maxLength = CustomName::kMaxName;
        s.placeholder = L"Type the name the other players will see";
        if (Form::Field(ui, ui.MakeId("customname-name"), col.Next(TextBoxHeight(ui, s), 18.f), c.name, s, kAccent)) Changed(ui);
    }

    // Style switches
    {
        StaggerItem in(ui, start, row++, 0.f, 10.f);
        Caption(ui, col, L"STYLE");
        const std::vector<std::wstring> texts = { L"GAME'S OWN LOOK", L"SHADOW", L"OUTLINE" };
        const std::vector<bool> on = { !c.style, c.style && c.shadow, c.style && c.outline };
        switch (Chips(ui, col, "customname-style", texts, on)) {
            case 0: c.style = !c.style; Changed(ui); break;
            case 1: c.style = true; c.shadow = !c.shadow; Changed(ui); break;
            case 2: c.style = true; c.outline = !c.outline; Changed(ui); break;
            default: break;
        }
    }

    if (!c.style) g.PushOpacity(0.4f);
    // Text style: typeface · italic · spacing
    {
        StaggerItem in(ui, start, row++, 0.f, 10.f);
        Caption(ui, col, L"TEXT STYLE");
        std::vector<std::wstring> texts;
        std::vector<bool> on;
        for (const wchar_t* t : kTypefaces) {
            texts.push_back(Text::Upper(t));
            on.push_back(Text::Equals(c.typeface, t, true));
        }
        texts.insert(texts.end(), { L"ITALIC", L"WIDE SPACING" });
        on.insert(on.end(), { c.italic, c.spacing });
        const int picked = Chips(ui, col, "customname-text", texts, on);
        const int faces = static_cast<int>(std::size(kTypefaces));
        if (picked >= 0) {
            if (picked < faces) c.typeface = kTypefaces[picked];
            else if (picked == faces) c.italic = !c.italic;
            else c.spacing = !c.spacing;
            c.style = true;
            Changed(ui);
        }
    }

    // Letters: the game shows nameplates in UPPERCASE; this wins over it
    {
        StaggerItem in(ui, start, row++, 0.f, 10.f);
        Caption(ui, col, L"LETTERS");
        static constexpr const wchar_t* kCases[] = { L"typed", L"upper", L"lower" };
        const std::vector<std::wstring> texts = { L"AS TYPED", L"UPPERCASE", L"lowercase" };
        std::vector<bool> on;
        for (const wchar_t* k : kCases) on.push_back(c.letterCase == k);
        const int picked = Chips(ui, col, "customname-case", texts, on);
        if (picked >= 0) {
            c.letterCase = kCases[picked];
            c.style = true;
            Changed(ui);
        }
    }

    // Colours
    {
        StaggerItem in(ui, start, row++, 0.f, 10.f);
        bool usesColor2 = false;
        for (const auto& a : kAnimations)
            if (c.animation == a.key) usesColor2 = a.usesColor2;
        Caption(ui, col, usesColor2 ? L"COLOUR 1" : L"COLOUR");
        if (Swatches(ui, col, "customname-color", c.color)) { c.style = true; Changed(ui); }
        if (usesColor2) {
            Caption(ui, col, L"COLOUR 2");
            if (Swatches(ui, col, "customname-color2", c.color2)) { c.style = true; Changed(ui); }
        }
    }

    // Animation
    {
        StaggerItem in(ui, start, row++, 0.f, 10.f);
        Caption(ui, col, L"ANIMATION");
        std::vector<std::wstring> texts;
        std::vector<bool> on;
        for (const auto& a : kAnimations) {
            texts.push_back(a.label);
            on.push_back(c.animation == a.key);
        }
        const int picked = Chips(ui, col, "customname-anim", texts, on);
        if (picked >= 0) {
            c.animation = kAnimations[picked].key;
            c.style = true;
            Changed(ui);
        }
    }

    // Size · speed
    {
        StaggerItem in(ui, start, row++, 0.f, 10.f);
        const Rect r = col.Next(40.f, 14.f);
        const Rect left = GridCell(r, 2, 0, 24.f), right = GridCell(r, 2, 1, 24.f);
        g.Text(L"SIZE  " + Text::FromDouble(c.size, 2) + L"x", left.Top(14.f), K::kDim, Mono(9.5f, true));
        if (Slider(ui, ui.MakeId("customname-size"), { left.x, left.y + 18.f, left.w, 20.f }, c.size, 0.8f, 1.6f, 0.05f)) { c.style = true; Changed(ui); }
        g.Text(L"ANIMATION SPEED  " + Text::FromDouble(c.speed, 2) + L"x", right.Top(14.f), K::kDim, Mono(9.5f, true));
        if (Slider(ui, ui.MakeId("customname-speed"), { right.x, right.y + 18.f, right.w, 20.f }, c.speed, 0.25f, 3.f, 0.05f)) { c.style = true; Changed(ui); }
    }
    if (!c.style) g.PopLayer();

    // What the others see, and the last add / remove result
    {
        StaggerItem in(ui, start, row++, 0.f, 10.f);
        const std::wstring note = message.empty()
            ? L"The name is sent to the other players in the match. Colours, size and animations only show on your screen. "
              L"Changes reach every version with Custom Name on, even while the game is open."
            : message;
        const Color accent = message.empty() ? kAccent : messageOk ? K::kGreen : K::kRed;
        const wchar_t glyph = message.empty() ? Glyphs::Info : messageOk ? Glyphs::CheckMark : Glyphs::Warning;
        const float h = K::NoteHeight(ui, note, col.w);
        K::Note(ui, col.Next(h, 4.f), glyph, note, accent);
    }
    frame.SetContentHeight(20.f + col.Height() + 12.f);

    // Footer: BACK · turn it on / off in this version
    const std::wstring back = L"BACK";
    const float bw = K::FooterButtonWidth(ui, back, Glyphs::Back, 110.f);
    if (K::FooterButton(ui, ui.MakeId("customname-back"), { frame.footer.x + 14.f, frame.footer.CY() - 18.f, bw, 36.f }, back, K::Kind::Secondary, true,
                        Glyphs::Back) ||
        (popup.IsOpen() && ui.Focus() == 0 && ui.KeyPressed(VK_ESCAPE)) || (popup.IsOpen() && frame.BackdropClicked())) {
        Flush();
        popup.Close();
    }
    const std::wstring action = busy ? (installed ? L"REMOVING\x2026" : L"ADDING\x2026") : installed ? L"TURN OFF IN THIS VERSION" : L"TURN ON IN THIS VERSION";
    const wchar_t glyph = installed ? Glyphs::Delete : Glyphs::Install;
    const float aw = K::FooterButtonWidth(ui, action, glyph, 220.f);
    const Rect ab{ frame.footer.R() - 14.f - aw, frame.footer.CY() - 18.f, aw, 36.f };
    const bool canTurnOn = installed || !Text::Trim(c.name).empty();
    if (K::FooterButton(ui, ui.MakeId("customname-toggle"), ab, action, installed ? K::Kind::Danger : K::Kind::Primary, win64 && !busy && canTurnOn, glyph,
                        kAccent))
        Toggle();
    if (busy) {
        Form::Spinner(ui, ab.x - 18.f, ab.CY(), 7.f, kAccent);
        ui.KeepAnimating();
    }
}

} // namespace SpeedrunLauncher::Main
