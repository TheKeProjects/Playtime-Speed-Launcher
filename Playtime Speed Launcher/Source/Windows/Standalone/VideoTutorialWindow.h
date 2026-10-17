#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Services/Community/VideoTutorialService.h"
#include "UI/Layout.h"
#include "UI/Window.h"
#include "Windows/Standalone/Common/VideoPlayerPanel.h"

namespace SpeedrunLauncher::Windows {

class ReportSkipWindow;

// "SKIPS AND GLITCHES" (F9, old VideoTutorialOverlay): searchable / filterable list of tutorial
// videos on the left, the embedded player and the video's info on the right.
//   VideoTutorialWindow.cpp  frame, player, info, report button
//   VideoTutorialList.cpp    search box, filter pills, grouped video list
class VideoTutorialWindow : public UI::Window {
public:
    using Video = Services::VideoTutorials::TutorialVideo;
    VideoTutorialWindow();
    ~VideoTutorialWindow() override;
    bool Open();

protected:
    void OnFrame() override;
    Color ClearColor() const override { return Colors::Transparent; }

private:
    // VideoTutorialList.cpp
    void DrawSidebar(const Rect& area);
    void DrawFilters(UI::Column& col);
    const std::vector<const Video*>& Filtered();
    void RefreshFilterLists();
    void DrawVideoList(const Rect& area);
    void DrawVideoItem(UI::Column& col, const Video& video, int index);

    // VideoTutorialWindow.cpp
    void Select(const Video& video);
    float DrawInfo(const Rect& area);   // returns its height

    VideoPlayerPanel player;
    std::wstring search;
    std::optional<std::wstring> chapterFilter, categoryFilter, runCategoryFilter, versionFilter, routeFilter, restrictionsFilter;
    std::wstring selectedId, loadingId;
    const Video* current = nullptr;
    std::unique_ptr<ReportSkipWindow> report;
    float infoHeight = 90.f;
    double openedAt = -1.0;   // first frame (drives the panel entrance)
    double listShownAt = 0.0; // the filtered list changed (rows slide in again)

    // Everything derived from the ~200-video catalogue is cached and rebuilt only when its inputs
    // change (it used to be recomputed on every frame)
    struct FilterLists {
        bool built = false;
        std::optional<std::wstring> chapter;   // chapter filter the lists were built for
        bool structured = false;
        std::vector<std::wstring> chapters, runCats, versions, routes, special, restrictions, categories;
    } lists;
    std::wstring filteredKey = L"\x1F<none>";
    std::vector<const Video*> filteredVideos;
    float visibleTop = 0.f, visibleBottom = 0.f;   // list viewport in content coordinates (rows outside aren't drawn)
};

} // namespace SpeedrunLauncher::Windows
