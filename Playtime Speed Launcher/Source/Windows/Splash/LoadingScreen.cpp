#include "Windows/Splash/LoadingScreen.h"
#include <windows.h>
#include <objbase.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <random>
#include <thread>
#include <vector>
#include "App/Launcher.h"
#include "App/MessageLoop.h"
#include "App/Paths.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Graphics/Animation.h"
#include "Graphics/Fonts.h"
#include "Graphics/Images.h"
#include "Localization/Localization.h"
#include "Resource.h"
#include "Seasonal/SeasonalCardArt.h"
#include "Seasonal/SeasonalTheme.h"
#include "Services/App/ResourceExtractor.h"
#include "UI/Style.h"
#include "UI/Window.h"

namespace SpeedrunLauncher::Splash {

using namespace UI;

namespace {

constexpr int kChapters = 5;   // Assets\Loading Screen\Chapter N.jpg / .png
constexpr double kEnter = 0.3, kExit = 0.35;
constexpr double kFade = 0.9;           // the picture fades in slowly
constexpr double kDrift = 5.0;          // seconds of slow zoom / pan
constexpr float kFillRate = 0.35f;      // the bar moves at most this much per second (~3 s minimum)
constexpr float kPictureW = 520.f, kPictureH = 292.f, kPad = 22.f;
constexpr float kDecor = 34.f;          // transparent strip above the panel for the seasonal decorations
constexpr float kWidth = 20.f + kPad * 2.f + kPictureW;
constexpr float kHeight = kDecor + 10.f + kPad + kPictureH + 22.f + 18.f + 10.f + 10.f + 22.f + 20.f;
constexpr unsigned kPictureSide = 1024;

// What is being loaded right now (the text under the picture)
enum class Stage { Unpacking, Translations, Fonts, Pictures, LauncherData, Ready };

// Written by the loading workers, read every frame
struct LoadState {
    std::atomic<float> progress{ 0.f };   // 0..1
    std::atomic<int> stage{ static_cast<int>(Stage::Unpacking) };
    std::atomic<int> done{ 0 }, total{ 0 };   // items of the current stage ("34 / 102")
    bool ready = false;                       // UI thread only
};

std::wstring StageText(Stage stage) {
    // On the very first start the translations aren't unpacked yet for the first line: English meanwhile
    const auto text = [](const char* key, const wchar_t* english) {
        const std::wstring& translated = Loc::Get(key);
        return translated == Text::Widen(key) ? std::wstring(english) : translated;
    };
    switch (stage) {
        case Stage::Unpacking: return text("loading_unpacking", L"Unpacking launcher files");
        case Stage::Translations: return text("loading_translations", L"Loading translations");
        case Stage::Fonts: return text("loading_fonts", L"Loading fonts");
        case Stage::Pictures: return text("loading_images", L"Loading images");
        case Stage::LauncherData: return text("loading_data", L"Loading launcher data");
        default: return text("loading_ready", L"Ready");
    }
}

// Every top-level picture in Assets\Banners (and its Multiplayers folder) and Assets\Images: what the main
// screen draws first
std::vector<std::wstring> MainScreenPictures() {
    std::vector<std::wstring> paths;
    for (const wchar_t* folder : { L"Banners", L"Banners\\Multiplayers", L"Images" })
        for (const wchar_t* pattern : { L"*.png", L"*.jpg" })
            for (auto& path : Files::ListFiles(Paths::Asset(folder), pattern)) paths.push_back(std::move(path));
    return paths;
}

class LoadingWindow : public Window {
public:
    explicit LoadingWindow(std::shared_ptr<LoadState> loadState) : state(std::move(loadState)) {}

    bool Open() {
        // One random picture per start, straight from the exe (nothing is unpacked yet): only that one
        // is decoded so the window shows up as soon as possible
        const int chapter = 1 + static_cast<int>(std::random_device{}() % kChapters);
        const std::wstring name = L"Loading Screen\\Chapter " + std::to_wstring(chapter);
        auto bytes = Services::ResourceExtractor::ReadEmbedded(L"Assets", name + L".png");
        if (!bytes) bytes = Services::ResourceExtractor::ReadEmbedded(L"Assets", name + L".jpg");
        if (bytes) picture = Images::FromMemory(*bytes, kPictureSide);
        Window::Options o;
        o.title = L"Playtime Speed Launcher";
        o.style = WS_POPUP;
        o.transparent = true;
        o.topmost = true;   // started from another app the launcher may not get the foreground
        o.width = static_cast<int>(kWidth);
        o.height = static_cast<int>(kHeight);
        o.showCommand = SW_HIDE;
        if (!Create(o)) return false;
        g.SetColorFilter(&Seasonal::Filter);   // follows the seasonal theme like the main window
        HINSTANCE instance = GetModuleHandleW(nullptr);
        SetIcon(static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, 32, 32, 0)),
                static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, 16, 16, 0)));
        RECT area{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
        const int w = static_cast<int>(kWidth * DpiScale()), h = static_cast<int>(kHeight * DpiScale());
        SetWindowPos(hwnd, nullptr, area.left + (area.right - area.left - w) / 2, area.top + (area.bottom - area.top - h) / 2, w, h,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        SetContinuous(true);
        openedAt = Async::Now();
        Show(SW_SHOW);
        Activate();
        return true;
    }

protected:
    Color ClearColor() const override { return Colors::Transparent; }

    void OnClosing(bool& cancel) override { cancel = !done; }   // Alt+F4 doesn't skip the loading

    void OnFrame() override {
        const double t = Async::Now() - openedAt;
        // The bar follows the real progress, never faster than kFillRate, and the screen leaves
        // once everything is loaded and the bar is full
        const float dt = static_cast<float>(t - lastFrame);
        lastFrame = t;
        shown = std::min(state->progress.load(), shown + kFillRate * std::clamp(dt, 0.f, 0.1f));
        if (state->ready && closingAt < 0.0 && shown >= 0.999f) closingAt = t;
        if (closingAt >= 0.0 && t - closingAt >= kExit) {
            if (!done) {
                done = true;
                Close();             // posted: the window goes away before the loop sees WM_QUIT
                PostQuitMessage(0);
            }
            return;
        }
        const float enter = Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>(t / kEnter)));
        const float exit = closingAt < 0.0 ? 0.f : Anim::EaseInCubic(Anim::Clamp01(static_cast<float>((t - closingAt) / kExit)));
        const float opacity = enter * (1.f - exit);
        const float scale = 0.96f + 0.04f * enter + 0.03f * exit;

        const Rect panel{ 10.f, kDecor, g.Width() - 20.f, g.Height() - kDecor - 10.f };
        g.PushOpacity(opacity);
        ui.PushScale(scale, panel.CX(), panel.CY());
        g.GlowRound(panel, 16.f, Palette::Teal, 22.f, 0.18f);
        g.Border(panel, 16.f, Argb(0xF207101A), Hex(0x153048), 1.f);
        Seasonal::CardArt::DrawPanelParticles(g, panel, 16.f, t, 0.5f);

        const Rect frame{ panel.x + kPad, panel.y + kPad, kPictureW, kPictureH };
        DrawPicture(frame, t);

        // What is being loaded + item counter, then the percentage
        const Stage stage = state->ready && shown >= 0.999f ? Stage::Ready : static_cast<Stage>(state->stage.load());
        if (stage != shownStage) {
            shownStage = stage;
            stageAt = t;
        }
        const float stageIn = Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>((t - stageAt) / 0.3)));
        const Rect status{ frame.x, frame.B() + 22.f, frame.w, 18.f };
        std::wstring text = StageText(stage);
        if (stage != Stage::Ready) {
            text += L"\x2026";
            const int total = state->total.load();
            if (total > 0) text += L"  " + std::to_wstring(std::min(state->done.load(), total)) + L" / " + std::to_wstring(total);
        }
        g.Text(text, status.Offset(6.f * (1.f - stageIn), 0.f), Rgba(160, 190, 210).WithAlpha(stageIn), Mono(11.f, true));
        g.Text(std::to_wstring(static_cast<int>(shown * 100.f + 0.5f)) + L"%", status, Palette::Teal, Mono(16.f, true).Right());

        // Percentage bar
        const Rect bar{ frame.x, status.B() + 10.f, frame.w, 10.f };
        g.FillRound(bar, 5.f, Hex(0x0D2535));
        if (shown > 0.f) {
            const Rect fill{ bar.x, bar.y, std::max(10.f, bar.w * shown), bar.h };
            g.GlowRound(fill, 5.f, Palette::Teal, 12.f, 0.4f);
            g.FillLinear(fill, 5.f, { { 0.f, Rgba(0, 150, 125) }, { 1.f, Palette::Teal } }, { 0.f, 0.f }, { 1.f, 0.f });
            // Light sweeping along the filled part
            const float sweep = static_cast<float>(std::fmod(t * 0.9, 1.3)) - 0.15f;
            g.PushRoundClip(fill, 5.f);
            g.FillLinear({ fill.x + fill.w * sweep - 50.f, fill.y, 100.f, fill.h }, 0.f,
                         { { 0.f, Rgba(255, 255, 255, 0) }, { 0.5f, Rgba(255, 255, 255, 70) }, { 1.f, Rgba(255, 255, 255, 0) } }, { 0.f, 0.f }, { 1.f, 0.f });
            g.PopLayer();
        }

        ui.PopTransform();
        g.PopLayer();
        Seasonal::CardArt::DrawPanelEdge(g, panel, t, opacity);
    }

private:
    // The picture in a wide frame: fades in slowly, then keeps drifting and zooming out a little
    void DrawPicture(const Rect& frame, double t) {
        constexpr float radius = 14.f;
        g.GlowRound(frame, radius, Palette::Teal, 16.f, 0.2f);
        g.FillRound(frame, radius, Hex(0x05090F));
        if (picture) {
            const float life = Anim::Clamp01(static_cast<float>(t / kDrift));
            const float zoom = 1.1f - 0.08f * Anim::EaseOutQuad(life);
            const float drift = 14.f * (life - 0.5f);
            const Rect r{ frame.CX() - frame.w * zoom * 0.5f + drift, frame.CY() - frame.h * zoom * 0.5f, frame.w * zoom, frame.h * zoom };
            g.PushRoundClip(frame, radius);
            g.DrawImage(*picture, r, Anim::EaseInOutSine(Anim::Clamp01(static_cast<float>(t / kFade))), Stretch::UniformToFill);
            // Soft dark vignette at the bottom so the picture blends into the panel
            g.FillLinear({ frame.x, frame.B() - 80.f, frame.w, 80.f }, 0.f, { { 0.f, Argb(0x0007101A) }, { 1.f, Argb(0xA007101A) } }, { 0.f, 0.f },
                         { 0.f, 1.f });
            g.PopLayer();
        }
        g.StrokeRound(frame, radius, Rgba(0, 204, 170, 90), 1.f);
    }

    std::shared_ptr<LoadState> state;
    ImagePtr picture;
    double openedAt = 0.0, closingAt = -1.0, lastFrame = 0.0, stageAt = 0.0;
    float shown = 0.f;   // progress on the bar
    Stage shownStage = Stage::Unpacking;
    bool done = false;
};

void SetStage(LoadState& state, Stage stage, int total = 0) {
    state.done = 0;
    state.total = total;
    state.stage = static_cast<int>(stage);
}

// Unpack (0..65 %) → translations (70 %) → font (75 %) → the main screen's pictures (75..95 %) →
// launcher data (100 %). The quick steps wait a moment so their line shows up.
void StartLoading(const std::shared_ptr<LoadState>& state) {
    Async::Run([state] {
        Services::ResourceExtractor::Extract([state](size_t done, size_t total) {
            state->total = static_cast<int>(total);
            state->done = static_cast<int>(done);
            if (total) state->progress = 0.65f * static_cast<float>(done) / static_cast<float>(total);
        });
        return true;
    }, [state](bool) {
        SetStage(*state, Stage::Translations);
        Async::After(150, [state] {
            Loc::LoadSaved();
            state->progress = 0.7f;
            SetStage(*state, Stage::Fonts);
            Async::After(150, [state] {
                Fonts::Initialize(Paths::Asset(L"Fonts\\Poppy Playtime.ttf"));
                state->progress = 0.75f;
                auto paths = MainScreenPictures();
                SetStage(*state, Stage::Pictures, static_cast<int>(paths.size()));
                Async::Run([state, paths = std::move(paths)] {
                    // Several pictures decode at once (WIC is thread-safe); each worker takes the next one
                    std::atomic<size_t> next{ 0 };
                    std::atomic<int> decoded{ 0 };
                    const auto decode = [&] {
                        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                        for (size_t i = next++; i < paths.size(); i = next++) {
                            auto pixels = std::make_shared<std::vector<uint32_t>>();
                            unsigned w = 0, h = 0;
                            const std::wstring& path = paths[i];
                            const bool ok = Images::Decode(path, 2048, w, h, *pixels);
                            // GPU upload + cache on the UI thread (failures are cached too, like Images::Load)
                            Async::Post([path, ok, w, h, pixels] { Images::Store(path, 2048, ok ? Images::FromPixels(w, h, pixels->data()) : nullptr); });
                            const int count = ++decoded;
                            state->done = count;
                            state->progress = 0.75f + 0.2f * static_cast<float>(count) / static_cast<float>(paths.size());
                        }
                        if (SUCCEEDED(hr)) CoUninitialize();
                    };
                    const size_t workers = std::clamp<size_t>(std::thread::hardware_concurrency(), 1, 4);
                    std::vector<std::thread> threads;
                    for (size_t i = 1; i < workers; ++i) threads.emplace_back(decode);
                    decode();
                    for (auto& thread : threads) thread.join();
                    return true;
                }, [state](bool) {
                    SetStage(*state, Stage::LauncherData);
                    Async::After(150, [state] {
                        App::L().Initialize();
                        state->progress = 1.f;
                        state->ready = true;
                    });
                });
            });
        });
    });
}

} // namespace

void RunLoadingScreen() {
    auto state = std::make_shared<LoadState>();
    Loc::LoadSaved();   // the translations still unpacked from the last start, so the first line is translated too
    LoadingWindow window(state);
    if (!window.Open()) {
        // No window (no GPU device...): load everything without the screen
        Services::ResourceExtractor::Extract();
        Loc::LoadSaved();
        Fonts::Initialize(Paths::Asset(L"Fonts\\Poppy Playtime.ttf"));
        App::L().Initialize();
        return;
    }
    StartLoading(state);
    App::RunMessageLoop();
}

} // namespace SpeedrunLauncher::Splash
