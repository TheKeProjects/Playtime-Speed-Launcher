#include "Services/Community/VideoTutorialService.h"
#include <regex>

namespace SpeedrunLauncher::Services::VideoTutorials {

std::optional<std::wstring> ExtractVideoId(const std::wstring& url) {
    static const std::wregex shortUrl(LR"re(youtu\.be/([A-Za-z0-9_-]+))re");
    static const std::wregex longUrl(LR"re([?&]v=([A-Za-z0-9_-]+))re");
    std::wsmatch m;
    if (std::regex_search(url, m, shortUrl) || std::regex_search(url, m, longUrl)) return m[1].str();
    return std::nullopt;
}

} // namespace SpeedrunLauncher::Services::VideoTutorials
