#pragma once
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Services/Community/SpeedrunComService.h"
#include "UI/Window.h"

namespace SpeedrunLauncher::Windows {

// speedrun.com leaderboards (old LeaderboardOverlay): chapter list on the left, one card per run
// type on the right with its subcategory selectors and the top runners.
class LeaderboardWindow : public UI::Window {
public:
    ~LeaderboardWindow() override { *alive = false; }
    bool Open();

protected:
    void OnFrame() override;
    Color ClearColor() const override { return Colors::Transparent; }

private:
    void SelectChapter(int number);
    void RequestRunners(const Services::SpeedrunCom::RunType& runType);
    float DrawCard(Services::SpeedrunCom::RunType& runType, size_t index, float x, float y, float width, float scrollOffset);

    int chapter = 0;
    std::map<int, std::vector<Services::SpeedrunCom::RunType>> runTypes;   // loaded chapters
    std::set<int> loadingChapters;
    std::map<std::wstring, std::vector<Services::SpeedrunCom::LeaderboardEntry>> runners;   // by query key
    std::set<std::wstring> loadingRunners;
    std::wstring openSelector;   // "<card>|<variable id>" of the open dropdown
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    double openedAt = -1.0;   // first frame (drives the panel entrance)
    int shownChapter = -1;    // chapter whose cards are on screen
    double cardsShownAt = 0.0;   // ...since then (cards rise in one after another)
};

} // namespace SpeedrunLauncher::Windows
