#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include "App/Launcher.h"
#include "App/MessageLoop.h"
#include "App/Paths.h"
#include "App/SteamShortcutHelper.h"
#include "Core/Async.h"
#include "Graphics/Device.h"
#include "Graphics/Fonts.h"
#include "Localization/Localization.h"
#include "Services/App/ResourceExtractor.h"
#include "Services/Fps/GameFpsService.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Splash/LoadingScreen.h"

using namespace SpeedrunLauncher;

namespace {

struct Args {
    int count = 0;
    wchar_t** values = nullptr;
    Args() { values = CommandLineToArgvW(GetCommandLineW(), &count); }
    ~Args() { LocalFree(values); }
    bool Is(int i, const wchar_t* text) const { return i < count && std::wstring(values[i]) == text; }
};

} // namespace

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int) {
    Args args;
    // Elevated FPS helper: no UI at all
    if (args.Is(1, Services::GameFpsService::HelperArg)) {
        if (args.count < 4) return 1;
        std::vector<std::wstring> names;
        for (int i = 4; i < args.count; ++i) names.emplace_back(args.values[i]);
        return Services::GameFpsService::RunHelper(static_cast<uint32_t>(std::wcstoul(args.values[2], nullptr, 10)), args.values[3], names);
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Async::Initialize();
    Device::Get().Init();

    int code = 0;
    if (args.Is(1, App::SteamShortcutHelperArg)) {
        Services::ResourceExtractor::Extract();
        Loc::LoadSaved();
        Fonts::Initialize(Paths::Asset(L"Fonts\\Poppy Playtime.ttf"));
        code = App::RunSteamShortcutHelper(args.count, args.values);
    } else {
        Fonts::Initialize(L"");   // system fonts for the loading screen; the Poppy font once it's unpacked
        // Unpacks the assets, loads the translations, the font, the main screen's pictures and the launcher data
        Splash::RunLoadingScreen();
        {
            Main::MainWindow window;
            if (window.Open()) code = App::RunMessageLoop();
        }
        App::L().Shutdown();
        Services::ResourceExtractor::CleanupSaves();
    }
    Async::Shutdown();
    Device::Get().Shutdown();
    CoUninitialize();
    return code;
}
