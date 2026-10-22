#include "Services/App/EasterEggSequences.h"
#include <windows.h>

namespace SpeedrunLauncher::Services {

EasterEggSequences::EasterEggSequences() {
    const auto letters = [](const char* s) {
        std::vector<unsigned> keys;
        for (; *s; ++s) keys.push_back(static_cast<unsigned>(*s));
        return keys;
    };
    sequences = {
        { { VK_UP, VK_UP, VK_DOWN, VK_DOWN, VK_LEFT, VK_RIGHT, VK_LEFT, VK_RIGHT, 'B', 'A', VK_RETURN }, L"Torrente.mp4" },
        { letters("GANE"), L"Gane.mp4" },
        { letters("RAULBECAS"), L"RaulBecas.mp4" },
        { letters("TOMH"), L"Tomh.mp4" },
        { letters("PICHA"), L"Picha.mp4" },
        { letters("GOKU"), L"Goku.mp4" },
        { letters("CATNAP"), L"Catnap.mp4" },
        { letters("BANME"), L"Aritro.png" },
        { letters("OHNO"), L"Ohno.mp4" },
    };
}

std::optional<std::wstring> EasterEggSequences::Feed(unsigned vk) {
    std::optional<std::wstring> completed;
    for (auto& s : sequences) {
        if (vk != s.keys[s.progress]) {
            s.progress = vk == s.keys[0] ? 1 : 0;
            continue;
        }
        if (++s.progress < s.keys.size()) continue;
        s.progress = 0;
        if (!completed) completed = s.video;
    }
    return completed;
}

} // namespace SpeedrunLauncher::Services
