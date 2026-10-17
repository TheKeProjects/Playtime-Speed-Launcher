#pragma once
#include <optional>
#include <string>
#include <vector>

namespace SpeedrunLauncher::Services {

// Key-sequence easter eggs (Konami code, typing "gane", ...). Feed every key press; when a
// sequence completes, the matching video file name (Assets\Videos\EasterEggs) is returned.
class EasterEggSequences {
public:
    EasterEggSequences();
    std::optional<std::wstring> Feed(unsigned vk);

private:
    struct Sequence {
        std::vector<unsigned> keys;
        std::wstring video;
        size_t progress = 0;
    };
    std::vector<Sequence> sequences;
};

} // namespace SpeedrunLauncher::Services
