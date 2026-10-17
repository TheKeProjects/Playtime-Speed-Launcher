#pragma once
#include <memory>
#include <optional>
#include "Services/App/UpdateService.h"
#include "UI/Window.h"

namespace SpeedrunLauncher::Windows {

// Fallback when the automatic updater fails: fetches the latest release regardless of the local
// version and offers a direct download of the exe, or the release page in the browser. Modal.
class ManualUpdateWindow : public UI::Window {
public:
    ~ManualUpdateWindow() override { *alive = false; }
    bool Open(HWND owner);

protected:
    void OnFrame() override;
    void OnClosing(bool& cancel) override;
    Color ClearColor() const override { return Colors::Transparent; }

private:
    HWND ownerWindow = nullptr;
    bool checking = true;
    std::optional<Services::Updates::UpdateInfo> release;
    bool failed = false, downloading = false;
    int percent = 0;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);   // guards async callbacks
};

} // namespace SpeedrunLauncher::Windows
