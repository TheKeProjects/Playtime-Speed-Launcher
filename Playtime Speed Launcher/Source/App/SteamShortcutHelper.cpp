#include "App/SteamShortcutHelper.h"
#include <windows.h>
#include <string>
#include "App/MessageLoop.h"
#include "Core/Async.h"
#include "Localization/Localization.h"
#include "Services/Platforms/SteamShortcutService.h"
#include "Windows/Dialogs/DialogWindow.h"

namespace SpeedrunLauncher::App {

namespace {

constexpr const wchar_t* kTitle = L"ADD TO STEAM";

void ShowResult(const std::wstring& message, bool success) {
    Dialogs::Options o;
    o.title = kTitle;
    o.close = L"OK";
    Dialogs::ShowText(nullptr, o, message, success ? Rgba(0, 204, 170, 200) : Rgba(160, 180, 200, 200),
                      [](Dialogs::Result) { PostQuitMessage(0); });
}

} // namespace

int RunSteamShortcutHelper(int argc, wchar_t** argv) {
    if (argc < 4) return 1;
    const std::wstring exePath = argv[2], iconPath = argv[3];
    namespace S = Services::SteamShortcuts;

    const bool wasRunning = S::IsSteamRunning();
    Dialogs::Options o;
    o.title = kTitle;
    o.primary = Loc::Get("add_to_steam_btn");
    o.close = Loc::Get("cancel");
    Dialogs::ShowText(nullptr, o, Loc::Get(wasRunning ? "add_to_steam_warning_running" : "add_to_steam_warning_not_running"),
                      Rgba(232, 160, 0, 200), [=](Dialogs::Result result) {
                          if (result != Dialogs::Result::Primary) {
                              PostQuitMessage(0);
                              return;
                          }
                          // Closing / restarting Steam takes seconds: off the UI thread
                          Async::Run([=]() -> std::pair<std::wstring, bool> {
                              const auto steamPath = S::FindSteamPath();
                              if (!steamPath) return { Loc::Get("add_to_steam_error_no_steam"), false };
                              if (wasRunning) S::CloseSteam(*steamPath);
                              const std::wstring error = S::AddOrUpdateShortcut(exePath, iconPath);
                              if (wasRunning) S::StartSteam(*steamPath);
                              if (!error.empty()) return { Loc::Fmt("add_to_steam_error_generic", error), false };
                              return { Loc::Get("add_to_steam_success"), true };
                          }, [](std::pair<std::wstring, bool> r) { ShowResult(r.first, r.second); });
                      });
    return App::RunMessageLoop();
}

} // namespace SpeedrunLauncher::App
