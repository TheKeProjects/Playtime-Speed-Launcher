#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "UI/Layout.h"
#include "UI/Widgets/Form.h"
#include "UI/Window.h"

namespace SpeedrunLauncher::Windows {

// "REPORT A SKIP": suggest a skip / glitch for the tutorial list (sent to the review webhook).
// Modal over the Skips and Glitches window, laid out like the bug report: an animated banner, the
// three section cards (basics, categories, details) on the left sliding in one by one, and on the
// right a checklist that ticks itself as the report fills in, the status and the send button.
class ReportSkipWindow : public UI::Window {
public:
    ~ReportSkipWindow() override { *alive = false; }
    bool Open(HWND owner);

protected:
    void OnFrame() override;
    void OnClosing(bool& cancel) override;
    Color ClearColor() const override { return Colors::Transparent; }

private:
    void Send();
    void BeginClose();
    // A section card: caption on top, rows drawn by `draw` into the card's column.
    void SectionCard(UI::Column& col, int index, wchar_t glyph, const std::wstring& title, const std::function<void(UI::Column&)>& draw);
    void RefreshOptions();

    // The pill options come from the ~200-video catalogue: built once, and again only when the
    // chapter (which scopes versions / routes) or the custom run categories change
    struct OptionLists {
        bool built = false;
        std::optional<std::wstring> chapter;
        size_t customCount = 0;
        std::vector<std::wstring> chapters, versions, runCategories, routes, restrictions;
    } options;

    HWND ownerWindow = nullptr;
    std::wstring name, videoUrl, author, description;
    std::optional<std::wstring> chapter;
    std::vector<std::wstring> versions, runCategories, routes, restrictions;   // selected
    std::vector<std::wstring> customRunCategories;
    bool addingCustom = false, customHadFocus = false;
    std::wstring customText;
    bool cores = false;
    std::wstring status;
    UI::Form::Status statusKind = UI::Form::Status::Info;
    bool sending = false, sent = false;
    double openedAt = -10.0, closingAt = -1.0;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

} // namespace SpeedrunLauncher::Windows
