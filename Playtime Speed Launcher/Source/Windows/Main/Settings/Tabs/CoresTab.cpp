// Settings → Cores: the affinity load manipulation (enable, Freeze priority, its three hotkeys).
#include "Features/InputRemaps.h"
#include "Input/KeyNames.h"
#include "Localization/Localization.h"
#include "Settings/CoresSettings.h"
#include "UI/Widgets/Controls.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Settings/SettingsKit.h"

namespace SpeedrunLauncher::Main::SettingsTabs {

using namespace UI;

void Cores(SettingsContext& c) {
    auto& ui = c.ui;
    auto& cores = Settings::Cores::Current();

    if (c.ToggleRow(ui.MakeId("cores-enable"), Loc::Get("cores_enable_label"), cores.enabled, 11.f)) {
        cores.enabled = !cores.enabled;
        cores.SaveEnabled();
        c.main.Remaps().RefreshCores();
    }
    c.Gap(16.f);
    c.Rule(0.f, 16.f);

    // Process priority while frozen (0 cores)
    c.Section(Loc::Get("cores_priority_section"));
    {
        auto reveal = c.Reveal();
        const Rect row = c.col.Next(26.f, 16.f);
        ui.g.Text(Loc::Get("cores_priority_label"), row, Palette::Label, Mono(12.f));
        int selected = cores.priorityHigh ? 0 : 1;
        if (ComboBox(ui, ui.MakeId("cores-priority"), RightAligned(row, 120.f, 26.f), selected,
                     { Loc::Get("cores_priority_high"), Loc::Get("cores_priority_low") })) {
            cores.priorityHigh = selected == 0;
            cores.SavePriority();
        }
    }
    c.Rule(0.f, 16.f);

    // Hotkeys
    c.Section(Loc::Get("cores_section"));
    c.Paragraph(Loc::Get("cores_warning"), 10.f, Palette::Warning, 0.f, 10.f, true);
    unsigned* keys[3] = { &cores.freezeVk, &cores.slowerVk, &cores.normalVk };
    const char* labels[3] = { "cores_freeze_label", "cores_slower_label", "cores_normal_label" };
    for (int i = 0; i < 3; ++i) {
        if (i) c.Gap(8.f);
        const int target = CaptureCoresFreeze + i;
        const bool capturing = c.capture.Active(target);
        const std::wstring text = capturing ? Loc::Get("hotkey_press_keys") : ::SpeedrunLauncher::Input::KeyNames::Upper(*keys[i]);
        if (c.KeyRow(ui.MakeId("cores-key", i), Loc::Get(labels[i]), text, capturing)) {
            unsigned* key = keys[i];
            auto* remaps = &c.main.Remaps();
            c.capture.Toggle(target, false, [key, remaps](const CapturedInput& in) {
                if (in.escape) return;
                *key = in.vk;
                Settings::Cores::Current().SaveHotkeys();
                remaps->RefreshCores();
            });
        }
    }
}

} // namespace SpeedrunLauncher::Main::SettingsTabs
