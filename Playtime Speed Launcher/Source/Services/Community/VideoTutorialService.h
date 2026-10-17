#pragma once
#include <optional>
#include <string>
#include <vector>

// The skip / route tutorial catalogue shown by the video tutorial overlay (F9).
namespace SpeedrunLauncher::Services::VideoTutorials {

struct TutorialVideo {
    std::wstring id;
    std::wstring title;
    std::wstring description;
    std::wstring url;
    std::wstring category = L"General";
    std::wstring chapter;
    std::vector<std::wstring> runCategories;
    std::wstring version;
    std::vector<std::wstring> routes;
    std::wstring restrictions;
    std::wstring author;
    bool cores = false;
    bool openInBrowser = false;
};

inline bool FlatList = true;

const std::vector<TutorialVideo>& All();   // VideoTutorialData.cpp

// YouTube video id from a youtu.be / watch?v= URL.
std::optional<std::wstring> ExtractVideoId(const std::wstring& url);

} // namespace SpeedrunLauncher::Services::VideoTutorials
