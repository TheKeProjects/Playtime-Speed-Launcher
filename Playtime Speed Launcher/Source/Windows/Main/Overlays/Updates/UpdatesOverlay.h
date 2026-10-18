#pragma once
#include <optional>
#include <string>
#include <vector>
#include "Services/App/ChangelogData.h"
#include "Services/App/UpdateService.h"
#include "UI/Layout.h"
#include "UI/Widgets/Panel.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// Launcher updates: the check view (current version, status banner, GameBanana banner, release
// details, manual-update link) and the install view: every version between the installed one and
// the new one as a folding card (the newest open), then the download progress.
class UpdatesOverlay : public Overlay {
public:
    using Overlay::Overlay;
    bool Visible() const override { return popup.Visible(); }
    void Draw(UI::Ui& ui, const Rect& screen) override;

    void Open();
    void StartupCheck() { Check(); }

private:
    void Check();
    void DrawCheckView(UI::Ui& ui, UI::Column& col);
    void DrawInstallView(UI::Ui& ui, const Rect& body);
    void DrawFooter(UI::Ui& ui, const Rect& footer);
    void Install();
    void ShowInstall(bool gameBanana);

    // What's new: the published changelog cut down to the versions newer than this one
    struct NewsLine {
        std::wstring text;
        bool mentions = false;
        float y = 0.f, h = 0.f;   // relative to the card body
    };
    struct NewsSection {
        std::wstring title;       // empty: lines before any "## " header
        float y = 0.f;
        std::vector<NewsLine> lines;
    };
    struct NewsCard {
        Services::Changelog::Entry entry;
        std::vector<NewsSection> sections;
        float bodyH = 0.f;
    };
    void BuildNews(UI::Ui& ui, const std::wstring& text, const std::wstring& latest, float width);
    float DrawNewsCard(UI::Ui& ui, size_t index, float x, float y, float w);

    UI::Popup popup;
    std::optional<Services::Updates::UpdateInfo> info;
    std::optional<Services::Updates::GbUpdateInfo> gb;
    bool checking = false;
    bool installView = false, installGb = false;
    bool downloading = false;
    int percent = 0;
    std::vector<NewsCard> news;
    std::vector<bool> newsOpen;
    std::wstring newsSource;
    float newsWidth = -1.f;
    unsigned newsLang = ~0u;
    double newsShownAt = -10.0;
};

} // namespace SpeedrunLauncher::Main
