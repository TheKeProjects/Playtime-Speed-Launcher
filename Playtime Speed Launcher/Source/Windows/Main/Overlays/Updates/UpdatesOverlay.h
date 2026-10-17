#pragma once
#include <optional>
#include "Services/App/UpdateService.h"
#include "UI/Layout.h"
#include "UI/Widgets/Panel.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// Launcher updates: the check view (current version, status banner, GameBanana banner, release
// details, manual-update link) and the install view (what's new + download progress).
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

    UI::Popup popup;
    std::optional<Services::Updates::UpdateInfo> info;
    std::optional<Services::Updates::GbUpdateInfo> gb;
    bool checking = false;
    bool installView = false, installGb = false;
    bool downloading = false;
    int percent = 0;
};

} // namespace SpeedrunLauncher::Main
