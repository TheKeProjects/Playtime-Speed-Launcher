#pragma once
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Services/Community/SpeedrunComService.h"
#include "UI/Window.h"
#include "Windows/Standalone/Common/VideoPlayerPanel.h"

namespace SpeedrunLauncher::Windows {

// speedrun.com leaderboards (old LeaderboardOverlay), following the seasonal theme:
//   chapter tabs, then the chapter's run types as tabs with their subcategory selectors;
//   a podium with the top 3 and, under it, the selected runner's details (date, country, comment,
//   links); the full top 10 on the right. A run's video plays inside the window (embedded
//   YouTube player like the skips and glitches tutorials); other sites open in the browser.
class LeaderboardWindow : public UI::Window {
public:
    LeaderboardWindow();
    ~LeaderboardWindow() override;
    bool Open();

protected:
    void OnFrame() override;
    void OnClosing(bool& cancel) override;
    Color ClearColor() const override { return Colors::Transparent; }

private:
    using Entry = Services::SpeedrunCom::LeaderboardEntry;

    void SelectChapter(int number);
    void SelectCategory(int index);
    void RequestRunners(const Services::SpeedrunCom::RunType& runType);
    void BeginClose();
    void PlayVideo(const Entry& entry);
    void CloseVideo();

    void DrawChapterTabs(const Rect& r);
    void DrawCategoryRow(const Rect& r);
    void DrawSelectors(Services::SpeedrunCom::RunType& runType, float right, float cy);
    void DrawPodium(const Rect& r, const std::vector<Entry>* list, double loadedAt);
    void DrawDetails(const Rect& r, const std::vector<Entry>* list);
    void DrawList(const Rect& r, const std::vector<Entry>* list, double loadedAt);
    void DrawVideo(const Rect& screen);
    // Flag + name in the runner's speedrun.com colour; returns the width used
    float DrawRunnerName(const Entry& e, float x, float y, float maxW, float h, const TextStyle& style, float hover = 0.f, bool center = false);

    int chapter = 0;
    std::map<int, std::vector<Services::SpeedrunCom::RunType>> runTypes;   // loaded chapters
    std::set<int> loadingChapters;
    std::map<int, int> category;                                           // selected run type per chapter
    std::map<std::wstring, std::vector<Entry>> runners;                    // by query key
    std::map<std::wstring, double> runnersLoadedAt;                        // podium / rows animate in after a load
    std::set<std::wstring> loadingRunners;
    std::wstring openSelector;          // variable id of the open dropdown
    int selectedRunner = 0;             // index in the current top 10 (details panel)
    double runnerChangedAt = -10.0;     // details slide in when it changes
    double categoryChangedAt = -10.0;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    double openedAt = -1.0;             // pop-in
    double closingAt = -1.0;            // pop-out running (the window closes when it ends)
    bool finished = false;

    // In-window video
    struct Video {
        bool open = false;
        std::wstring title, subtitle;
        double openedAt = 0.0, closingAt = -1.0;
    } video;
    VideoPlayerPanel player;
};

} // namespace SpeedrunLauncher::Windows
