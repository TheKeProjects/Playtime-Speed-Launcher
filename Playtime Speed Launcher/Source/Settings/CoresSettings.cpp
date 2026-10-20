#include "Settings/CoresSettings.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Text.h"
#include "Settings/ConfigFile.h"

namespace SpeedrunLauncher::Settings {

namespace {

// "0,77" → 77 (the value after the last comma)
bool LastNumber(const std::wstring& line, unsigned& out) {
    const auto parts = Text::Split(Text::Trim(line), L',');
    if (parts.empty()) return false;
    const auto v = Text::ToInt(parts.back());
    if (!v || *v < 0) return false;
    out = static_cast<unsigned>(*v);
    return true;
}

} // namespace

Cores& Cores::Current() {
    static Cores current = [] {
        Cores c;
        const auto lines = Files::ReadLines(Paths::DataFile(L"loadmanip_hotkeys.cfg"));
        if (lines.size() >= 3) {
            LastNumber(lines[0], c.normalVk);
            LastNumber(lines[1], c.slowerVk);
            LastNumber(lines[2], c.freezeVk);
        }
        c.enabled = Config::ReadFlag(L"cores_enabled.cfg", false);
        if (auto p = Config::ReadText(L"cores_priority.cfg")) c.priorityHigh = *p != L"low";
        return c;
    }();
    return current;
}

void Cores::SaveHotkeys() const {
    Config::WriteText(L"loadmanip_hotkeys.cfg", L"0," + std::to_wstring(normalVk) + L"\n0," + std::to_wstring(slowerVk) + L"\n0," +
                                                    std::to_wstring(freezeVk));
}

void Cores::SaveEnabled() const { Config::WriteFlag(L"cores_enabled.cfg", enabled); }
void Cores::SavePriority() const { Config::WriteText(L"cores_priority.cfg", priorityHigh ? L"high" : L"low"); }

} // namespace SpeedrunLauncher::Settings
