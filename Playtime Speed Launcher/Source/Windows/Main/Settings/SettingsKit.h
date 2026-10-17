#pragma once
#include <functional>
#include <string>
#include "UI/Layout.h"
#include "UI/Style.h"
#include "UI/Ui.h"
#include "UI/Widgets/Stagger.h"

namespace SpeedrunLauncher::Input {
struct Binding;
}

namespace SpeedrunLauncher::Main {

class MainWindow;

// What a "press a key / mouse button" capture produced.
struct CapturedInput {
    bool escape = false;
    unsigned vk = 0;          // keyboard key (0 for mouse)
    int mouseButton = 0;      // Input::MouseButtons (Middle / XButton1 / XButton2)
    unsigned modifiers = 0;   // Input::Mod* held when the key went down
};

// One capture at a time across the whole settings panel (the old code installed a
// PreviewKeyDown / PreviewMouseDown handler per button). Targets are arbitrary ints.
class KeyCapture {
public:
    // Clicking the capturing button again cancels; clicking another one switches.
    void Toggle(int target, bool allowMouse, std::function<void(const CapturedInput&)> onCaptured);
    void Cancel() { target = 0; handler = nullptr; }
    bool Active(int t) const { return target == t; }
    bool Any() const { return target != 0; }
    // Reads this frame's input; calls the handler once a key / allowed mouse button arrives.
    void Poll(UI::Ui& ui);

private:
    int target = 0;
    bool mouse = false;
    std::function<void(const CapturedInput&)> handler;
};

enum CaptureTarget {
    CaptureCheckpoint = 1, CaptureTutorial, CaptureF11Add,
    CaptureCoresFreeze, CaptureCoresSlower, CaptureCoresNormal,
    CaptureCh1Freeze, CaptureCh1Normal, CaptureCh1Unlit, CaptureCh1Lit,
    CaptureCh5Freeze, CaptureCh5Normal, CaptureCh5Unlit, CaptureCh5Lit,
    CaptureCh4Freeze, CaptureCh4Slow, CaptureCh4Normal,
    CaptureKeyClickAdd, CaptureKeyClickToggle,
    CaptureF11Slot = 1000,       // + binding index
    CaptureKeyClickSlot = 2000,  // + binding index
};

// Everything a tab needs while drawing: the ui, the window, a StackPanel-style column
// (Margin="24,18" inside the tab's scroll viewer) and the shared panel state.
struct SettingsContext {
    UI::Ui& ui;
    MainWindow& main;
    KeyCapture& capture;
    UI::Column col;
    int& loadManipPage;        // 0 hub, 1 Chapter 1, 2 Chapter 4, 3 Chapter 5
    bool& langOpen;
    // Rows slide in from the right one after another, starting at revealStart (tab opened)
    double revealStart = -10.0;
    int revealIndex = 0;
    // Wrap the drawing of one custom row: `auto reveal = c.Reveal();`
    UI::StaggerItem Reveal() { return UI::StaggerItem(ui, revealStart, revealIndex++, 28.f); }

    // ── Row helpers (the repeated XAML patterns) ──
    void Section(const std::wstring& text);                        // 9pt bold teal caption, 10 below
    void Rule(float above = 16.f, float below = 16.f);             // <Border BorderThickness="0,0,0,1"/>
    void Gap(float g) { col.Gap(g); }
    // Label left, ON/OFF toggle right (Height 26). Returns true when clicked.
    bool ToggleRow(UI::Id id, const std::wstring& label, bool on, float labelSize = 12.f);
    // Label left, key-binding button right (Height 30, MinWidth 130).
    bool KeyRow(UI::Id id, const std::wstring& label, const std::wstring& value, bool capturing);
    // Full-width selectable option (Padding 12,10).
    bool Option(UI::Id id, const std::wstring& text, bool selected, float marginBottom = 0.f, float fontSize = 11.f,
                Font font = Font::Mono);
    // Full-width "icon + text" button.
    bool GlyphOption(UI::Id id, wchar_t glyph, const std::wstring& text, float marginBottom = 0.f, bool selected = false);
    bool ImageOption(UI::Id id, const std::wstring& imageAsset, const std::wstring& text, float marginBottom = 0.f);
    // Wrapped text.
    void Paragraph(const std::wstring& text, float size, Color color, float marginTop = 0.f, float marginBottom = 0.f, bool bold = false);
    // Small "< BACK" button (Padding 10,5) at the left.
    bool BackButton(UI::Id id, const std::wstring& text, float marginBottom = 16.f);
    // Options laid out as Grid columns with 8 px gaps; returns the clicked index or -1.
    int OptionGrid(std::string_view idBase, const std::vector<std::wstring>& texts, int selected, int columns, float marginBottom = 0.f,
                   const std::vector<Font>& fonts = {}, const std::vector<float>& sizes = {});
};

// "Enter" / "Ratón 4" / "Sin asignar" for a key-or-mouse binding.
std::wstring BindingText(const Input::Binding& binding);

// Tabs (each in its own file under Settings\Tabs\)
namespace SettingsTabs {
void General(SettingsContext& c);
void Controls(SettingsContext& c);
void LoadManip(SettingsContext& c);
void Cores(SettingsContext& c);
void Steam(SettingsContext& c);
void Controller(SettingsContext& c);
void Discord(SettingsContext& c);
void Updates(SettingsContext& c);
void LiveSplit(SettingsContext& c);
void Overlays(SettingsContext& c);
void IconTheme(SettingsContext& c);
} // namespace SettingsTabs

} // namespace SpeedrunLauncher::Main
