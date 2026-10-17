#pragma once
#include <string>

namespace SpeedrunLauncher::Settings {

// Main window mode. Both keep the window maximized and borderless; "windowed" also shows a
// minimize button in the top bar. (window_mode_settings.json: {"Mode":"actual"})
struct WindowMode {
    std::wstring mode = L"actual";   // "actual" | "windowed"

    bool Windowed() const { return mode == L"windowed"; }
    void Save();
    static WindowMode& Current();
};

} // namespace SpeedrunLauncher::Settings
