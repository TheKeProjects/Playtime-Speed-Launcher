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

        const auto remap = Files::ReadLines(Paths::DataFile(L"chapter4_remap_hotkeys.cfg"));
        if (remap.size() >= 3) {
            if (auto b = Input::Binding::Parse(remap[0])) c.chapter4Freeze = *b;
            if (auto b = Input::Binding::Parse(remap[1])) c.chapter4Slow = *b;
            if (auto b = Input::Binding::Parse(remap[2])) c.chapter4Normal = *b;
        }
        c.chapter4Enabled = Config::ReadFlag(L"chapter4_remap_enabled.cfg", false);
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

void Cores::SaveChapter4Hotkeys() const {
    Config::WriteText(L"chapter4_remap_hotkeys.cfg",
                      chapter4Freeze.Token() + L"\n" + chapter4Slow.Token() + L"\n" + chapter4Normal.Token());
}

void Cores::SaveChapter4Enabled() const { Config::WriteFlag(L"chapter4_remap_enabled.cfg", chapter4Enabled); }

} // namespace SpeedrunLauncher::Settings
