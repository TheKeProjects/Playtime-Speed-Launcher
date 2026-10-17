#include "Windows/Main/MainWindow.h"
#include <algorithm>
#include "App/Launcher.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Core/System.h"
#include "Features/EasterEggs.h"
#include "Features/GameOverlays.h"
#include "Features/GameWatcher.h"
#include "Features/GlobalHotkeys.h"
#include "Features/InputRemaps.h"
#include "Features/LiveSplitPoller.h"
#include "Graphics/Animation.h"
#include "Input/KeyNames.h"
#include "Localization/Localization.h"
#include "Seasonal/SeasonalBackdrop.h"
#include "Seasonal/SeasonalOverlay.h"
#include "Seasonal/SeasonalTheme.h"
#include "Services/Audio/SoundEffects.h"
#include "Services/Community/OnlineUsersService.h"
#include "Services/Platforms/SteamCmdRunner.h"
#include "Services/Platforms/SteamDetector.h"
#include "Settings/DiscordPresenceSettings.h"
#include "Settings/HotkeySettings.h"
#include "Settings/IconThemeSettings.h"
#include "Settings/OverlaySettings.h"
#include "Settings/WindowModeSettings.h"
#include "Settings/KeyClickSettings.h"
#include "UI/Style.h"
#include "Windows/Dialogs/DialogWindow.h"
#include "Windows/Main/MainScreen/Bars.h"
#include "Windows/Main/MainScreen/MainLayout.h"
#include "Windows/Main/Overlays/AddToSteam/AddToSteamOverlay.h"
#include "Windows/Main/Overlays/BugReport/BugReportOverlay.h"
#include "Windows/Main/Overlays/Changelog/ChangelogOverlay.h"
#include "Windows/Main/Overlays/EasterEgg/EasterEggOverlay.h"
#include "Windows/Main/Overlays/HandMods/HandModsOverlay.h"
#include "Windows/Main/Overlays/LiveSplit/LiveSplitOverlay.h"
#include "Windows/Main/Overlays/Mods/ModPopups.h"
#include "Windows/Main/Overlays/SaveCard/CheckpointOverlay.h"
#include "Windows/Main/Overlays/SaveCard/SaveCardOverlay.h"
#include "Windows/Main/Overlays/Tutorials/TutorialPopups.h"
#include "Windows/Main/Overlays/Updates/UpdatesOverlay.h"
#include "Windows/Main/Overlays/Versions/VersionsOverlay.h"
#include "Windows/Main/Settings/SettingsOverlay.h"
#include "Windows/Main/Overlays/KeyClick/KeyClickWizard.h"
#include "Windows/Standalone/StandaloneWindows.h"
#include "Windows/Toasts/ToastWindow.h"


namespace SpeedrunLauncher::Main {

namespace {
MainWindow* instance = nullptr;
}

MainWindow* MainWindow::Instance() { return instance; }

MainWindow::MainWindow() {
    instance = this;
    watcher = std::make_unique<Features::GameWatcher>(*this);
    hotkeys = std::make_unique<Features::GlobalHotkeys>();
    remaps = std::make_unique<Features::InputRemaps>(*this);
    eggs = std::make_unique<Features::EasterEggs>(*this);
    liveSplitPoller = std::make_unique<Features::LiveSplitPoller>();
    gameOverlays = std::make_unique<Features::GameOverlays>();

    versions = std::make_unique<VersionsOverlay>(*this);
    settings = std::make_unique<SettingsOverlay>(*this);
    updates = std::make_unique<UpdatesOverlay>(*this);
    liveSplit = std::make_unique<LiveSplitOverlay>(*this);
    saveCard = std::make_unique<SaveCardOverlay>(*this);
    checkpoint = std::make_unique<CheckpointOverlay>(*this);
    bugReport = std::make_unique<BugReportOverlay>(*this);
    changelog = std::make_unique<ChangelogOverlay>(*this);
    eggVideo = std::make_unique<EasterEggOverlay>(*this);
    mods = std::make_unique<ModPopups>(*this);
    addToSteam = std::make_unique<AddToSteamOverlay>(*this);
    handMods = std::make_unique<HandModsOverlay>(*this);
    tutorials = std::make_unique<TutorialPopups>(*this);
    keyClickWizard = std::make_unique<KeyClickWizard>(*this);
    // Same stacking order as the overlays in the old MainWindow.xaml (later = on top)
    overlays = { versions.get(), settings.get(), keyClickWizard.get(), updates.get(), liveSplit.get(), saveCard.get(), checkpoint.get(),
                 bugReport.get(), changelog.get(), eggVideo.get(), mods.get(), addToSteam.get(), handMods.get(), tutorials.get() };
}

MainWindow::~MainWindow() {
    if (instance == this) instance = nullptr;
}

bool MainWindow::Open() {
    // Maximized and borderless, covering the whole monitor (both window modes)
    HMONITOR monitor = MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(monitor, &mi);
    UI::Window::Options o;
    o.title = L"Poppy Playtime \x2014 Speedrun Launcher";
    o.style = WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU;
    o.x = mi.rcMonitor.left;
    o.y = mi.rcMonitor.top;
    o.showCommand = SW_HIDE;
    if (!Create(o)) return false;
    SetWindowPos(hwnd, nullptr, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOZORDER);
    g.SetColorFilter(&Seasonal::Filter);
    ui.onHoverSound = [] { Services::Sfx::Play(Services::Sfx::Hover, true); };
    ui.onClickSound = [] { Services::Sfx::Play(Services::Sfx::Click); };
    ApplyIconTheme();
    Show(SW_SHOW);
    Activate();
    // Idle frames at ~60 fps (async data, seasonal scenery); videos and hover animations ask for
    // the full refresh rate themselves
    SetContinuous(true, true);

    hotkeys->onCheckpoint = [] { Standalone::ToggleCheckpointOverlay(); };
    hotkeys->onTutorial = [] { Standalone::ToggleVideoTutorialOverlay(); };
    hotkeys->onTempRemapApplied = [this] {
        Toasts::Spec spec;
        spec.width = 340.f;
        spec.duration = 8.0;
        spec.fadeOut = 0.6;
        spec.lines = { { Loc::Get("ue4ss_remap_hint"), 10.f, false, Rgba(160, 190, 210, 180), 0.f },
                       { Loc::Get("ue4ss_remap_keys"), 12.f, true, Rgba(0, 204, 170), 4.f } };
        ue4ssToast.Show(spec);
    };
    hotkeys->onTempRemapRestored = [this] { ue4ssToast.Close(); };
    hotkeys->Register(hwnd);
    Services::Sfx::SetVolume(Settings::Hotkeys::Current().volume);
    remaps->Initialize();
    ApplyKeyClickHotkey();
    gameOverlays->ApplyController();

    StartBackgroundWork();
    RefreshInfo();
    return true;
}

void MainWindow::StartBackgroundWork() {
    auto& l = App::L();
    // Steam installs: slow folder scan on a worker
    Async::Run([] {
        auto chapters = App::L().chapters;
        Services::SteamDetector::DetectAll(chapters);
        return chapters;
    }, [this](std::vector<Models::Chapter> detected) {
        auto& chapters = App::L().chapters;
        for (size_t i = 0; i < chapters.size() && i < detected.size(); ++i) {
            chapters[i].gameExePath = detected[i].gameExePath;
            chapters[i].detectedVersion = detected[i].detectedVersion;
        }
        RefreshInfo();
    });
    updates->StartupCheck();
    liveSplit->StartupCheck();
    if (!intro.Start()) HideIntro();
    watcher->Start();
    liveSplitPoller->Start();
    Services::OnlineUsers::Start([this](int count) { onlineText = std::to_wstring(count) + L" online"; });
    l.playtime.Load([this] { Invalidate(); });
    steamUser.Load();
    const auto& d = Settings::DiscordPresence::Current();
    l.discord->ApplySettings(d.showActivity, d.showVersion, d.showLiveSplit);
    l.discord->SetBrowsing();
}

// ── Actions ─────────────────────────────────────────────────────────────────

void MainWindow::SelectChapter(int index) {
    auto& l = App::L();
    if (index != l.selected) Services::Sfx::Play(Services::Sfx::Chapter);
    l.selected = index;
    if (!l.AnyGameRunning()) l.discord->SetChapterSelected(l.Selected(), l.VersionLabel(l.Selected()));
    RefreshInfo();
}

void MainWindow::RefreshInfo() {
    info.Refresh();
    App::L().RefreshModStates();
    snapshotDirty = true;
    Invalidate();
}

void MainWindow::ApplyLanguage() {
    RefreshInfo();
    for (Overlay* o : overlays) o->Refresh();
}

void MainWindow::ApplyIconTheme() {
    const int id = Settings::IconTheme::Current().IconResourceId();
    HINSTANCE module = GetModuleHandleW(nullptr);
    HICON big = static_cast<HICON>(LoadImageW(module, MAKEINTRESOURCEW(id ? id : 101), IMAGE_ICON, GetSystemMetrics(SM_CXICON),
                                              GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
    HICON smallIcon = static_cast<HICON>(LoadImageW(module, MAKEINTRESOURCEW(id ? id : 101), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                                GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    SetIcon(big, smallIcon);
}

void MainWindow::ApplySeasonalPalette() {
    Seasonal::Refresh();
    Invalidate();
}

void MainWindow::ApplyWindowMode() { Invalidate(); }

void MainWindow::PlaySelected() {
    auto& l = App::L();
    auto& ch = l.Selected();
    auto exe = l.ActiveExePath(ch);
    if (!exe || exe->empty() || !Files::Exists(*exe)) return;
    if (!Process::FindByName(Files::FileNameWithoutExtension(*exe)).empty()) return;
    l.discord->SetGameRunning(ch, l.VersionLabel(ch));
    // FPS counter: ask for admin now, while the launcher is still in front, then start the game
    if (Settings::Overlays::Current().fpsEnabled) {
        std::vector<std::wstring> names;
        if (auto shipping = l.ShippingExeName(*exe)) names.push_back(*shipping);
        names.push_back(Files::FileNameWithoutExtension(*exe));
        l.fps.Prepare(names);
    }
    Process::Open(*exe, L"", Files::DirectoryName(*exe));
    if (l.IsUe4ssActiveForChapter(ch)) hotkeys->ApplyUe4ssTempRemap(*exe);
}

void MainWindow::TogglePlatform() {
    auto& l = App::L();
    if (l.epic.IsEnabled()) {
        l.epic.SetEnabled(false);
        RefreshInfo();
        return;
    }
    // Switching to Epic: make sure there's a valid base folder first
    const auto enable = [this] {
        auto& epic = App::L().epic;
        if (!epic.HasExeForAnyChapter()) {
            ShowError(Loc::Get("epic_no_exe_error"));
            return;
        }
        epic.SetEnabled(true);
        RefreshInfo();
    };
    if (l.epic.BasePath() && l.epic.HasExeForAnyChapter()) {
        enable();
        return;
    }
    if (auto detected = l.epic.TryAutoDetect()) {
        l.epic.SetBasePath(detected);
        enable();
        return;
    }
    Dialogs::Options options;
    options.title = L"Epic Games";
    options.primary = Loc::Get("select_folder");
    options.close = Loc::Get("cancel");
    options.contentWidth = 380.f;
    Dialogs::Show(hwnd, options, Dialogs::Text(Loc::Get("epic_not_found_content"), Rgba(160, 190, 220, 220), 11.f),
                  [this, enable](Dialogs::Result result) {
                      if (result != Dialogs::Result::Primary) return;
                      const std::wstring picked = System::PickFolder(hwnd, L"");
                      if (picked.empty()) return;
                      // Accept PoppyPlaytimeChapterOne itself or its parent folder
                      const std::wstring sub = Files::Combine(picked, L"PoppyPlaytimeChapterOne");
                      App::L().epic.SetBasePath(Files::DirectoryExists(sub) ? sub : picked);
                      enable();
                  });
}

void MainWindow::Quit() {
    for (uint32_t pid : Process::FindByName(L"steamcmd")) Process::Kill(pid);
    versions->CancelDownloads();
    Close();
}

void MainWindow::HideIntro() {
    intro.Hide();
    // First run: fetch SteamCMD in the background once the intro is gone
    if (!Services::SteamCmd::Find()) versions->EnsureSteamCmd();
}

void MainWindow::OpenSettings() { settings->Open(); }
void MainWindow::OpenKeyClickWizard(int bindingIndex) { keyClickWizard->Open(bindingIndex); }

bool MainWindow::ApplyKeyClickHotkey() {
    UnregisterHotKey(hwnd, kKeyClickHotkeyId);
    const auto& s = Settings::KeyClick::Current();
    keyClickHotkeyOk = true;
    if (s.toggleVk) keyClickHotkeyOk = RegisterHotKey(hwnd, kKeyClickHotkeyId, s.toggleModifiers | MOD_NOREPEAT, s.toggleVk) != FALSE;
    return keyClickHotkeyOk;
}

void MainWindow::CheckSpamCombo(unsigned vk) {
    static const unsigned combo[10] = { VK_UP, VK_UP, VK_UP, VK_UP, VK_UP, VK_DOWN, VK_DOWN, VK_DOWN, VK_DOWN, VK_DOWN };
    recentKeys.push_back(vk);
    if (recentKeys.size() > 10) recentKeys.erase(recentKeys.begin());
    if (recentKeys.size() < 10 || !std::equal(recentKeys.begin(), recentKeys.end(), combo)) return;
    recentKeys.clear();
    auto& s = Settings::KeyClick::Current();
    s.spamUnlocked = !s.spamUnlocked;
    s.Save();
    Services::Sfx::Play(Services::Sfx::Chapter);
    Invalidate();
    const Color color = s.spamUnlocked ? Rgba(255, 176, 32) : Rgba(120, 140, 160);
    Toasts::Spec spec;
    spec.width = 300.f;
    spec.padding = { 14.f, 10.f, 14.f, 10.f };
    spec.duration = 3.0;
    spec.fadeIn = 0.2;
    spec.fadeOut = 0.4;
    spec.accent = color;
    spec.anchor = Toasts::Spec::Anchor::TopLeft;
    spec.lines = { { Loc::Get(s.spamUnlocked ? "keyclick_spam_unlocked" : "keyclick_spam_hidden"), 15.f, true, color, 0.f },
                   { Loc::Get("keyclick_section"), 10.f, false, Rgba(160, 190, 210, 120), 4.f } };
    coresToast.Show(spec);
}

void MainWindow::ToggleKeyClick() {
    auto& s = Settings::KeyClick::Current();
    s.enabled = !s.enabled;
    s.Save();
    remaps->RefreshKeyClicks();
    Invalidate();
    // Same corner toast as the Cores hotkeys
    const Color color = s.enabled ? Rgba(0, 204, 170) : Rgba(220, 60, 60);
    Toasts::Spec spec;
    spec.width = 300.f;
    spec.padding = { 14.f, 10.f, 14.f, 10.f };
    spec.duration = 3.0;
    spec.fadeIn = 0.2;
    spec.fadeOut = 0.4;
    spec.accent = color;
    spec.anchor = Toasts::Spec::Anchor::TopLeft;
    spec.lines = { { Loc::Get(s.enabled ? "keyclick_toast_on" : "keyclick_toast_off"), 15.f, true, color, 0.f },
                   { Loc::Get("keyclick_section"), 10.f, false, Rgba(160, 190, 210, 120), 4.f } };
    coresToast.Show(spec);
}
void MainWindow::OpenVersions() { versions->Open(App::L().selected); }
void MainWindow::OpenHandMods() { handMods->OpenHub(); }
void MainWindow::OpenLeaderboard() { Standalone::ToggleLeaderboard(); }
void MainWindow::OpenBugReport() { bugReport->Open(); }
void MainWindow::OpenChangelog() { changelog->Open(); }
void MainWindow::OpenBeginnerTutorials() { Standalone::ShowBeginnerTutorials(); }
void MainWindow::OpenUpdates() {
    settings->Close();
    updates->Open();
}
void MainWindow::OpenLiveSplit() {
    settings->Close();
    liveSplit->Open();
}
void MainWindow::OpenSaveCard(int chapter) {
    // Chapters 1-3 pick a checkpoint; later chapters manage their save slots
    if (chapter >= 1 && chapter <= 3) checkpoint->Open(chapter);
    else saveCard->Open(chapter);
}
void MainWindow::OpenUe4ss(int chapter) { mods->OpenUe4ss(chapter); }
void MainWindow::OpenLoadManip(int chapter) { mods->OpenLoadManip(chapter); }
void MainWindow::OpenFullBright(int chapter) { mods->OpenFullBright(chapter); }
void MainWindow::OpenAddToSteam() { addToSteam->Open(); }

void MainWindow::ShowError(const std::wstring& message) {
    Dialogs::Options options;
    options.title = L"Error";
    options.close = L"OK";
    Dialogs::ShowText(hwnd, options, message, Rgba(220, 100, 80, 220));
}

void MainWindow::ShowGameToast() {
    if (gameToast.Visible()) return;
    const auto& h = *hotkeys;
    Toasts::Spec spec;
    spec.width = 330.f;
    spec.duration = 10.0;
    spec.fadeOut = 0.8;
    spec.anchor = Toasts::Spec::Anchor::RightMiddle;
    spec.lines = { { Loc::Get("game_toast_hint"), 10.f, false, Rgba(160, 190, 210, 180), 0.f },
                   { Input::KeyNames::Hotkey(h.CheckpointModifiers(), h.CheckpointVk()), 13.f, true, Rgba(0, 204, 170), 3.f } };
    gameToast.Show(spec);
}

void MainWindow::ShowTutorialToast() {
    if (tutorialToast.Visible()) return;
    const auto& h = *hotkeys;
    Toasts::Spec spec;
    spec.width = 330.f;
    spec.duration = 15.0;
    spec.fadeOut = 0.8;
    spec.anchor = Toasts::Spec::Anchor::RightMiddle;
    spec.offsetY = 85.f;   // below the checkpoint toast
    spec.lines = { { Loc::Get("tutorial_toast_hint"), 10.f, false, Rgba(160, 190, 210, 180), 0.f },
                   { Input::KeyNames::Hotkey(h.TutorialModifiers(), h.TutorialVk()), 13.f, true, Rgba(0, 204, 170), 3.f } };
    tutorialToast.Show(spec);
}

// ── Frame ───────────────────────────────────────────────────────────────────

Color MainWindow::ClearColor() const { return Seasonal::Current().background; }

void MainWindow::OnFrame() {
    if (seenSeason != Seasonal::Revision()) {
        seenSeason = Seasonal::Revision();
        seasonChangedAt = ui.Time();
    }
    const Rect screen{ 0.f, 0.f, g.Width(), g.Height() };

    // ↑↑↑↑↑↓↓↓↓↓ shows / hides the key-to-click spam option
    for (const auto& k : input.keys)
        if (k.down && !k.repeat) CheckSpamCombo(k.vk);

    // Keys typed into the launcher feed the easter eggs (hooks feed them while one is installed)
    if (!remaps->AnyKeyboardHookActive() && ui.Focus() == 0) {
        for (const auto& k : input.keys)
            if (k.down && !k.repeat) eggs->HandleKey(k.vk);
    }

    // Any menu open? Then the scenery rests and, once it has stopped, the main screen is drawn
    // from a snapshot (refreshed when something changes, and every 5 s just in case)
    menuOpen = intro.Visible();
    for (Overlay* o : overlays)
        if (o->Visible()) menuOpen = true;
    const bool sceneStill = !Seasonal::IsSeasonal() || Seasonal::AnimationStopped();
    if (menuOpen && sceneStill && ui.Time() - seasonChangedAt > 1.0) {
        if (!g.MatchesWindow(screenSnapshot.Get())) {
            screenSnapshot = g.CreateWindowBitmap();
            snapshotDirty = true;
        }
        if (screenSnapshot && (snapshotDirty || ui.Time() - snapshotAt > 5.0)) {
            g.BeginOffscreen(screenSnapshot.Get());
            g.Clear(ClearColor());
            DrawMainScreen(screen);
            g.EndOffscreen();
            snapshotDirty = false;
            snapshotAt = ui.Time();
        }
        if (screenSnapshot) g.DrawBitmap(screenSnapshot.Get(), screen);
        else DrawMainScreen(screen);
    } else {
        DrawMainScreen(screen);
        snapshotDirty = true;   // the screen moved on: retake the snapshot next time
    }
    for (Overlay* o : overlays) {
        if (o == eggVideo.get() && intro.Visible()) {
            // The intro sits between the changelog and the easter egg video
        }
        if (o->Visible()) o->Draw(ui, screen);
        if (o == changelog.get() && intro.Visible() && intro.Draw(ui, screen)) HideIntro();
    }
}

void MainWindow::DrawMainScreen(const Rect& screen) {
    g.FillRect(screen, Seasonal::Current().background);
    const bool seasonal = Seasonal::IsSeasonal();
    // The seasonal scenery, card decorations and particles animate at ~60 fps. While a game is
    // running they slow down to a stop (and pick up again when it closes).
    const bool gameRunning = App::L().AnyGameRunning();
    // Another launcher window on top (skips, tutorials, a report, a dialog): the scenery behind it
    // rests too, so all the drawing time goes to the window being used
    bool ownWindowOnTop = false;
    if (const HWND fg = GetForegroundWindow(); fg && fg != hwnd) {
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        ownWindowOnTop = pid == GetCurrentProcessId();
    }
    const bool rest = gameRunning || ownWindowOnTop || menuOpen;
    resting = rest;
    const double seasonTime = Seasonal::AnimationTime(ui.Time(), rest);
    if (seasonal && !Seasonal::AnimationStopped()) ui.KeepAnimatingThrottled();
    // Nothing moves while resting: idle redraws drop to 10 fps (input still renders at full rate)
    SetIdleFps(rest && (!seasonal || Seasonal::AnimationStopped()) ? 10.0 : 60.0);

    // Parallax: the scenery layers follow the mouse a little (smoothed)
    const float targetX = input.mouseInside && screen.w > 0.f ? std::clamp(input.mouseX / screen.w * 2.f - 1.f, -1.f, 1.f) : 0.f;
    const float targetY = input.mouseInside && screen.h > 0.f ? std::clamp(input.mouseY / screen.h * 2.f - 1.f, -1.f, 1.f) : 0.f;
    const float parallaxX = seasonal ? ui.Animate(ui.MakeId("parallax-x"), targetX, 2.5f) : 0.f;
    const float parallaxY = seasonal ? ui.Animate(ui.MakeId("parallax-y"), targetY, 2.5f) : 0.f;

    // After a theme change the new scenery fades in
    const float sceneOpacity = Anim::EaseOutCubic(static_cast<float>((ui.Time() - seasonChangedAt) / 0.9));
    if (seasonal && sceneOpacity < 1.f) ui.KeepAnimating();
    {
        NoColorFilter keep(g);
        if (sceneOpacity < 1.f) g.PushOpacity(sceneOpacity);
        Seasonal::Backdrop::Draw(g, screen, MainLayout::TopBarHeight, seasonTime, parallaxX, parallaxY);
        if (sceneOpacity < 1.f) g.PopLayer();
    }
    const float infoH = info.Height(ui, screen.w);
    const MainLayout layout = MainLayout::Compute(screen, infoH);
    TopBar::Draw(ui, *this, layout.topBar);
    cards.Draw(ui, layout.cards);
    info.Draw(ui, layout.info);
    ActionBar::Draw(ui, *this, layout.actions);
    BottomBar::Draw(ui, *this, layout.bottomBar);
    steamUser.Draw(ui, layout.actions.R(), layout.actions.B());
    {
        NoColorFilter keep(g);
        if (sceneOpacity < 1.f) g.PushOpacity(sceneOpacity);
        Seasonal::Ambient::Draw(g, screen, seasonTime);
        if (sceneOpacity < 1.f) g.PopLayer();
    }
}

bool MainWindow::OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& result) {
    (void)lp;
    if (msg == WM_HOTKEY && static_cast<int>(wp) == kKeyClickHotkeyId) {
        ToggleKeyClick();
        result = 0;
        return true;
    }
    if (msg == WM_HOTKEY && hotkeys->Handle(wp)) {
        // Showing the checkpoint overlay hides the intro first
        if (static_cast<int>(wp) == Features::GlobalHotkeys::CheckpointId && intro.Visible()) HideIntro();
        result = 0;
        return true;
    }
    return false;
}

void MainWindow::OnClosing(bool& cancel) {
    cancel = false;
    auto& l = App::L();
    gameOverlays->Shutdown();
    l.cores.RestoreIfActive();
    hotkeys->Unregister();
    UnregisterHotKey(hwnd, kKeyClickHotkeyId);
    remaps->Shutdown();
    eggs->Hide();
    Standalone::CloseAll();
    gameToast.Close();
    tutorialToast.Close();
    coresToast.Close();
    ue4ssToast.Close();
    liveSplitPoller->Stop();
    Services::OnlineUsers::Stop();
    watcher->Stop();
}

void MainWindow::OnDestroyed() { PostQuitMessage(0); }

} // namespace SpeedrunLauncher::Main
