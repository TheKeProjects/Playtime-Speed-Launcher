﻿﻿﻿using System.Diagnostics;
using System.IO.Compression;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Imaging;
using System.Windows.Shapes;
using System.Windows.Threading;
using Microsoft.Win32;
using PixelFormat = System.Windows.Media.PixelFormats;
using SpeedrunLauncher.Models;
using SpeedrunLauncher.Services;
using SpeedrunLauncher.Services.Fps;
using SpeedrunLauncher.Services.OnlineUsers;
using IOPath = System.IO.Path;
using Loc = SpeedrunLauncher.Services.LocalizationService;

namespace SpeedrunLauncher;

public partial class MainWindow : Window
{
    private readonly List<ChapterInfo>                            _chapters          = ChapterInfo.GetAll();
    private readonly List<Border>                                 _cards             = [];
    private readonly List<TextBlock>                              _hoursTexts        = [];
    private readonly List<Button>                                 _ue4ssBtns         = [];
    private readonly InstallationsStore                           _store             = InstallationsStore.Load();
    private readonly ChapterPlaytimeStore                          _playtimeStore     = ChapterPlaytimeStore.Load();
    private readonly Dictionary<string, CancellationTokenSource> _activePolls       = [];
    private readonly Dictionary<string, List<string>>            _downloadLogs      = [];
    private readonly Dictionary<string, TextBlock>               _downloadLogBlocks = [];
    private int   _selected        = 0;
    private int   _versionsChapter = 0;
    private bool  _hidePresetRows  = true;
    private float _sfxVolume       = 0.5f;
    private bool  _volumeLoaded    = false;
    private static readonly Dictionary<string, string> LangNames = new()
    {
        ["es"] = "Español",
        ["en"] = "English",
    };

    private bool _popupWasOpen      = false;
    private int  _saveCardChapter   = 0;
    private int     _ue4ssTargetChapter = 0;
    private string? _ue4ssWin64Dir;
    private string? _ue4ssZipPath;
    private bool    _ue4ssTargetInstalledViaLoadManip;

    private readonly Dictionary<int, Button> _loadManipBtns = [];
    private int     _loadManipTargetChapter = 0;
    private string? _loadManipWin64Dir;
    private string? _loadManipPaksDir;
    private string? _loadManipZipPath;
    private string? _loadManipUe4ssZipPath;
    private string? _loadManipMarkerZipPath;
    private bool    _loadManipUe4ssInstalledThisSession;

    private readonly Dictionary<int, Button> _fullBrightBtns = [];
    private int     _fullBrightTargetChapter = 0;
    private string? _fullBrightWin64Dir;
    private string? _fullBrightPaksDir;
    private string? _fullBrightZipPath;
    private string? _fullBrightUe4ssZipPath;
    private string? _fullBrightMarkerZipPath;
    private string? _fullBrightConfigZipPath;

    private int                      _handModsTargetChapter = 0;
    private string?                  _handModsWin64Dir;
    private string?                  _handModsPaksDir;
    private List<HandModsService.HandMod>? _handModsList;
    private readonly Dictionary<int, List<HandModsService.HandMod>> _handModsCache = [];

    private readonly List<Button>    _handModsSubmitChapterChips = [];
    private readonly List<Button>    _handModsSubmitColorChips   = [];
    private int                      _handModsSubmitChapter      = 1;
    private readonly HashSet<string> _handModsSubmitColors       = [];
    private readonly List<string>    _handModsSubmitFiles        = [];

    private bool                     _capturingFullBrightKey;
    private string?                  _fullBrightCaptureTarget; // "KeyUnlit" or "KeyLit"
    private KeyEventHandler?         _fullBrightKeyCapture;
    private MouseButtonEventHandler? _fullBrightMouseCapture;

    // ── Chapter 5 FullBright key capture (mirrors the block above for Chapter 1) ────
    private bool                     _capturingChapter5FullBrightKey;
    private string?                  _chapter5FullBrightCaptureTarget; // "KeyUnlit" or "KeyLit"
    private KeyEventHandler?         _chapter5FullBrightKeyCapture;
    private MouseButtonEventHandler? _chapter5FullBrightMouseCapture;

    // ── Konami code easter egg ────────────────────────────────────────────────
    private static readonly Key[] KonamiSequence =
    [
        Key.Up, Key.Up, Key.Down, Key.Down, Key.Left, Key.Right, Key.Left, Key.Right, Key.B, Key.A, Key.Enter,
    ];
    private readonly List<Key> _konamiBuffer = [];
    private bool               _easterEggPlaying;

    // ── UE4SS temp hotkey remap ───────────────────────────────────────────────
    private bool    _ue4ssTempRemap    = false;
    private string? _ue4ssTempRemapExe = null;
    private uint    _savedHotkeyMod    = 0;
    private uint    _savedHotkeyVk     = 0;
    private uint    _savedTutMod       = 0;
    private uint    _savedTutVk        = 0;
    private Window? _ue4ssRemapToast   = null;

    // ── Global hotkey ─────────────────────────────────────────────────────────
    private HotkeyOverlay?          _hotkeyOverlay;
    private VideoTutorialOverlay?   _tutorialOverlay;
    private BeginnerTutorialOverlay? _beginnerTutorialOverlay;
    private LeaderboardOverlay?      _leaderboardOverlay;
    private uint             _hotkeyModifiers = MOD_CONTROL | MOD_SHIFT;
    private uint             _hotkeyVk        = VK_RETURN;
    private bool             _capturingHotkey;
    private KeyEventHandler? _hotkeyCapture;
    private uint             _tutorialHotkeyModifiers = 0;
    private uint             _tutorialHotkeyVk        = VK_F9;
    private bool             _capturingTutorialHotkey;
    private KeyEventHandler? _tutorialHotkeyCapture;

    // ── F11 remap ─────────────────────────────────────────────────────────────
    private readonly F11RemapService _f11Remap = new();
    private bool                     _capturingF11Remap;
    private int?                     _f11RemapCaptureIndex;
    private KeyEventHandler?         _f11RemapKeyCapture;
    private MouseButtonEventHandler? _f11RemapMouseCapture;

    // ── Game watcher ──────────────────────────────────────────────────────────
    private readonly bool[] _gameWasRunning;
    private readonly int[]  _runningChapterPid; // cached PID per chapter, refreshed every 2s by GameWatcherTick
    private readonly Dictionary<string, string?> _shippingExeNameCache = []; // exe path -> resolved shipping binary name (see ResolveShippingExeName)
    private bool             _gameWatcherInitialized;
    private Window?          _gameToast;
    private Window?          _tutorialToast;

    // ── Update system ─────────────────────────────────────────────────────────
    private readonly UpdateService _updateService = new();
    private UpdateInfo?   _updateInfo   = null;
    private GbUpdateInfo? _gbUpdateInfo = null;
    private bool _isDownloading      = false;
    private bool _showingInstallView  = false;
    private bool _isGbInstall         = false;

    // ── Epic Games ────────────────────────────────────────────────────────────
    private readonly EpicGamesService _epicService = new();

    // ── LiveSplit server poller ───────────────────────────────────────────────
    private readonly LiveSplitServerClient    _liveSplitClient  = new();
    private          CancellationTokenSource? _liveSplitPollCts;

    // ── Bug report ────────────────────────────────────────────────────────────
    private string?                       _bugImagePath         = null;
    private (string Id, string Username)? _bugReportDiscordUser = null;
    private CancellationTokenSource?      _discordAuthCts       = null;

    // ── LiveSplit ──────────────────────────────────────────────────────────────
    private readonly LiveSplitService _liveSplitService = new();

    // ── Discord Rich Presence ─────────────────────────────────────────────────
    private readonly DiscordPresenceService  _discordPresence = new();
    private readonly DiscordPresenceSettings _discordSettings = DiscordPresenceSettings.Load();
    private LiveSplitInfo? _liveSplitInfo         = null;
    private bool           _isLiveSplitDownloading = false;

    // ── Cores ─────────────────────────────────────────────────────────────────
    private readonly CoresService _cores = new();
    private uint _coresFreezeVk   = 0x42; // B
    private uint _coresSlowerVk   = 0x4E; // N
    private uint _coresNormalVk   = 0x4D; // M
    private Window? _coresToast;

    private bool _coresEnabled;
    private bool _coresPriorityHigh = true; // default: High

    // ── Chapter 1 Freeze/Normal loads (Controls tab) ─────────────────────────
    // Reads/writes LoadManip_Config.ini directly — the Load Manip mod itself reads this
    // file for its Freeze/Normal keybinds, so no OS-level hook/key-injection is needed.
    private bool                     _capturingChapter1Hotkey;
    private string?                  _chapter1CaptureTarget; // "KeyFreeze" or "KeyNormal"
    private KeyEventHandler?         _chapter1HotkeyCapture;
    private MouseButtonEventHandler? _chapter1MouseHotkeyCapture;

    // ── Chapter 5 Freeze/Normal loads (Controls tab) ─────────────────────────
    // Same LoadManip_Config.ini-based approach as Chapter 1 above.
    private bool                     _capturingChapter5Hotkey;
    private string?                  _chapter5CaptureTarget; // "KeyFreeze" or "KeyNormal"
    private KeyEventHandler?         _chapter5HotkeyCapture;
    private MouseButtonEventHandler? _chapter5MouseHotkeyCapture;

    // ── Chapter 4 Freeze/Slow/Normal loads (Controls tab) ────────────────────
    private HotkeyBinding _chapter4Freeze = new(HotkeyInputType.Keyboard, 0x49, 0); // I
    private HotkeyBinding _chapter4Slow   = new(HotkeyInputType.Keyboard, 0x4F, 0); // O
    private HotkeyBinding _chapter4Normal = new(HotkeyInputType.Keyboard, 0x50, 0); // P
    private bool _chapter4RemapEnabled;

    // ── Controller overlay ────────────────────────────────────────────────────
    private bool                    _overlayEnabled    = false;
    private string                  _overlayController = "xbox-controller";
    private string                  _overlayCorner      = "top-left";
    private ControllerOverlayWindow? _controllerOverlay;

    private static readonly string OverlayEnabledFile =
        IOPath.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "overlay_enabled.cfg");

    private static readonly string OverlayControllerFile =
        IOPath.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "overlay_controller.cfg");

    private static readonly string OverlayCornerFile =
        IOPath.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "overlay_corner.cfg");

    // ── FPS counter (scoped to the currently-running chapter's game process) ────
    private readonly GameFpsService _fpsService = new();
    private FpsOverlayWindow?       _fpsOverlay;

    private bool   _fpsOverlayEnabled = true;
    private string _fpsOverlayCorner  = "top-right";
    private string _fpsOverlaySize    = "medium";
    private string _fpsOverlayFont    = "poppy-playtime";

    private static readonly string FpsOverlayEnabledFile =
        IOPath.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "fps_overlay_enabled.cfg");

    private static readonly string FpsOverlayCornerFile =
        IOPath.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "fps_overlay_corner.cfg");

    private static readonly string FpsOverlaySizeFile =
        IOPath.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "fps_overlay_size.cfg");

    private static readonly string FpsOverlayFontFile =
        IOPath.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "fps_overlay_font.cfg");

    private static readonly string CoresHotkeyFile =
        IOPath.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "loadmanip_hotkeys.cfg");

    private static readonly string CoresEnabledFile =
        IOPath.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "cores_enabled.cfg");

    private static readonly string CoresPriorityFile =
        IOPath.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "cores_priority.cfg");

    private static readonly string Chapter4RemapHotkeyFile =
        IOPath.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "chapter4_remap_hotkeys.cfg");

    private static readonly string Chapter4RemapEnabledFile =
        IOPath.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "chapter4_remap_enabled.cfg");

    // ── Changelog Discord users ──────────────────────────────────────────────
    private static readonly Dictionary<string, string> ChangelogDiscordUsers = new()
    {
        ["Edwin"] = "460391445690449922",
        ["Technight"] = "257300997322440704",
        ["AdrianPG77"] = "752207247769206795",
        ["ᴢᴀᴇᴇ"] = "807763566849163264",
        ["lumpydesk_yt"] = "1143596461087522857",
    };

    // ── Palette ───────────────────────────────────────────────────────────────
    private static readonly Color Teal       = Color.FromArgb(255,   0, 204, 170);
    private static readonly Color TealDim    = Color.FromArgb(120,   0, 204, 170);
    private static readonly Color CardBorder = Color.FromArgb( 50,  13,  42,  59);
    private static readonly Color Overlay0   = Color.FromArgb(  0,   0,   0,   0);
    private static readonly Color Overlay1   = Color.FromArgb(210,   5,  10,  18);

    public MainWindow()
    {
        _gameWasRunning    = new bool[_chapters.Count];
        _runningChapterPid = new int[_chapters.Count];
        Loc.LoadSaved();
        InitializeComponent();
        ApplyIconTheme();
        var trophyPath = IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Images", "GoldTrophy.png");
        if (System.IO.File.Exists(trophyPath))
        {
            TrophyImage.Source = new BitmapImage(new Uri(trophyPath));
            LeaderboardTrophyImage.Source = new BitmapImage(new Uri(trophyPath));
        }
        var handIconPath = IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Images", "Hand.png");
        if (System.IO.File.Exists(handIconPath))
        {
            HandModsMenuIcon.Source = new BitmapImage(new Uri(handIconPath));
        }
        var steamIconPath = IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Images", "Steam.jpg");
        if (System.IO.File.Exists(steamIconPath))
        {
            SteamBtnIcon.Source = new BitmapImage(new Uri(steamIconPath));
            AddToSteamBtnIcon.Source = new BitmapImage(new Uri(steamIconPath));
        }

        var steamIconsDir = IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Steam", "icons");
        void LoadSteamIcon(Image img, string fileName)
        {
            var path = IOPath.Combine(steamIconsDir, fileName);
            if (System.IO.File.Exists(path)) img.Source = new BitmapImage(new Uri(path));
        }
        LoadSteamIcon(AddToSteamIconDefaultImg,   "iconHD.png");
        LoadSteamIcon(AddToSteamIconChristmasImg, "iconHD Christmas.png");
        LoadSteamIcon(AddToSteamIconHalloweenImg, "iconHD Halloween.png");
        LoadSteamIcon(AddToSteamIconLgbtqImg,     "iconHD LGBTQ+.png");
        LoadSteamIcon(AddToSteamIconSummerImg,    "iconHD Summer.png");
        InitLangSelector();
        ApplyLanguage();
        SetupWindow();
        AddHandler(UIElement.PreviewKeyDownEvent, new KeyEventHandler(CaptureKonamiKeyDown), true);
        CardsScrollViewer.PreviewMouseWheel += (s, e) =>
        {
            if (CardsScrollViewer.ScrollableWidth <= 0) return;
            CardsScrollViewer.ScrollToHorizontalOffset(CardsScrollViewer.HorizontalOffset - e.Delta);
            e.Handled = true;
        };
        _ = DetectVersionsAsync();
        _ = DetectUpdatesAsync();
        _ = DetectLiveSplitAsync();
        PlayIntro();
        StartGameWatcher();
        _fpsService.FpsUpdated += fps =>
            Dispatcher.BeginInvoke(() =>
            {
                _fpsOverlay?.SetFps(fps);
            });
        StartLiveSplitPoller();
        Services.VideoTutorialService.Initialize();
        OnlineUsersService.OnlineCountUpdated += count =>
            Dispatcher.BeginInvoke(() => OnlineUsersText.Text = $"{count} online");
        ApplyOnlineUsersVisibility();
        OnlineUsersService.Start();
        LoadSteamUser();
        _discordPresence.ApplySettings(
            _discordSettings.ShowActivity,
            _discordSettings.ShowVersion,
            _discordSettings.ShowLiveSplit);
        _discordPresence.SetBrowsing();
        Loaded += (_, _) =>
            Dispatcher.BeginInvoke(DispatcherPriority.Background,
                new Action(() => AttachButtonSounds(this)));
    }

    private void InitLangSelector()
    {
        EsFlagImg.Source = CreateFlagBitmap("es");
        EnFlagImg.Source = CreateFlagBitmap("en");
        UpdateLangButton(Loc.CurrentLang);
    }

    private void UpdateLangButton(string lang)
    {
        LangSelectedText.Text = LangNames.TryGetValue(lang, out var name) ? name : lang;
        LangSelectedFlagImg.Source = CreateFlagBitmap(lang);
    }

    // Generates a flag bitmap without any external files.
    private static BitmapSource CreateFlagBitmap(string lang)
    {
        const int W = 20, H = 14;
        var pixels = new uint[W * H];
        if (lang == "es")
        {
            // Spain: red (top 25%), yellow (middle 50%), red (bottom 25%)
            for (int y = 0; y < H; y++)
            {
                uint c = (y < H / 4 || y >= H * 3 / 4) ? 0xFFAA151B : 0xFFF1BF00;
                for (int x = 0; x < W; x++) pixels[y * W + x] = c;
            }
        }
        else if (lang == "en")
        {
            // USA: 7 alternating red/white stripes + blue canton
            int stripeH = H / 7;
            for (int y = 0; y < H; y++)
            {
                int stripe = y / Math.Max(stripeH, 1);
                for (int x = 0; x < W; x++)
                {
                    bool inCanton = x < W * 2 / 5 && y < H * 4 / 7;
                    pixels[y * W + x] = inCanton ? 0xFF3C3B6E
                        : stripe % 2 == 0 ? 0xFFB22234
                        : 0xFFFFFFFF;
                }
            }
        }
        else
        {
            for (int i = 0; i < W * H; i++) pixels[i] = 0x88888888;
        }

        var bmp = new WriteableBitmap(W, H, 96, 96, PixelFormats.Bgra32, null);
        bmp.WritePixels(new Int32Rect(0, 0, W, H), pixels, W * 4, 0);
        return bmp;
    }

    private void ApplyLanguage()
    {
        UpdateLangButton(Loc.CurrentLang);
        PlayButtonText.Text       = Loc.Get("play_button");
        SettingsHeaderText.Text   = Loc.Get("settings_title");
        SoundEffectsLabel.Text    = Loc.Get("sound_effects");
        CloseSettingsBtnText.Text = Loc.Get("back");
        IntroSkipText.Text        = Loc.Get("intro_skip");
        AddInstallBtnText.Text    = Loc.Get("add_install");
        CloseVersionsBtnText.Text = Loc.Get("back");
        LanguageLabel.Text         = Loc.Get("language_label");
        WindowModeSectionLabel.Text  = Loc.Get("window_mode_section");
        WindowModeActualBtnText.Text = Loc.Get("window_mode_actual");
        WindowModeWindowedBtnText.Text = Loc.Get("window_mode_windowed");
        WindowModeHintText.Text      = Loc.Get("window_mode_hint");
        RefreshWindowModeButtons();
        OnlineUsersSectionLabel.Text = Loc.Get("online_users_section");
        OnlineUsersShowLabel.Text    = Loc.Get("online_users_show_label");
        RefreshOnlineUsersToggle();
        ControlsSectionLabel.Text  = Loc.Get("controls_section");
        CheckpointHotkeyLabel.Text = Loc.Get("checkpoint_hotkey_label");
        TutorialHotkeyLabel.Text   = Loc.Get("tutorial_hotkey_label");
        RefreshHotkeyButton();
        RefreshTutorialHotkeyButton();
        F11RemapSectionLabel.Text  = Loc.Get("f11_remap_section");
        F11RemapEnableLabel.Text   = Loc.Get("f11_remap_enable_label");
        F11RemapHintText.Text      = Loc.Get("f11_remap_hint");
        RefreshF11RemapUI();
        CoresSectionLabel.Text = Loc.Get("cores_section");
        CoresWarningText.Text  = Loc.Get("cores_warning");
        CoresFreezeLabel.Text  = Loc.Get("cores_freeze_label");
        CoresSlowerLabel.Text  = Loc.Get("cores_slower_label");
        CoresNormalLabel.Text  = Loc.Get("cores_normal_label");
        RefreshCoresButtons();
        CoresPrioritySectionLabel.Text = Loc.Get("cores_priority_section");
        CoresPriorityLabel.Text        = Loc.Get("cores_priority_label");
        CoresPriorityHighItem.Content  = Loc.Get("cores_priority_high");
        CoresPriorityLowItem.Content   = Loc.Get("cores_priority_low");
        RefreshCoresPriorityUI();
        SettingsTabGeneralText.Text      = Loc.Get("settings_tab_general");
        SettingsTabControlsText.Text     = Loc.Get("settings_tab_controls");
        SettingsTabSteamText.Text        = Loc.Get("settings_tab_steam");
        SettingsTabControllerText.Text   = Loc.Get("settings_tab_controller");
        SettingsTabDiscordText.Text      = Loc.Get("settings_tab_discord");
        SettingsTabUpdatesText.Text      = Loc.Get("settings_tab_updates");
        SettingsTabLiveSplitText.Text    = Loc.Get("settings_tab_livesplit");
        SettingsTabCoresText.Text        = Loc.Get("settings_tab_cores");
        SettingsTabOverlaysText.Text     = Loc.Get("settings_tab_overlays");
        OverlaysSectionLabel.Text        = Loc.Get("overlays_section");
        OverlayEnableLabel.Text          = Loc.Get("overlay_enable_label");
        OverlayControllerLabel.Text      = Loc.Get("overlay_controller_label");
        OverlayDualSenseBtnText.Text     = Loc.Get("overlay_dualsense");
        OverlayXboxBtnText.Text          = Loc.Get("overlay_xbox");
        OverlayKeyboardBtnText.Text      = Loc.Get("overlay_keyboard");
        OverlayCornerLabel.Text             = Loc.Get("overlay_corner_label");
        OverlayCornerTopLeftBtnText.Text     = Loc.Get("overlay_corner_topleft");
        OverlayCornerTopRightBtnText.Text    = Loc.Get("overlay_corner_topright");
        OverlayCornerBottomLeftBtnText.Text  = Loc.Get("overlay_corner_bottomleft");
        OverlayCornerBottomRightBtnText.Text = Loc.Get("overlay_corner_bottomright");
        FpsOverlaySectionLabel.Text             = Loc.Get("fps_overlay_section");
        FpsOverlayEnableLabel.Text               = Loc.Get("fps_overlay_enable_label");
        FpsOverlayCornerLabel.Text               = Loc.Get("overlay_corner_label");
        FpsOverlayCornerTopLeftBtnText.Text      = Loc.Get("overlay_corner_topleft");
        FpsOverlayCornerTopRightBtnText.Text     = Loc.Get("overlay_corner_topright");
        FpsOverlayCornerBottomLeftBtnText.Text   = Loc.Get("overlay_corner_bottomleft");
        FpsOverlayCornerBottomRightBtnText.Text  = Loc.Get("overlay_corner_bottomright");
        FpsOverlaySizeLabel.Text                 = Loc.Get("fps_overlay_size_label");
        FpsOverlaySizeSmallBtnText.Text          = Loc.Get("fps_overlay_size_small");
        FpsOverlaySizeMediumBtnText.Text         = Loc.Get("fps_overlay_size_medium");
        FpsOverlaySizeLargeBtnText.Text          = Loc.Get("fps_overlay_size_large");
        FpsOverlayFontLabel.Text                 = Loc.Get("fps_overlay_font_label");
        FpsOverlayFontPoppyBtnText.Text          = Loc.Get("fps_overlay_font_poppy");
        FpsOverlayFontPoppyBtnText.FontFamily    = FpsOverlayWindow.PoppyPlaytimeFont;
        FpsOverlayFontMonospaceBtnText.Text      = Loc.Get("fps_overlay_font_monospace");
        SettingsTabIconThemeText.Text    = Loc.Get("settings_tab_icontheme");
        SettingsTabLoadManipText.Text    = Loc.Get("settings_tab_loadmanip");
        HandModsHubLabel.Text            = Loc.Get("handmods_hub_label");
        HandModsChapter1BtnText.Text     = Loc.Get("chapter1_section");
        HandModsChapter2BtnText.Text     = Loc.Get("chapter2_section");
        HandModsChapter3BtnText.Text     = Loc.Get("chapter3_section");
        HandModsChapter4BtnText.Text     = Loc.Get("chapter4_section");
        HandModsChapter5BtnText.Text     = Loc.Get("chapter5_section");
        HandModsSubmitEntryBtnText.Text  = Loc.Get("handmods_submit_entry_btn");
        HandModsSubmitDiscordLabel.Text  = Loc.Get("handmods_submit_discord_label");
        HandModsSubmitDiscordConnectBtnText.Text    = Loc.Get("bug_report_discord_connect_btn");
        HandModsSubmitDiscordWaitingText.Text       = Loc.Get("bug_report_discord_waiting");
        HandModsSubmitDiscordCancelAuthBtnText.Text = Loc.Get("bug_report_discord_cancel");
        HandModsSubmitChapterLabel.Text  = Loc.Get("handmods_submit_chapter_label");
        HandModsSubmitNameLabel.Text     = Loc.Get("handmods_submit_name_label");
        HandModsSubmitColorsLabel.Text   = Loc.Get("handmods_submit_colors_label");
        HandModsSubmitFilesLabel.Text    = Loc.Get("handmods_submit_files_label");
        HandModsSubmitFilesBtnText.Text  = Loc.Get("handmods_submit_files_btn");
        HandModsSubmitFilesText.Text     = Loc.Get("handmods_submit_no_files");
        HandModsSubmitSendBtnText.Text   = Loc.Get("handmods_submit_send_btn");
        IconThemeSectionLabel.Text       = Loc.Get("icontheme_section");
        IconThemeHintText.Text           = Loc.Get("icontheme_hint");
        IconThemeDefaultBtnText.Text     = Loc.Get("icontheme_default");
        IconThemeClassicBtnText.Text     = Loc.Get("icontheme_classic");
        IconThemeLgbtqBtnText.Text       = Loc.Get("icontheme_lgbtq");
        IconThemeSummerBtnText.Text      = Loc.Get("icontheme_summer");
        IconThemeHalloweenBtnText.Text   = Loc.Get("icontheme_halloween");
        IconThemeChristmasBtnText.Text   = Loc.Get("icontheme_christmas");
        CoresEnableLabel.Text            = Loc.Get("cores_enable_label");
        LoadManipControlsSectionLabel.Text = Loc.Get("loadmanip_controls_section");
        LoadManipChapter1NavBtnText.Text = Loc.Get("chapter1_section");
        LoadManipChapter4NavBtnText.Text = Loc.Get("chapter4_section");
        LoadManipChapter5NavBtnText.Text = Loc.Get("chapter5_section");
        LoadManipChapter1BackBtnText.Text = Loc.Get("back");
        LoadManipChapter4BackBtnText.Text = Loc.Get("back");
        LoadManipChapter5BackBtnText.Text = Loc.Get("back");
        Chapter1SectionLabel.Text        = Loc.Get("chapter1_section");
        Chapter1FreezeLabel.Text         = Loc.Get("chapter1_freeze_label");
        Chapter1NormalLabel.Text         = Loc.Get("chapter1_normal_label");
        Chapter5SectionLabel.Text        = Loc.Get("chapter5_section");
        Chapter5FreezeLabel.Text         = Loc.Get("chapter5_freeze_label");
        Chapter5NormalLabel.Text         = Loc.Get("chapter5_normal_label");
        Chapter1HintText.Text            = Loc.Get("chapter1_hint");
        Chapter1FullbrightSectionLabel.Text = Loc.Get("chapter1_fullbright_section");
        Chapter1FullbrightUnlitLabel.Text   = Loc.Get("chapter1_fullbright_unlit_label");
        Chapter1FullbrightLitLabel.Text     = Loc.Get("chapter1_fullbright_lit_label");
        Chapter5FullbrightSectionLabel.Text = Loc.Get("chapter5_fullbright_section");
        Chapter5FullbrightUnlitLabel.Text   = Loc.Get("chapter5_fullbright_unlit_label");
        Chapter5FullbrightLitLabel.Text     = Loc.Get("chapter5_fullbright_lit_label");
        RefreshChapter1UI();
        RefreshChapter5LoadManipUI();
        RefreshFullBrightKeysUI();
        RefreshChapter5FullBrightUI();
        Chapter4SectionLabel.Text        = Loc.Get("chapter4_section");
        Chapter4EnableLabel.Text         = Loc.Get("chapter4_enable_label");
        Chapter4FreezeLabel.Text         = Loc.Get("chapter4_freeze_label");
        Chapter4SlowLabel.Text           = Loc.Get("chapter4_slow_label");
        Chapter4NormalLabel.Text         = Loc.Get("chapter4_normal_label");
        Chapter4HintText.Text            = Loc.Get("chapter4_hint");
        RefreshChapter4UI();
        ToolTipService.SetToolTip(SettingsButton, Loc.Get("settings_tooltip"));

        OpenUpdatesBtnText.Text        = Loc.Get("updates_header");
        OpenUpdatesBtnBadge.Text       = "↑ UPDATE";

        OpenLiveSplitBtnText.Text      = "LiveSplit";
        OpenLiveSplitBtnBadge.Text     = "↑ UPDATE";
        CopyForSteamBtnText.Text       = Loc.Get("steam_launch_btn");
        AddToSteamBtnText.Text         = Loc.Get("add_to_steam_btn");
        AddToSteamIconPickerLabel.Text = Loc.Get("add_to_steam_icon_label");
        AddToSteamCancelBtnText.Text   = Loc.Get("cancel");
        AddToSteamConfirmBtnText.Text  = Loc.Get("add_to_steam_btn");
        SteamTutorialBtnText.Text      = Loc.Get("steam_tutorial_btn");
        CloseLiveSplitBtnText.Text     = Loc.Get("back");
        LiveSplitInstalledVersionLabel.Text = Loc.Get("livesplit_installed_version");
        LiveSplitLatestVersionLabel.Text    = Loc.Get("livesplit_latest_version");
        RefreshLiveSplitButton();
        if (LiveSplitOverlay.Visibility == Visibility.Visible)
            RefreshLiveSplitOverlay();
        UpdatesHeaderText.Text         = Loc.Get("updates_header");
        UpdateCurrentVersionLabel.Text = Loc.Get("updates_current_version");
        UpdateCurrentVersionText.Text  = AppVersion.GetDisplayVersion();
        VersionLabel.Text              = AppVersion.GetDisplayVersion();
        UpdateCheckHint.Text           = Loc.Get("updates_check_hint");
        UpdateDetailsLabel.Text        = Loc.Get("updates_details_label");
        UpdateLatestVersionLabel.Text  = Loc.Get("updates_latest_version");
        UpdateFileNameLabel.Text       = Loc.Get("updates_file_name");
        UpdateFileSizeLabel.Text       = Loc.Get("updates_file_size");
        WhatsNewLabel.Text             = Loc.Get("updates_whats_new");
        AcceptInstallBtnText.Text      = Loc.Get("updates_download_btn");
        CancelInstallBtnText.Text      = Loc.Get("updates_cancel_btn");
        CloseUpdatesBtnText.Text       = Loc.Get("updates_close");
        ManualUpdateLinkText.Text      = Loc.Get("updates_manual_link");

        DiscordShowActivityLabel.Text    = Loc.Get("discord_show_activity");
        DiscordShowVersionLabel.Text     = Loc.Get("discord_show_version");
        DiscordShowLiveSplitLabel.Text   = Loc.Get("discord_show_livesplit");
        RefreshDiscordToggles();

        SaveCardHeaderText.Text        = Loc.Get("save_card_header");
        SaveCardDeleteBtnText.Text     = Loc.Get("save_card_delete_btn");
        SaveCardSaveBtnText.Text       = Loc.Get("save_card_save_btn");
        CloseSaveCardBtnText.Text      = Loc.Get("back");

        CheckpointSelectHeaderText.Text   = Loc.Get("checkpoint_select_header");
        CloseCheckpointSelectBtnText.Text = Loc.Get("back");

        AutoSplitterHeaderText.Text   = Loc.Get("auto_splitter_header");
        CloseAutoSplitterBtnText.Text = Loc.Get("back");

        ChangelogBtnText.Text      = Loc.Get("changelog_btn");
        ChangelogHeaderText.Text   = Loc.Get("changelog_header");
        CloseChangelogBtnText.Text = Loc.Get("back");

        BugReportBtnText.Text              = Loc.Get("bug_report_btn");
        BugReportHeaderText.Text           = Loc.Get("bug_report_title");
        BugDiscordLabel.Text                   = Loc.Get("bug_report_discord_label");
        BugDiscordConnectBtnText.Text          = Loc.Get("bug_report_discord_connect_btn");
        BugDiscordWaitingText.Text             = Loc.Get("bug_report_discord_waiting");
        BugDiscordCancelAuthBtnText.Text       = Loc.Get("bug_report_discord_cancel");
        DiscordInfoHeader.Text                 = Loc.Get("bug_report_discord_info_header");
        DiscordInfoWhyTitle.Text               = Loc.Get("bug_report_discord_info_why_title");
        DiscordInfoWhyText.Text                = Loc.Get("bug_report_discord_info_why_text");
        DiscordInfoPermsTitle.Text             = Loc.Get("bug_report_discord_info_perms_title");
        DiscordInfoPermIdentifyName.Text       = Loc.Get("bug_report_discord_info_perm_name");
        DiscordInfoPermIdentifyDesc.Text       = Loc.Get("bug_report_discord_info_perm_desc");
        DiscordInfoNoAccessText.Text           = Loc.Get("bug_report_discord_info_no_access");
        DiscordInfoCloseBtnText.Text           = Loc.Get("back");
        BugETitleLabel.Text                = Loc.Get("bug_report_etitle_label");
        BugDescLabel.Text                  = Loc.Get("bug_report_desc_label");
        BugImageLabel.Text                 = Loc.Get("bug_report_image_label");
        BugImageBtnText.Text               = Loc.Get("bug_report_image_btn");
        BugImageFileName.Text              = Loc.Get("bug_report_image_none");
        SendBugReportBtnText.Text          = Loc.Get("bug_report_send_btn");
        CloseBugReportBtnText.Text         = Loc.Get("back");

        CardsPanel.Children.Clear();
        _cards.Clear();
        BuildCards();
        SelectChapter(_selected);

        if (VersionsOverlay.Visibility == Visibility.Visible)
            BuildInstallationsList();
    }

    private void LoadSteamUser()
    {
        try
        {
            var user = Services.SteamCmdRunner.GetActiveUser();
            if (user is null) return;

            SteamPersonaName.Text = user.PersonaName;

            var avatarPath = Services.SteamCmdRunner.GetAvatarPath(user.SteamId64);
            if (avatarPath != null)
            {
                var bmp = new BitmapImage();
                bmp.BeginInit();
                bmp.UriSource    = new Uri(avatarPath);
                bmp.CacheOption  = BitmapCacheOption.OnLoad;
                bmp.EndInit();
                SteamAvatar.Source = bmp;
            }

            SteamUserPanel.Visibility = Visibility.Visible;
        }
        catch { }
    }

    private static string TranslatePresetName(string name) =>
        name.Replace("Parche", Loc.Get("patch"));

    private void LangSelectorBtn_PreviewMouseDown(object sender, MouseButtonEventArgs e)
    {
        // Capture state before StaysOpen=False closes the popup on focus loss.
        _popupWasOpen = LangPopup.IsOpen;
    }

    private void LangSelectorBtn_Click(object sender, RoutedEventArgs e)
    {
        LangPopup.IsOpen = !_popupWasOpen;
    }

    private void LangPopup_Opened(object sender, EventArgs e)
    {
        var ease = new CubicEase { EasingMode = EasingMode.EaseOut };

        var scaleAnim = new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(180)) { EasingFunction = ease };
        LangDropdownScale.BeginAnimation(ScaleTransform.ScaleYProperty, scaleAnim);

        var opacityAnim = new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(150)) { EasingFunction = ease };
        LangDropdownBorder.BeginAnimation(UIElement.OpacityProperty, opacityAnim);

        var rotateAnim = new DoubleAnimation(0, 180, TimeSpan.FromMilliseconds(200)) { EasingFunction = ease };
        LangChevronRotate.BeginAnimation(RotateTransform.AngleProperty, rotateAnim);
    }

    private void LangPopup_Closed(object sender, EventArgs e)
    {
        var ease = new CubicEase { EasingMode = EasingMode.EaseIn };
        var rotateAnim = new DoubleAnimation(180, 0, TimeSpan.FromMilliseconds(200)) { EasingFunction = ease };
        LangChevronRotate.BeginAnimation(RotateTransform.AngleProperty, rotateAnim);
    }

    private void LangOptionBtn_Click(object sender, RoutedEventArgs e)
    {
        if (sender is Button btn && btn.Tag is string lang && lang != Loc.CurrentLang)
        {
            LangPopup.IsOpen = false;
            Loc.Load(lang);
            ApplyLanguage();
        }
        else
        {
            LangPopup.IsOpen = false;
        }
    }

    // ── Intro video ───────────────────────────────────────────────────────────

    private void PlayIntro()
    {
        var videoPath = IOPath.Combine(Services.ResourceExtractor.TempDir, "Assets", "Videos", "Introduccion.mp4");
        if (!File.Exists(videoPath))
        {
            IntroOverlay.Visibility = Visibility.Collapsed;
            return;
        }
        IntroPlayer.Source = new Uri(videoPath);
        IntroPlayer.MediaEnded += (_, _) => Dispatcher.BeginInvoke(new Action(HideIntro));
        IntroPlayer.Play();
    }

    private void HideIntro()
    {
        IntroPlayer.Stop();
        IntroOverlay.Visibility = Visibility.Collapsed;
        if (SteamCmdRunner.Find() is null)
            _ = AcquireSteamCmdAsync();
    }

    private void IntroOverlay_MouseDown(object sender, MouseButtonEventArgs e) => HideIntro();

    // ── Window ────────────────────────────────────────────────────────────────

    private void SetupWindow()
    {
        Title = "Poppy Playtime — Speedrun Launcher";
        WindowState = WindowState.Maximized;
        WindowStyle = WindowStyle.None;
        ApplyWindowMode();
    }

    private void ApplyWindowMode()
    {
        bool windowed = WindowModeSettings.Current.Mode == "windowed";
        MinimizeBtn.Visibility = windowed ? Visibility.Visible : Visibility.Collapsed;
    }

    private void MinimizeBtn_Click(object sender, RoutedEventArgs e) => WindowState = WindowState.Minimized;

    private void WindowModeActualBtn_Click(object sender, RoutedEventArgs e)   => SetWindowMode("actual");
    private void WindowModeWindowedBtn_Click(object sender, RoutedEventArgs e) => SetWindowMode("windowed");

    private void SetWindowMode(string mode)
    {
        var settings = WindowModeSettings.Current;
        settings.Mode = mode;
        settings.Save();
        RefreshWindowModeButtons();
        ApplyWindowMode();
    }

    private void RefreshWindowModeButtons()
    {
        var selectedBrush  = new SolidColorBrush(Teal);
        var selectedBg     = new SolidColorBrush(Color.FromArgb(255, 0, 40, 30));
        var selectedBorder = new SolidColorBrush(Teal);
        var dimBrush       = new SolidColorBrush(Color.FromArgb(255, 58, 106, 138));
        var dimBg          = new SolidColorBrush(Color.FromArgb(255, 6, 15, 24));
        var dimBorder      = new SolidColorBrush(Color.FromArgb(255, 13, 37, 53));

        void Style(Button btn, TextBlock text, bool selected)
        {
            btn.Background  = selected ? selectedBg : dimBg;
            btn.BorderBrush = selected ? selectedBorder : dimBorder;
            text.Foreground = selected ? selectedBrush : dimBrush;
        }

        var mode = WindowModeSettings.Current.Mode;
        Style(WindowModeActualBtn,   WindowModeActualBtnText,   mode == "actual");
        Style(WindowModeWindowedBtn, WindowModeWindowedBtnText, mode == "windowed");
    }

    // ── Sound ─────────────────────────────────────────────────────────────────

    [System.Runtime.InteropServices.DllImport("user32.dll")]
    private static extern bool RegisterHotKey(nint hWnd, int id, uint fsModifiers, uint vk);

    [System.Runtime.InteropServices.DllImport("user32.dll")]
    private static extern bool UnregisterHotKey(nint hWnd, int id);

    private const uint MOD_ALT     = 0x0001;
    private const uint MOD_CONTROL = 0x0002;
    private const uint MOD_SHIFT   = 0x0004;
    private const uint MOD_WIN     = 0x0008;
    private const uint VK_RETURN   = 0x0D;
    private const uint VK_F1       = 0x70;
    private const uint VK_F2       = 0x71;
    private const uint VK_F9       = 0x78;
    private const int  HOTKEY_ID   = 9001;
    private const int  TUTORIAL_HOTKEY_ID = 9002;

    private static readonly string HotkeyFile =
        IOPath.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "hotkey.cfg");

    private static readonly string TutorialHotkeyFile =
        IOPath.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "tutorial_hotkey.cfg");

    private static readonly string VolumeFile =
        IOPath.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "volume.cfg");

    protected override void OnSourceInitialized(EventArgs e)
    {
        base.OnSourceInitialized(e);
        var helper = new System.Windows.Interop.WindowInteropHelper(this);
        var source = System.Windows.Interop.HwndSource.FromHwnd(helper.Handle);
        source?.AddHook(WndProc);
        LoadHotkey();
        LoadTutorialHotkey();
        LoadVolume();
        RegisterHotKey(helper.Handle, HOTKEY_ID, _hotkeyModifiers, _hotkeyVk);
        RegisterHotKey(helper.Handle, TUTORIAL_HOTKEY_ID, _tutorialHotkeyModifiers, _tutorialHotkeyVk);
        RefreshHotkeyButton();
        LoadCoresHotkeys();
        LoadCoresEnabled();
        LoadCoresPriority();
        if (_coresEnabled) InstallCoresHook();
        LoadChapter4RemapHotkeys();
        LoadChapter4RemapEnabled();
        if (_chapter4RemapEnabled) InstallChapter4RemapHook();
        LoadOverlaySettings();
        _f11Remap.Load();
        _f11Remap.Refresh();
        RefreshF11RemapUI();
    }

    protected override void OnClosing(System.ComponentModel.CancelEventArgs e)
    {
        base.OnClosing(e);
        if (_controllerOverlay != null)
        {
            _controllerOverlay.Closed -= OverlayWindow_Closed;
            _controllerOverlay.Close();
        }
        _cores.RestoreIfActive();
        var hwnd = new System.Windows.Interop.WindowInteropHelper(this).Handle;
        UnregisterHotKey(hwnd, HOTKEY_ID);
        UnregisterHotKey(hwnd, TUTORIAL_HOTKEY_ID);
        UninstallCoresHook();
        UninstallChapter4RemapHook();
        _hotkeyOverlay?.Close();
        _tutorialOverlay?.Close();
        _beginnerTutorialOverlay?.Close();
        _leaderboardOverlay?.Close();
        _gameToast?.Close();
        _tutorialToast?.Close();
        _coresToast?.Close();
        _ue4ssRemapToast?.Close();
        _liveSplitPollCts?.Cancel();
        _liveSplitClient.Dispose();
        OnlineUsersService.Stop();
        _discordPresence.Dispose();
        _f11Remap.Dispose();
        _fpsOverlay?.Close();
        _fpsService.Dispose();
    }

    private nint WndProc(nint hwnd, int msg, nint wParam, nint lParam, ref bool handled)
    {
        if (msg == 0x0312)
        {
            if ((int)wParam == HOTKEY_ID)
            {
                ToggleHotkeyOverlay();
                handled = true;
            }
            else if ((int)wParam == TUTORIAL_HOTKEY_ID)
            {
                ToggleTutorialOverlay();
                handled = true;
            }
        }
        return 0;
    }

    private void ToggleTutorialOverlay()
    {
        if (_tutorialOverlay is { IsVisible: true })
        {
            _tutorialOverlay.Close();
        }
        else
        {
            _tutorialOverlay = new VideoTutorialOverlay(_discordPresence);
            _tutorialOverlay.Closed += (_, _) =>
            {
                _tutorialOverlay = null;
                var ch = _chapters.Count > 0 ? _chapters[_selected] : null;
                if (ch != null) _discordPresence.SetChapterSelected(ch, GetVersionLabel(ch));
                else _discordPresence.SetBrowsing();
            };
            _tutorialOverlay.Show();
            _tutorialOverlay.Activate();
        }
    }

    private void ToggleHotkeyOverlay()
    {
        if (_hotkeyOverlay is { IsVisible: true })
        {
            _hotkeyOverlay.Close();
        }
        else
        {
            if (IntroOverlay.Visibility == Visibility.Visible)
                HideIntro();

            var exePaths = _chapters.Select(c => c.GameExePath).ToArray();
            _hotkeyOverlay = new HotkeyOverlay(exePaths);

            var runningCh = GetRunningChapter();
            if (runningCh != null)
                _discordPresence.SetSelectingCheckpoint(runningCh, GetVersionLabel(runningCh));

            _hotkeyOverlay.Closed += (_, _) =>
            {
                _hotkeyOverlay = null;
                var ch = GetRunningChapter();
                if (ch != null)
                    _discordPresence.SetGameRunning(ch, GetVersionLabel(ch));
            };
            _hotkeyOverlay.Show();
            _hotkeyOverlay.Activate();
        }
    }

    // ── Hotkey configuration ──────────────────────────────────────────────────

    private void LoadHotkey()
    {
        try
        {
            if (!File.Exists(HotkeyFile)) return;
            var parts = File.ReadAllText(HotkeyFile).Trim().Split(',');
            if (parts.Length == 2
                && uint.TryParse(parts[0], out var mod)
                && uint.TryParse(parts[1], out var vk))
            {
                _hotkeyModifiers = mod;
                _hotkeyVk        = vk;
            }
        }
        catch { }
    }

    private void SaveHotkey()
    {
        try
        {
            var dir = IOPath.GetDirectoryName(HotkeyFile)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(HotkeyFile, $"{_hotkeyModifiers},{_hotkeyVk}");
        }
        catch { }
    }

    private static string FormatHotkey(uint modifiers, uint vk)
    {
        var parts = new List<string>();
        if ((modifiers & MOD_CONTROL) != 0) parts.Add("Ctrl");
        if ((modifiers & MOD_ALT)     != 0) parts.Add("Alt");
        if ((modifiers & MOD_SHIFT)   != 0) parts.Add("Shift");
        if ((modifiers & MOD_WIN)     != 0) parts.Add("Win");
        var key = KeyInterop.KeyFromVirtualKey((int)vk);
        parts.Add(KeyToString(key));
        return string.Join(" + ", parts);
    }

    private static string KeyToString(Key key) => key switch
    {
        Key.Return => "Enter",
        Key.Back   => "Backspace",
        Key.Escape => "Esc",
        Key.Space  => "Space",
        Key.Prior  => "PgUp",
        Key.Next   => "PgDn",
        Key.Delete => "Del",
        Key.Insert => "Ins",
        Key.Left   => "←",
        Key.Right  => "→",
        Key.Up     => "↑",
        Key.Down   => "↓",
        _ => key.ToString(),
    };

    private void RefreshHotkeyButton()
    {
        CheckpointHotkeyText.Text       = FormatHotkey(_hotkeyModifiers, _hotkeyVk);
        CheckpointHotkeyBtn.BorderBrush = new SolidColorBrush(Color.FromArgb(255, 26, 58, 85));
        CheckpointHotkeyText.Foreground = new SolidColorBrush(Color.FromArgb(255, 138, 170, 187));
    }

    private void CheckpointHotkeyBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_capturingHotkey)
        {
            _capturingHotkey = false;
            if (_hotkeyCapture != null)
            {
                RemoveHandler(UIElement.PreviewKeyDownEvent, _hotkeyCapture);
                _hotkeyCapture = null;
            }
            RefreshHotkeyButton();
            return;
        }

        _capturingHotkey = true;
        CheckpointHotkeyText.Text       = Loc.Get("hotkey_press_keys");
        CheckpointHotkeyText.Foreground = new SolidColorBrush(Color.FromArgb(255, 0, 204, 170));
        CheckpointHotkeyBtn.BorderBrush = new SolidColorBrush(Color.FromArgb(255, 0, 204, 170));

        _hotkeyCapture = CaptureHotkeyKeyDown;
        AddHandler(UIElement.PreviewKeyDownEvent, _hotkeyCapture, true);
    }

    private void CaptureHotkeyKeyDown(object sender, KeyEventArgs e)
    {
        var key = e.Key == Key.System ? e.SystemKey : e.Key;

        if (key is Key.LeftCtrl or Key.RightCtrl
                or Key.LeftShift or Key.RightShift
                or Key.LeftAlt or Key.RightAlt
                or Key.LWin or Key.RWin
                or Key.None)
            return;

        RemoveHandler(UIElement.PreviewKeyDownEvent, _hotkeyCapture);
        _hotkeyCapture   = null;
        _capturingHotkey = false;

        if (key == Key.Escape)
        {
            RefreshHotkeyButton();
            e.Handled = true;
            return;
        }

        var modifiers = 0u;
        if ((Keyboard.Modifiers & ModifierKeys.Control) != 0) modifiers |= MOD_CONTROL;
        if ((Keyboard.Modifiers & ModifierKeys.Shift)   != 0) modifiers |= MOD_SHIFT;
        if ((Keyboard.Modifiers & ModifierKeys.Alt)     != 0) modifiers |= MOD_ALT;
        if ((Keyboard.Modifiers & ModifierKeys.Windows) != 0) modifiers |= MOD_WIN;

        var vk = (uint)KeyInterop.VirtualKeyFromKey(key);

        var helper = new System.Windows.Interop.WindowInteropHelper(this);
        UnregisterHotKey(helper.Handle, HOTKEY_ID);
        _hotkeyModifiers = modifiers;
        _hotkeyVk        = vk;
        RegisterHotKey(helper.Handle, HOTKEY_ID, _hotkeyModifiers, _hotkeyVk);
        SaveHotkey();
        RefreshHotkeyButton();

        e.Handled = true;
    }

    // ── Tutorial hotkey configuration ─────────────────────────────────────────

    private void LoadTutorialHotkey()
    {
        try
        {
            if (!File.Exists(TutorialHotkeyFile)) return;
            var parts = File.ReadAllText(TutorialHotkeyFile).Trim().Split(',');
            if (parts.Length == 2
                && uint.TryParse(parts[0], out var mod)
                && uint.TryParse(parts[1], out var vk))
            {
                _tutorialHotkeyModifiers = mod;
                _tutorialHotkeyVk        = vk;
            }
        }
        catch { }
    }

    private void SaveTutorialHotkey()
    {
        try
        {
            var dir = IOPath.GetDirectoryName(TutorialHotkeyFile)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(TutorialHotkeyFile, $"{_tutorialHotkeyModifiers},{_tutorialHotkeyVk}");
        }
        catch { }
    }

    private void LoadVolume()
    {
        try
        {
            if (!File.Exists(VolumeFile)) return;
            if (float.TryParse(File.ReadAllText(VolumeFile).Trim(),
                    System.Globalization.NumberStyles.Float,
                    System.Globalization.CultureInfo.InvariantCulture,
                    out var vol))
            {
                _sfxVolume = Math.Clamp(vol, 0f, 1f);
                VolumeSlider.Value = _sfxVolume * 100.0;
            }
        }
        catch { }
        finally { _volumeLoaded = true; }
    }

    private void SaveVolume()
    {
        // Skip saves triggered by the XAML default Value or by LoadVolume itself,
        // so a restart doesn't overwrite the stored volume with the slider's default.
        if (!_volumeLoaded) return;

        try
        {
            var dir = IOPath.GetDirectoryName(VolumeFile)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(VolumeFile,
                _sfxVolume.ToString("F4", System.Globalization.CultureInfo.InvariantCulture));
        }
        catch { }
    }

    private void RefreshTutorialHotkeyButton()
    {
        TutorialHotkeyText.Text       = FormatHotkey(_tutorialHotkeyModifiers, _tutorialHotkeyVk);
        TutorialHotkeyBtn.BorderBrush = new SolidColorBrush(Color.FromArgb(255, 26, 58, 85));
        TutorialHotkeyText.Foreground = new SolidColorBrush(Color.FromArgb(255, 138, 170, 187));
    }

    private void TutorialHotkeyBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_capturingTutorialHotkey)
        {
            _capturingTutorialHotkey = false;
            if (_tutorialHotkeyCapture != null)
            {
                RemoveHandler(UIElement.PreviewKeyDownEvent, _tutorialHotkeyCapture);
                _tutorialHotkeyCapture = null;
            }
            RefreshTutorialHotkeyButton();
            return;
        }

        _capturingTutorialHotkey = true;
        TutorialHotkeyText.Text       = Loc.Get("hotkey_press_keys");
        TutorialHotkeyText.Foreground = new SolidColorBrush(Color.FromArgb(255, 0, 204, 170));
        TutorialHotkeyBtn.BorderBrush = new SolidColorBrush(Color.FromArgb(255, 0, 204, 170));

        _tutorialHotkeyCapture = CaptureTutorialHotkeyKeyDown;
        AddHandler(UIElement.PreviewKeyDownEvent, _tutorialHotkeyCapture, true);
    }

    private void CaptureTutorialHotkeyKeyDown(object sender, KeyEventArgs e)
    {
        var key = e.Key == Key.System ? e.SystemKey : e.Key;

        if (key is Key.LeftCtrl or Key.RightCtrl
                or Key.LeftShift or Key.RightShift
                or Key.LeftAlt or Key.RightAlt
                or Key.LWin or Key.RWin
                or Key.None)
            return;

        RemoveHandler(UIElement.PreviewKeyDownEvent, _tutorialHotkeyCapture);
        _tutorialHotkeyCapture   = null;
        _capturingTutorialHotkey = false;

        if (key == Key.Escape)
        {
            RefreshTutorialHotkeyButton();
            e.Handled = true;
            return;
        }

        var modifiers = 0u;
        if ((Keyboard.Modifiers & ModifierKeys.Control) != 0) modifiers |= MOD_CONTROL;
        if ((Keyboard.Modifiers & ModifierKeys.Shift)   != 0) modifiers |= MOD_SHIFT;
        if ((Keyboard.Modifiers & ModifierKeys.Alt)     != 0) modifiers |= MOD_ALT;
        if ((Keyboard.Modifiers & ModifierKeys.Windows) != 0) modifiers |= MOD_WIN;

        var vk = (uint)KeyInterop.VirtualKeyFromKey(key);

        var helper = new System.Windows.Interop.WindowInteropHelper(this);
        UnregisterHotKey(helper.Handle, TUTORIAL_HOTKEY_ID);
        _tutorialHotkeyModifiers = modifiers;
        _tutorialHotkeyVk        = vk;
        RegisterHotKey(helper.Handle, TUTORIAL_HOTKEY_ID, _tutorialHotkeyModifiers, _tutorialHotkeyVk);
        SaveTutorialHotkey();
        RefreshTutorialHotkeyButton();

        e.Handled = true;
    }

    // ── Settings tabs ───────────────────────────────────────────────────────

    private int _settingsTab;

    private void SettingsTabGeneral_Click(object sender, RoutedEventArgs e)      => SelectSettingsTab(0);
    private void SettingsTabControls_Click(object sender, RoutedEventArgs e)     => SelectSettingsTab(1);
    private void SettingsTabLoadManip_Click(object sender, RoutedEventArgs e)   => SelectSettingsTab(2);
    private void SettingsTabCores_Click(object sender, RoutedEventArgs e)        => SelectSettingsTab(3);
    private void SettingsTabSteam_Click(object sender, RoutedEventArgs e)        => SelectSettingsTab(4);
    private void SettingsTabController_Click(object sender, RoutedEventArgs e)   => SelectSettingsTab(5);
    private void SettingsTabDiscord_Click(object sender, RoutedEventArgs e)      => SelectSettingsTab(6);
    private void SettingsTabUpdates_Click(object sender, RoutedEventArgs e)      => SelectSettingsTab(7);
    private void SettingsTabLiveSplit_Click(object sender, RoutedEventArgs e)    => SelectSettingsTab(8);
    private void SettingsTabOverlays_Click(object sender, RoutedEventArgs e)    => SelectSettingsTab(9);
    private void SettingsTabIconTheme_Click(object sender, RoutedEventArgs e)   => SelectSettingsTab(10);

    private void SelectSettingsTab(int index)
    {
        _settingsTab = index;

        SettingsGeneralScroll.Visibility      = index == 0 ? Visibility.Visible : Visibility.Collapsed;
        SettingsControlsScroll.Visibility     = index == 1 ? Visibility.Visible : Visibility.Collapsed;
        SettingsLoadManipScroll.Visibility    = index == 2 ? Visibility.Visible : Visibility.Collapsed;
        SettingsCoresScroll.Visibility        = index == 3 ? Visibility.Visible : Visibility.Collapsed;
        SettingsSteamScroll.Visibility        = index == 4 ? Visibility.Visible : Visibility.Collapsed;
        SettingsControllerScroll.Visibility   = index == 5 ? Visibility.Visible : Visibility.Collapsed;
        SettingsDiscordScroll.Visibility      = index == 6 ? Visibility.Visible : Visibility.Collapsed;
        SettingsUpdatesScroll.Visibility      = index == 7 ? Visibility.Visible : Visibility.Collapsed;
        SettingsLiveSplitScroll.Visibility    = index == 8 ? Visibility.Visible : Visibility.Collapsed;
        SettingsOverlaysScroll.Visibility     = index == 9 ? Visibility.Visible : Visibility.Collapsed;
        SettingsIconThemeScroll.Visibility    = index == 10 ? Visibility.Visible : Visibility.Collapsed;

        if (index == 2)
        {
            SelectLoadManipSubPage(0);
            RefreshChapter1UI();
            RefreshChapter5LoadManipUI();
            RefreshFullBrightKeysUI();
            RefreshChapter4UI();
        }
        if (index == 3) RefreshCoresToggle();
        if (index == 6) RefreshDiscordToggles();
        if (index == 9) RefreshOverlaysTab();
        if (index == 10) RefreshIconThemeButtons();

        var tabs = new[]
        {
            (SettingsTabGeneralBorder,      SettingsTabGeneralText),
            (SettingsTabControlsBorder,     SettingsTabControlsText),
            (SettingsTabLoadManipBorder,    SettingsTabLoadManipText),
            (SettingsTabCoresBorder,        SettingsTabCoresText),
            (SettingsTabSteamBorder,        SettingsTabSteamText),
            (SettingsTabControllerBorder,   SettingsTabControllerText),
            (SettingsTabDiscordBorder,      SettingsTabDiscordText),
            (SettingsTabUpdatesBorder,      SettingsTabUpdatesText),
            (SettingsTabLiveSplitBorder,    SettingsTabLiveSplitText),
            (SettingsTabOverlaysBorder,     SettingsTabOverlaysText),
            (SettingsTabIconThemeBorder,    SettingsTabIconThemeText),
        };

        var tealBrush  = new SolidColorBrush(Teal);
        var transBrush = Brushes.Transparent;
        var dimBrush   = new SolidColorBrush(Color.FromArgb(255, 58, 106, 138));

        var redBrush = new SolidColorBrush(Color.FromRgb(204, 34, 0));

        for (int i = 0; i < tabs.Length; i++)
        {
            var (border, text) = tabs[i];
            bool isUpdatesTab = i == 7 && _updateAlertActive && i != index;
            border.BorderBrush = i == index ? tealBrush : transBrush;
            text.Foreground    = i == index ? tealBrush : isUpdatesTab ? redBrush : dimBrush;
        }
    }

    // ── Load Manip tab: chapter picker (hub) + per-chapter sub-pages ─────────

    private void LoadManipChapter1NavBtn_Click(object sender, RoutedEventArgs e) => SelectLoadManipSubPage(1);
    private void LoadManipChapter4NavBtn_Click(object sender, RoutedEventArgs e) => SelectLoadManipSubPage(2);
    private void LoadManipChapter5NavBtn_Click(object sender, RoutedEventArgs e) => SelectLoadManipSubPage(3);
    private void LoadManipChapter1BackBtn_Click(object sender, RoutedEventArgs e) => SelectLoadManipSubPage(0);
    private void LoadManipChapter4BackBtn_Click(object sender, RoutedEventArgs e) => SelectLoadManipSubPage(0);
    private void LoadManipChapter5BackBtn_Click(object sender, RoutedEventArgs e) => SelectLoadManipSubPage(0);

    /// <summary>0=chapter picker hub, 1=Chapter 1 (Load Manip + FullBright), 2=Chapter 4
    /// (Load Manip remap keys), 3=Chapter 5 (Load Manip keys).</summary>
    private void SelectLoadManipSubPage(int page)
    {
        LoadManipHubPanel.Visibility      = page == 0 ? Visibility.Visible : Visibility.Collapsed;
        LoadManipChapter1Panel.Visibility = page == 1 ? Visibility.Visible : Visibility.Collapsed;
        LoadManipChapter4Panel.Visibility = page == 2 ? Visibility.Visible : Visibility.Collapsed;
        LoadManipChapter5Panel.Visibility = page == 3 ? Visibility.Visible : Visibility.Collapsed;
    }

    // ── Cores hotkeys ─────────────────────────────────────────────────────────

    private bool             _capturingCoresHotkey;
    private int              _coresCaptureTarget; // 0=freeze, 1=slower, 2=normal
    private KeyEventHandler? _coresHotkeyCapture;

    // Low-level keyboard hook for cores (only fires when game is foreground)
    private nint _coresKeyboardHook;
    private LowLevelKeyboardProc? _coresKeyboardProc;
    private delegate nint LowLevelKeyboardProc(int nCode, nint wParam, nint lParam);

    // ── Chapter 4 Freeze/Slow/Normal loads hotkeys ───────────────────────────
    // Same pure input remap as Chapter 1, with a third slot (Slow Loads).

    private bool                     _capturingChapter4Hotkey;
    private int                      _chapter4CaptureTarget; // 0=freeze, 1=slow, 2=normal
    private KeyEventHandler?         _chapter4HotkeyCapture;
    private MouseButtonEventHandler? _chapter4MouseHotkeyCapture;

    private const uint Chapter4FreezeTargetVk = 0x49; // I — fixed in-game hotkey, not remappable
    private const uint Chapter4SlowTargetVk   = 0x4F; // O — fixed in-game hotkey, not remappable
    private const uint Chapter4NormalTargetVk = 0x50; // P — fixed in-game hotkey, not remappable

    // Low-level keyboard/mouse hooks for chapter 4 loads (only fire while a chapter 4 game is foreground)
    private nint _chapter4KeyboardHook;
    private nint _chapter4MouseHook;
    private LowLevelKeyboardProc? _chapter4KeyboardProc;
    private LowLevelKeyboardProc? _chapter4MouseProc;
    private readonly HashSet<uint> _chapter4HeldTriggers = [];

    private const int  WM_XBUTTONDOWN  = 0x020B;
    private const int  WM_MBUTTONDOWN  = 0x0207;
    private const uint LLKHF_INJECTED  = 0x10;
    private const uint INPUT_KEYBOARD  = 1;
    private const uint KEYEVENTF_KEYUP = 0x0002;

    [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
    private struct MSLLHOOKSTRUCT
    {
        public int  ptX;
        public int  ptY;
        public uint mouseData;
        public uint flags;
        public uint time;
        public nint dwExtraInfo;
    }

    [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
    private struct KBDLLHOOKSTRUCT
    {
        public uint vkCode;
        public uint scanCode;
        public uint flags;
        public uint time;
        public nint dwExtraInfo;
    }

    [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
    private struct RemapInput
    {
        public uint type;
        public RemapInputUnion u;
    }

    [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Explicit)]
    private struct RemapInputUnion
    {
        [System.Runtime.InteropServices.FieldOffset(0)] public RemapKeybdInput ki;
        // Unused, but must stay: the real Win32 INPUT union is sized by its largest member
        // (MOUSEINPUT, 32 bytes on x64) — dropping it shrinks Marshal.SizeOf<RemapInput>() below
        // what SendInput expects, so every call fails with ERROR_INVALID_PARAMETER.
        [System.Runtime.InteropServices.FieldOffset(0)] public RemapMouseInput mi;
    }

    [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
    private struct RemapKeybdInput
    {
        public ushort wVk;
        public ushort wScan;
        public uint   dwFlags;
        public uint   time;
        public nint   dwExtraInfo;
    }

    [System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
    private struct RemapMouseInput
    {
        public int  dx;
        public int  dy;
        public uint mouseData;
        public uint dwFlags;
        public uint time;
        public nint dwExtraInfo;
    }

    [System.Runtime.InteropServices.DllImport("user32.dll", SetLastError = true)]
    private static extern uint SendInput(uint nInputs, RemapInput[] pInputs, int cbSize);

    [System.Runtime.InteropServices.DllImport("user32.dll")]
    private static extern uint MapVirtualKey(uint uCode, uint uMapType);

    private const uint MAPVK_VK_TO_VSC = 0;

    private static void SendRemapKeyEvent(uint vk, ushort scan, bool keyUp)
    {
        var inputs = new RemapInput[1];
        inputs[0].type       = INPUT_KEYBOARD;
        inputs[0].u.ki.wVk   = (ushort)vk;
        inputs[0].u.ki.wScan = scan;
        inputs[0].u.ki.dwFlags = keyUp ? KEYEVENTF_KEYUP : 0;
        SendInput(1, inputs, System.Runtime.InteropServices.Marshal.SizeOf<RemapInput>());
    }

    private static void SendRemapKey(uint vk)
    {
        // Include the hardware scan code, not just the virtual-key — some games/mods that
        // poll raw key state ignore synthetic input that lacks one.
        var scan = (ushort)MapVirtualKey(vk, MAPVK_VK_TO_VSC);

        SendRemapKeyEvent(vk, scan, keyUp: false);

        // Hold the key down for a human-like duration before releasing. A same-tick down+up
        // (as this used to send) can fall entirely between two polls of a mod that checks key
        // state once per game frame instead of hooking WM_KEYDOWN, so it never gets observed.
        System.Threading.Tasks.Task.Delay(60).ContinueWith(_ => SendRemapKeyEvent(vk, scan, keyUp: true));
    }

    [System.Runtime.InteropServices.DllImport("user32.dll", SetLastError = true)]
    private static extern nint SetWindowsHookEx(int idHook, LowLevelKeyboardProc lpfn, nint hMod, uint dwThreadId);

    [System.Runtime.InteropServices.DllImport("user32.dll", SetLastError = true)]
    private static extern bool UnhookWindowsHookEx(nint hhk);

    [System.Runtime.InteropServices.DllImport("user32.dll")]
    private static extern nint CallNextHookEx(nint hhk, int nCode, nint wParam, nint lParam);

    [System.Runtime.InteropServices.DllImport("kernel32.dll", CharSet = System.Runtime.InteropServices.CharSet.Auto)]
    private static extern nint GetModuleHandle(string? lpModuleName);

    [System.Runtime.InteropServices.DllImport("user32.dll")]
    private static extern nint GetForegroundWindow();

    [System.Runtime.InteropServices.DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(nint hWnd, out uint processId);

    private void InstallCoresHook()
    {
        UninstallCoresHook();
        using var curProcess = System.Diagnostics.Process.GetCurrentProcess();
        using var curModule  = curProcess.MainModule!;
        var hMod = GetModuleHandle(curModule.ModuleName);
        _coresKeyboardProc = CoresKeyboardHookProc;
        _coresKeyboardHook = SetWindowsHookEx(13, _coresKeyboardProc, hMod, 0);
    }

    private void UninstallCoresHook()
    {
        if (_coresKeyboardHook != 0)
        {
            UnhookWindowsHookEx(_coresKeyboardHook);
            _coresKeyboardHook = 0;
        }
        _coresKeyboardProc = null;
    }

    private bool IsGameForeground()
    {
        // Uses the PID cached by GameWatcherTick (refreshed every 2s) instead of enumerating
        // processes here — this is called on every keystroke/click while a remap hook is active,
        // and Process.GetProcessesByName on that hot path was causing severe input lag.
        var fg = GetForegroundWindow();
        if (fg == 0) return false;
        GetWindowThreadProcessId(fg, out var pid);
        for (int i = 0; i < _chapters.Count; i++)
            if (_gameWasRunning[i] && _runningChapterPid[i] == (int)pid) return true;
        return false;
    }

    private nint CoresKeyboardHookProc(int nCode, nint wParam, nint lParam)
    {
        if (nCode >= 0 && (int)wParam == 0x0100) // WM_KEYDOWN
        {
            var vkCode = (uint)System.Runtime.InteropServices.Marshal.ReadInt32(lParam);
            if ((vkCode == _coresFreezeVk || vkCode == _coresSlowerVk || vkCode == _coresNormalVk) && IsGameForeground())
            {
                var mode = vkCode == _coresFreezeVk ? CoresMode.Freeze
                         : vkCode == _coresSlowerVk ? CoresMode.Slower
                         : CoresMode.Normal;
                Dispatcher.BeginInvoke(() => HandleCoresHotkey(mode));
            }
        }
        return CallNextHookEx(_coresKeyboardHook, nCode, wParam, lParam);
    }

    private void InstallChapter4RemapHook()
    {
        UninstallChapter4RemapHook();
        using var curProcess = System.Diagnostics.Process.GetCurrentProcess();
        using var curModule  = curProcess.MainModule!;
        var hMod = GetModuleHandle(curModule.ModuleName);
        _chapter4KeyboardProc = Chapter4KeyboardHookProc;
        _chapter4KeyboardHook = SetWindowsHookEx(13, _chapter4KeyboardProc, hMod, 0);

        // Only pay for a global mouse hook (which adds latency to every mouse move system-wide)
        // when a binding actually needs it.
        if (_chapter4Freeze.Type == HotkeyInputType.Mouse || _chapter4Slow.Type == HotkeyInputType.Mouse
            || _chapter4Normal.Type == HotkeyInputType.Mouse)
        {
            _chapter4MouseProc = Chapter4MouseHookProc;
            _chapter4MouseHook = SetWindowsHookEx(14, _chapter4MouseProc, hMod, 0);
        }
    }

    private void UninstallChapter4RemapHook()
    {
        if (_chapter4KeyboardHook != 0)
        {
            UnhookWindowsHookEx(_chapter4KeyboardHook);
            _chapter4KeyboardHook = 0;
        }
        if (_chapter4MouseHook != 0)
        {
            UnhookWindowsHookEx(_chapter4MouseHook);
            _chapter4MouseHook = 0;
        }
        _chapter4KeyboardProc = null;
        _chapter4MouseProc    = null;
        _chapter4HeldTriggers.Clear();
    }

    private bool Chapter4RemapActive() =>
        _chapter4RemapEnabled && GetRunningChapter()?.Number == 4 && IsGameForeground();

    private nint Chapter4KeyboardHookProc(int nCode, nint wParam, nint lParam)
    {
        if (nCode >= 0)
        {
            var data     = System.Runtime.InteropServices.Marshal.PtrToStructure<KBDLLHOOKSTRUCT>(lParam);
            var msg      = (int)wParam;
            var isDown   = msg is 0x0100 or 0x0104; // WM_KEYDOWN, WM_SYSKEYDOWN
            var isUp     = msg is 0x0101 or 0x0105; // WM_KEYUP, WM_SYSKEYUP
            var injected = (data.flags & LLKHF_INJECTED) != 0;

            var isTargetKey = data.vkCode == Chapter4FreezeTargetVk
                || data.vkCode == Chapter4SlowTargetVk
                || data.vkCode == Chapter4NormalTargetVk;

            if (!injected && Chapter4RemapActive())
            {
                HandleChapter4Trigger(data.vkCode, isDown, isUp);

                // Swallow the real in-game Freeze/Slow/Normal keys so they aren't double-processed —
                // our own synthetic presses (reported as injected) still get through above.
                if (isTargetKey) return 1;
            }
        }
        return CallNextHookEx(_chapter4KeyboardHook, nCode, wParam, lParam);
    }

    private void HandleChapter4Trigger(uint vkCode, bool isDown, bool isUp)
    {
        bool isFreeze = _chapter4Freeze.Type == HotkeyInputType.Keyboard && _chapter4Freeze.KeyVk == vkCode;
        bool isSlow   = _chapter4Slow.Type   == HotkeyInputType.Keyboard && _chapter4Slow.KeyVk   == vkCode;
        bool isNormal = _chapter4Normal.Type == HotkeyInputType.Keyboard && _chapter4Normal.KeyVk == vkCode;
        if (!isFreeze && !isSlow && !isNormal) return;

        if (isDown && _chapter4HeldTriggers.Add(vkCode))
        {
            // Defer off the hook's call stack: injecting a keyboard event synchronously from
            // within the keyboard hook that's currently handling a real event of the same key
            // (the default I→I / O→O / P→P binding) can get silently dropped by Windows.
            var targetVk = isFreeze ? Chapter4FreezeTargetVk : isSlow ? Chapter4SlowTargetVk : Chapter4NormalTargetVk;
            Dispatcher.BeginInvoke(() => SendRemapKey(targetVk));
        }
        else if (isUp)
            _chapter4HeldTriggers.Remove(vkCode);
    }

    private nint Chapter4MouseHookProc(int nCode, nint wParam, nint lParam)
    {
        if (nCode >= 0 && ((int)wParam == WM_XBUTTONDOWN || (int)wParam == WM_MBUTTONDOWN) && Chapter4RemapActive())
        {
            var data    = System.Runtime.InteropServices.Marshal.PtrToStructure<MSLLHOOKSTRUCT>(lParam);
            var xButton = (int)(data.mouseData >> 16);
            var pressed = (int)wParam switch
            {
                WM_MBUTTONDOWN => HotkeyMouseButtons.Middle,
                WM_XBUTTONDOWN => xButton == 1 ? HotkeyMouseButtons.XButton1
                                : xButton == 2 ? HotkeyMouseButtons.XButton2 : 0,
                _ => 0,
            };

            if (pressed != 0)
            {
                bool isFreeze = _chapter4Freeze.Type == HotkeyInputType.Mouse && _chapter4Freeze.MouseButton == pressed;
                bool isSlow   = _chapter4Slow.Type   == HotkeyInputType.Mouse && _chapter4Slow.MouseButton   == pressed;
                bool isNormal = _chapter4Normal.Type == HotkeyInputType.Mouse && _chapter4Normal.MouseButton == pressed;
                if (isFreeze || isSlow || isNormal)
                {
                    var targetVk = isFreeze ? Chapter4FreezeTargetVk : isSlow ? Chapter4SlowTargetVk : Chapter4NormalTargetVk;
                    Dispatcher.BeginInvoke(() => SendRemapKey(targetVk));
                }
            }
        }
        return CallNextHookEx(_chapter4MouseHook, nCode, wParam, lParam);
    }

    private void LoadCoresHotkeys()
    {
        try
        {
            if (!File.Exists(CoresHotkeyFile)) return;
            var lines = File.ReadAllLines(CoresHotkeyFile);
            if (lines.Length >= 3)
            {
                if (uint.TryParse(lines[0].Trim().Split(',').Last(), out var nVk)) _coresNormalVk = nVk;
                if (uint.TryParse(lines[1].Trim().Split(',').Last(), out var sVk)) _coresSlowerVk = sVk;
                if (uint.TryParse(lines[2].Trim().Split(',').Last(), out var fVk)) _coresFreezeVk = fVk;
            }
        }
        catch { }
    }

    private void SaveCoresHotkeys()
    {
        try
        {
            var dir = IOPath.GetDirectoryName(CoresHotkeyFile)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(CoresHotkeyFile,
                $"0,{_coresNormalVk}\n" +
                $"0,{_coresSlowerVk}\n" +
                $"0,{_coresFreezeVk}");
        }
        catch { }
    }

    private void RefreshCoresButtons()
    {
        CoresFreezeText.Text = FormatKeyName(_coresFreezeVk);
        CoresSlowerText.Text = FormatKeyName(_coresSlowerVk);
        CoresNormalText.Text = FormatKeyName(_coresNormalVk);

        var normalBrush = new SolidColorBrush(Color.FromArgb(255, 26, 58, 85));
        var normalFg    = new SolidColorBrush(Color.FromArgb(255, 138, 170, 187));
        CoresFreezeBtn.BorderBrush = normalBrush;
        CoresFreezeText.Foreground = normalFg;
        CoresSlowerBtn.BorderBrush = normalBrush;
        CoresSlowerText.Foreground = normalFg;
        CoresNormalBtn.BorderBrush = normalBrush;
        CoresNormalText.Foreground = normalFg;
    }

    private void LoadCoresEnabled()
    {
        try
        {
            if (File.Exists(CoresEnabledFile))
                _coresEnabled = File.ReadAllText(CoresEnabledFile).Trim() == "1";
        }
        catch { }
    }

    private void SaveCoresEnabled()
    {
        try
        {
            var dir = IOPath.GetDirectoryName(CoresEnabledFile)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(CoresEnabledFile, _coresEnabled ? "1" : "0");
        }
        catch { }
    }

    private void RefreshCoresToggle()
    {
        SetToggle(CoresEnableText, _coresEnabled);
    }

    private void LoadCoresPriority()
    {
        try
        {
            if (File.Exists(CoresPriorityFile))
                _coresPriorityHigh = File.ReadAllText(CoresPriorityFile).Trim() != "low";
        }
        catch { }
    }

    private void SaveCoresPriority()
    {
        try
        {
            var dir = IOPath.GetDirectoryName(CoresPriorityFile)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(CoresPriorityFile, _coresPriorityHigh ? "high" : "low");
        }
        catch { }
    }

    private void RefreshCoresPriorityUI()
    {
        CoresPriorityCombo.SelectionChanged -= CoresPriorityCombo_SelectionChanged;
        CoresPriorityCombo.SelectedIndex = _coresPriorityHigh ? 0 : 1;
        CoresPriorityCombo.SelectionChanged += CoresPriorityCombo_SelectionChanged;
    }

    private void CoresPriorityCombo_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        _coresPriorityHigh = CoresPriorityCombo.SelectedIndex == 0;
        SaveCoresPriority();
    }

    private void CoresEnableBtn_Click(object sender, RoutedEventArgs e)
    {
        _coresEnabled = !_coresEnabled;
        SaveCoresEnabled();
        RefreshCoresToggle();

        if (_coresEnabled)
            InstallCoresHook();
        else
            UninstallCoresHook();
    }

    // ── Chapter 1 Freeze/Normal loads (Controls tab) ─────────────────────────
    // Reads/writes LoadManip_Config.ini directly (KeyFreeze/KeyNormal) — the Load Manip mod
    // itself reads this file, so no OS-level hook/key-injection is needed for Chapter 1 anymore.

    /// <summary>Resolves the live, already-installed LoadManip_Config.ini for the currently
    /// active install of the given chapter, or null if Load Manip isn't installed there.</summary>
    private string? GetActiveLoadManipConfigPath(int chapterNumber = 1)
    {
        var chapter = _chapters.FirstOrDefault(c => c.Number == chapterNumber);
        var exePath = chapter != null ? GetActiveExePath(chapter) : null;
        if (string.IsNullOrEmpty(exePath)) return null;

        var win64 = FindWin64Dir(IOPath.GetDirectoryName(exePath)!);
        if (win64 is null) return null;

        var projectRoot = LoadManipFilesService.GetProjectRoot(win64);
        if (projectRoot is null) return null;

        var path = IOPath.Combine(projectRoot, LoadManipFilesService.ConfigFileName);
        return File.Exists(path) ? path : null;
    }

    private static (string? freeze, string? normal) ParseLoadManipConfig(string path)
    {
        string? freeze = null, normal = null;
        foreach (var line in File.ReadAllLines(path))
        {
            var t = line.Trim();
            if (t.Length == 0 || t.StartsWith("#") || t.StartsWith("[") || !t.Contains('='))
                continue;

            var parts = t.Split('=', 2);
            var key   = parts[0].Trim();
            var value = parts[1].Trim();
            if (key.Equals("KeyFreeze", StringComparison.OrdinalIgnoreCase)) freeze = value;
            else if (key.Equals("KeyNormal", StringComparison.OrdinalIgnoreCase)) normal = value;
        }
        return (freeze, normal);
    }

    /// <summary>Maps a value stored in LoadManip_Config.ini to what's shown in the launcher UI —
    /// e.g. the raw UE4SS key name "XBUTTON_ONE" displays as "Mouse 4". Keyboard keys pass through
    /// unchanged since their raw name (e.g. "I", "F5") already reads fine.</summary>
    private static string LoadManipKeyDisplayName(string configValue) => configValue switch
    {
        "XBUTTON_ONE" => "Mouse 4",
        "XBUTTON_TWO" => "Mouse 5",
        "MIDDLE_MOUSE_BUTTON" => "Middle Mouse",
        _ => configValue,
    };

    /// <summary>Shows editable Freeze/Normal key rows reading from the live installed
    /// config.ini when Load Manip is installed for the active Chapter 1 exe, or
    /// "(load manip not installed)" otherwise.</summary>
    private void RefreshChapter1UI()
    {
        if (_capturingChapter1Hotkey) CancelChapter1Capture();

        var path = GetActiveLoadManipConfigPath();
        var (freeze, normal) = path != null ? ParseLoadManipConfig(path) : (null, null);
        bool editable = freeze != null && normal != null;

        Chapter1FreezeRow.Visibility               = editable ? Visibility.Visible : Visibility.Collapsed;
        Chapter1NormalRow.Visibility                = editable ? Visibility.Visible : Visibility.Collapsed;
        Chapter1LoadManipNotInstalledText.Visibility = editable ? Visibility.Collapsed : Visibility.Visible;
        Chapter1LoadManipNotInstalledText.Text       = Loc.Get("chapter1_loadmanip_not_installed");

        var normalBrush = new SolidColorBrush(Color.FromArgb(255, 26, 58, 85));
        var normalFg    = new SolidColorBrush(Color.FromArgb(255, 138, 170, 187));
        Chapter1FreezeBtn.BorderBrush  = normalBrush;
        Chapter1FreezeText.Foreground  = normalFg;
        Chapter1NormalBtn.BorderBrush  = normalBrush;
        Chapter1NormalText.Foreground  = normalFg;

        if (editable)
        {
            Chapter1FreezeText.Text = LoadManipKeyDisplayName(freeze!);
            Chapter1NormalText.Text = LoadManipKeyDisplayName(normal!);
        }
    }

    private void Chapter1FreezeBtn_Click(object sender, RoutedEventArgs e) =>
        StartChapter1Capture("KeyFreeze", Chapter1FreezeBtn, Chapter1FreezeText);
    private void Chapter1NormalBtn_Click(object sender, RoutedEventArgs e) =>
        StartChapter1Capture("KeyNormal", Chapter1NormalBtn, Chapter1NormalText);

    private void StartChapter1Capture(string configKey, Button btn, TextBlock text)
    {
        if (_capturingChapter1Hotkey)
        {
            var wasThis = _chapter1CaptureTarget == configKey;
            CancelChapter1Capture();
            RefreshChapter1UI();
            if (wasThis) return;
        }

        _capturingChapter1Hotkey = true;
        _chapter1CaptureTarget   = configKey;
        text.Text       = Loc.Get("f11_remap_press_input");
        text.Foreground = new SolidColorBrush(Teal);
        btn.BorderBrush = new SolidColorBrush(Teal);

        _chapter1HotkeyCapture = CaptureChapter1KeyDown;
        AddHandler(UIElement.PreviewKeyDownEvent, _chapter1HotkeyCapture, true);

        _chapter1MouseHotkeyCapture = CaptureChapter1MouseDown;
        AddHandler(UIElement.PreviewMouseDownEvent, _chapter1MouseHotkeyCapture, true);
    }

    private void CancelChapter1Capture()
    {
        _capturingChapter1Hotkey = false;
        if (_chapter1HotkeyCapture != null)
        {
            RemoveHandler(UIElement.PreviewKeyDownEvent, _chapter1HotkeyCapture);
            _chapter1HotkeyCapture = null;
        }
        if (_chapter1MouseHotkeyCapture != null)
        {
            RemoveHandler(UIElement.PreviewMouseDownEvent, _chapter1MouseHotkeyCapture);
            _chapter1MouseHotkeyCapture = null;
        }
    }

    private void ApplyChapter1Key(string configKey, string value)
    {
        var path = GetActiveLoadManipConfigPath();
        if (path != null)
            LoadManipFilesService.UpdateConfigKey(path, configKey, value);
        RefreshChapter1UI();
    }

    private void CaptureChapter1KeyDown(object sender, KeyEventArgs e)
    {
        var key = e.Key == Key.System ? e.SystemKey : e.Key;

        if (key is Key.LeftCtrl or Key.RightCtrl
                or Key.LeftShift or Key.RightShift
                or Key.LeftAlt or Key.RightAlt
                or Key.LWin or Key.RWin
                or Key.None)
            return;

        var target = _chapter1CaptureTarget!;
        CancelChapter1Capture();

        if (key == Key.Escape)
        {
            RefreshChapter1UI();
            e.Handled = true;
            return;
        }

        var name = WpfKeyToUnrealKeyName(key);
        if (name != null) ApplyChapter1Key(target, name);
        else RefreshChapter1UI();
        e.Handled = true;
    }

    private void CaptureChapter1MouseDown(object sender, MouseButtonEventArgs e)
    {
        var name = e.ChangedButton switch
        {
            MouseButton.Middle   => "MIDDLE_MOUSE_BUTTON",
            MouseButton.XButton1 => "XBUTTON_ONE",
            MouseButton.XButton2 => "XBUTTON_TWO",
            _ => null,
        };
        if (name is null) return;

        var target = _chapter1CaptureTarget!;
        CancelChapter1Capture();
        ApplyChapter1Key(target, name);
        e.Handled = true;
    }

    /// <summary>Shows editable Freeze/Normal key rows reading from the live installed
    /// config.ini when Load Manip is installed for the active Chapter 5 exe, or
    /// "(load manip not installed)" otherwise. Mirrors <see cref="RefreshChapter1UI"/>.</summary>
    private void RefreshChapter5LoadManipUI()
    {
        if (_capturingChapter5Hotkey) CancelChapter5Capture();

        var path = GetActiveLoadManipConfigPath(5);
        var (freeze, normal) = path != null ? ParseLoadManipConfig(path) : (null, null);
        bool editable = freeze != null && normal != null;

        Chapter5FreezeRow.Visibility               = editable ? Visibility.Visible : Visibility.Collapsed;
        Chapter5NormalRow.Visibility                = editable ? Visibility.Visible : Visibility.Collapsed;
        Chapter5LoadManipNotInstalledText.Visibility = editable ? Visibility.Collapsed : Visibility.Visible;
        Chapter5LoadManipNotInstalledText.Text       = Loc.Get("chapter5_loadmanip_not_installed");

        var normalBrush = new SolidColorBrush(Color.FromArgb(255, 26, 58, 85));
        var normalFg    = new SolidColorBrush(Color.FromArgb(255, 138, 170, 187));
        Chapter5FreezeBtn.BorderBrush  = normalBrush;
        Chapter5FreezeText.Foreground  = normalFg;
        Chapter5NormalBtn.BorderBrush  = normalBrush;
        Chapter5NormalText.Foreground  = normalFg;

        if (editable)
        {
            Chapter5FreezeText.Text = LoadManipKeyDisplayName(freeze!);
            Chapter5NormalText.Text = LoadManipKeyDisplayName(normal!);
        }
    }

    private void Chapter5FreezeBtn_Click(object sender, RoutedEventArgs e) =>
        StartChapter5Capture("KeyFreeze", Chapter5FreezeBtn, Chapter5FreezeText);
    private void Chapter5NormalBtn_Click(object sender, RoutedEventArgs e) =>
        StartChapter5Capture("KeyNormal", Chapter5NormalBtn, Chapter5NormalText);

    private void StartChapter5Capture(string configKey, Button btn, TextBlock text)
    {
        if (_capturingChapter5Hotkey)
        {
            var wasThis = _chapter5CaptureTarget == configKey;
            CancelChapter5Capture();
            RefreshChapter5LoadManipUI();
            if (wasThis) return;
        }

        _capturingChapter5Hotkey = true;
        _chapter5CaptureTarget   = configKey;
        text.Text       = Loc.Get("f11_remap_press_input");
        text.Foreground = new SolidColorBrush(Teal);
        btn.BorderBrush = new SolidColorBrush(Teal);

        _chapter5HotkeyCapture = CaptureChapter5KeyDown;
        AddHandler(UIElement.PreviewKeyDownEvent, _chapter5HotkeyCapture, true);

        _chapter5MouseHotkeyCapture = CaptureChapter5MouseDown;
        AddHandler(UIElement.PreviewMouseDownEvent, _chapter5MouseHotkeyCapture, true);
    }

    private void CancelChapter5Capture()
    {
        _capturingChapter5Hotkey = false;
        if (_chapter5HotkeyCapture != null)
        {
            RemoveHandler(UIElement.PreviewKeyDownEvent, _chapter5HotkeyCapture);
            _chapter5HotkeyCapture = null;
        }
        if (_chapter5MouseHotkeyCapture != null)
        {
            RemoveHandler(UIElement.PreviewMouseDownEvent, _chapter5MouseHotkeyCapture);
            _chapter5MouseHotkeyCapture = null;
        }
    }

    private void ApplyChapter5Key(string configKey, string value)
    {
        var path = GetActiveLoadManipConfigPath(5);
        if (path != null)
            LoadManipFilesService.UpdateConfigKey(path, configKey, value);
        RefreshChapter5LoadManipUI();
    }

    private void CaptureChapter5KeyDown(object sender, KeyEventArgs e)
    {
        var key = e.Key == Key.System ? e.SystemKey : e.Key;

        if (key is Key.LeftCtrl or Key.RightCtrl
                or Key.LeftShift or Key.RightShift
                or Key.LeftAlt or Key.RightAlt
                or Key.LWin or Key.RWin
                or Key.None)
            return;

        var target = _chapter5CaptureTarget!;
        CancelChapter5Capture();

        if (key == Key.Escape)
        {
            RefreshChapter5LoadManipUI();
            e.Handled = true;
            return;
        }

        var name = WpfKeyToUnrealKeyName(key);
        if (name != null) ApplyChapter5Key(target, name);
        else RefreshChapter5LoadManipUI();
        e.Handled = true;
    }

    private void CaptureChapter5MouseDown(object sender, MouseButtonEventArgs e)
    {
        var name = e.ChangedButton switch
        {
            MouseButton.Middle   => "MIDDLE_MOUSE_BUTTON",
            MouseButton.XButton1 => "XBUTTON_ONE",
            MouseButton.XButton2 => "XBUTTON_TWO",
            _ => null,
        };
        if (name is null) return;

        var target = _chapter5CaptureTarget!;
        CancelChapter5Capture();
        ApplyChapter5Key(target, name);
        e.Handled = true;
    }

    private static bool TryParseHotkeyBinding(string line, out HotkeyBinding binding)
    {
        binding = default;
        var p = line.Trim();
        if (p.Length < 2) return false;

        if (p[0] == 'K' && uint.TryParse(p[1..], out var vk))
        {
            binding = new HotkeyBinding(HotkeyInputType.Keyboard, vk, 0);
            return true;
        }
        if (p[0] == 'M' && int.TryParse(p[1..], out var mb))
        {
            binding = new HotkeyBinding(HotkeyInputType.Mouse, 0, mb);
            return true;
        }
        return false;
    }

    private static string HotkeyBindingToToken(HotkeyBinding b) => b.Type switch
    {
        HotkeyInputType.Mouse => $"M{b.MouseButton}",
        _                     => $"K{b.KeyVk}",
    };

    private string HotkeyBindingToString(HotkeyBinding binding) => binding.Type switch
    {
        HotkeyInputType.Keyboard => KeyToString(KeyInterop.KeyFromVirtualKey((int)binding.KeyVk)),
        HotkeyInputType.Mouse => binding.MouseButton switch
        {
            HotkeyMouseButtons.Middle   => Loc.Get("f11_remap_mouse3"),
            HotkeyMouseButtons.XButton1 => Loc.Get("f11_remap_mouse4"),
            HotkeyMouseButtons.XButton2 => Loc.Get("f11_remap_mouse5"),
            _ => Loc.Get("f11_remap_none"),
        },
        _ => Loc.Get("f11_remap_none"),
    };

    private void LoadChapter4RemapHotkeys()
    {
        try
        {
            if (!File.Exists(Chapter4RemapHotkeyFile)) return;
            var lines = File.ReadAllLines(Chapter4RemapHotkeyFile);
            if (lines.Length >= 3)
            {
                if (TryParseHotkeyBinding(lines[0], out var freeze)) _chapter4Freeze = freeze;
                if (TryParseHotkeyBinding(lines[1], out var slow))   _chapter4Slow   = slow;
                if (TryParseHotkeyBinding(lines[2], out var normal)) _chapter4Normal = normal;
            }
        }
        catch { }
    }

    private void SaveChapter4RemapHotkeys()
    {
        try
        {
            var dir = IOPath.GetDirectoryName(Chapter4RemapHotkeyFile)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(Chapter4RemapHotkeyFile,
                $"{HotkeyBindingToToken(_chapter4Freeze)}\n{HotkeyBindingToToken(_chapter4Slow)}\n{HotkeyBindingToToken(_chapter4Normal)}");
        }
        catch { }
    }

    private void LoadChapter4RemapEnabled()
    {
        try
        {
            if (File.Exists(Chapter4RemapEnabledFile))
                _chapter4RemapEnabled = File.ReadAllText(Chapter4RemapEnabledFile).Trim() == "1";
        }
        catch { }
    }

    private void SaveChapter4RemapEnabled()
    {
        try
        {
            var dir = IOPath.GetDirectoryName(Chapter4RemapEnabledFile)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(Chapter4RemapEnabledFile, _chapter4RemapEnabled ? "1" : "0");
        }
        catch { }
    }

    private void RefreshChapter4Buttons()
    {
        Chapter4FreezeText.Text = HotkeyBindingToString(_chapter4Freeze);
        Chapter4SlowText.Text   = HotkeyBindingToString(_chapter4Slow);
        Chapter4NormalText.Text = HotkeyBindingToString(_chapter4Normal);

        var normalBrush = new SolidColorBrush(Color.FromArgb(255, 26, 58, 85));
        var normalFg    = new SolidColorBrush(Color.FromArgb(255, 138, 170, 187));
        Chapter4FreezeBtn.BorderBrush  = normalBrush;
        Chapter4FreezeText.Foreground  = normalFg;
        Chapter4SlowBtn.BorderBrush    = normalBrush;
        Chapter4SlowText.Foreground    = normalFg;
        Chapter4NormalBtn.BorderBrush  = normalBrush;
        Chapter4NormalText.Foreground  = normalFg;
    }

    private void RefreshChapter4UI()
    {
        SetToggle(Chapter4EnableText, _chapter4RemapEnabled);
        RefreshChapter4Buttons();
    }

    private void Chapter4EnableBtn_Click(object sender, RoutedEventArgs e)
    {
        _chapter4RemapEnabled = !_chapter4RemapEnabled;
        SaveChapter4RemapEnabled();
        RefreshChapter4UI();

        if (_chapter4RemapEnabled)
            InstallChapter4RemapHook();
        else
            UninstallChapter4RemapHook();
    }

    private void Chapter4FreezeBtn_Click(object sender, RoutedEventArgs e) =>
        StartChapter4Capture(0, Chapter4FreezeBtn, Chapter4FreezeText);
    private void Chapter4SlowBtn_Click(object sender, RoutedEventArgs e) =>
        StartChapter4Capture(1, Chapter4SlowBtn, Chapter4SlowText);
    private void Chapter4NormalBtn_Click(object sender, RoutedEventArgs e) =>
        StartChapter4Capture(2, Chapter4NormalBtn, Chapter4NormalText);

    private void StartChapter4Capture(int target, Button btn, TextBlock text)
    {
        if (_capturingChapter4Hotkey)
        {
            var wasThis = _chapter4CaptureTarget == target;
            CancelChapter4Capture();
            RefreshChapter4Buttons();
            if (wasThis) return;
        }

        _capturingChapter4Hotkey = true;
        _chapter4CaptureTarget   = target;
        text.Text       = Loc.Get("f11_remap_press_input");
        text.Foreground = new SolidColorBrush(Teal);
        btn.BorderBrush = new SolidColorBrush(Teal);

        _chapter4HotkeyCapture = CaptureChapter4KeyDown;
        AddHandler(UIElement.PreviewKeyDownEvent, _chapter4HotkeyCapture, true);

        _chapter4MouseHotkeyCapture = CaptureChapter4MouseDown;
        AddHandler(UIElement.PreviewMouseDownEvent, _chapter4MouseHotkeyCapture, true);
    }

    private void CancelChapter4Capture()
    {
        _capturingChapter4Hotkey = false;
        if (_chapter4HotkeyCapture != null)
        {
            RemoveHandler(UIElement.PreviewKeyDownEvent, _chapter4HotkeyCapture);
            _chapter4HotkeyCapture = null;
        }
        if (_chapter4MouseHotkeyCapture != null)
        {
            RemoveHandler(UIElement.PreviewMouseDownEvent, _chapter4MouseHotkeyCapture);
            _chapter4MouseHotkeyCapture = null;
        }
    }

    private void ApplyChapter4Binding(int target, HotkeyBinding binding)
    {
        switch (target)
        {
            case 0: _chapter4Freeze = binding; break;
            case 1: _chapter4Slow   = binding; break;
            case 2: _chapter4Normal = binding; break;
        }

        SaveChapter4RemapHotkeys();
        RefreshChapter4Buttons();
        if (_chapter4RemapEnabled) InstallChapter4RemapHook();
    }

    private void CaptureChapter4KeyDown(object sender, KeyEventArgs e)
    {
        var key = e.Key == Key.System ? e.SystemKey : e.Key;

        if (key is Key.LeftCtrl or Key.RightCtrl
                or Key.LeftShift or Key.RightShift
                or Key.LeftAlt or Key.RightAlt
                or Key.LWin or Key.RWin
                or Key.None)
            return;

        var target = _chapter4CaptureTarget;
        CancelChapter4Capture();

        if (key == Key.Escape)
        {
            RefreshChapter4Buttons();
            e.Handled = true;
            return;
        }

        var vk = (uint)KeyInterop.VirtualKeyFromKey(key);
        ApplyChapter4Binding(target, new HotkeyBinding(HotkeyInputType.Keyboard, vk, 0));
        e.Handled = true;
    }

    private void CaptureChapter4MouseDown(object sender, MouseButtonEventArgs e)
    {
        int? mouseButton = e.ChangedButton switch
        {
            MouseButton.Middle   => HotkeyMouseButtons.Middle,
            MouseButton.XButton1 => HotkeyMouseButtons.XButton1,
            MouseButton.XButton2 => HotkeyMouseButtons.XButton2,
            _ => null,
        };

        if (mouseButton is null) return;

        var target = _chapter4CaptureTarget;
        CancelChapter4Capture();

        ApplyChapter4Binding(target, new HotkeyBinding(HotkeyInputType.Mouse, 0, mouseButton.Value));
        e.Handled = true;
    }

    // ── Controller overlay ────────────────────────────────────────────────────

    private void LoadOverlaySettings()
    {
        try
        {
            if (File.Exists(OverlayEnabledFile))
                _overlayEnabled = File.ReadAllText(OverlayEnabledFile).Trim() == "1";
            if (File.Exists(OverlayControllerFile))
                _overlayController = File.ReadAllText(OverlayControllerFile).Trim();
            if (File.Exists(OverlayCornerFile))
                _overlayCorner = File.ReadAllText(OverlayCornerFile).Trim();
        }
        catch { }

        if (_overlayEnabled)
            Dispatcher.InvokeAsync(ApplyOverlayWindow, System.Windows.Threading.DispatcherPriority.Loaded);

        try
        {
            if (File.Exists(FpsOverlayEnabledFile))
                _fpsOverlayEnabled = File.ReadAllText(FpsOverlayEnabledFile).Trim() == "1";
            if (File.Exists(FpsOverlayCornerFile))
                _fpsOverlayCorner = File.ReadAllText(FpsOverlayCornerFile).Trim();
            if (File.Exists(FpsOverlaySizeFile))
                _fpsOverlaySize = File.ReadAllText(FpsOverlaySizeFile).Trim();
            if (File.Exists(FpsOverlayFontFile))
                _fpsOverlayFont = File.ReadAllText(FpsOverlayFontFile).Trim();
        }
        catch { }
    }

    private void SaveFpsOverlayEnabled()
    {
        try
        {
            var dir = IOPath.GetDirectoryName(FpsOverlayEnabledFile)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(FpsOverlayEnabledFile, _fpsOverlayEnabled ? "1" : "0");
        }
        catch { }
    }

    private void SaveFpsOverlayCorner()
    {
        try
        {
            var dir = IOPath.GetDirectoryName(FpsOverlayCornerFile)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(FpsOverlayCornerFile, _fpsOverlayCorner);
        }
        catch { }
    }

    private void SaveFpsOverlaySize()
    {
        try
        {
            var dir = IOPath.GetDirectoryName(FpsOverlaySizeFile)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(FpsOverlaySizeFile, _fpsOverlaySize);
        }
        catch { }
    }

    private void SaveFpsOverlayFont()
    {
        try
        {
            var dir = IOPath.GetDirectoryName(FpsOverlayFontFile)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(FpsOverlayFontFile, _fpsOverlayFont);
        }
        catch { }
    }

    private static double FpsOverlayFontSizeFor(string size) => size switch
    {
        "small" => 14,
        "large" => 26,
        _       => 18, // medium / default / unrecognized
    };

    private static FontFamily FpsOverlayFontFamilyFor(string font) =>
        font == "monospace" ? FpsOverlayWindow.MonospaceFont : FpsOverlayWindow.PoppyPlaytimeFont;

    // Re-applies the current corner/size/font settings to the FPS overlay if it's showing
    // right now (game already running) — same "reposition/restyle live" pattern ApplyOverlayWindow
    // uses for the controller overlay.
    private void ApplyFpsOverlayAppearance()
    {
        if (_fpsOverlay == null) return;
        _fpsOverlay.SetFont(FpsOverlayFontFamilyFor(_fpsOverlayFont));
        _fpsOverlay.SetSize(FpsOverlayFontSizeFor(_fpsOverlaySize));
        _fpsOverlay.PlaceInCorner(SystemParameters.WorkArea, _fpsOverlayCorner);
    }

    private void RefreshFpsOverlayTab()
    {
        SetToggle(FpsOverlayEnableText, _fpsOverlayEnabled);
        RefreshFpsOverlayCornerButtons();
        RefreshFpsOverlaySizeButtons();
        RefreshFpsOverlayFontButtons();
    }

    private void RefreshFpsOverlayCornerButtons()
    {
        var selectedBrush  = new SolidColorBrush(Teal);
        var selectedBg     = new SolidColorBrush(Color.FromArgb(255, 0, 40, 30));
        var selectedBorder = new SolidColorBrush(Teal);
        var dimBrush       = new SolidColorBrush(Color.FromArgb(255, 58, 106, 138));
        var dimBg          = new SolidColorBrush(Color.FromArgb(255, 6, 15, 24));
        var dimBorder      = new SolidColorBrush(Color.FromArgb(255, 13, 37, 53));

        void Style(Button btn, TextBlock text, bool selected)
        {
            btn.Background  = selected ? selectedBg : dimBg;
            btn.BorderBrush = selected ? selectedBorder : dimBorder;
            text.Foreground = selected ? selectedBrush : dimBrush;
        }

        Style(FpsOverlayCornerTopLeftBtn,     FpsOverlayCornerTopLeftBtnText,     _fpsOverlayCorner == "top-left");
        Style(FpsOverlayCornerTopRightBtn,    FpsOverlayCornerTopRightBtnText,    _fpsOverlayCorner == "top-right");
        Style(FpsOverlayCornerBottomLeftBtn,  FpsOverlayCornerBottomLeftBtnText,  _fpsOverlayCorner == "bottom-left");
        Style(FpsOverlayCornerBottomRightBtn, FpsOverlayCornerBottomRightBtnText, _fpsOverlayCorner == "bottom-right");
    }

    private void RefreshFpsOverlaySizeButtons()
    {
        var selectedBrush  = new SolidColorBrush(Teal);
        var selectedBg     = new SolidColorBrush(Color.FromArgb(255, 0, 40, 30));
        var selectedBorder = new SolidColorBrush(Teal);
        var dimBrush       = new SolidColorBrush(Color.FromArgb(255, 58, 106, 138));
        var dimBg          = new SolidColorBrush(Color.FromArgb(255, 6, 15, 24));
        var dimBorder      = new SolidColorBrush(Color.FromArgb(255, 13, 37, 53));

        void Style(Button btn, TextBlock text, bool selected)
        {
            btn.Background  = selected ? selectedBg : dimBg;
            btn.BorderBrush = selected ? selectedBorder : dimBorder;
            text.Foreground = selected ? selectedBrush : dimBrush;
        }

        Style(FpsOverlaySizeSmallBtn,  FpsOverlaySizeSmallBtnText,  _fpsOverlaySize == "small");
        Style(FpsOverlaySizeMediumBtn, FpsOverlaySizeMediumBtnText, _fpsOverlaySize == "medium");
        Style(FpsOverlaySizeLargeBtn,  FpsOverlaySizeLargeBtnText,  _fpsOverlaySize == "large");
    }

    private void RefreshFpsOverlayFontButtons()
    {
        var selectedBrush  = new SolidColorBrush(Teal);
        var selectedBg     = new SolidColorBrush(Color.FromArgb(255, 0, 40, 30));
        var selectedBorder = new SolidColorBrush(Teal);
        var dimBrush       = new SolidColorBrush(Color.FromArgb(255, 58, 106, 138));
        var dimBg          = new SolidColorBrush(Color.FromArgb(255, 6, 15, 24));
        var dimBorder      = new SolidColorBrush(Color.FromArgb(255, 13, 37, 53));

        void Style(Button btn, TextBlock text, bool selected)
        {
            btn.Background  = selected ? selectedBg : dimBg;
            btn.BorderBrush = selected ? selectedBorder : dimBorder;
            text.Foreground = selected ? selectedBrush : dimBrush;
        }

        Style(FpsOverlayFontPoppyBtn,     FpsOverlayFontPoppyBtnText,     _fpsOverlayFont == "poppy-playtime");
        Style(FpsOverlayFontMonospaceBtn, FpsOverlayFontMonospaceBtnText, _fpsOverlayFont == "monospace");
    }

    private void FpsOverlayEnableBtn_Click(object sender, RoutedEventArgs e)
    {
        _fpsOverlayEnabled = !_fpsOverlayEnabled;
        SaveFpsOverlayEnabled();
        SetToggle(FpsOverlayEnableText, _fpsOverlayEnabled);

        if (_fpsOverlayEnabled)
        {
            // A tracked game may already be running — start immediately instead of waiting for
            // the next launch transition GameWatcherTick would otherwise catch.
            for (int i = 0; i < _runningChapterPid.Length; i++)
            {
                if (_runningChapterPid[i] == 0) continue;
                StartFpsTracking(_runningChapterPid[i]);
                break;
            }
        }
        else
        {
            StopFpsTracking();
        }
    }

    private void SetFpsOverlayCorner(string corner)
    {
        _fpsOverlayCorner = corner;
        SaveFpsOverlayCorner();
        RefreshFpsOverlayCornerButtons();
        ApplyFpsOverlayAppearance();
    }

    private void FpsOverlayCornerTopLeftBtn_Click(object sender, RoutedEventArgs e)     => SetFpsOverlayCorner("top-left");
    private void FpsOverlayCornerTopRightBtn_Click(object sender, RoutedEventArgs e)    => SetFpsOverlayCorner("top-right");
    private void FpsOverlayCornerBottomLeftBtn_Click(object sender, RoutedEventArgs e)  => SetFpsOverlayCorner("bottom-left");
    private void FpsOverlayCornerBottomRightBtn_Click(object sender, RoutedEventArgs e) => SetFpsOverlayCorner("bottom-right");

    private void SetFpsOverlaySize(string size)
    {
        _fpsOverlaySize = size;
        SaveFpsOverlaySize();
        RefreshFpsOverlaySizeButtons();
        ApplyFpsOverlayAppearance();
    }

    private void FpsOverlaySizeSmallBtn_Click(object sender, RoutedEventArgs e)  => SetFpsOverlaySize("small");
    private void FpsOverlaySizeMediumBtn_Click(object sender, RoutedEventArgs e) => SetFpsOverlaySize("medium");
    private void FpsOverlaySizeLargeBtn_Click(object sender, RoutedEventArgs e)  => SetFpsOverlaySize("large");

    private void SetFpsOverlayFont(string font)
    {
        _fpsOverlayFont = font;
        SaveFpsOverlayFont();
        RefreshFpsOverlayFontButtons();
        ApplyFpsOverlayAppearance();
    }

    private void FpsOverlayFontPoppyBtn_Click(object sender, RoutedEventArgs e)     => SetFpsOverlayFont("poppy-playtime");
    private void FpsOverlayFontMonospaceBtn_Click(object sender, RoutedEventArgs e) => SetFpsOverlayFont("monospace");

    private void SaveOverlayEnabled()
    {
        try
        {
            var dir = IOPath.GetDirectoryName(OverlayEnabledFile)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(OverlayEnabledFile, _overlayEnabled ? "1" : "0");
        }
        catch { }
    }

    private void SaveOverlayController()
    {
        try
        {
            var dir = IOPath.GetDirectoryName(OverlayControllerFile)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(OverlayControllerFile, _overlayController);
        }
        catch { }
    }

    private void SaveOverlayCorner()
    {
        try
        {
            var dir = IOPath.GetDirectoryName(OverlayCornerFile)!;
            Directory.CreateDirectory(dir);
            File.WriteAllText(OverlayCornerFile, _overlayCorner);
        }
        catch { }
    }

    private void RefreshOverlaysTab()
    {
        SetToggle(OverlayEnableText, _overlayEnabled);
        RefreshOverlayControllerButtons();
        RefreshOverlayCornerButtons();
        RefreshFpsOverlayTab();
    }

    private void RefreshOverlayControllerButtons()
    {
        var selectedBrush  = new SolidColorBrush(Teal);
        var selectedBg     = new SolidColorBrush(Color.FromArgb(255, 0, 40, 30));
        var selectedBorder = new SolidColorBrush(Teal);
        var dimBrush       = new SolidColorBrush(Color.FromArgb(255, 58, 106, 138));
        var dimBg          = new SolidColorBrush(Color.FromArgb(255, 6, 15, 24));
        var dimBorder      = new SolidColorBrush(Color.FromArgb(255, 13, 37, 53));

        void Style(Button btn, TextBlock text, bool selected)
        {
            btn.Background  = selected ? selectedBg : dimBg;
            btn.BorderBrush = selected ? selectedBorder : dimBorder;
            text.Foreground = selected ? selectedBrush : dimBrush;
        }

        Style(OverlayDualSenseBtn, OverlayDualSenseBtnText, _overlayController == "dualsense");
        Style(OverlayXboxBtn,      OverlayXboxBtnText,      _overlayController == "xbox-controller");
        Style(OverlayKeyboardBtn,  OverlayKeyboardBtnText,  _overlayController == "keyboard");
    }

    private void RefreshOverlayCornerButtons()
    {
        var selectedBrush  = new SolidColorBrush(Teal);
        var selectedBg     = new SolidColorBrush(Color.FromArgb(255, 0, 40, 30));
        var selectedBorder = new SolidColorBrush(Teal);
        var dimBrush       = new SolidColorBrush(Color.FromArgb(255, 58, 106, 138));
        var dimBg          = new SolidColorBrush(Color.FromArgb(255, 6, 15, 24));
        var dimBorder      = new SolidColorBrush(Color.FromArgb(255, 13, 37, 53));

        void Style(Button btn, TextBlock text, bool selected)
        {
            btn.Background  = selected ? selectedBg : dimBg;
            btn.BorderBrush = selected ? selectedBorder : dimBorder;
            text.Foreground = selected ? selectedBrush : dimBrush;
        }

        Style(OverlayCornerTopLeftBtn,     OverlayCornerTopLeftBtnText,     _overlayCorner == "top-left");
        Style(OverlayCornerTopRightBtn,    OverlayCornerTopRightBtnText,    _overlayCorner == "top-right");
        Style(OverlayCornerBottomLeftBtn,  OverlayCornerBottomLeftBtnText,  _overlayCorner == "bottom-left");
        Style(OverlayCornerBottomRightBtn, OverlayCornerBottomRightBtnText, _overlayCorner == "bottom-right");
    }

    private void SetOverlayCorner(string corner)
    {
        _overlayCorner = corner;
        SaveOverlayCorner();
        RefreshOverlayCornerButtons();
        if (_overlayEnabled) ApplyOverlayWindow(); // reposition
    }

    private void OverlayCornerTopLeftBtn_Click(object sender, RoutedEventArgs e)     => SetOverlayCorner("top-left");
    private void OverlayCornerTopRightBtn_Click(object sender, RoutedEventArgs e)    => SetOverlayCorner("top-right");
    private void OverlayCornerBottomLeftBtn_Click(object sender, RoutedEventArgs e)  => SetOverlayCorner("bottom-left");
    private void OverlayCornerBottomRightBtn_Click(object sender, RoutedEventArgs e) => SetOverlayCorner("bottom-right");

    private void OverlayEnableBtn_Click(object sender, RoutedEventArgs e)
    {
        _overlayEnabled = !_overlayEnabled;
        SaveOverlayEnabled();
        SetToggle(OverlayEnableText, _overlayEnabled);
        ApplyOverlayWindow();
    }

    private void OverlayDualSenseBtn_Click(object sender, RoutedEventArgs e)
    {
        _overlayController = "dualsense";
        SaveOverlayController();
        RefreshOverlayControllerButtons();
        if (_overlayEnabled) ApplyOverlayWindow(); // reload with new skin
    }

    private void OverlayXboxBtn_Click(object sender, RoutedEventArgs e)
    {
        _overlayController = "xbox-controller";
        SaveOverlayController();
        RefreshOverlayControllerButtons();
        if (_overlayEnabled) ApplyOverlayWindow(); // reload with new skin
    }

    private void OverlayKeyboardBtn_Click(object sender, RoutedEventArgs e)
    {
        _overlayController = "keyboard";
        SaveOverlayController();
        RefreshOverlayControllerButtons();
        if (_overlayEnabled) ApplyOverlayWindow(); // reload with new skin
    }

    private void ApplyOverlayWindow()
    {
        // Close existing window
        if (_controllerOverlay != null)
        {
            _controllerOverlay.Closed -= OverlayWindow_Closed;
            _controllerOverlay.Close();
            _controllerOverlay = null;
        }

        if (!_overlayEnabled) return;

        _controllerOverlay = new ControllerOverlayWindow(_overlayController);
        _controllerOverlay.Closed += OverlayWindow_Closed;

        // Position in the configured screen corner
        var screen = SystemParameters.WorkArea;
        const int margin = 20;
        _controllerOverlay.Left = _overlayCorner is "top-right" or "bottom-right"
            ? screen.Right - _controllerOverlay.Width - margin
            : screen.Left  + margin;
        _controllerOverlay.Top = _overlayCorner is "bottom-left" or "bottom-right"
            ? screen.Bottom - _controllerOverlay.Height - margin
            : screen.Top    + margin;

        _controllerOverlay.Show();
    }

    private void OverlayWindow_Closed(object? sender, EventArgs e)
    {
        _controllerOverlay = null;
        _overlayEnabled    = false;
        SaveOverlayEnabled();
        SetToggle(OverlayEnableText, false);
    }

    private static string FormatKeyName(uint vk)
    {
        var key = KeyInterop.KeyFromVirtualKey((int)vk);
        return key.ToString().ToUpperInvariant();
    }

    private void CoresFreezeBtn_Click(object sender, RoutedEventArgs e) =>
        StartCoresCapture(0, CoresFreezeBtn, CoresFreezeText);
    private void CoresSlowerBtn_Click(object sender, RoutedEventArgs e) =>
        StartCoresCapture(1, CoresSlowerBtn, CoresSlowerText);
    private void CoresNormalBtn_Click(object sender, RoutedEventArgs e) =>
        StartCoresCapture(2, CoresNormalBtn, CoresNormalText);

    private void StartCoresCapture(int target, Button btn, TextBlock text)
    {
        if (_capturingCoresHotkey)
        {
            var wasThis = _coresCaptureTarget == target;
            CancelCoresCapture();
            RefreshCoresButtons();
            if (wasThis) return;
        }

        _capturingCoresHotkey = true;
        _coresCaptureTarget   = target;
        text.Text       = Loc.Get("hotkey_press_keys");
        text.Foreground = new SolidColorBrush(Teal);
        btn.BorderBrush = new SolidColorBrush(Teal);

        _coresHotkeyCapture = CaptureCoresKeyDown;
        AddHandler(UIElement.PreviewKeyDownEvent, _coresHotkeyCapture, true);
    }

    private void CancelCoresCapture()
    {
        _capturingCoresHotkey = false;
        if (_coresHotkeyCapture != null)
        {
            RemoveHandler(UIElement.PreviewKeyDownEvent, _coresHotkeyCapture);
            _coresHotkeyCapture = null;
        }
    }

    private void CaptureCoresKeyDown(object sender, KeyEventArgs e)
    {
        var key = e.Key == Key.System ? e.SystemKey : e.Key;

        if (key is Key.LeftCtrl or Key.RightCtrl
                or Key.LeftShift or Key.RightShift
                or Key.LeftAlt or Key.RightAlt
                or Key.LWin or Key.RWin
                or Key.None)
            return;

        var target = _coresCaptureTarget;
        CancelCoresCapture();

        if (key == Key.Escape)
        {
            RefreshCoresButtons();
            e.Handled = true;
            return;
        }

        var vk = (uint)KeyInterop.VirtualKeyFromKey(key);

        switch (target)
        {
            case 0: _coresFreezeVk = vk; break;
            case 1: _coresSlowerVk = vk; break;
            case 2: _coresNormalVk = vk; break;
        }

        SaveCoresHotkeys();
        RefreshCoresButtons();
        if (_coresEnabled) InstallCoresHook();
        e.Handled = true;
    }

    private void HandleCoresHotkey(CoresMode mode)
    {
        var (proc, _) = _cores.FindGameProcess(_chapters);
        if (proc == null && _cores.CurrentMode == CoresMode.Normal) return;
        proc?.Dispose();

        var freezePriority = _coresPriorityHigh ? ProcessPriorityClass.High : ProcessPriorityClass.Idle;
        var processName = _cores.ApplyMode(mode, _chapters, freezePriority);
        if (processName == null) return;
        ShowCoresToast(mode, processName, _cores.DetectedChapter);
    }

    private void ShowCoresToast(CoresMode mode, string? processName, int chapter)
    {
        _coresToast?.Close();

        const double W        = 340;
        const double Duration = 5;

        var modeLabel = mode switch
        {
            CoresMode.Slower => "1 CORE",
            CoresMode.Freeze => "0 CORES",
            _ => "ALL CORES",
        };
        var modeColor = mode switch
        {
            CoresMode.Slower => Color.FromArgb(255, 230, 180, 40),
            CoresMode.Freeze => Color.FromArgb(255, 220, 60,  60),
            _ => Color.FromArgb(255, 0, 204, 170),
        };

        var modeText = new TextBlock
        {
            Text       = modeLabel,
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize   = 16,
            FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(modeColor),
        };

        var textStack = new StackPanel { Margin = new Thickness(14, 10, 14, 10) };
        textStack.Children.Add(modeText);
        textStack.Children.Add(new TextBlock
        {
            Text       = $"Ch.{chapter}  —  {processName}",
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize   = 10,
            Foreground = new SolidColorBrush(Color.FromArgb(120, 160, 190, 210)),
            Margin     = new Thickness(0, 4, 0, 0),
        });

        var progressFg = new Border
        {
            Background          = new SolidColorBrush(modeColor),
            Height              = 3,
            HorizontalAlignment = HorizontalAlignment.Left,
            Width               = W - 2,
        };

        var progressGrid = new Grid { Height = 3 };
        progressGrid.Children.Add(new Border
        {
            Background = new SolidColorBrush(Color.FromArgb(40, modeColor.R, modeColor.G, modeColor.B)),
        });
        progressGrid.Children.Add(progressFg);

        var innerGrid = new Grid();
        innerGrid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        innerGrid.RowDefinitions.Add(new RowDefinition { Height = new GridLength(3) });
        Grid.SetRow(textStack,    0);
        Grid.SetRow(progressGrid, 1);
        innerGrid.Children.Add(textStack);
        innerGrid.Children.Add(progressGrid);

        var outerBorder = new Border
        {
            Background      = new SolidColorBrush(Color.FromArgb(240, 9, 20, 30)),
            BorderBrush     = new SolidColorBrush(Color.FromArgb(255, 21, 48, 72)),
            BorderThickness = new Thickness(1),
            CornerRadius    = new CornerRadius(6),
            ClipToBounds    = true,
            Child           = innerGrid,
        };

        var screen = SystemParameters.WorkArea;
        var toast = new Window
        {
            WindowStyle        = WindowStyle.None,
            AllowsTransparency = true,
            Background         = Brushes.Transparent,
            ResizeMode         = ResizeMode.NoResize,
            ShowInTaskbar      = false,
            Topmost            = true,
            Width              = W,
            SizeToContent      = SizeToContent.Height,
            Left               = screen.Left + 20,
            Top                = screen.Top + 20,
            Opacity            = 0,
            Content            = outerBorder,
        };

        toast.Loaded += (_, _) =>
        {
            toast.BeginAnimation(UIElement.OpacityProperty,
                new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(200)));

            var fullW = progressGrid.ActualWidth;
            progressFg.Width = fullW;
            progressFg.BeginAnimation(FrameworkElement.WidthProperty,
                new DoubleAnimation(fullW, 0, TimeSpan.FromSeconds(Duration)));

            var fadeTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(Duration - 0.5) };
            fadeTimer.Tick += (_, _) =>
            {
                fadeTimer.Stop();
                toast.BeginAnimation(UIElement.OpacityProperty,
                    new DoubleAnimation(1, 0, TimeSpan.FromMilliseconds(400)));
            };
            fadeTimer.Start();

            var closeTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(Duration) };
            closeTimer.Tick += (_, _) => { closeTimer.Stop(); toast.Close(); };
            closeTimer.Start();
        };

        _coresToast = toast;
        toast.Closed += (_, _) => { if (ReferenceEquals(_coresToast, toast)) _coresToast = null; };
        toast.Show();
    }

    // ── F11 remap configuration ───────────────────────────────────────────────

    private const int F11RemapAddSlot = -1;

    private void F11RemapEnableBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_capturingF11Remap)
            CancelF11RemapCapture();

        _f11Remap.SetEnabled(!_f11Remap.Enabled);
        RefreshF11RemapUI();
    }

    private void F11RemapAddKeyBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_capturingF11Remap)
        {
            var wasAdding = _f11RemapCaptureIndex == F11RemapAddSlot;
            CancelF11RemapCapture();
            if (wasAdding)
            {
                RefreshF11RemapUI();
                return;
            }
        }

        StartF11RemapCapture(F11RemapAddSlot);
    }

    private void F11RemapKeyBtn_Click(int index)
    {
        if (_capturingF11Remap)
        {
            var wasThis = _f11RemapCaptureIndex == index;
            CancelF11RemapCapture();
            if (wasThis)
            {
                RefreshF11RemapUI();
                return;
            }
        }

        StartF11RemapCapture(index);
    }

    private void StartF11RemapCapture(int slot)
    {
        _capturingF11Remap    = true;
        _f11RemapCaptureIndex = slot;

        _f11RemapKeyCapture = CaptureF11RemapKeyDown;
        AddHandler(UIElement.PreviewKeyDownEvent, _f11RemapKeyCapture, true);

        _f11RemapMouseCapture = CaptureF11RemapMouseDown;
        AddHandler(UIElement.PreviewMouseDownEvent, _f11RemapMouseCapture, true);

        RefreshF11RemapUI();
    }

    private void CancelF11RemapCapture()
    {
        _capturingF11Remap    = false;
        _f11RemapCaptureIndex = null;
        if (_f11RemapKeyCapture != null)
        {
            RemoveHandler(UIElement.PreviewKeyDownEvent, _f11RemapKeyCapture);
            _f11RemapKeyCapture = null;
        }
        if (_f11RemapMouseCapture != null)
        {
            RemoveHandler(UIElement.PreviewMouseDownEvent, _f11RemapMouseCapture);
            _f11RemapMouseCapture = null;
        }
    }

    private void CaptureF11RemapKeyDown(object sender, KeyEventArgs e)
    {
        var key = e.Key == Key.System ? e.SystemKey : e.Key;

        if (key is Key.LeftCtrl or Key.RightCtrl
                or Key.LeftShift or Key.RightShift
                or Key.LeftAlt or Key.RightAlt
                or Key.LWin or Key.RWin
                or Key.None)
            return;

        var slot = _f11RemapCaptureIndex;
        CancelF11RemapCapture();

        if (key != Key.Escape && key != Key.F11)
        {
            var vk = (uint)KeyInterop.VirtualKeyFromKey(key);
            if (slot is null or F11RemapAddSlot)
                _f11Remap.AddBinding(F11RemapInputType.Keyboard, keyVk: vk);
            else
                _f11Remap.ReplaceBinding(slot.Value, F11RemapInputType.Keyboard, keyVk: vk);
        }

        RefreshF11RemapUI();
        e.Handled = true;
    }

    private void CaptureF11RemapMouseDown(object sender, MouseButtonEventArgs e)
    {
        int? mouseButton = e.ChangedButton switch
        {
            MouseButton.Middle   => F11RemapService.MouseMiddle,
            MouseButton.XButton1 => F11RemapService.MouseXButton1,
            MouseButton.XButton2 => F11RemapService.MouseXButton2,
            _ => null,
        };

        if (mouseButton is null) return;

        var slot = _f11RemapCaptureIndex;
        CancelF11RemapCapture();

        if (slot is null or F11RemapAddSlot)
            _f11Remap.AddBinding(F11RemapInputType.Mouse, mouseButton: mouseButton.Value);
        else
            _f11Remap.ReplaceBinding(slot.Value, F11RemapInputType.Mouse, mouseButton: mouseButton.Value);

        RefreshF11RemapUI();
        e.Handled = true;
    }

    private void RefreshF11RemapUI()
    {
        SetToggle(F11RemapEnableText, _f11Remap.Enabled);

        F11RemapBindingsPanel.Children.Clear();
        for (var i = 0; i < _f11Remap.Bindings.Count; i++)
            F11RemapBindingsPanel.Children.Add(BuildF11RemapBindingRow(i));

        var addingNew = _f11RemapCaptureIndex == F11RemapAddSlot;
        F11RemapAddKeyText.Text       = addingNew ? Loc.Get("f11_remap_press_input") : Loc.Get("f11_remap_add_key");
        F11RemapAddKeyText.Foreground = new SolidColorBrush(addingNew
            ? Color.FromArgb(255, 0, 204, 170) : Color.FromArgb(255, 138, 170, 187));
        F11RemapAddKeyBtn.BorderBrush = new SolidColorBrush(addingNew
            ? Color.FromArgb(255, 0, 204, 170) : Color.FromArgb(255, 26, 58, 85));
    }

    private Grid BuildF11RemapBindingRow(int index)
    {
        var row = new Grid { Margin = new Thickness(0, index == 0 ? 0 : 8, 0, 0) };
        row.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        row.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        row.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

        var label = new TextBlock
        {
            Text = _f11Remap.Bindings.Count > 1
                ? Loc.Get("f11_remap_key_label_n", index + 1)
                : Loc.Get("f11_remap_key_label"),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 12,
            Foreground = new SolidColorBrush(Color.FromArgb(255, 138, 170, 187)),
            VerticalAlignment = VerticalAlignment.Center,
        };
        Grid.SetColumn(label, 0);
        row.Children.Add(label);

        var capturing = _f11RemapCaptureIndex == index;
        var accent    = new SolidColorBrush(Color.FromArgb(255, 0, 204, 170));
        var normal    = new SolidColorBrush(Color.FromArgb(255, 138, 170, 187));

        var keyBtn = new Button
        {
            Height = 30, MinWidth = 130,
            HorizontalAlignment = HorizontalAlignment.Right,
            Background = new SolidColorBrush(Color.FromArgb(255, 10, 24, 37)),
            BorderBrush = capturing ? accent : new SolidColorBrush(Color.FromArgb(255, 26, 58, 85)),
            BorderThickness = new Thickness(1),
            Padding = new Thickness(10, 0, 10, 0),
            Content = new TextBlock
            {
                Text = capturing ? Loc.Get("f11_remap_press_input") : BindingToString(_f11Remap.Bindings[index]),
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize = 10,
                Foreground = capturing ? accent : normal,
                VerticalAlignment = VerticalAlignment.Center,
            },
        };
        ButtonHelper.SetCornerRadius(keyBtn, new CornerRadius(4));
        keyBtn.Click += (_, _) => F11RemapKeyBtn_Click(index);
        Grid.SetColumn(keyBtn, 1);
        row.Children.Add(keyBtn);

        var removeBtn = new Button
        {
            Width = 26, Height = 30,
            Margin = new Thickness(6, 0, 0, 0),
            Background = new SolidColorBrush(Color.FromArgb(40, 180, 30, 30)),
            BorderThickness = new Thickness(0), Padding = new Thickness(0),
            Content = new TextBlock
            {
                Text = "×", FontSize = 14,
                Foreground = new SolidColorBrush(Color.FromArgb(200, 200, 60, 60)),
                HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center,
            },
        };
        ButtonHelper.SetCornerRadius(removeBtn, new CornerRadius(3));
        removeBtn.Click += (_, _) =>
        {
            if (_capturingF11Remap) CancelF11RemapCapture();
            _f11Remap.RemoveBinding(index);
            RefreshF11RemapUI();
        };
        Grid.SetColumn(removeBtn, 2);
        row.Children.Add(removeBtn);

        return row;
    }

    private string BindingToString(F11RemapBinding binding) => binding.Type switch
    {
        F11RemapInputType.Keyboard => KeyToString(KeyInterop.KeyFromVirtualKey((int)binding.KeyVk)),
        F11RemapInputType.Mouse => binding.MouseButton switch
        {
            F11RemapService.MouseMiddle   => Loc.Get("f11_remap_mouse3"),
            F11RemapService.MouseXButton1 => Loc.Get("f11_remap_mouse4"),
            F11RemapService.MouseXButton2 => Loc.Get("f11_remap_mouse5"),
            _ => Loc.Get("f11_remap_none"),
        },
        _ => Loc.Get("f11_remap_none"),
    };

    // ── Game watcher ──────────────────────────────────────────────────────────

    // ── LiveSplit server poller ───────────────────────────────────────────────

    private void StartLiveSplitPoller()
    {
        _liveSplitPollCts = new CancellationTokenSource();
        _ = PollLiveSplitAsync(_liveSplitPollCts.Token);
    }

    private async Task PollLiveSplitAsync(CancellationToken ct)
    {
        while (!ct.IsCancellationRequested)
        {
            try { await Task.Delay(1000, ct); }
            catch (OperationCanceledException) { break; }

            try
            {
                var line = await Task.Run(() => QueryLiveSplit(), ct);
                _discordPresence.UpdateLiveSplitLine(line);
            }
            catch { }
        }
    }

    private string QueryLiveSplit()
    {
        if (!_liveSplitClient.IsConnected && !_liveSplitClient.TryConnect())
            return "LiveSplit not connected";

        var phase = _liveSplitClient.GetTimerPhase();
        if (phase is null)
            return "LiveSplit not connected";

        if (phase is "Running" or "Paused" or "Ended")
        {
            var raw = _liveSplitClient.GetCurrentTime();
            return raw is null ? "LiveSplit not connected" : "⏱ " + FormatLiveSplitTime(raw);
        }

        // NotRunning = LiveSplit is open but no run has started yet
        return "LiveSplit: Ready";
    }

    private static string FormatLiveSplitTime(string raw)
    {
        if (!TimeSpan.TryParse(raw.Trim(), out var ts)) return raw.Trim();
        var cs = ts.Milliseconds / 10; // centiseconds
        return ts.Hours > 0
            ? $"{(int)ts.TotalHours}:{ts.Minutes:D2}:{ts.Seconds:D2}.{cs:D2}"
            : $"{(int)ts.TotalMinutes}:{ts.Seconds:D2}.{cs:D2}";
    }

    private static readonly TimeSpan GameWatcherInterval = TimeSpan.FromSeconds(2);

    private void StartGameWatcher()
    {
        var timer = new DispatcherTimer { Interval = GameWatcherInterval };
        timer.Tick += GameWatcherTick;
        timer.Start();
    }

    private void GameWatcherTick(object? sender, EventArgs e)
    {
        bool anyRunning    = false;
        bool stateChanged  = false;

        for (int i = 0; i < _chapters.Count; i++)
        {
            var path = GetActiveExePath(_chapters[i]);
            if (string.IsNullOrEmpty(path)) { _gameWasRunning[i] = false; _runningChapterPid[i] = 0; continue; }

            var exeName = IOPath.GetFileNameWithoutExtension(path);
            bool running;
            try
            {
                var procs = Process.GetProcessesByName(exeName);
                // Some installs point at a thin UE launcher stub (e.g. "ch5_pro.exe") that stays
                // resident as a parent process alongside the real renderer it spawns
                // ("ch5_pro-Win64-Shipping.exe"). The stub never presents a frame, so FPS tracking
                // must target the shipping process's PID when one is present, not the stub's.
                // The shipping binary's name doesn't always match the stub's own filename though
                // (e.g. Chapter 4's stub is "Playtime_Chapter4.exe" but its shipping binary is
                // "ch4_pro-Win64-Shipping.exe"), so it's resolved from disk instead of guessed.
                var shippingExeName = ResolveShippingExeName(path);
                var shippingProcs = shippingExeName != null
                    ? Process.GetProcessesByName(shippingExeName)
                    : [];
                var trackedProcs = shippingProcs.Length > 0 ? shippingProcs : procs;

                running = procs.Length > 0 || shippingProcs.Length > 0;
                _runningChapterPid[i] = running ? trackedProcs[0].Id : 0;

                foreach (var p in procs) p.Dispose();
                foreach (var p in shippingProcs) p.Dispose();
            }
            catch (Exception)
            {
                running = false;
                _runningChapterPid[i] = 0;
            }

            if (_gameWatcherInitialized && running && !_gameWasRunning[i])
            {
                if (_gameToast    == null || !_gameToast.IsVisible)    ShowGameToast();
                if (_tutorialToast == null || !_tutorialToast.IsVisible) ShowTutorialToast();
            }

            if (running && !_gameWasRunning[i]) StartFpsTracking(_runningChapterPid[i]);
            else if (!running && _gameWasRunning[i]) StopFpsTracking();

            if (running)
            {
                _playtimeStore.AddSeconds(_chapters[i].Number, GameWatcherInterval.TotalSeconds);
                UpdateHoursText(i);
            }

            if (running != _gameWasRunning[i]) stateChanged = true;
            _gameWasRunning[i] = running;
            if (running) anyRunning = true;
        }

        // Discord always reflects what the user selected in the launcher
        if (_gameWatcherInitialized && !anyRunning && stateChanged)
            _discordPresence.SetChapterSelected(_chapters[_selected], GetVersionLabel(_chapters[_selected]));

        if (stateChanged) RefreshInfo();

        // Restore UE4SS temp remap when the launched game exits
        if (_ue4ssTempRemap && !string.IsNullOrEmpty(_ue4ssTempRemapExe) && _gameWatcherInitialized)
        {
            bool remapRunning;
            try { remapRunning = Process.GetProcessesByName(IOPath.GetFileNameWithoutExtension(_ue4ssTempRemapExe)).Length > 0; }
            catch { remapRunning = false; }
            if (!remapRunning) RestoreUe4ssHotkeys();
        }

        _gameWatcherInitialized = true;
    }

    // Starts (or restarts, if a different chapter's game launched) FPS tracking for the
    // process that was just detected running, and shows the overlay HUD. Requires a one-time
    // UAC prompt per launcher session/game (see GameFpsService) — if the user declines it, the
    // overlay simply never appears, which ElevationDeclined would let us report if needed.
    private void StartFpsTracking(int gamePid)
    {
        if (gamePid == 0 || !_fpsOverlayEnabled) return;

        _fpsService.Start(gamePid);

        if (_fpsOverlay == null)
        {
            _fpsOverlay = new FpsOverlayWindow();
            // Show() first: PlaceInCorner (inside ApplyFpsOverlayAppearance) needs a real
            // Width/Height, which SizeToContent only resolves once shown/laid out. The window
            // also re-pins itself to its corner on any later SizeChanged (see FpsOverlayWindow),
            // covering the custom font finishing its load a moment after this first pass.
            _fpsOverlay.Show();
            ApplyFpsOverlayAppearance();
        }
    }

    private void StopFpsTracking()
    {
        _fpsService.Stop();
        _fpsOverlay?.Close();
        _fpsOverlay = null;
    }

    private void ShowGameToast()
    {
        _gameToast?.Close();

        const double W = 330;

        var progressFg = new Border
        {
            Background          = new SolidColorBrush(Color.FromArgb(255, 0, 204, 170)),
            Height              = 3,
            HorizontalAlignment = HorizontalAlignment.Left,
            Width               = W - 2,
        };

        var progressGrid = new Grid { Height = 3 };
        progressGrid.Children.Add(new Border
        {
            Background = new SolidColorBrush(Color.FromArgb(40, 0, 204, 170)),
        });
        progressGrid.Children.Add(progressFg);

        var hintText = new TextBlock
        {
            Text       = Loc.Get("game_toast_hint"),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize   = 10,
            Foreground = new SolidColorBrush(Color.FromArgb(180, 160, 190, 210)),
        };
        var keyText = new TextBlock
        {
            Text       = FormatHotkey(_hotkeyModifiers, _hotkeyVk),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize   = 13,
            FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(Color.FromArgb(255, 0, 204, 170)),
            Margin     = new Thickness(0, 3, 0, 0),
        };

        var textStack = new StackPanel { Margin = new Thickness(14, 12, 14, 10) };
        textStack.Children.Add(hintText);
        textStack.Children.Add(keyText);

        var innerGrid = new Grid();
        innerGrid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        innerGrid.RowDefinitions.Add(new RowDefinition { Height = new GridLength(3) });
        Grid.SetRow(textStack,    0);
        Grid.SetRow(progressGrid, 1);
        innerGrid.Children.Add(textStack);
        innerGrid.Children.Add(progressGrid);

        var outerBorder = new Border
        {
            Background      = new SolidColorBrush(Color.FromArgb(240, 9, 20, 30)),
            BorderBrush     = new SolidColorBrush(Color.FromArgb(255, 21, 48, 72)),
            BorderThickness = new Thickness(1),
            CornerRadius    = new CornerRadius(6),
            ClipToBounds    = true,
            Child           = innerGrid,
        };

        var screen = SystemParameters.WorkArea;
        var toast = new Window
        {
            WindowStyle        = WindowStyle.None,
            AllowsTransparency = true,
            Background         = Brushes.Transparent,
            ResizeMode         = ResizeMode.NoResize,
            ShowInTaskbar      = false,
            Topmost            = true,
            Width              = W,
            SizeToContent      = SizeToContent.Height,
            Left               = screen.Right - W - 20,
            Top                = screen.Top + screen.Height / 2,
            Opacity            = 0,
            Content            = outerBorder,
        };

        toast.Loaded += (_, _) =>
        {
            toast.Top = screen.Top + (screen.Height - toast.ActualHeight) / 2;

            toast.BeginAnimation(UIElement.OpacityProperty,
                new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(250)));

            var fullW = progressGrid.ActualWidth;
            progressFg.Width = fullW;
            progressFg.BeginAnimation(FrameworkElement.WidthProperty,
                new DoubleAnimation(fullW, 0, TimeSpan.FromSeconds(10)));

            var fadeTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(9) };
            fadeTimer.Tick += (_, _) =>
            {
                fadeTimer.Stop();
                toast.BeginAnimation(UIElement.OpacityProperty,
                    new DoubleAnimation(1, 0, TimeSpan.FromMilliseconds(800)));
            };
            fadeTimer.Start();

            var closeTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(10) };
            closeTimer.Tick += (_, _) => { closeTimer.Stop(); toast.Close(); };
            closeTimer.Start();
        };

        _gameToast = toast;
        toast.Closed += (_, _) => { if (ReferenceEquals(_gameToast, toast)) _gameToast = null; };
        toast.Show();
    }

    private void ShowTutorialToast()
    {
        _tutorialToast?.Close();

        const double W        = 330;
        const double Duration = 15;

        var progressFg = new Border
        {
            Background          = new SolidColorBrush(Color.FromArgb(255, 0, 204, 170)),
            Height              = 3,
            HorizontalAlignment = HorizontalAlignment.Left,
            Width               = W - 2,
        };

        var progressGrid = new Grid { Height = 3 };
        progressGrid.Children.Add(new Border
        {
            Background = new SolidColorBrush(Color.FromArgb(40, 0, 204, 170)),
        });
        progressGrid.Children.Add(progressFg);

        var hintText = new TextBlock
        {
            Text       = Loc.Get("tutorial_toast_hint"),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize   = 10,
            Foreground = new SolidColorBrush(Color.FromArgb(180, 160, 190, 210)),
        };
        var keyText = new TextBlock
        {
            Text       = FormatHotkey(_tutorialHotkeyModifiers, _tutorialHotkeyVk),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize   = 13,
            FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(Color.FromArgb(255, 0, 204, 170)),
            Margin     = new Thickness(0, 3, 0, 0),
        };

        var textStack = new StackPanel { Margin = new Thickness(14, 12, 14, 10) };
        textStack.Children.Add(hintText);
        textStack.Children.Add(keyText);

        var innerGrid = new Grid();
        innerGrid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        innerGrid.RowDefinitions.Add(new RowDefinition { Height = new GridLength(3) });
        Grid.SetRow(textStack,    0);
        Grid.SetRow(progressGrid, 1);
        innerGrid.Children.Add(textStack);
        innerGrid.Children.Add(progressGrid);

        var outerBorder = new Border
        {
            Background      = new SolidColorBrush(Color.FromArgb(240, 9, 20, 30)),
            BorderBrush     = new SolidColorBrush(Color.FromArgb(255, 21, 48, 72)),
            BorderThickness = new Thickness(1),
            CornerRadius    = new CornerRadius(6),
            ClipToBounds    = true,
            Child           = innerGrid,
        };

        var screen = SystemParameters.WorkArea;
        var toast = new Window
        {
            WindowStyle        = WindowStyle.None,
            AllowsTransparency = true,
            Background         = Brushes.Transparent,
            ResizeMode         = ResizeMode.NoResize,
            ShowInTaskbar      = false,
            Topmost            = true,
            Width              = W,
            SizeToContent      = SizeToContent.Height,
            Left               = screen.Right - W - 20,
            Top                = screen.Top + screen.Height / 2,
            Opacity            = 0,
            Content            = outerBorder,
        };

        toast.Loaded += (_, _) =>
        {
            // Place below center, offset enough to clear the checkpoint toast (~65px tall + 10px gap)
            toast.Top = screen.Top + (screen.Height - toast.ActualHeight) / 2 + 85;

            toast.BeginAnimation(UIElement.OpacityProperty,
                new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(250)));

            var fullW = progressGrid.ActualWidth;
            progressFg.Width = fullW;
            progressFg.BeginAnimation(FrameworkElement.WidthProperty,
                new DoubleAnimation(fullW, 0, TimeSpan.FromSeconds(Duration)));

            var fadeTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(Duration - 1) };
            fadeTimer.Tick += (_, _) =>
            {
                fadeTimer.Stop();
                toast.BeginAnimation(UIElement.OpacityProperty,
                    new DoubleAnimation(1, 0, TimeSpan.FromMilliseconds(800)));
            };
            fadeTimer.Start();

            var closeTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(Duration) };
            closeTimer.Tick += (_, _) => { closeTimer.Stop(); toast.Close(); };
            closeTimer.Start();
        };

        _tutorialToast = toast;
        toast.Closed += (_, _) => { if (ReferenceEquals(_tutorialToast, toast)) _tutorialToast = null; };
        toast.Show();
    }

    [System.Runtime.InteropServices.DllImport("winmm.dll",
        CharSet = System.Runtime.InteropServices.CharSet.Unicode)]
    private static extern bool PlaySoundW(string pszSound, nint hmod, uint fdwSound);

    [System.Runtime.InteropServices.DllImport("winmm.dll")]
    private static extern int waveOutSetVolume(nint hwo, uint dwVolume);

    private void PlaySfx(string fileName, bool noStop = false)
    {
        if (_sfxVolume <= 0f) return;
        const uint SND_ASYNC     = 0x0001;
        const uint SND_FILENAME  = 0x20000;
        const uint SND_NODEFAULT = 0x0002;
        const uint SND_NOSTOP    = 0x0010;
        uint vol    = (uint)(_sfxVolume * 0xFFFF);
        uint stereo = (vol & 0xFFFF) | ((vol & 0xFFFF) << 16);
        waveOutSetVolume(0, stereo);
        var path = IOPath.Combine(Services.ResourceExtractor.TempDir, "Assets", "Sounds", fileName);
        if (File.Exists(path))
            PlaySoundW(path, 0, SND_ASYNC | SND_FILENAME | SND_NODEFAULT | (noStop ? SND_NOSTOP : 0));
    }

    private void AttachButtonSounds(DependencyObject root)
    {
        int n = VisualTreeHelper.GetChildrenCount(root);
        for (int i = 0; i < n; i++)
        {
            var child = VisualTreeHelper.GetChild(root, i);
            if (child is Button btn)
            {
                btn.MouseEnter += (_, _) => PlaySfx("OpcionMover.WAV", noStop: true);
                btn.Click      += (_, _) => PlaySfx("SelecOption.WAV");
            }
            AttachButtonSounds(child);
        }
    }

    // ── Chapter cards ─────────────────────────────────────────────────────────

    private static string GetBannerPath(int chapterNumber)
    {
        var bannerDir  = IOPath.Combine(Services.ResourceExtractor.TempDir, "Assets", "Banners");
        var bannerPath = IOPath.Combine(bannerDir, $"Chapter {chapterNumber}.jpg");
        if (!File.Exists(bannerPath))
            bannerPath = IOPath.Combine(bannerDir, $"Chapter {chapterNumber}.png");
        return bannerPath;
    }

    private void BuildCards()
    {
        for (int i = 0; i < _chapters.Count; i++)
        {
            var chapter = _chapters[i];
            var card = MakeCard(chapter, GetBannerPath(i + 1), out var hoursText);
            var idx = i;
            card.MouseDown   += (_, _) => SelectChapter(idx);
            card.MouseEnter  += (_, _) => { OnCardHover(idx, true); PlaySfx("OpcionMover.WAV", noStop: true); };
            card.MouseLeave  += (_, _) => OnCardHover(idx, false);
            _cards.Add(card);
            _hoursTexts.Add(hoursText);
            CardsPanel.Children.Add(card);
        }
        RefreshUe4ssBtnStates();
        RefreshLoadManipBtnStates();
        RefreshFullBrightBtnStates();
    }

    private void UpdateHoursText(int index)
    {
        var chapter = _chapters[index];
        _hoursTexts[index].Text = ChapterPlaytimeStore.Format(_playtimeStore.GetPlaytime(chapter.Number));
    }

    private Border MakeCard(ChapterInfo chapter, string bannerPath, out TextBlock hoursText)
    {
        var grid = new Grid();

        if (File.Exists(bannerPath))
            grid.Children.Add(new Image
            {
                Source = new BitmapImage(new Uri(bannerPath)),
                Stretch = Stretch.UniformToFill,
                HorizontalAlignment = HorizontalAlignment.Center,
                VerticalAlignment   = VerticalAlignment.Center,
            });

        var grad = new LinearGradientBrush
        {
            StartPoint = new System.Windows.Point(0, 0),
            EndPoint   = new System.Windows.Point(0, 1),
        };
        grad.GradientStops.Add(new GradientStop { Color = Overlay0, Offset = 0.30 });
        grad.GradientStops.Add(new GradientStop { Color = Overlay1, Offset = 0.85 });
        grid.Children.Add(new Rectangle { Fill = grad });

        var bottom = new StackPanel
        {
            VerticalAlignment = VerticalAlignment.Bottom,
            Margin = new Thickness(12, 0, 12, 12),
        };
        bottom.Children.Add(new TextBlock
        {
            Text = Loc.Get($"ch{chapter.Number}_title"),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 12, FontWeight = FontWeights.SemiBold,
            Foreground = new SolidColorBrush(Color.FromArgb(230, 230, 240, 255)),
            TextWrapping = TextWrapping.Wrap,
        });

        hoursText = new TextBlock
        {
            Text = ChapterPlaytimeStore.Format(_playtimeStore.GetPlaytime(chapter.Number)),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 10,
            Foreground = new SolidColorBrush(TealDim),
            Margin = new Thickness(0, 2, 0, 0),
        };
        bottom.Children.Add(hoursText);

        if (chapter.Number == 1 || chapter.Number == 2 || chapter.Number == 3 || chapter.Number >= 4)
        {
            var actionsPanel = new StackPanel
            {
                Orientation = Orientation.Horizontal,
                HorizontalAlignment = HorizontalAlignment.Left,
                Margin = new Thickness(0, 8, 0, 0),
            };

            var splitsDir = IOPath.Combine(Services.ResourceExtractor.TempDir, "Assets", "Splits", $"Chapter {chapter.Number}");
            if (Directory.Exists(splitsDir))
            {
                var autoSplitterBtn = new Button
                {
                    Background = new SolidColorBrush(Color.FromArgb(180, 9, 20, 30)),
                    BorderBrush = new SolidColorBrush(Color.FromArgb(140, 0, 204, 170)),
                    BorderThickness = new Thickness(1),
                    Padding = new Thickness(10, 5, 10, 5),
                    Margin = new Thickness(0, 0, 6, 0),
                    Tag = chapter.Number,
                };
                ButtonHelper.SetCornerRadius(autoSplitterBtn, new CornerRadius(3));
                autoSplitterBtn.Content = new TextBlock
                {
                    FontFamily = new FontFamily("Segoe MDL2 Assets"),
                    Text = "",
                    FontSize = 11,
                    Foreground = new SolidColorBrush(Color.FromArgb(200, 0, 204, 170)),
                    VerticalAlignment = VerticalAlignment.Center,
                };
                autoSplitterBtn.MouseDown += (s, ev) => ev.Handled = true;
                autoSplitterBtn.Click += AutoSplitterCardBtn_Click;
                actionsPanel.Children.Add(autoSplitterBtn);
            }

            var saveBtn = new Button
            {
                Background = new SolidColorBrush(Color.FromArgb(180, 9, 20, 30)),
                BorderBrush = new SolidColorBrush(Color.FromArgb(140, 0, 204, 170)),
                BorderThickness = new Thickness(1),
                Padding = new Thickness(10, 5, 10, 5),
                Tag = chapter.Number,
            };
            ButtonHelper.SetCornerRadius(saveBtn, new CornerRadius(3));
            var saveBtnContent = new StackPanel { Orientation = Orientation.Horizontal };
            saveBtnContent.Children.Add(new TextBlock
            {
                FontFamily = new FontFamily("Segoe MDL2 Assets"),
                Text = "",
                FontSize = 11,
                Foreground = new SolidColorBrush(Color.FromArgb(200, 0, 204, 170)),
                VerticalAlignment = VerticalAlignment.Center,
            });
            saveBtnContent.Children.Add(new TextBlock
            {
                Text = "+",
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize = 12, FontWeight = FontWeights.Bold,
                Foreground = new SolidColorBrush(Color.FromArgb(200, 0, 204, 170)),
                VerticalAlignment = VerticalAlignment.Center,
                Margin = new Thickness(4, 0, 0, 0),
            });
            saveBtn.Content = saveBtnContent;
            saveBtn.MouseDown += (s, ev) => ev.Handled = true;
            saveBtn.Click += SaveCardOpenBtn_Click;
            actionsPanel.Children.Add(saveBtn);

            var ue4ssBtn = new Button
            {
                Background      = new SolidColorBrush(Color.FromArgb(180, 9, 20, 30)),
                BorderBrush     = new SolidColorBrush(Color.FromArgb(140, 0, 204, 170)),
                BorderThickness = new Thickness(1),
                Padding         = new Thickness(10, 5, 10, 5),
                Margin          = new Thickness(6, 0, 0, 0),
                Tag             = chapter.Number,
            };
            ButtonHelper.SetCornerRadius(ue4ssBtn, new CornerRadius(3));
            ue4ssBtn.Content = new TextBlock
            {
                Text              = "UE4SS",
                FontFamily        = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize          = 9,
                FontWeight        = FontWeights.Bold,
                Foreground        = new SolidColorBrush(Color.FromArgb(200, 0, 204, 170)),
                VerticalAlignment = VerticalAlignment.Center,
            };
            ue4ssBtn.MouseDown += (s, ev) => ev.Handled = true;
            ue4ssBtn.Click     += Ue4ssCardBtn_Click;
            actionsPanel.Children.Add(ue4ssBtn);
            _ue4ssBtns.Add(ue4ssBtn);

            if (chapter.Number == 1 || chapter.Number == 4 || chapter.Number == 5)
            {
                var loadManipBtn = new Button
                {
                    Background      = new SolidColorBrush(Color.FromArgb(180, 9, 20, 30)),
                    BorderBrush     = new SolidColorBrush(Color.FromArgb(140, 0, 204, 170)),
                    BorderThickness = new Thickness(1),
                    Padding         = new Thickness(10, 5, 10, 5),
                    Margin          = new Thickness(6, 0, 0, 0),
                    Tag             = chapter.Number,
                };
                ButtonHelper.SetCornerRadius(loadManipBtn, new CornerRadius(3));
                var loadManipContent = new StackPanel { Orientation = Orientation.Horizontal };
                loadManipContent.Children.Add(new Image
                {
                    Source            = (DrawingImage)FindResource("LoadManipIcon"),
                    Width             = 12,
                    Height            = 12,
                    Margin            = new Thickness(0, 0, 5, 0),
                    VerticalAlignment = VerticalAlignment.Center,
                });
                loadManipContent.Children.Add(new TextBlock
                {
                    Text              = "MANIP",
                    FontFamily        = new FontFamily("Cascadia Code, Consolas, Courier New"),
                    FontSize          = 9,
                    FontWeight        = FontWeights.Bold,
                    Foreground        = new SolidColorBrush(Color.FromArgb(200, 0, 204, 170)),
                    VerticalAlignment = VerticalAlignment.Center,
                });
                loadManipBtn.Content = loadManipContent;
                loadManipBtn.MouseDown += (s, ev) => ev.Handled = true;
                loadManipBtn.Click     += LoadManipCardBtn_Click;
                actionsPanel.Children.Add(loadManipBtn);
                _loadManipBtns[chapter.Number] = loadManipBtn;
            }

            if (chapter.Number == 1 || chapter.Number == 5)
            {
                var fullBrightBtn = new Button
                {
                    Background      = new SolidColorBrush(Color.FromArgb(180, 9, 20, 30)),
                    BorderBrush     = new SolidColorBrush(Color.FromArgb(140, 0, 204, 170)),
                    BorderThickness = new Thickness(1),
                    Padding         = new Thickness(10, 5, 10, 5),
                    Margin          = new Thickness(6, 0, 0, 0),
                    Tag             = chapter.Number,
                };
                ButtonHelper.SetCornerRadius(fullBrightBtn, new CornerRadius(3));
                fullBrightBtn.Content = new TextBlock
                {
                    Text              = "FULLBRIGHT",
                    FontFamily        = new FontFamily("Cascadia Code, Consolas, Courier New"),
                    FontSize          = 9,
                    FontWeight        = FontWeights.Bold,
                    Foreground        = new SolidColorBrush(Color.FromArgb(200, 0, 204, 170)),
                    VerticalAlignment = VerticalAlignment.Center,
                };
                fullBrightBtn.MouseDown += (s, ev) => ev.Handled = true;
                fullBrightBtn.Click     += FullBrightCardBtn_Click;
                actionsPanel.Children.Add(fullBrightBtn);
                _fullBrightBtns[chapter.Number] = fullBrightBtn;
            }

            bottom.Children.Add(actionsPanel);
        }

        grid.Children.Add(bottom);

        if (!chapter.IsAvailable)
        {
            var overlay = new Border { Background = new SolidColorBrush(Color.FromArgb(170, 5, 10, 18)) };

            var overlayContent = new StackPanel
            {
                HorizontalAlignment = HorizontalAlignment.Center,
                VerticalAlignment   = VerticalAlignment.Center,
            };

            var lockedImgPath = IOPath.Combine(Services.ResourceExtractor.TempDir, "Assets", "Images", "BLOQUEADO.png");
            if (File.Exists(lockedImgPath))
            {
                overlayContent.Children.Add(new Border
                {
                    Width = 120, Height = 120,
                    CornerRadius = new CornerRadius(8),
                    BorderBrush = new SolidColorBrush(Teal),
                    BorderThickness = new Thickness(2),
                    Background = new SolidColorBrush(Color.FromArgb(160, 9, 20, 30)),
                    Effect = new System.Windows.Media.Effects.DropShadowEffect
                    {
                        Color = Teal, BlurRadius = 20, ShadowDepth = 0, Opacity = 0.8,
                    },
                    Child = new Image
                    {
                        Source = new BitmapImage(new Uri(lockedImgPath)),
                        Width = 90, Height = 90,
                        Stretch = Stretch.Uniform,
                        HorizontalAlignment = HorizontalAlignment.Center,
                        VerticalAlignment   = VerticalAlignment.Center,
                    },
                });
            }

            overlayContent.Children.Add(new TextBlock
            {
                Text = Loc.Get("coming_soon"),
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize = 9, Foreground = new SolidColorBrush(TealDim),
                HorizontalAlignment = HorizontalAlignment.Center,
                VerticalAlignment   = VerticalAlignment.Center,
                TextWrapping = TextWrapping.Wrap, TextAlignment = TextAlignment.Center,
                Margin = new Thickness(0, 10, 0, 0),
            });

            overlay.Child = overlayContent;
            grid.Children.Add(overlay);
        }

        return new Border
        {
            Width = 340, Height = 520, CornerRadius = new CornerRadius(6),
            BorderBrush = new SolidColorBrush(CardBorder), BorderThickness = new Thickness(1),
            Opacity = chapter.IsAvailable ? 0.6 : 0.3,
            Background = Brushes.Transparent,
            Child = grid,
        };
    }

    // ── Selection ─────────────────────────────────────────────────────────────

    private void SelectChapter(int index)
    {
        if (index != _selected) PlaySfx("SelecChapter.WAV");
        _selected = index;
        if (!_gameWasRunning.Any(x => x))
            _discordPresence.SetChapterSelected(_chapters[index], GetVersionLabel(_chapters[index]));
        for (int i = 0; i < _cards.Count; i++)
        {
            bool sel = i == index;
            _cards[i].Width           = sel ? 400 : 340;
            _cards[i].Height          = sel ? 610 : 520;
            _cards[i].Opacity         = sel ? 1.0 : (_chapters[i].IsAvailable ? 0.55 : 0.28);
            _cards[i].BorderBrush     = new SolidColorBrush(sel ? Teal : CardBorder);
            _cards[i].BorderThickness = new Thickness(sel ? 2 : 1);
        }
        RefreshInfo();
    }

    private void OnCardHover(int index, bool enter)
    {
        if (index == _selected) return;
        _cards[index].Opacity = enter ? 0.85 : (_chapters[index].IsAvailable ? 0.55 : 0.28);
    }

    // Returns the exe that should actually be launched for the chapter,
    // respecting manual selection → Epic toggle → Steam auto-detect priority.
    private string? GetActiveExePath(ChapterInfo ch)
    {
        var selPath = _store.GetSelectedPath(ch.Number);
        if (selPath != null) return selPath;

        // In Epic mode never fall back to the Steam exe — if there is no Epic
        // path for this chapter it is treated as "not installed".
        if (_epicService.IsEnabled)
            return _epicService.GetExePath(ch.Number);

        return ch.GameExePath;
    }

    private void RefreshPlatformButton()
    {
        var isEpic = _epicService.IsEnabled;
        PlatformToggleText.Text       = isEpic ? "EPIC" : "STEAM";
        PlatformToggleText.Foreground = isEpic
            ? new SolidColorBrush(Color.FromArgb(255, 232, 120,  0))
            : new SolidColorBrush(Color.FromArgb(255,  42,  90, 122));
        PlatformToggleBorder.BorderBrush = isEpic
            ? new SolidColorBrush(Color.FromArgb(180, 150,  60,  0))
            : new SolidColorBrush(Color.FromArgb(255,  26,  58,  85));
    }

    private void RefreshInfo()
    {
        var ch      = _chapters[_selected];
        var selPath = _store.GetSelectedPath(ch.Number);

        TitleText.Text       = Loc.Get($"ch{ch.Number}_title");
        DescriptionText.Text = Loc.Get($"ch{ch.Number}_desc");

        if (selPath != null)
        {
            var custom = _store.GetCustoms(ch.Number).FirstOrDefault(x => x.ExePath == selPath);
            VersionText.Text = Loc.Get("version_prefix") + " " + (custom?.Name ?? IOPath.GetFileNameWithoutExtension(selPath));
        }
        else if (_epicService.IsEnabled && _epicService.GetExePath(ch.Number) is not null)
        {
            VersionText.Text = Loc.Get("version_prefix") + " " + Loc.Get("version_auto_epic");
        }
        else if (_epicService.IsEnabled)
        {
            // Epic mode but no known exe for this chapter — treat as not installed.
            VersionText.Text = ch.IsAvailable
                ? Loc.Get("version_prefix") + " " + Loc.Get("version_not_installed")
                : Loc.Get("version_prefix") + " " + Loc.Get("version_none");
        }
        else
        {
            VersionText.Text = ch.IsInstalled
                ? Loc.Get("version_prefix") + " " + Loc.Get("version_auto_steam")
                : ch.IsAvailable
                    ? Loc.Get("version_prefix") + " " + Loc.Get("version_not_installed")
                    : Loc.Get("version_prefix") + " " + Loc.Get("version_none");
        }

        RefreshPlatformButton();

        var activeExe = GetActiveExePath(ch);
        var canPlay   = !string.IsNullOrEmpty(activeExe) && File.Exists(activeExe);
        var isRunning = IsProcessRunning(activeExe);
        StatusText.Text      = isRunning ? Loc.Get("status_playing")
                             : canPlay   ? Loc.Get("status_installed")
                             : ch.IsAvailable ? Loc.Get("status_not_found")
                             : Loc.Get("status_coming_soon");
        PlayButton.IsEnabled = canPlay && !isRunning;
        PlayButton.Opacity   = canPlay ? 1.0 : 0.35;

        RefreshUe4ssBtnStates();
        RefreshLoadManipBtnStates();
        RefreshFullBrightBtnStates();
    }

    private string GetVersionLabel(ChapterInfo ch)
    {
        var selPath = _store.GetSelectedPath(ch.Number);
        if (selPath != null)
        {
            var custom = _store.GetCustoms(ch.Number).FirstOrDefault(x => x.ExePath == selPath);
            return custom?.Name ?? IOPath.GetFileNameWithoutExtension(selPath);
        }
        if (_epicService.IsEnabled && _epicService.GetExePath(ch.Number) is not null)
            return Loc.Get("version_auto_epic");
        return ch.IsInstalled ? Loc.Get("version_auto_steam")
             : ch.IsAvailable ? Loc.Get("version_not_installed")
             : Loc.Get("version_none");
    }

    private static bool IsProcessRunning(string? exePath)
    {
        if (string.IsNullOrEmpty(exePath)) return false;
        try { return Process.GetProcessesByName(IOPath.GetFileNameWithoutExtension(exePath)).Length > 0; }
        catch { return false; }
    }

    private ChapterInfo? GetRunningChapter()
    {
        for (int i = 0; i < _chapters.Count; i++)
            if (_gameWasRunning[i]) return _chapters[i];
        return null;
    }

    private async Task DetectVersionsAsync()
    {
        await Task.Run(() => SteamDetector.DetectAll(_chapters));
        _ = Dispatcher.BeginInvoke(new Action(RefreshInfo));
    }

    // ── Versions overlay ──────────────────────────────────────────────────────

    private void OpenVersionsOverlay()
    {
        _versionsChapter = _selected;
        BuildInstallationsList();
        VersionsOverlay.Visibility = Visibility.Visible;
    }

    private void BuildInstallationsList()
    {
        InstallsList.Children.Clear();

        var ch    = _chapters[_versionsChapter];
        var chNum = ch.Number;
        var sel   = _store.GetSelectedPath(chNum);

        VersionsHeader.Text = Loc.Get("versions_header", chNum);

        TogglePresetsBtn.Visibility = Visibility.Visible;
        TogglePresetsBtn.Content = new TextBlock
        {
            Text = _hidePresetRows ? Loc.Get("show_installers") : Loc.Get("hide_installers"),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 11, FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(Color.FromArgb(255, 45, 90, 120)),
        };

        if (!_epicService.IsEnabled)
        {
            InstallsList.Children.Add(
                MakeInstallRow(Loc.Get("auto_name"), Loc.Get("auto_subtitle"),
                    isAuto: true, isSelected: sel is null, chapterNum: chNum,
                    exePath: ch.GameExePath ?? ""));
        }
        else
        {
            var epicExe      = _epicService.GetExePath(chNum);
            var epicIconPath = IOPath.Combine(Services.ResourceExtractor.TempDir, "Assets", "Images", "Epic.png");
            InstallsList.Children.Add(
                MakeInstallRow(Loc.Get("auto_name"), Loc.Get("version_auto_epic"),
                    isAuto: true, isSelected: sel is null, chapterNum: chNum,
                    exePath: epicExe ?? "", iconOverride: epicIconPath));
        }

        if (ch.Presets.Count > 0 && !_hidePresetRows)
        {
            InstallsList.Children.Add(MakeSectionLabel(Loc.Get("section_historic")));
            foreach (var preset in ch.Presets)
            {
                var isInstalled   = _store.IsManifestInstalled(preset.ManifestId);
                var isDownloading = _activePolls.ContainsKey(preset.ManifestId);
                InstallsList.Children.Add(MakePresetRow(preset, chNum, isInstalled, isDownloading, sel));
            }
        }

        var customs = _store.GetCustoms(chNum);
        if (customs.Count > 0)
        {
            InstallsList.Children.Add(MakeSectionLabel(Loc.Get("section_custom")));
            for (int i = 0; i < customs.Count; i++)
            {
                var inst = customs[i];
                InstallsList.Children.Add(
                    MakeInstallRow(inst.Name, inst.ExePath,
                        isAuto: false, isSelected: sel == inst.ExePath,
                        chapterNum: chNum, exePath: inst.ExePath, inst: inst,
                        isFirst: i == 0, isLast: i == customs.Count - 1));
            }
        }
    }

    private static TextBlock MakeSectionLabel(string text) => new()
    {
        Text = text,
        FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
        FontSize = 9, Foreground = new SolidColorBrush(Color.FromArgb(160, 0, 204, 170)),
        Margin = new Thickness(2, 10, 0, 2),
    };

    private Border MakeInstallRow(string name, string subtitle,
        bool isAuto, bool isSelected, int chapterNum, string exePath,
        InstallationInfo? inst = null, string? iconOverride = null,
        bool isFirst = false, bool isLast = false)
    {
        var grid = new Grid();
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(46) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

        var iconBorder = new Border
        {
            Width = 34, Height = 34, CornerRadius = new CornerRadius(4),
            Background = iconOverride is not null
                ? Brushes.Transparent
                : new SolidColorBrush(isAuto ? Color.FromArgb(255, 18, 60, 110) : Color.FromArgb(255, 20, 38, 55)),
            HorizontalAlignment = HorizontalAlignment.Left, VerticalAlignment = VerticalAlignment.Center,
        };
        var customIcon = inst?.IconPath;
        var steamImg   = IOPath.Combine(Services.ResourceExtractor.TempDir, "Assets", "Images", "Steam.jpg");
        var chapterImg = IOPath.Combine(Services.ResourceExtractor.TempDir, "Assets", "Images", $"Chapter {chapterNum}.png");
        iconBorder.Child =
            iconOverride is not null && File.Exists(iconOverride)
                ? new Image { Source = new BitmapImage(new Uri(iconOverride)), Stretch = Stretch.UniformToFill }
            : !isAuto && customIcon is not null && File.Exists(customIcon)
                ? (UIElement)new Image { Source = new BitmapImage(new Uri(customIcon)), Stretch = Stretch.UniformToFill }
            : isAuto && File.Exists(steamImg)
                ? new Image { Source = new BitmapImage(new Uri(steamImg)), Stretch = Stretch.UniformToFill }
            : !isAuto && File.Exists(chapterImg)
                ? new Image { Source = new BitmapImage(new Uri(chapterImg)), Stretch = Stretch.UniformToFill }
            : new TextBlock
            {
                FontFamily = new FontFamily("Segoe MDL2 Assets"),
                Text = isAuto ? "\uE774" : "\uE8E5",
                FontSize = 15, Foreground = new SolidColorBrush(Teal),
                HorizontalAlignment = HorizontalAlignment.Center,
                VerticalAlignment = VerticalAlignment.Center,
            };
        Grid.SetColumn(iconBorder, 0);
        grid.Children.Add(iconBorder);

        var info = new StackPanel { VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(6, 0, 0, 0) };
        info.Children.Add(new TextBlock
        {
            Text = name, FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 13, FontWeight = FontWeights.SemiBold,
            Foreground = new SolidColorBrush(Color.FromArgb(255, 210, 220, 230)),
        });
        info.Children.Add(new TextBlock
        {
            Text = subtitle, FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 10, Foreground = new SolidColorBrush(Color.FromArgb(255, 45, 90, 120)),
            TextTrimming = TextTrimming.CharacterEllipsis,
        });
        Grid.SetColumn(info, 1);
        grid.Children.Add(info);

        var right = new StackPanel
        {
            Orientation = Orientation.Horizontal, VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(8, 0, 0, 0),
        };

        if (isSelected)
        {
            right.Children.Add(new TextBlock
            {
                Text = Loc.Get("selected_label"), FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize = 11, Foreground = new SolidColorBrush(Teal), VerticalAlignment = VerticalAlignment.Center,
            });
        }
        else
        {
            var capExeSel = exePath; var capChSel = chapterNum; var capAutoSel = isAuto;
            var selBtn = MakeSmallButton(Loc.Get("select_btn"), Teal);
            selBtn.MinWidth = 100;
            selBtn.Click += (_, _) => { _store.SetSelected(capChSel, capAutoSel ? null : capExeSel); BuildInstallationsList(); RefreshInfo(); };
            right.Children.Add(selBtn);
        }

        if (!isAuto)
        {
            var capPathMove = exePath; var capChMove = chapterNum;

            Button MakeMoveBtn(string glyph, int direction, bool enabled)
            {
                var btn = new Button
                {
                    Width = 22, Height = 26,
                    Background = new SolidColorBrush(Color.FromArgb(40, 120, 130, 140)),
                    BorderThickness = new Thickness(0), Padding = new Thickness(0),
                    Margin = new Thickness(4, 0, 0, 0),
                    IsEnabled = enabled,
                    Opacity = enabled ? 1.0 : 0.25,
                    Content = new TextBlock
                    {
                        FontFamily = new FontFamily("Segoe MDL2 Assets"), Text = glyph,
                        FontSize = 10, Foreground = new SolidColorBrush(Color.FromArgb(220, 160, 180, 200)),
                        HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center,
                    },
                };
                ButtonHelper.SetCornerRadius(btn, new CornerRadius(3));
                btn.Click += (_, _) =>
                {
                    _store.MoveCustom(capChMove, capPathMove, direction);
                    BuildInstallationsList();
                };
                return btn;
            }

            right.Children.Add(MakeMoveBtn("", -1, !isFirst));
            right.Children.Add(MakeMoveBtn("", 1, !isLast));

            var capPathEdit = exePath; var capChEdit = chapterNum;

            var editBtn = new Button
            {
                Width = 26, Height = 26,
                Background = new SolidColorBrush(Color.FromArgb(40, 0, 140, 200)),
                BorderThickness = new Thickness(0), Padding = new Thickness(0),
                Margin = new Thickness(8, 0, 0, 0),
                Content = new TextBlock
                {
                    FontFamily = new FontFamily("Segoe MDL2 Assets"), Text = "\uE70F",
                    FontSize = 11, Foreground = new SolidColorBrush(Color.FromArgb(220, 80, 170, 230)),
                    HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center,
                },
            };
            ButtonHelper.SetCornerRadius(editBtn, new CornerRadius(3));

            editBtn.Click += (_, _) =>
            {
                var (saved, newName, newIconPath) = ShowEditInstallDialog(name, inst?.IconPath, capChEdit);
                if (saved)
                {
                    _store.UpdateCustom(capChEdit, capPathEdit, newName, newIconPath);
                    BuildInstallationsList();
                }
            };
            right.Children.Add(editBtn);

            var del = new Button
            {
                Width = 26, Height = 26,
                Background = new SolidColorBrush(Color.FromArgb(40, 180, 30, 30)),
                BorderThickness = new Thickness(0), Padding = new Thickness(0),
                Margin = new Thickness(4, 0, 0, 0),
                Content = new TextBlock
                {
                    Text = "×", FontSize = 14,
                    Foreground = new SolidColorBrush(Color.FromArgb(200, 200, 60, 60)),
                    HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center,
                },
            };
            ButtonHelper.SetCornerRadius(del, new CornerRadius(3));
            var capPath = exePath; var capCh = chapterNum;
            del.Click += (_, _) => { _store.RemoveCustom(capCh, capPath); BuildInstallationsList(); RefreshInfo(); };
            right.Children.Add(del);
        }

        Grid.SetColumn(right, 2);
        grid.Children.Add(right);

        var normalBg = new SolidColorBrush(isSelected ? Color.FromArgb(30, 0, 204, 170) : Color.FromArgb(12, 255, 255, 255));
        var hoverBg  = new SolidColorBrush(isSelected ? Color.FromArgb(50, 0, 204, 170) : Color.FromArgb(28, 255, 255, 255));
        var row = new Border
        {
            Background   = normalBg,
            CornerRadius = new CornerRadius(4),
            Padding      = new Thickness(10, 8, 10, 8),
            Child        = grid,
            Margin       = new Thickness(0, 0, 0, 2),
        };

        var capExe = exePath; var capChNum = chapterNum; var capAuto = isAuto;
        row.MouseEnter  += (_, _) => row.Background = hoverBg;
        row.MouseLeave  += (_, _) => row.Background = normalBg;
        row.MouseDown   += (_, _) => { _store.SetSelected(capChNum, capAuto ? null : capExe); BuildInstallationsList(); RefreshInfo(); };
        return row;
    }

    // Shows a modal edit-installation dialog; returns (saved, newName, newIconPath)
    private (bool saved, string name, string? iconPath) ShowEditInstallDialog(
        string currentName, string? currentIconPath, int chapterNum)
    {
        var iconPathHolder = new string?[] { currentIconPath };
        var nameBox = new TextBox
        {
            Text = currentName, MinWidth = 200,
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"), FontSize = 11,
            Background = new SolidColorBrush(Color.FromRgb(10, 20, 32)),
            Foreground = new SolidColorBrush(Color.FromRgb(200, 210, 220)),
            BorderBrush = new SolidColorBrush(Color.FromRgb(30, 60, 90)),
            CaretBrush = new SolidColorBrush(Color.FromRgb(0, 204, 170)),
        };
        var iconBtn = MakeSmallButton(Loc.Get("icon_btn"), Color.FromArgb(255, 80, 170, 230));
        iconBtn.Click += (_, _) =>
        {
            var picker = new OpenFileDialog { Filter = "Images|*.jpg;*.jpeg;*.png;*.bmp" };
            if (picker.ShowDialog(this) == true)
                iconPathHolder[0] = picker.FileName;
        };

        var panel = new StackPanel { MinWidth = 260 };
        panel.Children.Add(new TextBlock
        {
            Text = Loc.Get("edit_install_title"),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 10, FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(Color.FromArgb(180, 0, 204, 170)),
            Margin = new Thickness(0, 0, 0, 8),
        });
        panel.Children.Add(nameBox);
        panel.Children.Add(iconBtn);

        var result = WpfDialog.Show(this, Loc.Get("edit_install_title"), panel,
            primaryText: Loc.Get("save_btn"), closeText: Loc.Get("cancel") ?? "Cancel");

        if (result == WpfDialogResult.Primary)
            return (true, nameBox.Text.Trim().Length > 0 ? nameBox.Text.Trim() : currentName, iconPathHolder[0]);
        return (false, currentName, currentIconPath);
    }

    private static Button MakeSmallButton(string text, Color foreColor)
    {
        var btn = new Button
        {
            Height = 28, MinWidth = 80, Padding = new Thickness(8, 0, 8, 0),
            Background = new SolidColorBrush(Color.FromArgb(255, 8, 30, 55)),
            BorderBrush = new SolidColorBrush(Color.FromArgb(180, 0, 120, 100)),
            BorderThickness = new Thickness(1),
            Content = new TextBlock
            {
                Text = text, FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize = 10, FontWeight = FontWeights.Bold,
                Foreground = new SolidColorBrush(foreColor),
            },
        };
        ButtonHelper.SetCornerRadius(btn, new CornerRadius(3));
        return btn;
    }

    private FrameworkElement MakePresetRow(ChapterPreset preset, int chapterNum, bool isInstalled, bool isDownloading, string? currentSel = null)
    {
        var grid = new Grid();
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(46) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

        var iconBorder = new Border
        {
            Width = 34, Height = 34, CornerRadius = new CornerRadius(4),
            Background = new SolidColorBrush(Color.FromArgb(255, 14, 42, 78)),
            HorizontalAlignment = HorizontalAlignment.Left, VerticalAlignment = VerticalAlignment.Center,
        };
        var chapterImgPreset = IOPath.Combine(Services.ResourceExtractor.TempDir, "Assets", "Images", $"Chapter {chapterNum}.png");
        iconBorder.Child = File.Exists(chapterImgPreset)
            ? (UIElement)new Image { Source = new BitmapImage(new Uri(chapterImgPreset)), Stretch = Stretch.UniformToFill }
            : new TextBlock
            {
                Text = "S", FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize = 16, FontWeight = FontWeights.Bold,
                Foreground = new SolidColorBrush(Color.FromArgb(255, 100, 170, 220)),
                HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center,
            };
        Grid.SetColumn(iconBorder, 0);
        grid.Children.Add(iconBorder);

        var info = new StackPanel { VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(6, 0, 0, 0) };
        info.Children.Add(new TextBlock
        {
            Text = TranslatePresetName(preset.Name), FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 13, FontWeight = FontWeights.SemiBold,
            Foreground = new SolidColorBrush(Color.FromArgb(255, 200, 210, 225)),
        });
        info.Children.Add(new TextBlock
        {
            Text = preset.Command, FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 9, Foreground = new SolidColorBrush(Color.FromArgb(160, 60, 120, 160)),
        });
        Grid.SetColumn(info, 1);
        grid.Children.Add(info);

        var right = new StackPanel { VerticalAlignment = VerticalAlignment.Center, Margin = new Thickness(8, 0, 0, 0) };

        var installedExe = isInstalled
            ? _store.GetCustoms(chapterNum).FirstOrDefault(x => x.Name == preset.Name)?.ExePath
            : null;
        var isSelected = installedExe != null && installedExe == currentSel;

        if (isDownloading)
        {
            right.Children.Add(new TextBlock
            {
                Text = Loc.Get("downloading"), FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize = 10, Foreground = new SolidColorBrush(Color.FromArgb(255, 200, 160, 0)),
                VerticalAlignment = VerticalAlignment.Center,
            });
        }
        else if (isInstalled)
        {
            if (isSelected)
            {
                right.Children.Add(new TextBlock
                {
                    Text = Loc.Get("selected_label"), FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                    FontSize = 11, Foreground = new SolidColorBrush(Teal),
                    VerticalAlignment = VerticalAlignment.Center,
                });
            }
            else if (installedExe != null)
            {
                var selBtn = MakeSmallButton(Loc.Get("select_btn"), Teal);
                selBtn.MinWidth = 100;
                var capExe = installedExe; var capCh = chapterNum;
                selBtn.Click += (_, _) => { _store.SetSelected(capCh, capExe); BuildInstallationsList(); RefreshInfo(); };
                right.Children.Add(selBtn);
            }
            else
            {
                _store.UnmarkManifestInstalled(preset.ManifestId);
            }
        }
        else
        {
            var installLabel = string.IsNullOrEmpty(preset.DownloadSize)
                ? Loc.Get("install_btn")
                : Loc.Get("install_with_size", preset.DownloadSize);

            var installBtn = MakeSmallButton(installLabel, Teal);
            installBtn.MinWidth = 90;
            var capPreset = preset; var capCh = chapterNum;
            installBtn.Click += async (_, _) =>
            {
                try { await PickInstallModeAsync(capPreset, capCh); }
                catch (Exception ex) { ShowErrorAsync($"{Loc.Get("error_unexpected")}\n{ex.Message}"); }
            };
            right.Children.Add(installBtn);
        }

        Grid.SetColumn(right, 2);
        grid.Children.Add(right);

        var normalBgColor = isSelected
            ? Color.FromArgb(30, 0, 204, 170)
            : Color.FromArgb(18, 100, 160, 220);
        var rowBorder = new Border
        {
            Background   = new SolidColorBrush(normalBgColor),
            CornerRadius = isDownloading ? new CornerRadius(4, 4, 0, 0) : new CornerRadius(4),
            Padding      = new Thickness(10, 8, 10, 8),
            Child        = grid,
            Margin       = new Thickness(0, 0, 0, isDownloading ? 0 : 2),
        };

        if (!isDownloading) return rowBorder;

        // Live log area while downloading
        var existingLines = _downloadLogs.TryGetValue(preset.ManifestId, out var dl) ? dl : [];
        var logTb = new TextBlock
        {
            Text = existingLines.Count > 0
                ? string.Join("\n", existingLines.TakeLast(10))
                : Loc.Get("steamcmd_initializing"),
            FontFamily   = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize     = 9,
            Foreground   = new SolidColorBrush(Color.FromArgb(200, 0, 200, 150)),
            TextWrapping = TextWrapping.Wrap,
        };
        _downloadLogBlocks[preset.ManifestId] = logTb;

        var logScroll = new ScrollViewer
        {
            MaxHeight = 110,
            VerticalScrollBarVisibility   = ScrollBarVisibility.Auto,
            HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled,
            Content = logTb,
        };

        var logBorder = new Border
        {
            Background   = new SolidColorBrush(Color.FromArgb(25, 0, 200, 140)),
            CornerRadius = new CornerRadius(0, 0, 4, 4),
            Padding      = new Thickness(10, 6, 10, 6),
            Child        = logScroll,
            Margin       = new Thickness(0, 0, 0, 2),
        };

        var container = new StackPanel();
        container.Children.Add(rowBorder);
        container.Children.Add(logBorder);
        return container;
    }

    // ── Preset install flow (SteamCMD) ────────────────────────────────────────

    private async Task PickInstallModeAsync(ChapterPreset preset, int chapterNum)
    {
        var monoFont = new FontFamily("Cascadia Code, Consolas, Courier New");
        var dimColor = new SolidColorBrush(Color.FromArgb(160, 120, 160, 190));
        var tealDim  = new SolidColorBrush(Color.FromArgb(160, 0, 204, 170));

        var panel = new StackPanel { MinWidth = 400 };

        panel.Children.Add(new TextBlock
        {
            Text = Loc.Get("credentials_version_label"),
            FontFamily = monoFont, FontSize = 11,
            Foreground = dimColor,
            Margin = new Thickness(0, 0, 0, 6),
        });
        panel.Children.Add(new Border
        {
            Background   = new SolidColorBrush(Color.FromArgb(50, 0, 204, 170)),
            CornerRadius = new CornerRadius(4),
            Padding      = new Thickness(10, 6, 10, 6),
            Margin       = new Thickness(0, 0, 0, 16),
            Child        = new TextBlock
            {
                Text = preset.Command,
                FontFamily = monoFont, FontSize = 10,
                Foreground = new SolidColorBrush(Teal),
            },
        });

        panel.Children.Add(new TextBlock
        {
            Text = Loc.Get("install_mode_label"),
            FontFamily = monoFont, FontSize = 11,
            Foreground = dimColor,
            Margin = new Thickness(0, 0, 0, 10),
        });

        // AUTO description
        var autoDesc = new Border
        {
            Background   = new SolidColorBrush(Color.FromArgb(20, 0, 204, 170)),
            BorderBrush  = new SolidColorBrush(Color.FromArgb(50, 0, 204, 170)),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(3),
            Padding      = new Thickness(10, 8, 10, 8),
            Margin       = new Thickness(0, 0, 0, 6),
        };
        var autoDescStack = new StackPanel();
        autoDescStack.Children.Add(new TextBlock
        {
            Text = Loc.Get("install_mode_auto_title"),
            FontFamily = monoFont, FontSize = 11, FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(Teal),
        });
        autoDescStack.Children.Add(new TextBlock
        {
            Text = Loc.Get("install_mode_auto_desc"),
            FontFamily = monoFont, FontSize = 10,
            Foreground = dimColor,
            Margin = new Thickness(0, 3, 0, 0),
            TextWrapping = TextWrapping.Wrap,
        });
        autoDesc.Child = autoDescStack;
        panel.Children.Add(autoDesc);

        // MANUAL description
        var manualDesc = new Border
        {
            Background   = new SolidColorBrush(Color.FromArgb(20, 80, 130, 200)),
            BorderBrush  = new SolidColorBrush(Color.FromArgb(50, 80, 130, 200)),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(3),
            Padding      = new Thickness(10, 8, 10, 8),
        };
        var manualDescStack = new StackPanel();
        manualDescStack.Children.Add(new TextBlock
        {
            Text = Loc.Get("install_mode_manual_title"),
            FontFamily = monoFont, FontSize = 11, FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(Color.FromArgb(255, 80, 160, 230)),
        });
        manualDescStack.Children.Add(new TextBlock
        {
            Text = Loc.Get("install_mode_manual_desc"),
            FontFamily = monoFont, FontSize = 10,
            Foreground = dimColor,
            Margin = new Thickness(0, 3, 0, 0),
            TextWrapping = TextWrapping.Wrap,
        });
        manualDesc.Child = manualDescStack;
        panel.Children.Add(manualDesc);

        var result = await WpfDialog.ShowAsync(this,
            Loc.Get("install_dialog_title", TranslatePresetName(preset.Name)),
            panel,
            primaryText:   Loc.Get("install_mode_auto_btn"),
            secondaryText: Loc.Get("install_mode_manual_btn"),
            closeText:     Loc.Get("cancel"));

        if (result == WpfDialogResult.Primary)
            await StartPresetInstallAsync(preset, chapterNum);
        else if (result == WpfDialogResult.Secondary)
            await StartPresetInstallManualAsync(preset, chapterNum);
    }

    private async Task StartPresetInstallManualAsync(ChapterPreset preset, int chapterNum)
    {
        // Steam Console uses the Steam install's own steamapps/content folder
        var steamDir    = SteamCmdRunner.GetSteamInstallPath();
        var depotFolder = IOPath.Combine(
            steamDir ?? IOPath.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "Steam"),
            "steamapps", "content", $"app_{preset.AppId}", $"depot_{preset.DepotId}");
        Directory.CreateDirectory(depotFolder);

        var command  = $"download_depot {preset.AppId} {preset.DepotId} {preset.ManifestId}";
        var monoFont = new FontFamily("Cascadia Code, Consolas, Courier New");
        var dimColor = new SolidColorBrush(Color.FromArgb(200, 160, 200, 230));
        var tealDim  = new SolidColorBrush(Color.FromArgb(140, 0, 204, 170));

        // ── Build the instructions dialog with a checkbox-gated OK button ──────
        bool instrAccepted = false;
        await Task.Run(() => { }).ContinueWith(_ => { }, TaskScheduler.FromCurrentSynchronizationContext());

        var dlg = new Window
        {
            Owner                     = this,
            WindowStartupLocation     = WindowStartupLocation.CenterOwner,
            WindowStyle               = WindowStyle.None,
            ResizeMode                = ResizeMode.NoResize,
            SizeToContent             = SizeToContent.WidthAndHeight,
            ShowInTaskbar             = false,
            Background                = new SolidColorBrush(Color.FromRgb(9, 20, 30)),
        };

        // Steps list: (number, text, extra content or null)
        void AddStep(StackPanel parent, string num, string text, UIElement? extra = null)
        {
            var row = new Grid { Margin = new Thickness(0, 0, 0, 10) };
            row.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(22) });
            row.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            var numTb = new TextBlock
            {
                Text = num, FontFamily = monoFont, FontSize = 11, FontWeight = FontWeights.Bold,
                Foreground = new SolidColorBrush(Teal), VerticalAlignment = VerticalAlignment.Top,
            };
            Grid.SetColumn(numTb, 0);
            var col = new StackPanel();
            col.Children.Add(new TextBlock
            {
                Text = text, FontFamily = monoFont, FontSize = 11,
                Foreground = dimColor, TextWrapping = TextWrapping.Wrap,
            });
            if (extra != null) col.Children.Add(extra);
            Grid.SetColumn(col, 1);
            row.Children.Add(numTb);
            row.Children.Add(col);
            parent.Children.Add(row);
        }

        var steps = new StackPanel { MinWidth = 440, MaxWidth = 520 };

        // Step 1 — open Steam console
        AddStep(steps, "1.", Loc.Get("install_manual_step1"));

        // Step 2 — paste command
        AddStep(steps, "2.", Loc.Get("install_manual_step2"),
            new Border
            {
                Background = new SolidColorBrush(Color.FromArgb(40, 0, 204, 170)),
                BorderBrush = new SolidColorBrush(Color.FromArgb(60, 0, 204, 170)),
                BorderThickness = new Thickness(1), CornerRadius = new CornerRadius(3),
                Padding = new Thickness(10, 6, 10, 6), Margin = new Thickness(0, 6, 0, 0),
                Child = new TextBlock
                {
                    Text = command, FontFamily = monoFont, FontSize = 11,
                    Foreground = new SolidColorBrush(Teal), TextWrapping = TextWrapping.Wrap,
                },
            });

        // Step 3 — wait for download, folder info
        AddStep(steps, "3.", Loc.Get("install_manual_step3"),
            new Border
            {
                Background = new SolidColorBrush(Color.FromArgb(25, 80, 130, 200)),
                BorderBrush = new SolidColorBrush(Color.FromArgb(50, 80, 130, 200)),
                BorderThickness = new Thickness(1), CornerRadius = new CornerRadius(3),
                Padding = new Thickness(10, 6, 10, 6), Margin = new Thickness(0, 6, 0, 0),
                Child = new TextBlock
                {
                    Text = depotFolder, FontFamily = monoFont, FontSize = 9,
                    Foreground = new SolidColorBrush(Color.FromArgb(255, 80, 160, 230)),
                    TextWrapping = TextWrapping.Wrap,
                },
            });

        // Step 4 — move the downloaded folder
        AddStep(steps, "4.", Loc.Get("install_manual_step4"));

        // Step 5 — add manually via ADD button
        AddStep(steps, "5.", Loc.Get("install_manual_step5"));

        // Checkbox — gates the OK button
        var checkbox = new CheckBox
        {
            Content = new TextBlock
            {
                Text = Loc.Get("install_manual_confirm"),
                FontFamily = monoFont, FontSize = 11,
                Foreground = dimColor, TextWrapping = TextWrapping.Wrap,
            },
            Foreground = new SolidColorBrush(Teal),
            Margin = new Thickness(0, 6, 0, 0),
            Cursor = Cursors.Hand,
        };
        steps.Children.Add(checkbox);

        // ── Dialog layout (title bar + scroll content + footer) ──
        var okBtn = new Button
        {
            Height = 36, MinWidth = 160, IsEnabled = false,
            Background = new SolidColorBrush(Color.FromRgb(0, 80, 60)),
            BorderBrush = new SolidColorBrush(Color.FromArgb(180, 0, 180, 130)),
            BorderThickness = new Thickness(1), Padding = new Thickness(12, 0, 12, 0),
            Content = new TextBlock
            {
                Text = Loc.Get("install_manual_open_btn"),
                FontFamily = monoFont, FontSize = 11, FontWeight = FontWeights.Bold,
                Foreground = new SolidColorBrush(Color.FromRgb(0, 204, 170)),
            },
            Cursor = Cursors.Hand,
        };
        ButtonHelper.SetCornerRadius(okBtn, new CornerRadius(3));

        var cancelBtn = new Button
        {
            Height = 36, MinWidth = 100, Margin = new Thickness(0, 0, 8, 0),
            Background = new SolidColorBrush(Color.FromRgb(10, 24, 37)),
            BorderBrush = new SolidColorBrush(Color.FromRgb(13, 37, 53)),
            BorderThickness = new Thickness(1), Padding = new Thickness(12, 0, 12, 0),
            Content = new TextBlock
            {
                Text = Loc.Get("cancel"),
                FontFamily = monoFont, FontSize = 11, FontWeight = FontWeights.Bold,
                Foreground = new SolidColorBrush(Color.FromArgb(200, 58, 106, 138)),
            },
            Cursor = Cursors.Hand,
        };
        ButtonHelper.SetCornerRadius(cancelBtn, new CornerRadius(3));

        checkbox.Checked   += (_, _) => okBtn.IsEnabled = true;
        checkbox.Unchecked += (_, _) => okBtn.IsEnabled = false;
        okBtn.Click     += (_, _) => { instrAccepted = true; dlg.Close(); };
        cancelBtn.Click += (_, _) => dlg.Close();

        var titleBar = new Border
        {
            Background = new SolidColorBrush(Color.FromRgb(6, 15, 24)),
            BorderBrush = new SolidColorBrush(Color.FromRgb(13, 32, 48)),
            BorderThickness = new Thickness(0, 0, 0, 1), Height = 48,
            Padding = new Thickness(20, 0, 20, 0),
            Child = new TextBlock
            {
                Text = Loc.Get("install_manual_title"),
                FontFamily = monoFont, FontSize = 14, FontWeight = FontWeights.Bold,
                Foreground = new SolidColorBrush(Color.FromRgb(0, 204, 170)),
                VerticalAlignment = VerticalAlignment.Center,
            },
        };

        var footer = new Border
        {
            Background = new SolidColorBrush(Color.FromRgb(6, 15, 24)),
            BorderBrush = new SolidColorBrush(Color.FromRgb(13, 32, 48)),
            BorderThickness = new Thickness(0, 1, 0, 0), Height = 52,
            Padding = new Thickness(14, 0, 14, 0),
            Child = new StackPanel
            {
                Orientation = Orientation.Horizontal,
                HorizontalAlignment = HorizontalAlignment.Right,
                VerticalAlignment = VerticalAlignment.Center,
                Children = { cancelBtn, okBtn },
            },
        };

        var contentBorder = new Border { Padding = new Thickness(20, 16, 20, 16), Child = steps };

        var mainGrid = new Grid();
        mainGrid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        mainGrid.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
        mainGrid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        Grid.SetRow(titleBar, 0);
        Grid.SetRow(contentBorder, 1);
        Grid.SetRow(footer, 2);
        mainGrid.Children.Add(titleBar);
        mainGrid.Children.Add(contentBorder);
        mainGrid.Children.Add(footer);

        dlg.Content = new Border
        {
            BorderBrush = new SolidColorBrush(Color.FromRgb(21, 48, 72)),
            BorderThickness = new Thickness(1),
            Child = mainGrid,
        };

        dlg.ShowDialog();
        if (!instrAccepted) return;

        // Copy command to clipboard and open Steam Console + the depot folder
        try { Clipboard.SetText(command); } catch { }
        Process.Start(new ProcessStartInfo { FileName = "steam://open/console", UseShellExecute = true });
        Process.Start(new ProcessStartInfo { FileName = "explorer.exe", Arguments = $"\"{depotFolder}\"" });

        // "Continue when done" dialog
        var waitPanel = new StackPanel { MinWidth = 360 };
        waitPanel.Children.Add(new TextBlock
        {
            Text = Loc.Get("install_manual_waiting"),
            FontFamily   = monoFont, FontSize = 11,
            Foreground   = dimColor,
            TextWrapping = TextWrapping.Wrap,
        });

        var waitResult = await WpfDialog.ShowAsync(this,
            Loc.Get("install_manual_title"), waitPanel,
            primaryText: Loc.Get("steamguard_phone_continue"),
            closeText:   Loc.Get("cancel"));

        if (waitResult != WpfDialogResult.Primary) return;

        var depotPath = SteamDetector.FindDepotDownloadPath(preset.AppId, preset.DepotId, null);
        if (depotPath is null)
        {
            var notFoundContent = new TextBlock
            {
                Text = Loc.Get("files_not_found_content"),
                TextWrapping = TextWrapping.Wrap,
                FontFamily   = monoFont, FontSize = 12,
                Foreground   = new SolidColorBrush(Color.FromArgb(255, 160, 180, 200)),
            };
            var notFoundResult = WpfDialog.Show(this,
                Loc.Get("files_not_found_title"), notFoundContent,
                primaryText: Loc.Get("select_folder_manually"),
                closeText:   Loc.Get("close"));

            if (notFoundResult != WpfDialogResult.Primary) return;

            var picker = new OpenFolderDialog();
            if (picker.ShowDialog(this) != true) return;
            depotPath = picker.FolderName;
        }

        try { await MoveAndRegisterAsync(preset, chapterNum, depotPath); }
        catch (Exception ex) { ShowErrorAsync($"{Loc.Get("error_register")}\n{ex.Message}"); }
    }

    private async Task StartPresetInstallAsync(ChapterPreset preset, int chapterNum)
    {
        var steamcmdPath = SteamCmdRunner.Find() ?? await AcquireSteamCmdAsync();
        if (steamcmdPath is null) return;

        var suggestedUser = SteamCmdRunner.GetLoggedInUsername() ?? _store.GetSteamUsername() ?? "";
        var creds = await PromptCredentialsAsync(suggestedUser, preset);
        if (creds is null) return;
        var (username, password) = creds.Value;
        _store.SetSteamUsername(username);

        SteamCmdRunner.CopyCredentials(steamcmdPath);

        var cts = new CancellationTokenSource();
        _activePolls[preset.ManifestId] = cts;
        _downloadLogs[preset.ManifestId] = [$"[ 00:00 ]  {Loc.Get("steamcmd_in_progress")}"];
        _discordPresence.SetInstalling(_chapters[chapterNum - 1], preset.Name);
        BuildInstallationsList();

        var downloadStart = DateTime.Now;
        var manifestId    = preset.ManifestId;

        void RefreshLogBlock()
        {
            if (!_downloadLogBlocks.TryGetValue(manifestId, out var tb)) return;
            var lines = _downloadLogs.TryGetValue(manifestId, out var l) ? l : [];
            tb.Text = string.Join("\n", lines.TakeLast(12));
            if (tb.Parent is ScrollViewer sv)
                sv.ScrollToVerticalOffset(sv.ScrollableHeight);
        }

        var ticker = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1) };
        ticker.Tick += (_, _) =>
        {
            if (!_downloadLogs.TryGetValue(manifestId, out var lines)) return;
            var elapsed = DateTime.Now - downloadStart;
            lines[0] = $"[ {elapsed:mm\\:ss} ]  {Loc.Get("steamcmd_in_progress")}";
            RefreshLogBlock();
        };
        ticker.Start();

        // Watch steamapps/content for new files and folders created by SteamCMD
        var contentPath = IOPath.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "steamcmd", "steamapps", "content");
        Directory.CreateDirectory(contentPath);

        var contentWatcher = new System.IO.FileSystemWatcher(contentPath)
        {
            IncludeSubdirectories = true,
            NotifyFilter = System.IO.NotifyFilters.FileName | System.IO.NotifyFilters.DirectoryName,
        };
        contentWatcher.Created += (_, e) =>
        {
            var rel   = IOPath.GetRelativePath(contentPath, e.FullPath);
            var isDir = Directory.Exists(e.FullPath);
            var tag   = isDir ? "[DIR] " : "[FILE]";
            Dispatcher.Invoke(() =>
            {
                if (!_downloadLogs.TryGetValue(manifestId, out var lines)) return;
                lines.Add($"{tag}  {rel}");
                RefreshLogBlock();
            });
        };
        contentWatcher.EnableRaisingEvents = true;

        Task<string?> GuardPrompt(string _)
        {
            string? code = null;
            Dispatcher.Invoke(() =>
            {
                var panel = new StackPanel { MinWidth = 300 };
                panel.Children.Add(new TextBlock
                {
                    Text = Loc.Get("steamguard_code_label"),
                    FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"), FontSize = 11,
                    Foreground = new SolidColorBrush(Color.FromArgb(200, 160, 190, 220)),
                    TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 0, 0, 8),
                });
                var tb = new TextBox
                {
                    FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                    Background = new SolidColorBrush(Color.FromRgb(10, 20, 32)),
                    Foreground = new SolidColorBrush(Color.FromRgb(200, 210, 220)),
                    BorderBrush = new SolidColorBrush(Color.FromRgb(30, 60, 90)),
                    CaretBrush = new SolidColorBrush(Color.FromRgb(0, 204, 170)),
                };
                panel.Children.Add(tb);
                var result = WpfDialog.Show(this, Loc.Get("steamguard_title"), panel,
                    primaryText: Loc.Get("understood"), closeText: Loc.Get("cancel"));
                if (result == WpfDialogResult.Primary)
                    code = tb.Text.Trim();
            });
            return Task.FromResult(code);
        }

        var progress = new Progress<string>(_ => { });

        var runTask = SteamCmdRunner.RunAsync(steamcmdPath, username, password,
            preset.AppId, preset.DepotId, preset.ManifestId,
            progress, GuardPrompt, cts.Token);

        if (!string.IsNullOrEmpty(password))
        {
            var infoPanel = new StackPanel { MinWidth = 320 };
            infoPanel.Children.Add(new TextBlock
            {
                Text = Loc.Get("steamguard_phone_body"),
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"), FontSize = 11,
                Foreground = new SolidColorBrush(Color.FromArgb(220, 160, 200, 230)),
                TextWrapping = TextWrapping.Wrap,
            });
            await WpfDialog.ShowAsync(this, Loc.Get("steamguard_phone_title"), infoPanel,
                primaryText: Loc.Get("steamguard_phone_continue"));
        }

        try
        {
            await runTask;
        }
        catch (OperationCanceledException) { }
        catch (Exception ex) { ShowErrorAsync($"{Loc.Get("error_steamcmd_msg")}\n{ex.Message}"); }
        finally
        {
            ticker.Stop();
            contentWatcher.EnableRaisingEvents = false;
            contentWatcher.Dispose();
            _activePolls.Remove(preset.ManifestId);
            _downloadLogBlocks.Remove(preset.ManifestId);
            _discordPresence.SetChapterSelected(_chapters[chapterNum - 1], GetVersionLabel(_chapters[chapterNum - 1]));
            BuildInstallationsList();
        }

        if (cts.IsCancellationRequested) return;

        var depotPath = SteamDetector.FindDepotDownloadPath(preset.AppId, preset.DepotId, steamcmdPath);
        if (depotPath is null)
        {
            var notFoundContent = new TextBlock
            {
                Text = Loc.Get("files_not_found_content"),
                TextWrapping = TextWrapping.Wrap,
                FontFamily   = new FontFamily("Cascadia Code, Consolas, Courier New"), FontSize = 12,
                Foreground   = new SolidColorBrush(Color.FromArgb(255, 160, 180, 200)),
            };

            var notFoundResult = WpfDialog.Show(this,
                Loc.Get("files_not_found_title"), notFoundContent,
                primaryText: Loc.Get("select_folder_manually"),
                closeText:   Loc.Get("close"));

            if (notFoundResult != WpfDialogResult.Primary) return;

            var picker = new OpenFolderDialog();
            if (picker.ShowDialog(this) != true) return;
            depotPath = picker.FolderName;
        }

        try { await MoveAndRegisterAsync(preset, chapterNum, depotPath); }
        catch (Exception ex) { ShowErrorAsync($"{Loc.Get("error_register")}\n{ex.Message}"); }
    }

    private async Task<string?> AcquireSteamCmdAsync()
    {
        var content = new TextBlock
        {
            Text = Loc.Get("steamcmd_not_found_content"),
            TextWrapping = TextWrapping.Wrap,
            FontFamily   = new FontFamily("Cascadia Code, Consolas, Courier New"), FontSize = 12,
            Foreground   = new SolidColorBrush(Color.FromArgb(255, 160, 180, 200)),
        };

        var result = WpfDialog.Show(this,
            Loc.Get("steamcmd_not_found_title"), content,
            primaryText:   Loc.Get("steamcmd_download_auto"),
            secondaryText: Loc.Get("steamcmd_find"),
            closeText:     Loc.Get("cancel"));

        if (result == WpfDialogResult.Primary)
        {
            try
            {
                var progressLabel = new TextBlock
                {
                    Text = Loc.Get("starting"),
                    FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"), FontSize = 12,
                    Foreground = new SolidColorBrush(Teal),
                };
                var progressDlg = new WpfDialog(this, Loc.Get("downloading_steamcmd_title"), progressLabel);
                progressDlg.Show();
                var path = await SteamCmdRunner.DownloadAsync(new Progress<string>(msg => progressLabel.Text = msg));
                progressDlg.Close();
                return path;
            }
            catch (Exception ex) { ShowErrorAsync($"{Loc.Get("error_download_steamcmd")}\n{ex.Message}"); return null; }
        }
        else if (result == WpfDialogResult.Secondary)
        {
            var picker = new OpenFileDialog { Filter = "steamcmd.exe|steamcmd.exe|Executables|*.exe" };
            if (picker.ShowDialog(this) == true) return picker.FileName;
        }

        return null;
    }

    private async Task<(string username, string? password)?> PromptCredentialsAsync(
        string suggestedUser, ChapterPreset preset)
    {
        var mono   = new FontFamily("Cascadia Code, Consolas, Courier New");
        var dimFg  = new SolidColorBrush(Color.FromArgb(120, 150, 180, 200));
        var inputBg = new SolidColorBrush(Color.FromRgb(10, 20, 32));
        var inputFg = new SolidColorBrush(Color.FromRgb(200, 210, 220));
        var inputBd = new SolidColorBrush(Color.FromRgb(30, 60, 90));

        // ── Input controls ──────────────────────────────────────────────────
        var userTb = new TextBox
        {
            Text        = suggestedUser,
            FontFamily  = mono,
            Background  = inputBg, Foreground = inputFg,
            BorderBrush = inputBd,
            CaretBrush  = new SolidColorBrush(Teal),
        };
        var passPb = new PasswordBox
        {
            FontFamily = mono, Background = inputBg,
            Foreground = inputFg, BorderBrush = inputBd,
        };
        var saveCb = new CheckBox
        {
            Content     = Loc.Get("save_password"),
            FontFamily  = mono, FontSize = 10,
            Foreground  = new SolidColorBrush(Color.FromArgb(180, 150, 180, 200)),
            Margin      = new Thickness(0, 10, 0, 0),
            IsChecked   = false,
        };

        // ── Manual-entry section (always built, hidden when account selected) ─
        var manualSection = new StackPanel();
        manualSection.Children.Add(new TextBlock
        {
            Text = Loc.Get("username_placeholder") ?? "Username",
            Foreground = dimFg, FontSize = 9, FontFamily = mono,
            Margin = new Thickness(0, 0, 0, 2),
        });
        manualSection.Children.Add(userTb);
        manualSection.Children.Add(new TextBlock
        {
            Text = Loc.Get("password_placeholder") ?? "Password",
            Foreground = dimFg, FontSize = 9, FontFamily = mono,
            Margin = new Thickness(0, 8, 0, 2),
        });
        manualSection.Children.Add(passPb);
        manualSection.Children.Add(saveCb);

        // ── Main panel ───────────────────────────────────────────────────────
        var panel = new StackPanel { MaxWidth = 460 };
        panel.Children.Add(new TextBlock
        {
            Text       = Loc.Get("credentials_version_label"),
            FontFamily = mono, FontSize = 11,
            Foreground = new SolidColorBrush(Color.FromArgb(160, 120, 160, 190)),
            Margin     = new Thickness(0, 0, 0, 6),
        });
        panel.Children.Add(new Border
        {
            Background   = new SolidColorBrush(Color.FromArgb(50, 0, 204, 170)),
            CornerRadius = new CornerRadius(4), Padding = new Thickness(10, 5, 10, 5),
            Margin       = new Thickness(0, 0, 0, 12),
            Child        = new TextBlock
            {
                Text       = preset.Command,
                FontFamily = mono, FontSize = 10,
                Foreground = new SolidColorBrush(Teal),
            },
        });

        // ── Saved-account banners ────────────────────────────────────────────
        Services.SavedAccount? selectedAccount = null;

        var savedAccounts = Services.SteamCredentialStore.LoadAll();
        if (savedAccounts.Count > 0)
        {
            panel.Children.Add(new TextBlock
            {
                Text       = Loc.Get("saved_accounts") ?? "SAVED ACCOUNTS",
                FontFamily = mono, FontSize = 8, FontWeight = FontWeights.Bold,
                Foreground = new SolidColorBrush(Color.FromArgb(140, 0, 204, 170)),
                Margin     = new Thickness(0, 0, 0, 6),
            });

            Border? activeBanner = null;
            var accountsWrap = new StackPanel { Margin = new Thickness(0, 0, 0, 10) };

            foreach (var acct in savedAccounts)
            {
                var acc = acct;
                var nameTb = new TextBlock
                {
                    Text       = acc.Username,
                    FontFamily = mono, FontSize = 11, FontWeight = FontWeights.Bold,
                    Foreground = new SolidColorBrush(Color.FromRgb(180, 210, 230)),
                    VerticalAlignment = VerticalAlignment.Center,
                };
                var banner = new Border
                {
                    Background      = new SolidColorBrush(Color.FromArgb(30, 10, 40, 60)),
                    BorderBrush     = new SolidColorBrush(Color.FromArgb(80, 30, 60, 90)),
                    BorderThickness = new Thickness(1),
                    CornerRadius    = new CornerRadius(4),
                    Padding         = new Thickness(12, 8, 12, 8),
                    Margin          = new Thickness(0, 0, 0, 4),
                    Cursor          = Cursors.Hand,
                    Child           = new StackPanel
                    {
                        Orientation = Orientation.Horizontal,
                        Children    =
                        {
                            new TextBlock
                            {
                                Text       = "",
                                FontFamily = new FontFamily("Segoe MDL2 Assets"),
                                FontSize   = 14,
                                Foreground = new SolidColorBrush(Color.FromArgb(120, 0, 204, 170)),
                                VerticalAlignment = VerticalAlignment.Center,
                                Margin     = new Thickness(0, 0, 10, 0),
                            },
                            nameTb,
                        },
                    },
                };

                banner.MouseEnter += (_, _) =>
                {
                    if (activeBanner != banner)
                        banner.Background = new SolidColorBrush(Color.FromArgb(50, 0, 80, 100));
                };
                banner.MouseLeave += (_, _) =>
                {
                    if (activeBanner != banner)
                        banner.Background = new SolidColorBrush(Color.FromArgb(30, 10, 40, 60));
                };
                banner.MouseDown += (_, _) =>
                {
                    if (activeBanner != null)
                    {
                        activeBanner.BorderBrush = new SolidColorBrush(Color.FromArgb(80, 30, 60, 90));
                        activeBanner.Background  = new SolidColorBrush(Color.FromArgb(30, 10, 40, 60));
                    }
                    selectedAccount          = acc;
                    activeBanner             = banner;
                    banner.BorderBrush       = new SolidColorBrush(Color.FromArgb(220, 0, 204, 170));
                    banner.Background        = new SolidColorBrush(Color.FromArgb(40, 0, 204, 170));
                    manualSection.Visibility = Visibility.Collapsed;
                };

                accountsWrap.Children.Add(banner);
            }

            // Pre-select the first account
            if (savedAccounts.Count > 0)
            {
                var firstBanner = (Border)accountsWrap.Children[0];
                selectedAccount            = savedAccounts[0];
                activeBanner               = firstBanner;
                firstBanner.BorderBrush    = new SolidColorBrush(Color.FromArgb(220, 0, 204, 170));
                firstBanner.Background     = new SolidColorBrush(Color.FromArgb(40, 0, 204, 170));
                manualSection.Visibility   = Visibility.Collapsed;
            }

            panel.Children.Add(accountsWrap);

            // "Use another account" button
            var useAnotherBtn = new Button
            {
                Content         = Loc.Get("use_another_account"),
                FontFamily      = mono, FontSize = 9,
                Background      = new SolidColorBrush(Color.FromArgb(20, 255, 255, 255)),
                BorderBrush     = new SolidColorBrush(Color.FromArgb(50, 100, 140, 160)),
                BorderThickness = new Thickness(1),
                Foreground      = new SolidColorBrush(Color.FromArgb(160, 100, 160, 200)),
                Padding         = new Thickness(10, 4, 10, 4),
                Cursor          = Cursors.Hand,
                Margin          = new Thickness(0, 0, 0, 12),
                HorizontalAlignment = HorizontalAlignment.Left,
            };
            ButtonHelper.SetCornerRadius(useAnotherBtn, new CornerRadius(3));
            useAnotherBtn.Click += (_, _) =>
            {
                selectedAccount          = null;
                activeBanner             = null;
                userTb.Text              = suggestedUser;
                passPb.Password          = "";
                manualSection.Visibility = Visibility.Visible;
            };
            panel.Children.Add(useAnotherBtn);
        }

        panel.Children.Add(manualSection);

        var result = await WpfDialog.ShowAsync(this,
            Loc.Get("install_dialog_title", TranslatePresetName(preset.Name)), panel,
            primaryText: Loc.Get("start_download"),
            closeText:   Loc.Get("cancel"));

        if (result != WpfDialogResult.Primary) return null;

        // Use saved account
        if (selectedAccount != null)
        {
            var pass = Services.SteamCredentialStore.Decrypt(selectedAccount);
            return (selectedAccount.Username, pass);
        }

        // Use manual entry
        var user = userTb.Text.Trim();
        if (string.IsNullOrEmpty(user)) return null;
        var password = passPb.Password.Length > 0 ? passPb.Password : null;

        if (saveCb.IsChecked == true && password != null)
            Services.SteamCredentialStore.Save(user, password);

        return (user, password);
    }

    private async Task MoveAndRegisterAsync(ChapterPreset preset, int chapterNum, string depotPath)
    {
        var iconPathHolder = new string?[] { null };

        var nameBox = new TextBox
        {
            Text = TranslatePresetName(preset.Name),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            Background = new SolidColorBrush(Color.FromRgb(10, 20, 32)),
            Foreground = new SolidColorBrush(Color.FromRgb(200, 210, 220)),
            BorderBrush = new SolidColorBrush(Color.FromRgb(30, 60, 90)),
            CaretBrush = new SolidColorBrush(Color.FromRgb(0, 204, 170)),
        };

        var iconPreview = new Border
        {
            Width = 40, Height = 40, CornerRadius = new CornerRadius(4),
            Background = new SolidColorBrush(Color.FromArgb(255, 20, 38, 55)),
        };
        var defaultChapterImg = IOPath.Combine(Services.ResourceExtractor.TempDir, "Assets", "Images", $"Chapter {chapterNum}.png");
        if (File.Exists(defaultChapterImg))
            iconPreview.Child = new Image { Source = new BitmapImage(new Uri(defaultChapterImg)), Stretch = Stretch.UniformToFill };

        var iconBtn = MakeSmallButton(Loc.Get("choose_icon"), Color.FromArgb(255, 80, 170, 230));
        iconBtn.Height = 40;
        iconBtn.Click += (_, _) =>
        {
            var ip = new OpenFileDialog { Filter = "Images|*.jpg;*.jpeg;*.png;*.bmp" };
            if (ip.ShowDialog(this) != true) return;
            iconPathHolder[0] = ip.FileName;
            iconPreview.Child = new Image { Source = new BitmapImage(new Uri(ip.FileName)), Stretch = Stretch.UniformToFill };
        };

        var iconRow = new StackPanel { Orientation = Orientation.Horizontal, VerticalAlignment = VerticalAlignment.Center };
        iconRow.Children.Add(iconPreview);
        iconBtn.Margin = new Thickness(8, 0, 0, 0);
        iconRow.Children.Add(iconBtn);

        var panel = new StackPanel { MinWidth = 340 };
        panel.Children.Add(new TextBlock
        {
            Text = Loc.Get("files_downloaded_msg"), TextWrapping = TextWrapping.Wrap,
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"), FontSize = 11,
            Foreground = new SolidColorBrush(Color.FromArgb(200, 140, 170, 200)),
            Margin = new Thickness(0, 0, 0, 8),
        });
        panel.Children.Add(new TextBlock
        {
            Text = Loc.Get("name_label"), FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 9, FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(Color.FromArgb(160, 0, 204, 170)),
            Margin = new Thickness(0, 0, 0, 4),
        });
        panel.Children.Add(nameBox);
        panel.Children.Add(new TextBlock
        {
            Text = Loc.Get("icon_label"), FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 9, FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(Color.FromArgb(160, 0, 204, 170)),
            Margin = new Thickness(0, 8, 0, 4),
        });
        panel.Children.Add(iconRow);

        var moveResult = await WpfDialog.ShowAsync(this,
            Loc.Get("download_ready_title", TranslatePresetName(preset.Name)), panel,
            primaryText:   Loc.Get("select_folder"),
            secondaryText: Loc.Get("later"));
        if (moveResult != WpfDialogResult.Primary) return;

        var customName = nameBox.Text.Trim();
        if (string.IsNullOrEmpty(customName)) customName = preset.Name;

        var folderDlg = new OpenFolderDialog();
        if (folderDlg.ShowDialog(this) != true) return;
        var folderPath = folderDlg.FolderName;

        _store.SetPreferredPath(chapterNum, folderPath);
        var safeName = string.Concat(customName.Select(
            c => IOPath.GetInvalidFileNameChars().Contains(c) ? '_' : c));
        var destPath = IOPath.Combine(folderPath, safeName);

        var progressLabel = new TextBlock
        {
            Text = Loc.Get("moving_files"),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"), FontSize = 12,
            Foreground = new SolidColorBrush(Teal), TextWrapping = TextWrapping.Wrap,
        };
        var progressBar = new ProgressBar { IsIndeterminate = true, Margin = new Thickness(0, 10, 0, 0) };
        var progressPanel = new StackPanel { MinWidth = 300 };
        progressPanel.Children.Add(progressLabel);
        progressPanel.Children.Add(progressBar);

        var progressDlg = new WpfDialog(this, Loc.Get("preparing_version_title"), progressPanel);
        progressDlg.Show();

        try
        {
            await Task.Run(() =>
            {
                Dispatcher.BeginInvoke(new Action(() => progressLabel.Text = Loc.Get("moving_files_dest")));
                MoveDirectory(depotPath, destPath);
                Dispatcher.BeginInvoke(new Action(() => progressLabel.Text = Loc.Get("cleaning_temp")));
                try
                {
                    var appDir = IOPath.GetDirectoryName(depotPath);
                    if (appDir != null && Directory.Exists(appDir))
                        Directory.Delete(appDir, recursive: true);
                }
                catch { }
                Dispatcher.BeginInvoke(new Action(() => progressLabel.Text = Loc.Get("registering")));
            });

            progressDlg.Close();

            var exe = SteamDetector.FindGameExe(destPath);
            if (exe is null) { ShowErrorAsync($"{Loc.Get("error_no_exe")}\n{destPath}"); return; }

            _store.AddCustom(chapterNum, customName, exe);
            _store.MarkManifestInstalled(preset.ManifestId);
            if (iconPathHolder[0] is not null)
                _store.UpdateCustom(chapterNum, exe, customName, iconPathHolder[0]);

            BuildInstallationsList();
            RefreshInfo();
        }
        catch (Exception ex) { progressDlg.Close(); ShowErrorAsync($"{Loc.Get("error_move")}\n{ex.Message}"); }
    }

    private static void MoveDirectory(string src, string dst)
    {
        Directory.CreateDirectory(dst);
        foreach (var file in Directory.GetFiles(src, "*", SearchOption.AllDirectories))
        {
            var rel  = IOPath.GetRelativePath(src, file);
            var dst2 = IOPath.Combine(dst, rel);
            Directory.CreateDirectory(IOPath.GetDirectoryName(dst2)!);
            File.Copy(file, dst2, overwrite: true);
        }
        Directory.Delete(src, recursive: true);
    }

    private Task ShowSteamGuardPopupAsync()
    {
        try
        {
            var content = new TextBlock
            {
                Text = Loc.Get("steamguard_content"),
                TextWrapping = TextWrapping.Wrap,
                FontFamily   = new FontFamily("Cascadia Code, Consolas, Courier New"), FontSize = 12,
                Foreground   = new SolidColorBrush(Color.FromArgb(255, 160, 190, 220)),
            };
            WpfDialog.Show(this, Loc.Get("steamguard_title"), content,
                closeText: Loc.Get("understood"));
        }
        catch { }
        return Task.CompletedTask;
    }

    private void ShowErrorAsync(string message)
    {
        try
        {
            var content = new TextBlock { Text = message, TextWrapping = TextWrapping.Wrap };
            WpfDialog.Show(this, Loc.Get("error_title"), content,
                closeText: Loc.Get("close"));
        }
        catch { }
    }

    // ── Overlay buttons ───────────────────────────────────────────────────────

    private void PrevChapterBtn_Click(object sender, RoutedEventArgs e)
    {
        _versionsChapter = (_versionsChapter - 1 + _chapters.Count) % _chapters.Count;
        BuildInstallationsList();
    }

    private void NextChapterBtn_Click(object sender, RoutedEventArgs e)
    {
        _versionsChapter = (_versionsChapter + 1) % _chapters.Count;
        BuildInstallationsList();
    }

    private void AddInstallBtn_Click(object sender, RoutedEventArgs e)
    {
        var picker = new OpenFileDialog { Filter = "Executables|*.exe" };
        if (picker.ShowDialog(this) != true) return;

        _store.AddCustom(_chapters[_versionsChapter].Number,
            IOPath.GetFileNameWithoutExtension(picker.FileName), picker.FileName);
        BuildInstallationsList();
    }

    private void TogglePresetsBtn_Click(object sender, RoutedEventArgs e)
    {
        _hidePresetRows = !_hidePresetRows;
        BuildInstallationsList();
    }

    private void CloseVersionsBtn_Click(object sender, RoutedEventArgs e) =>
        VersionsOverlay.Visibility = Visibility.Collapsed;

    // ── Platform toggle (Steam ↔ Epic Games) ──────────────────────────────────

    private void PlatformToggleBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_epicService.IsEnabled)
        {
            _epicService.SetEnabled(false);
            RefreshPlatformButton();
            RefreshInfo();
            return;
        }

        // Switching to Epic — ensure we have a valid base path.
        var basePath = _epicService.BasePath;

        if (basePath == null || !_epicService.HasExeForAnyChapter())
        {
            basePath = _epicService.TryAutoDetect();

            if (basePath == null)
            {
                var content = new TextBlock
                {
                    Text         = Loc.Get("epic_not_found_content"),
                    FontFamily   = new FontFamily("Cascadia Code, Consolas, Courier New"),
                    FontSize     = 11,
                    Foreground   = new SolidColorBrush(Color.FromArgb(220, 160, 190, 220)),
                    TextWrapping = TextWrapping.Wrap,
                    MaxWidth     = 380,
                };

                var result = WpfDialog.Show(this, "Epic Games",
                    content,
                    primaryText: Loc.Get("select_folder"),
                    closeText:   Loc.Get("cancel"));

                if (result != WpfDialogResult.Primary) return;

                var picker = new OpenFolderDialog();
                if (picker.ShowDialog(this) != true) return;

                // Accept PoppyPlaytimeChapterOne directly OR its parent folder
                var picked = picker.FolderName;
                var sub    = IOPath.Combine(picked, "PoppyPlaytimeChapterOne");
                basePath = Directory.Exists(sub) ? sub : picked;
            }

            _epicService.SetBasePath(basePath);
        }

        if (!_epicService.HasExeForAnyChapter())
        {
            ShowErrorAsync(Loc.Get("epic_no_exe_error"));
            return;
        }

        _epicService.SetEnabled(true);
        RefreshPlatformButton();
        RefreshInfo();
    }

    // ── Main buttons ──────────────────────────────────────────────────────────

    private void HandModsMenuButton_Click(object sender, RoutedEventArgs e) => OpenHandModsHub();

    private void LeaderboardButton_Click(object sender, RoutedEventArgs e)
    {
        if (_leaderboardOverlay is { IsVisible: true })
        {
            _leaderboardOverlay.Close();
        }
        else
        {
            _leaderboardOverlay = new LeaderboardOverlay();
            _leaderboardOverlay.Closed += (_, _) => _leaderboardOverlay = null;
            _leaderboardOverlay.Show();
            _leaderboardOverlay.Activate();
        }
    }

    private void PlayButton_Click(object sender, RoutedEventArgs e)
    {
        var ch  = _chapters[_selected];
        var exe = GetActiveExePath(ch);
        if (string.IsNullOrEmpty(exe) || !File.Exists(exe)) return;
        if (IsProcessRunning(exe)) return;

        _discordPresence.SetGameRunning(ch, GetVersionLabel(ch));

        try
        {
            Process.Start(new ProcessStartInfo
            {
                FileName = exe, WorkingDirectory = IOPath.GetDirectoryName(exe), UseShellExecute = true,
            });
        }
        catch { }

        if (IsUe4ssActiveForChapter(ch))
            ApplyUe4ssTempRemap(exe);
    }

    private void SettingsButton_Click(object sender, RoutedEventArgs e)
    {
        SelectSettingsTab(0);
        RefreshCoresButtons();
        RefreshCoresPriorityUI();
        SettingsOverlay.Visibility = Visibility.Visible;
    }

    private void CloseSettingsBtn_Click(object sender, RoutedEventArgs e) =>
        SettingsOverlay.Visibility = Visibility.Collapsed;

    private void SaveCardOpenBtn_Click(object sender, RoutedEventArgs e)
    {
        if (sender is Button btn && btn.Tag is int chapterNum)
            _saveCardChapter = chapterNum;

        if (_saveCardChapter == 1 || _saveCardChapter == 2 || _saveCardChapter == 3)
        {
            PopulateCheckpointList();
            CheckpointSelectOverlay.Visibility = Visibility.Visible;
            return;
        }

        if (_saveCardChapter == 4)
        {
            SaveCardSaveBtnText.Text        = Loc.Get("ch4_load_btn");
            SaveCardSaveBtnPlus.Visibility  = Visibility.Collapsed;
        }
        else if (_saveCardChapter == 5)
        {
            SaveCardSaveBtnText.Text        = Loc.Get("ch5_load_btn");
            SaveCardSaveBtnPlus.Visibility  = Visibility.Collapsed;
        }
        else
        {
            SaveCardSaveBtnText.Text        = Loc.Get("save_card_save_btn");
            SaveCardSaveBtnPlus.Visibility  = Visibility.Visible;
        }

        SaveCardDeleteBtn.Visibility = (_saveCardChapter == 4 || _saveCardChapter == 5)
            ? Visibility.Visible
            : Visibility.Collapsed;

        SaveCardOverlay.Visibility = Visibility.Visible;
    }

    private void PopulateCheckpointList()
    {
        CheckpointListPanel.Children.Clear();
        var savesDir = IOPath.Combine(Services.ResourceExtractor.TempDir, "Assets", "Saves", $"Chapter {_saveCardChapter}");
        if (!Directory.Exists(savesDir)) return;

        var folders = Directory.GetDirectories(savesDir)
            .OrderBy(d =>
            {
                var m = System.Text.RegularExpressions.Regex.Match(IOPath.GetFileName(d), @"^\[(\d+)\]");
                return m.Success ? int.Parse(m.Groups[1].Value) : 999;
            });

        var localApp = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var (savFileName, destDir) = _saveCardChapter switch
        {
            1 => ("Chap1Checkpoint.sav", IOPath.Combine(localApp, "Poppy_Playtime",       "Saved", "SaveGames")),
            2 => ("Chap2Checkpoint.sav", IOPath.Combine(localApp, "Playtime_Prototype4",  "Saved", "SaveGames")),
            3 => ("Playtime.sav",        IOPath.Combine(localApp, "Playtime_Chapter3",    "Saved", "SaveGames")),
            _ => (string.Empty, string.Empty),
        };
        if (string.IsNullOrEmpty(savFileName)) return;

        bool first = true;
        foreach (var folder in folders)
        {
            var savFile = IOPath.Combine(folder, savFileName);
            if (!File.Exists(savFile)) continue;

            var checkpointName = IOPath.GetFileName(folder);
            var btn = new Button
            {
                HorizontalAlignment = HorizontalAlignment.Stretch,
                HorizontalContentAlignment = HorizontalAlignment.Left,
                Background = new SolidColorBrush(Color.FromArgb(0xFF, 0x0A, 0x18, 0x25)),
                BorderBrush = new SolidColorBrush(Color.FromArgb(0xFF, 0x0D, 0x25, 0x35)),
                BorderThickness = new Thickness(1),
                Padding = new Thickness(14, 9, 14, 9),
                Margin = first ? new Thickness(0) : new Thickness(0, 6, 0, 0),
                Tag = (savFile, IOPath.Combine(destDir, savFileName)),
            };
            ButtonHelper.SetCornerRadius(btn, new CornerRadius(4));
            btn.Click += CheckpointBtn_Click;

            var content = new StackPanel { Orientation = Orientation.Horizontal };
            content.Children.Add(new TextBlock
            {
                FontFamily = new FontFamily("Segoe MDL2 Assets"),
                Text = "",
                FontSize = 13,
                Foreground = new SolidColorBrush(Color.FromArgb(0xFF, 0x00, 0xCC, 0xAA)),
                VerticalAlignment = VerticalAlignment.Center,
            });
            content.Children.Add(new TextBlock
            {
                Text = checkpointName,
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize = 11,
                FontWeight = FontWeights.Bold,
                Foreground = new SolidColorBrush(Color.FromArgb(0xFF, 0xCC, 0xDD, 0xEE)),
                VerticalAlignment = VerticalAlignment.Center,
                Margin = new Thickness(10, 0, 0, 0),
            });
            btn.Content = content;
            CheckpointListPanel.Children.Add(btn);
            first = false;
        }
    }

    private void CheckpointBtn_Click(object sender, RoutedEventArgs e)
    {
        if (sender is not Button btn || btn.Tag is not (string sourcePath, string destPath)) return;

        try
        {
            Directory.CreateDirectory(IOPath.GetDirectoryName(destPath)!);
            File.Copy(sourcePath, destPath, overwrite: true);
        }
        catch { }

        CheckpointSelectOverlay.Visibility = Visibility.Collapsed;
    }

    private void CloseCheckpointSelectBtn_Click(object sender, RoutedEventArgs e) =>
        CheckpointSelectOverlay.Visibility = Visibility.Collapsed;

    // ── Auto splitter ────────────────────────────────────────────────────────

    private void AutoSplitterCardBtn_Click(object sender, RoutedEventArgs e)
    {
        if (sender is not Button btn || btn.Tag is not int chapterNum) return;

        PopulateAutoSplitterList(chapterNum);
        AutoSplitterOverlay.Visibility = Visibility.Visible;
    }

    private void PopulateAutoSplitterList(int chapterNum)
    {
        AutoSplitterListPanel.Children.Clear();
        var splitsDir = IOPath.Combine(Services.ResourceExtractor.TempDir, "Assets", "Splits", $"Chapter {chapterNum}");
        if (!Directory.Exists(splitsDir)) return;

        bool first = true;
        foreach (var file in Directory.EnumerateFiles(splitsDir, "*.asl").OrderBy(IOPath.GetFileName))
        {
            var btn = new Button
            {
                HorizontalAlignment = HorizontalAlignment.Stretch,
                HorizontalContentAlignment = HorizontalAlignment.Left,
                Background = new SolidColorBrush(Color.FromArgb(0xFF, 0x0A, 0x18, 0x25)),
                BorderBrush = new SolidColorBrush(Color.FromArgb(0xFF, 0x0D, 0x25, 0x35)),
                BorderThickness = new Thickness(1),
                Padding = new Thickness(14, 9, 14, 9),
                Margin = first ? new Thickness(0) : new Thickness(0, 6, 0, 0),
                Tag = (file, chapterNum),
            };
            ButtonHelper.SetCornerRadius(btn, new CornerRadius(4));
            btn.Click += AutoSplitterFileBtn_Click;

            var content = new StackPanel { Orientation = Orientation.Horizontal };
            content.Children.Add(new TextBlock
            {
                FontFamily = new FontFamily("Segoe MDL2 Assets"),
                Text = "",
                FontSize = 13,
                Foreground = new SolidColorBrush(Color.FromArgb(0xFF, 0x00, 0xCC, 0xAA)),
                VerticalAlignment = VerticalAlignment.Center,
            });
            content.Children.Add(new TextBlock
            {
                Text = IOPath.GetFileNameWithoutExtension(file),
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize = 11,
                FontWeight = FontWeights.Bold,
                Foreground = new SolidColorBrush(Color.FromArgb(0xFF, 0xCC, 0xDD, 0xEE)),
                VerticalAlignment = VerticalAlignment.Center,
                Margin = new Thickness(10, 0, 0, 0),
            });
            btn.Content = content;
            AutoSplitterListPanel.Children.Add(btn);
            first = false;
        }
    }

    private void AutoSplitterFileBtn_Click(object sender, RoutedEventArgs e)
    {
        if (sender is not Button btn || btn.Tag is not (string sourcePath, int chapterNum)) return;

        try
        {
            var destDir = IOPath.Combine(LiveSplitService.DefaultInstallDir, "Components", "CUSTOM SPLITS", $"CHAPTER {chapterNum}");
            Directory.CreateDirectory(destDir);
            File.Copy(sourcePath, IOPath.Combine(destDir, IOPath.GetFileName(sourcePath)), overwrite: true);
        }
        catch { }

        AutoSplitterOverlay.Visibility = Visibility.Collapsed;
    }

    private void CloseAutoSplitterBtn_Click(object sender, RoutedEventArgs e) =>
        AutoSplitterOverlay.Visibility = Visibility.Collapsed;

    private void SaveCardDeleteBtn_Click(object sender, RoutedEventArgs e)
    {
        var localApp = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        string folder, pattern;

        if (_saveCardChapter == 4)
        {
            folder  = IOPath.Combine(localApp, "ch4_pro", "Saved", "SaveGames");
            pattern = "SaveGame_*";
        }
        else if (_saveCardChapter == 5)
        {
            folder  = IOPath.Combine(localApp, "ch5_pro", "Saved", "SaveGames");
            pattern = "PoppySave_*";
        }
        else
        {
            SaveCardOverlay.Visibility = Visibility.Collapsed;
            return;
        }

        try
        {
            if (Directory.Exists(folder))
            {
                foreach (var file in Directory.GetFiles(folder, pattern))
                    File.Delete(file);
            }
        }
        catch { }

        SaveCardOverlay.Visibility = Visibility.Collapsed;
    }

    private void SaveCardSaveBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_saveCardChapter == 4)
        {
            var localApp = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            var destDir  = IOPath.Combine(localApp, "ch4_pro", "Saved", "SaveGames");
            var srcDir   = IOPath.Combine(Services.ResourceExtractor.TempDir, "Assets", "Saves", "Chapter 4");

            try
            {
                if (Directory.Exists(destDir))
                    foreach (var f in Directory.GetFiles(destDir, "SaveGame_*"))
                        File.Delete(f);

                if (Directory.Exists(srcDir))
                {
                    Directory.CreateDirectory(destDir);
                    foreach (var f in Directory.GetFiles(srcDir))
                        File.Copy(f, IOPath.Combine(destDir, IOPath.GetFileName(f)), overwrite: true);
                }
            }
            catch { }
        }
        else if (_saveCardChapter == 5)
        {
            var localApp = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
            var destDir  = IOPath.Combine(localApp, "ch5_pro", "Saved", "SaveGames");
            var srcDir   = IOPath.Combine(Services.ResourceExtractor.TempDir, "Assets", "Saves", "Chapter 5");

            try
            {
                if (Directory.Exists(destDir))
                    foreach (var f in Directory.GetFiles(destDir, "PoppySave_*"))
                        File.Delete(f);

                if (Directory.Exists(srcDir))
                {
                    Directory.CreateDirectory(destDir);
                    foreach (var f in Directory.GetFiles(srcDir))
                        File.Copy(f, IOPath.Combine(destDir, IOPath.GetFileName(f)), overwrite: true);
                }
            }
            catch { }
        }

        SaveCardOverlay.Visibility = Visibility.Collapsed;
    }

    private void CloseSaveCardBtn_Click(object sender, RoutedEventArgs e) =>
        SaveCardOverlay.Visibility = Visibility.Collapsed;

    private void CopyForSteamBtn_Click(object sender, RoutedEventArgs e)
    {
        var exePath = Environment.ProcessPath ?? System.Diagnostics.Process.GetCurrentProcess().MainModule?.FileName;
        if (string.IsNullOrEmpty(exePath)) return;
        try { Clipboard.SetText($"\"{exePath}\" %command%"); } catch { }
        ShowTutorialVideoPopup();
    }

    // ── Add to Steam (writes/updates our own shortcuts.vdf entry) ────────────

    private string? _addToSteamSelectedIconFile;

    private static string DefaultSteamIconFile() => IconThemeSettings.Current.EffectiveTheme switch
    {
        "lgbtq"     => "iconHD LGBTQ+.png",
        "summer"    => "iconHD Summer.png",
        "halloween" => "iconHD Halloween.png",
        "christmas" => "iconHD Christmas.png",
        _           => "iconHD.png",
    };

    private void AddToSteamBtn_Click(object sender, RoutedEventArgs e)
    {
        _addToSteamSelectedIconFile = DefaultSteamIconFile();
        HighlightSelectedSteamIconBtn();
        AddToSteamWarningText.Text = SteamShortcutService.IsSteamRunning()
            ? Loc.Get("add_to_steam_warning_running")
            : Loc.Get("add_to_steam_warning_not_running");
        OpenAddToSteamOverlay();
    }

    private void AddToSteamIconBtn_Click(object sender, RoutedEventArgs e)
    {
        if (sender is Button { Tag: string fileName })
        {
            _addToSteamSelectedIconFile = fileName;
            HighlightSelectedSteamIconBtn();
        }
    }

    private void HighlightSelectedSteamIconBtn()
    {
        var buttons = new[]
        {
            (AddToSteamIconDefaultBtn,   "iconHD.png"),
            (AddToSteamIconChristmasBtn, "iconHD Christmas.png"),
            (AddToSteamIconHalloweenBtn, "iconHD Halloween.png"),
            (AddToSteamIconLgbtqBtn,     "iconHD LGBTQ+.png"),
            (AddToSteamIconSummerBtn,    "iconHD Summer.png"),
        };
        var selectedBrush = new SolidColorBrush(Teal);
        var normalBrush   = new SolidColorBrush(Color.FromArgb(255, 26, 58, 85));
        foreach (var (btn, file) in buttons)
            btn.BorderBrush = file == _addToSteamSelectedIconFile ? selectedBrush : normalBrush;
    }

    private void OpenAddToSteamOverlay()
    {
        AddToSteamOverlay.Opacity    = 0;
        AddToSteamOverlay.Visibility = Visibility.Visible;
        var ease = new CubicEase { EasingMode = EasingMode.EaseOut };
        AddToSteamOverlay.BeginAnimation(UIElement.OpacityProperty,
            new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(220)));
        AddToSteamPopupScale.BeginAnimation(ScaleTransform.ScaleXProperty,
            new DoubleAnimation(0.85, 1, TimeSpan.FromMilliseconds(260)) { EasingFunction = ease });
        AddToSteamPopupScale.BeginAnimation(ScaleTransform.ScaleYProperty,
            new DoubleAnimation(0.85, 1, TimeSpan.FromMilliseconds(260)) { EasingFunction = ease });
    }

    private void CloseAddToSteamOverlay()
    {
        var ease = new CubicEase { EasingMode = EasingMode.EaseIn };
        var fade = new DoubleAnimation(1, 0, TimeSpan.FromMilliseconds(180)) { EasingFunction = ease };
        fade.Completed += (_, _) => AddToSteamOverlay.Visibility = Visibility.Collapsed;
        AddToSteamOverlay.BeginAnimation(UIElement.OpacityProperty, fade);
    }

    private void AddToSteamCancelBtn_Click(object sender, RoutedEventArgs e) => CloseAddToSteamOverlay();

    private void AddToSteamConfirmBtn_Click(object sender, RoutedEventArgs e)
    {
        CloseAddToSteamOverlay();

        var exePath = Environment.ProcessPath ?? System.Diagnostics.Process.GetCurrentProcess().MainModule?.FileName;
        if (string.IsNullOrEmpty(exePath))
        {
            ShowAddToSteamDialog(Loc.Get("add_to_steam_error_no_exe"));
            return;
        }

        var iconFile = _addToSteamSelectedIconFile ?? DefaultSteamIconFile();
        var iconSourcePath = IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Steam", "icons", iconFile);
        if (!File.Exists(iconSourcePath))
        {
            ShowAddToSteamDialog(Loc.Get("add_to_steam_error_no_icon"));
            return;
        }

        // The actual close-Steam/edit-vdf/restart-Steam sequence runs in a standalone helper
        // process instead of here: if this exe is the one Steam is currently tracking as "the
        // running game" (e.g. it replaced the real game's exe), Steam kills that tracked
        // process the moment it shuts down, which would kill this operation mid-flight. A
        // freshly spawned, Steam-untracked process isn't subject to that.
        try
        {
            var psi = new ProcessStartInfo(exePath) { UseShellExecute = true, Verb = "runas" };
            psi.ArgumentList.Add(SteamShortcutHelperEntryPoint.Arg);
            psi.ArgumentList.Add(exePath);
            psi.ArgumentList.Add(iconSourcePath);
            Process.Start(psi);
        }
        catch (Exception ex)
        {
            ShowAddToSteamDialog(string.Format(Loc.Get("add_to_steam_error_generic"), ex.Message));
            return;
        }

        // A graceful Application.Shutdown() isn't enough here: if this exe is the one Steam is
        // tracking as "the running game" (old replace-the-exe setup), Steam's library still
        // shows it as running until the process is actually gone, and the helper is about to
        // shut Steam down right after this — so this process must be dead first, not just
        // mid-cleanup. Hard-exit instead of waiting on window/thread teardown.
        Environment.Exit(0);
    }

    private void ShowAddToSteamDialog(string message, bool success = false)
    {
        WpfDialog.Show(this, "ADD TO STEAM", new TextBlock
        {
            Text         = message,
            FontFamily   = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize     = 12,
            Foreground   = new SolidColorBrush(success
                ? Color.FromArgb(200, 0, 204, 170)
                : Color.FromArgb(200, 160, 180, 200)),
            TextWrapping = TextWrapping.Wrap,
            MaxWidth     = 360,
        }, closeText: "OK");
    }

    private void CaptureKonamiKeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Escape && _easterEggPlaying)
        {
            HideEasterEggVideoPopup();
            return;
        }

        var key = e.Key == Key.System ? e.SystemKey : e.Key;

        if (key != KonamiSequence[_konamiBuffer.Count])
        {
            _konamiBuffer.Clear();
            if (key == KonamiSequence[0]) _konamiBuffer.Add(key);
            return;
        }

        _konamiBuffer.Add(key);
        if (_konamiBuffer.Count < KonamiSequence.Length) return;

        _konamiBuffer.Clear();
        ShowEasterEggVideoPopup();
    }

    private void ShowEasterEggVideoPopup()
    {
        if (_easterEggPlaying) return;

        var videoPath = IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Videos", "EasterEgg.mp4");
        if (!File.Exists(videoPath)) return;

        _easterEggPlaying = true;

        EasterEggPlayer.Source = new Uri(videoPath);
        EasterEggOverlay.Visibility = Visibility.Visible;
        EasterEggPlayer.Play();
    }

    private void HideEasterEggVideoPopup()
    {
        EasterEggPlayer.Stop();
        EasterEggPlayer.Source = null;
        EasterEggOverlay.Visibility = Visibility.Collapsed;
        _easterEggPlaying = false;
    }

    private void EasterEggOverlay_MouseDown(object sender, MouseButtonEventArgs e) => HideEasterEggVideoPopup();

    private void EasterEggPlayer_MediaEnded(object sender, RoutedEventArgs e) => HideEasterEggVideoPopup();

    private void EasterEggPlayer_MediaFailed(object sender, ExceptionRoutedEventArgs e) => HideEasterEggVideoPopup();

    private void ShowTutorialVideoPopup()
    {
        var videoPath = IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Videos", "Tutorial.mp4");
        if (!File.Exists(videoPath)) return;

        var media = new MediaElement
        {
            Source = new Uri(videoPath),
            LoadedBehavior = MediaState.Manual,
            UnloadedBehavior = MediaState.Close,
            Stretch = Stretch.Uniform,
        };
        media.Loaded     += (_, _) => media.Play();
        media.MediaEnded += (_, _) => { media.Position = TimeSpan.Zero; media.Play(); };

        var hint = new TextBlock
        {
            Text = "Click to close",
            Foreground = new SolidColorBrush(Color.FromArgb(180, 0, 204, 170)),
            FontSize = 11,
            HorizontalAlignment = HorizontalAlignment.Center,
            Margin = new Thickness(0, 4, 0, 6),
        };

        var panel = new StackPanel();
        panel.Children.Add(media);
        panel.Children.Add(hint);

        var popup = new Window
        {
            WindowStyle = WindowStyle.None,
            ResizeMode = ResizeMode.NoResize,
            Width = 960,
            Height = 570,
            Owner = this,
            WindowStartupLocation = WindowStartupLocation.CenterOwner,
            Background = new SolidColorBrush(Color.FromRgb(9, 12, 30)),
            ShowInTaskbar = false,
            Content = panel,
        };
        popup.MouseDown += (_, _) => popup.Close();
        popup.KeyDown   += (_, e) => { if (e.Key == Key.Escape) popup.Close(); };
        popup.Show();
    }

    // ── Step-by-step image tutorials (Steam launch setup / Controller setup) ──

    private sealed record TutorialStep(string? ImagePath, string? Caption, bool ShowCopyButton = false, bool ShowControllerIcon = false);

    private void SteamTutorialBtn_Click(object sender, RoutedEventArgs e)
    {
        var dir = IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Images", "Tutorial");
        ShowTutorialStepsPopup(
            new TutorialStep(IOPath.Combine(dir, "1.png"), null),
            new TutorialStep(IOPath.Combine(dir, "2.png"), Loc.Get("steam_tutorial_paste_here"), ShowCopyButton: true),
            new TutorialStep(IOPath.Combine(dir, "3.png"), null),
            new TutorialStep(null, Loc.Get("controller_setup_restart"), ShowControllerIcon: true));
    }

    private void ShowTutorialStepsPopup(params TutorialStep[] steps)
    {
        if (steps.Length == 0) return;

        var teal = new SolidColorBrush(Teal);
        var dim  = new SolidColorBrush(Color.FromArgb(255, 138, 170, 187));
        int step = 0;
        int last = steps.Length - 1;

        var image = new Image { Stretch = Stretch.Uniform, MaxHeight = 320, Margin = new Thickness(0, 0, 0, 14) };
        RenderOptions.SetBitmapScalingMode(image, BitmapScalingMode.HighQuality);

        var controllerIcon = new TextBlock
        {
            FontFamily = new FontFamily("Segoe MDL2 Assets"), Text = "",
            FontSize = 72, Foreground = teal,
            HorizontalAlignment = HorizontalAlignment.Center,
            Margin = new Thickness(0, 30, 0, 20),
            Visibility = Visibility.Collapsed,
        };

        var caption = new TextBlock
        {
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 13, FontWeight = FontWeights.Bold, Foreground = dim,
            TextAlignment = TextAlignment.Center, TextWrapping = TextWrapping.Wrap,
            Margin = new Thickness(0, 0, 0, 12),
        };

        var copyBtn = MakeSmallButton(Loc.Get("steam_tutorial_copy_btn"), Teal);
        copyBtn.MinWidth = 170;
        copyBtn.HorizontalAlignment = HorizontalAlignment.Center;

        var copiedMsg = new TextBlock
        {
            Text = Loc.Get("steam_tutorial_copied"),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 11, Foreground = teal,
            HorizontalAlignment = HorizontalAlignment.Center,
            Margin = new Thickness(0, 8, 0, 0),
            Visibility = Visibility.Hidden,
        };

        copyBtn.Click += (_, _) =>
        {
            var exePath = Environment.ProcessPath ?? Process.GetCurrentProcess().MainModule?.FileName;
            if (string.IsNullOrEmpty(exePath)) return;
            try { Clipboard.SetText($"\"{exePath}\" %command%"); }
            catch { return; }
            copiedMsg.Visibility = Visibility.Visible;
        };

        var stepLabel = new TextBlock
        {
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 10, Foreground = dim,
            HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center,
        };

        var prevBtn = MakeSmallButton(Loc.Get("steam_tutorial_prev"), dim.Color);
        var nextBtn = MakeSmallButton(Loc.Get("steam_tutorial_next"), Teal);
        prevBtn.MinWidth = 90;
        nextBtn.MinWidth = 90;

        Window? popup = null;

        void Render()
        {
            var s = steps[step];
            copiedMsg.Visibility = Visibility.Hidden;

            var hasImage = s.ImagePath is not null && File.Exists(s.ImagePath);
            image.Visibility = hasImage ? Visibility.Visible : Visibility.Collapsed;
            image.Source     = hasImage ? new BitmapImage(new Uri(s.ImagePath!)) : null;

            controllerIcon.Visibility = s.ShowControllerIcon ? Visibility.Visible : Visibility.Collapsed;
            copyBtn.Visibility        = s.ShowCopyButton     ? Visibility.Visible : Visibility.Collapsed;

            caption.Text       = s.Caption ?? "";
            caption.Visibility = string.IsNullOrEmpty(s.Caption) ? Visibility.Collapsed : Visibility.Visible;

            stepLabel.Text    = $"{step + 1} / {steps.Length}";
            prevBtn.IsEnabled = step > 0;
            ((TextBlock)nextBtn.Content).Text = step == last ? Loc.Get("steam_tutorial_done") : Loc.Get("steam_tutorial_next");
        }

        prevBtn.Click += (_, _) => { if (step > 0) { step--; Render(); } };
        nextBtn.Click += (_, _) =>
        {
            if (step < last) { step++; Render(); }
            else popup?.Close();
        };

        var nav = new Grid { Margin = new Thickness(0, 16, 0, 0) };
        nav.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        nav.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        nav.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        Grid.SetColumn(prevBtn, 0);
        Grid.SetColumn(stepLabel, 1);
        Grid.SetColumn(nextBtn, 2);
        nav.Children.Add(prevBtn);
        nav.Children.Add(stepLabel);
        nav.Children.Add(nextBtn);

        var panel = new StackPanel { Margin = new Thickness(28), VerticalAlignment = VerticalAlignment.Center };
        panel.Children.Add(image);
        panel.Children.Add(controllerIcon);
        panel.Children.Add(caption);
        panel.Children.Add(copyBtn);
        panel.Children.Add(copiedMsg);
        panel.Children.Add(nav);

        popup = new Window
        {
            WindowStyle = WindowStyle.None,
            ResizeMode = ResizeMode.NoResize,
            Width = 640,
            Height = 580,
            Owner = this,
            WindowStartupLocation = WindowStartupLocation.CenterOwner,
            Background = new SolidColorBrush(Color.FromRgb(9, 12, 30)),
            ShowInTaskbar = false,
            Content = panel,
        };
        popup.KeyDown += (_, ev) => { if (ev.Key == Key.Escape) popup.Close(); };

        Render();
        popup.Show();
    }

    private void VersionBtn_Click(object sender, RoutedEventArgs e) => OpenVersionsOverlay();

    private void VolumeSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        _sfxVolume = (float)(e.NewValue / 100.0);
        if (VolumeValueText is not null)
            VolumeValueText.Text = $"{(int)e.NewValue}%";
        SaveVolume();
    }

    private void QuitButton_Click(object sender, RoutedEventArgs e)
    {
        foreach (var cts in _activePolls.Values)
            cts.Cancel();

        foreach (var proc in Process.GetProcessesByName("steamcmd"))
            try { proc.Kill(); } catch { }

        _updateService.Dispose();
        Close();
    }

    // ── Beginner tutorials ────────────────────────────────────────────────────

    private void BeginnerTutorialsBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_beginnerTutorialOverlay is { IsVisible: true })
        {
            _beginnerTutorialOverlay.Close();
        }
        else
        {
            _beginnerTutorialOverlay = new BeginnerTutorialOverlay();
            _beginnerTutorialOverlay.Closed += (_, _) => _beginnerTutorialOverlay = null;
            _beginnerTutorialOverlay.Show();
            _beginnerTutorialOverlay.Activate();
        }
    }

    // ── Updates ───────────────────────────────────────────────────────────────

    private async Task DetectUpdatesAsync()
    {
        _updateInfo   = await _updateService.CheckForUpdatesAsync();
        _gbUpdateInfo = await _updateService.CheckGameBananaUpdateAsync();

        _ = Dispatcher.BeginInvoke(new Action(() =>
        {
            VersionLabel.Text = AppVersion.GetDisplayVersion();

            if (_updateInfo.IsUpdateAvailable || (_gbUpdateInfo?.IsUpdateAvailable ?? false))
                StartSettingsUpdateAnimation();

            if (UpdatesOverlay.Visibility == Visibility.Visible)
                RefreshUpdateCheckView();
        }));
    }

    private void SettingsUpdateBadge_Click(object sender, RoutedEventArgs e) =>
        SettingsOverlay.Visibility = Visibility.Visible;

    private void OpenUpdatesBtn_Click(object sender, RoutedEventArgs e)
    {
        SettingsOverlay.Visibility = Visibility.Collapsed;
        ShowUpdateCheckView();
        UpdatesOverlay.Visibility = Visibility.Visible;
    }

    private bool _updateAlertActive;

    private void StartSettingsUpdateAnimation()
    {
        _updateAlertActive = true;
        SettingsUpdateBadge.Visibility  = Visibility.Visible;

        // Highlight the Updates sidebar tab with red text + breathing red border
        SettingsTabUpdatesText.Foreground = new SolidColorBrush(Color.FromRgb(204, 34, 0));
        var alertBorderBrush = new SolidColorBrush(Color.FromRgb(204, 34, 0));
        SettingsTabUpdatesAlertBorder.BorderBrush = alertBorderBrush;
        var alertAnim = new ColorAnimation
        {
            From           = Color.FromRgb(204, 34, 0),
            To             = Color.FromRgb(80, 10, 0),
            Duration       = TimeSpan.FromMilliseconds(800),
            AutoReverse    = true,
            RepeatBehavior = RepeatBehavior.Forever,
            EasingFunction = new SineEase { EasingMode = EasingMode.EaseInOut },
        };
        alertBorderBrush.BeginAnimation(SolidColorBrush.ColorProperty, alertAnim);

        // Highlight the in-settings updates button
        OpenUpdatesBtnIcon.Foreground   = new SolidColorBrush(Color.FromRgb(200, 60, 20));
        OpenUpdatesBtnText.Foreground   = new SolidColorBrush(Color.FromRgb(220, 80, 30));
        OpenUpdatesBtnBadge.Visibility  = Visibility.Visible;

        // Animate settings button border from dim blue → bright red, pulsing
        var borderBrush = new SolidColorBrush(Color.FromRgb(17, 34, 51));
        SettingsButton.BorderBrush = borderBrush;
        var borderAnim = new ColorAnimation
        {
            From           = Color.FromRgb(17, 34, 51),
            To             = Color.FromRgb(200, 30, 30),
            Duration       = TimeSpan.FromMilliseconds(700),
            AutoReverse    = true,
            RepeatBehavior = RepeatBehavior.Forever,
            EasingFunction = new SineEase { EasingMode = EasingMode.EaseInOut },
        };
        borderBrush.BeginAnimation(SolidColorBrush.ColorProperty, borderAnim);

        // Pulse the badge opacity
        var badgeAnim = new DoubleAnimation
        {
            From           = 1.0,
            To             = 0.55,
            Duration       = TimeSpan.FromMilliseconds(600),
            AutoReverse    = true,
            RepeatBehavior = RepeatBehavior.Forever,
            EasingFunction = new SineEase { EasingMode = EasingMode.EaseInOut },
        };
        SettingsUpdateBadge.BeginAnimation(UIElement.OpacityProperty, badgeAnim);
    }

    private void ShowUpdateCheckView()
    {
        _showingInstallView              = false;
        UpdateCheckView.Visibility       = Visibility.Visible;
        UpdateInstallView.Visibility     = Visibility.Collapsed;
        InstallButtonsPanel.Visibility   = Visibility.Collapsed;
        DownloadProgressPanel.Visibility = Visibility.Collapsed;
        RefreshUpdateCheckView();
    }

    private void RefreshUpdateCheckView()
    {
        UpdateStatusBtn.Visibility     = Visibility.Collapsed;
        GbUpdateBtn.Visibility         = Visibility.Collapsed;
        UpdateDetailsBorder.Visibility = Visibility.Collapsed;

        if (_updateInfo == null) return;

        UpdateStatusBtn.Visibility = Visibility.Visible;

        if (_updateInfo.IsUpdateAvailable)
        {
            UpdateStatusBtn.Background  = new SolidColorBrush(Color.FromArgb(255, 46, 125, 50));
            UpdateStatusIcon.Text       = "\uE896";
            UpdateStatusTitle.Text      = Loc.Get("updates_available_title");
            UpdateStatusMessage.Text    = Loc.Get("updates_available_msg", $"v{_updateInfo.LatestVersion}");
            UpdateStatusBtn.IsEnabled   = true;

            UpdateDetailsBorder.Visibility = Visibility.Visible;
            UpdateLatestVersionText.Text   = $"v{_updateInfo.LatestVersion}";
            UpdateFileNameText.Text        = _updateInfo.FileName;
            UpdateFileSizeText.Text        = UpdateService.FormatFileSize(_updateInfo.FileSize);
        }
        else if (_updateInfo.LatestVersion == AppVersion.CURRENT_VERSION && !string.IsNullOrEmpty(_updateInfo.LatestVersion))
        {
            UpdateStatusBtn.Background = new SolidColorBrush(Color.FromArgb(255, 21, 101, 192));
            UpdateStatusIcon.Text      = "\uE930";
            UpdateStatusTitle.Text     = Loc.Get("updates_up_to_date_title");
            UpdateStatusMessage.Text   = Loc.Get("updates_up_to_date_msg");
            UpdateStatusBtn.IsEnabled  = false;
        }
        else
        {
            UpdateStatusBtn.Background = new SolidColorBrush(Color.FromArgb(255, 230, 81, 0));
            UpdateStatusIcon.Text      = "\uE814";
            UpdateStatusTitle.Text     = Loc.Get("updates_error_title");
            UpdateStatusMessage.Text   = Loc.Get("updates_error_msg");
            UpdateStatusBtn.IsEnabled  = false;
        }

        if (!_updateInfo.IsUpdateAvailable && (_gbUpdateInfo?.IsUpdateAvailable ?? false))
        {
            GbUpdateBtn.Visibility = Visibility.Visible;
            GbUpdateTitle.Text     = Loc.Get("updates_gb_title");
            GbUpdateMessage.Text   = Loc.Get("updates_gb_msg", $"v{_gbUpdateInfo.LatestVersion}");
        }
    }

    private void ShowInstallView(bool isGb)
    {
        _showingInstallView = true;
        _isGbInstall        = isGb;

        UpdateCheckView.Visibility       = Visibility.Collapsed;
        UpdateInstallView.Visibility     = Visibility.Visible;
        InstallButtonsPanel.Visibility   = Visibility.Visible;
        DownloadProgressPanel.Visibility = Visibility.Collapsed;

        if (isGb && _gbUpdateInfo != null)
        {
            InstallTitleText.Text    = Loc.Get("updates_install_ready", $"v{_gbUpdateInfo.LatestVersion}");
            InstallSubtitleText.Text = Loc.Get("updates_install_subtitle",
                _gbUpdateInfo.FileName, UpdateService.FormatFileSize(_gbUpdateInfo.FileSize));
            SetChangelogTextWithMentions(ChangelogText,
                string.IsNullOrWhiteSpace(_gbUpdateInfo.Changelog) ? "—" : _gbUpdateInfo.Changelog);
        }
        else if (_updateInfo != null)
        {
            InstallTitleText.Text    = Loc.Get("updates_install_ready", $"v{_updateInfo.LatestVersion}");
            InstallSubtitleText.Text = Loc.Get("updates_install_subtitle",
                _updateInfo.FileName, UpdateService.FormatFileSize(_updateInfo.FileSize));
            SetChangelogTextWithMentions(ChangelogText,
                string.IsNullOrWhiteSpace(_updateInfo.Changelog) ? "—" : _updateInfo.Changelog);
        }

        WhatsNewLabel.Text = Loc.Get("updates_whats_new");
    }

    private void CheckUpdatesBanner_Click(object sender, RoutedEventArgs e)
    {
        _updateInfo   = null;
        _gbUpdateInfo = null;
        UpdateStatusBtn.Visibility     = Visibility.Collapsed;
        GbUpdateBtn.Visibility         = Visibility.Collapsed;
        UpdateDetailsBorder.Visibility = Visibility.Collapsed;
        UpdateCheckHint.Text           = Loc.Get("updates_checking");

        _ = Task.Run(async () =>
        {
            _updateInfo   = await _updateService.CheckForUpdatesAsync();
            _gbUpdateInfo = await _updateService.CheckGameBananaUpdateAsync();
            _ = Dispatcher.BeginInvoke(new Action(() =>
            {
                UpdateCheckHint.Text = Loc.Get("updates_check_hint");
                RefreshUpdateCheckView();
                if (_updateInfo.IsUpdateAvailable || (_gbUpdateInfo?.IsUpdateAvailable ?? false))
                    StartSettingsUpdateAnimation();
            }));
        });
    }

    private void UpdateStatusBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_updateInfo?.IsUpdateAvailable ?? false)
            ShowInstallView(isGb: false);
    }

    private void GbUpdateBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_gbUpdateInfo?.IsUpdateAvailable ?? false)
            ShowInstallView(isGb: true);
    }

    private async void AcceptInstallBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_isDownloading) return;
        _isDownloading = true;

        InstallButtonsPanel.Visibility   = Visibility.Collapsed;
        DownloadProgressPanel.Visibility = Visibility.Visible;

        var progress = new Progress<int>(pct =>
            Dispatcher.BeginInvoke(new Action(() =>
            {
                UpdateDownloadProgressBar.Value = pct;
                UpdateDownloadProgressText.Text = Loc.Get("updates_downloading", pct);
            })));

        bool ok;
        if (_isGbInstall && _gbUpdateInfo != null)
            ok = await _updateService.DownloadAndInstallGbUpdateAsync(_gbUpdateInfo, progress);
        else if (_updateInfo != null)
            ok = await _updateService.DownloadAndInstallUpdateAsync(_updateInfo, progress);
        else
            ok = false;

        if (!ok)
        {
            _isDownloading = false;
            ShowUpdateCheckView();
        }
    }

    private void CancelInstallBtn_Click(object sender, RoutedEventArgs e) => ShowUpdateCheckView();

    private void CloseUpdatesBtn_Click(object sender, RoutedEventArgs e)
    {
        UpdatesOverlay.Visibility = Visibility.Collapsed;
        if (_showingInstallView) ShowUpdateCheckView();
    }

    private void ManualUpdateLink_Click(object sender, RoutedEventArgs e)
    {
        var window = new ManualUpdateWindow(_updateService) { Owner = this };
        window.ShowDialog();
    }

    // ── LiveSplit ──────────────────────────────────────────────────────────────

    private async Task DetectLiveSplitAsync()
    {
        _liveSplitInfo = await _liveSplitService.CheckAsync();

        _ = Dispatcher.BeginInvoke(new Action(() =>
        {
            RefreshLiveSplitButton();

            if (_liveSplitInfo.IsUpdateAvailable)
                StartLiveSplitUpdateAnimation();

            if (LiveSplitOverlay.Visibility == Visibility.Visible)
                RefreshLiveSplitOverlay();
        }));
    }

    private void RefreshLiveSplitButton()
    {
        if (_liveSplitInfo == null) return;

        if (_liveSplitInfo.IsInstalled && !_liveSplitInfo.IsUpdateAvailable)
        {
            OpenLiveSplitBtnText.Foreground = new SolidColorBrush(Color.FromRgb(0, 120, 90));
        }
        else
        {
            OpenLiveSplitBtnText.Foreground = new SolidColorBrush(Color.FromRgb(58, 106, 138));
        }
    }

    private void StartLiveSplitUpdateAnimation()
    {
        OpenLiveSplitBtnBadge.Visibility = Visibility.Visible;

        OpenLiveSplitBtnText.Foreground = new SolidColorBrush(Color.FromRgb(220, 80, 30));

        var badgeAnim = new DoubleAnimation
        {
            From           = 1.0,
            To             = 0.55,
            Duration       = TimeSpan.FromMilliseconds(600),
            AutoReverse    = true,
            RepeatBehavior = RepeatBehavior.Forever,
            EasingFunction = new SineEase { EasingMode = EasingMode.EaseInOut },
        };
        OpenLiveSplitBtnBadge.BeginAnimation(UIElement.OpacityProperty, badgeAnim);
    }

    private void OpenLiveSplitBtn_Click(object sender, RoutedEventArgs e)
    {
        SettingsOverlay.Visibility = Visibility.Collapsed;
        RefreshLiveSplitOverlay();
        LiveSplitOverlay.Visibility = Visibility.Visible;
    }

    private void RefreshLiveSplitOverlay()
    {
        var info = _liveSplitInfo;

        LiveSplitProgressPanel.Visibility = Visibility.Collapsed;
        _isLiveSplitDownloading           = false;

        // Subtitle
        if (info == null)
        {
            LiveSplitSubtitleText.Text = Loc.Get("livesplit_checking");
            LiveSplitActionBtn.Visibility = Visibility.Collapsed;
        }
        else if (!info.IsInstalled)
        {
            LiveSplitSubtitleText.Text    = Loc.Get("livesplit_not_installed");
            LiveSplitActionBtnText.Text   = Loc.Get("livesplit_install_btn");
            LiveSplitActionBtn.Visibility = Visibility.Visible;
        }
        else if (info.IsUpdateAvailable)
        {
            LiveSplitSubtitleText.Text    = Loc.Get("livesplit_update_available");
            LiveSplitActionBtnText.Text   = Loc.Get("livesplit_update_btn");
            LiveSplitActionBtn.Visibility = Visibility.Visible;
        }
        else
        {
            LiveSplitSubtitleText.Text    = Loc.Get("livesplit_up_to_date");
            LiveSplitActionBtnText.Text   = Loc.Get("livesplit_launch_btn");
            LiveSplitActionBtn.Visibility = Visibility.Visible;
        }

        // Version texts
        LiveSplitInstalledVersionText.Text = (info?.IsInstalled ?? false)
            ? (string.IsNullOrEmpty(info!.InstalledVersion) ? "—" : info.InstalledVersion)
            : Loc.Get("livesplit_none");
        LiveSplitLatestVersionText.Text = string.IsNullOrEmpty(info?.LatestVersion)
            ? "—"
            : info.LatestVersion;

        // Install path
        if (info?.IsInstalled == true && !string.IsNullOrEmpty(info.InstallPath))
        {
            LiveSplitPathText.Text         = info.InstallPath;
            LiveSplitPathBorder.Visibility = Visibility.Visible;
        }
        else
        {
            LiveSplitPathBorder.Visibility = Visibility.Collapsed;
        }
    }

    private async void LiveSplitActionBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_isLiveSplitDownloading || _liveSplitInfo == null) return;

        // If installed and up to date → launch
        if (_liveSplitInfo.IsInstalled && !_liveSplitInfo.IsUpdateAvailable)
        {
            LiveSplitService.Launch(_liveSplitInfo.InstallPath!);
            LiveSplitOverlay.Visibility = Visibility.Collapsed;
            return;
        }

        _isLiveSplitDownloading          = true;
        LiveSplitActionBtn.Visibility    = Visibility.Collapsed;
        LiveSplitProgressPanel.Visibility = Visibility.Visible;
        LiveSplitProgressText.Text       = Loc.Get("livesplit_downloading", 0);
        LiveSplitProgressBar.Value       = 0;

        var progress = new Progress<int>(pct =>
            Dispatcher.BeginInvoke(new Action(() =>
            {
                LiveSplitProgressBar.Value  = pct;
                LiveSplitProgressText.Text  = Loc.Get("livesplit_downloading", pct);
            })));

        var ok = await _liveSplitService.DownloadAndInstallAsync(
            _liveSplitInfo, LiveSplitService.DefaultInstallDir, progress);

        _isLiveSplitDownloading = false;

        if (ok)
        {
            _liveSplitInfo = await _liveSplitService.CheckAsync();
            _ = Dispatcher.BeginInvoke(new Action(() =>
            {
                OpenLiveSplitBtnBadge.Visibility = Visibility.Collapsed;
                OpenLiveSplitBtnBadge.BeginAnimation(UIElement.OpacityProperty, null);
                RefreshLiveSplitButton();
                RefreshLiveSplitOverlay();
            }));
        }
        else
        {
            _ = Dispatcher.BeginInvoke(new Action(() =>
            {
                LiveSplitProgressPanel.Visibility = Visibility.Collapsed;
                LiveSplitActionBtn.Visibility     = Visibility.Visible;
            }));
        }
    }

    private void CloseLiveSplitBtn_Click(object sender, RoutedEventArgs e)
    {
        LiveSplitOverlay.Visibility = Visibility.Collapsed;
    }

    // ── Discord presence settings ─────────────────────────────────────────────

    private void RefreshDiscordToggles()
    {
        SetToggle(DiscordShowActivityText,   _discordSettings.ShowActivity);
        SetToggle(DiscordShowVersionText,    _discordSettings.ShowVersion);
        SetToggle(DiscordShowLiveSplitText,  _discordSettings.ShowLiveSplit);
    }

    private static void SetToggle(TextBlock tb, bool on)
    {
        tb.Text       = on ? "ON" : "OFF";
        tb.Foreground = on
            ? new SolidColorBrush(Color.FromArgb(255,   0, 204, 170))
            : new SolidColorBrush(Color.FromArgb(255,  42,  90, 122));
    }

    private void DiscordShowActivityBtn_Click(object sender, RoutedEventArgs e)
    {
        _discordSettings.ShowActivity = !_discordSettings.ShowActivity;
        _discordSettings.Save();
        _discordPresence.ApplySettings(
            _discordSettings.ShowActivity,
            _discordSettings.ShowVersion,
            _discordSettings.ShowLiveSplit);
        RefreshDiscordToggles();
    }

    private void DiscordShowVersionBtn_Click(object sender, RoutedEventArgs e)
    {
        _discordSettings.ShowVersion = !_discordSettings.ShowVersion;
        _discordSettings.Save();
        _discordPresence.ApplySettings(
            _discordSettings.ShowActivity,
            _discordSettings.ShowVersion,
            _discordSettings.ShowLiveSplit);
        RefreshDiscordToggles();
    }

    private void DiscordShowLiveSplitBtn_Click(object sender, RoutedEventArgs e)
    {
        _discordSettings.ShowLiveSplit = !_discordSettings.ShowLiveSplit;
        _discordSettings.Save();
        _discordPresence.ApplySettings(
            _discordSettings.ShowActivity,
            _discordSettings.ShowVersion,
            _discordSettings.ShowLiveSplit);
        RefreshDiscordToggles();

        if (_discordSettings.ShowLiveSplit)
            ShowLiveSplitTcpNotice();
    }

    // ── Online users counter (visual toggle only — heartbeat keeps running) ────

    private void RefreshOnlineUsersToggle()
    {
        SetToggle(OnlineUsersShowText, HeartbeatSettings.Current.ShowCounter);
    }

    private void ApplyOnlineUsersVisibility()
    {
        var visibility = HeartbeatSettings.Current.ShowCounter ? Visibility.Visible : Visibility.Collapsed;
        OnlineUsersDot.Visibility  = visibility;
        OnlineUsersText.Visibility = visibility;
    }

    private void OnlineUsersShowBtn_Click(object sender, RoutedEventArgs e)
    {
        HeartbeatSettings.Current.ShowCounter = !HeartbeatSettings.Current.ShowCounter;
        HeartbeatSettings.Current.Save();
        ApplyOnlineUsersVisibility();
        RefreshOnlineUsersToggle();
    }

    private void ApplyIconTheme()
    {
        var resourceName = IconThemeSettings.Current.EmbeddedIconResourceName;
        if (resourceName == null) return; // "default" theme: keep the executable's built-in icon.ico
        using var stream = System.Reflection.Assembly.GetExecutingAssembly()
            .GetManifestResourceStream(resourceName);
        if (stream == null) return;
        var bitmap = new BitmapImage();
        bitmap.BeginInit();
        bitmap.CacheOption = BitmapCacheOption.OnLoad;
        bitmap.StreamSource = stream;
        bitmap.EndInit();
        bitmap.Freeze();
        Icon = bitmap;
    }

    private void SetIconTheme(string theme)
    {
        IconThemeSettings.Current.Theme = theme;
        IconThemeSettings.Current.Save();
        ApplyIconTheme();
        _discordPresence.RefreshIconTheme();
        RefreshIconThemeButtons();
    }

    private void IconThemeDefaultBtn_Click(object sender, RoutedEventArgs e)   => SetIconTheme("default");
    private void IconThemeClassicBtn_Click(object sender, RoutedEventArgs e)   => SetIconTheme("classic");
    private void IconThemeLgbtqBtn_Click(object sender, RoutedEventArgs e)     => SetIconTheme("lgbtq");
    private void IconThemeSummerBtn_Click(object sender, RoutedEventArgs e)    => SetIconTheme("summer");
    private void IconThemeHalloweenBtn_Click(object sender, RoutedEventArgs e) => SetIconTheme("halloween");
    private void IconThemeChristmasBtn_Click(object sender, RoutedEventArgs e) => SetIconTheme("christmas");

    private void RefreshIconThemeButtons()
    {
        var selectedBrush  = new SolidColorBrush(Teal);
        var selectedBg     = new SolidColorBrush(Color.FromArgb(255, 0, 40, 30));
        var selectedBorder = new SolidColorBrush(Teal);
        var dimBrush       = new SolidColorBrush(Color.FromArgb(255, 58, 106, 138));
        var dimBg          = new SolidColorBrush(Color.FromArgb(255, 6, 15, 24));
        var dimBorder      = new SolidColorBrush(Color.FromArgb(255, 13, 37, 53));

        void Style(Button btn, TextBlock text, bool selected)
        {
            btn.Background  = selected ? selectedBg : dimBg;
            btn.BorderBrush = selected ? selectedBorder : dimBorder;
            text.Foreground = selected ? selectedBrush : dimBrush;
        }

        var theme = IconThemeSettings.Current.Theme;
        Style(IconThemeDefaultBtn,   IconThemeDefaultBtnText,   theme == "default");
        Style(IconThemeClassicBtn,   IconThemeClassicBtnText,   theme == "classic");
        Style(IconThemeLgbtqBtn,     IconThemeLgbtqBtnText,     theme == "lgbtq");
        Style(IconThemeSummerBtn,    IconThemeSummerBtnText,    theme == "summer");
        Style(IconThemeHalloweenBtn, IconThemeHalloweenBtnText, theme == "halloween");
        Style(IconThemeChristmasBtn, IconThemeChristmasBtnText, theme == "christmas");
    }

    private void ShowLiveSplitTcpNotice()
    {
        var mono   = new FontFamily("Cascadia Code, Consolas, Courier New");
        var muted  = Color.FromArgb(255, 138, 170, 187);
        var tutDir = IOPath.Combine(Services.ResourceExtractor.TempDir, "Assets", "Images", "Tutorial Live Split");

        var pages = new[]
        {
            (Step: Loc.Get("livesplit_tcp_step1"), Img: IOPath.Combine(tutDir, "1.png"), Footer: (string?)null),
            (Step: Loc.Get("livesplit_tcp_step2"), Img: IOPath.Combine(tutDir, "2.png"), Footer: Loc.Get("livesplit_tcp_reminder")),
        };
        int page = 0;
        int last = pages.Length - 1;

        var stepText = new TextBlock
        {
            FontFamily = mono, FontSize = 11,
            Foreground = new SolidColorBrush(Teal),
            Margin     = new Thickness(0, 0, 0, 10),
        };

        var img = new Image { MaxWidth = 340, Stretch = Stretch.Uniform, HorizontalAlignment = HorizontalAlignment.Left };
        RenderOptions.SetBitmapScalingMode(img, BitmapScalingMode.HighQuality);

        var footerText = new TextBlock
        {
            FontFamily   = mono, FontSize = 10,
            Foreground   = new SolidColorBrush(Color.FromArgb(150, 120, 160, 190)),
            TextWrapping = TextWrapping.Wrap,
            Margin       = new Thickness(0, 12, 0, 0),
        };

        var pageLabel = new TextBlock
        {
            FontFamily          = mono, FontSize = 10,
            Foreground          = new SolidColorBrush(muted),
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment   = VerticalAlignment.Center,
        };
        var prevBtn = MakeSmallButton(Loc.Get("steam_tutorial_prev"), muted);
        var nextBtn = MakeSmallButton(Loc.Get("steam_tutorial_next"), Teal);
        prevBtn.MinWidth = 80;
        nextBtn.MinWidth = 80;

        WpfDialog? dlg = null;

        void RenderPage()
        {
            var p = pages[page];
            stepText.Text = p.Step;
            img.Source    = new BitmapImage(new Uri(p.Img));
            footerText.Text       = p.Footer ?? "";
            footerText.Visibility = p.Footer != null ? Visibility.Visible : Visibility.Collapsed;
            pageLabel.Text    = $"{page + 1} / {pages.Length}";
            prevBtn.IsEnabled = page > 0;
            ((TextBlock)nextBtn.Content).Text = page == last
                ? Loc.Get("steam_tutorial_done")
                : Loc.Get("steam_tutorial_next");
        }

        prevBtn.Click += (_, _) => { if (page > 0) { page--; RenderPage(); } };
        nextBtn.Click += (_, _) =>
        {
            if (page < last) { page++; RenderPage(); }
            else dlg?.Close();
        };

        var nav = new Grid { Margin = new Thickness(0, 16, 0, 0) };
        nav.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        nav.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        nav.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        Grid.SetColumn(prevBtn,   0);
        Grid.SetColumn(pageLabel, 1);
        Grid.SetColumn(nextBtn,   2);
        nav.Children.Add(prevBtn);
        nav.Children.Add(pageLabel);
        nav.Children.Add(nextBtn);

        var panel = new StackPanel { MinWidth = 370 };
        panel.Children.Add(stepText);
        panel.Children.Add(img);
        panel.Children.Add(footerText);
        panel.Children.Add(nav);

        RenderPage();

        dlg = new WpfDialog(this, Loc.Get("livesplit_tcp_title"), panel);
        dlg.ShowDialog();
    }

    // ── Changelog ─────────────────────────────────────────────────────────────

    private void ChangelogButton_Click(object sender, RoutedEventArgs e)
    {
        BuildChangelogPanel();
        ChangelogOverlay.Visibility = Visibility.Visible;
    }

    private void CloseChangelogBtn_Click(object sender, RoutedEventArgs e) =>
        ChangelogOverlay.Visibility = Visibility.Collapsed;

    private void BuildChangelogPanel()
    {
        ChangelogPanel.Children.Clear();

        bool isFirst = true;
        foreach (var entry in ChangelogData.Entries)
        {
            bool isCurrent = entry.Version == AppVersion.CURRENT_VERSION;

            if (!isFirst)
            {
                ChangelogPanel.Children.Add(new Border
                {
                    BorderBrush     = new SolidColorBrush(Color.FromArgb(255, 13, 32, 48)),
                    BorderThickness = new Thickness(0, 0, 0, 1),
                    Margin          = new Thickness(0, 0, 0, 12),
                });
            }
            isFirst = false;

            // Version header row
            var headerRow = new StackPanel { Orientation = Orientation.Horizontal, Margin = new Thickness(0, 0, 0, 6) };

            var versionBadge = new Border
            {
                Background      = new SolidColorBrush(isCurrent ? Color.FromArgb(255, 0, 80, 60) : Color.FromArgb(255, 6, 15, 24)),
                BorderBrush     = new SolidColorBrush(isCurrent ? Color.FromArgb(255, 0, 180, 130) : Color.FromArgb(255, 13, 37, 53)),
                BorderThickness = new Thickness(1),
                CornerRadius    = new CornerRadius(3),
                Padding         = new Thickness(8, 3, 8, 3),
                Margin          = new Thickness(0, 0, 10, 0),
            };
            versionBadge.Child = new TextBlock
            {
                Text             = $"v{entry.Version}",
                FontFamily       = new System.Windows.Media.FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize         = 13,
                FontWeight       = FontWeights.Bold,
                Foreground       = new SolidColorBrush(isCurrent ? Color.FromArgb(255, 0, 204, 170) : Color.FromArgb(255, 58, 106, 138)),
                VerticalAlignment = VerticalAlignment.Center,
            };
            headerRow.Children.Add(versionBadge);

            headerRow.Children.Add(new TextBlock
            {
                Text              = entry.Date,
                FontFamily        = new System.Windows.Media.FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize          = 11,
                Foreground        = new SolidColorBrush(Color.FromArgb(255, 26, 58, 80)),
                VerticalAlignment = VerticalAlignment.Center,
            });

            if (isCurrent)
            {
                headerRow.Children.Add(new TextBlock
                {
                    Text              = Loc.Get("changelog_current"),
                    FontFamily        = new System.Windows.Media.FontFamily("Cascadia Code, Consolas, Courier New"),
                    FontSize          = 10,
                    Foreground        = new SolidColorBrush(Color.FromArgb(255, 0, 120, 90)),
                    VerticalAlignment = VerticalAlignment.Center,
                    Margin            = new Thickness(10, 0, 0, 0),
                });
            }

            var card = new StackPanel { Margin = new Thickness(0, 0, 0, 12) };
            card.Children.Add(headerRow);

            // Change items. A "## " prefix marks a category header (e.g. "## Additions")
            // instead of a bulleted change line.
            bool isFirstChange = true;
            foreach (var change in entry.Changes)
            {
                if (change.StartsWith("## "))
                {
                    card.Children.Add(new TextBlock
                    {
                        Text         = change[3..],
                        FontFamily   = new System.Windows.Media.FontFamily("Cascadia Code, Consolas, Courier New"),
                        FontSize     = 10,
                        FontWeight   = FontWeights.Bold,
                        Foreground   = new SolidColorBrush(Color.FromArgb(255, 0, 204, 170)),
                        Margin       = new Thickness(0, isFirstChange ? 4 : 10, 0, 2),
                    });
                    isFirstChange = false;
                    continue;
                }

                var tb = new TextBlock
                {
                    FontFamily   = new System.Windows.Media.FontFamily("Cascadia Code, Consolas, Courier New"),
                    FontSize     = 12,
                    Foreground   = new SolidColorBrush(Color.FromArgb(255, 138, 170, 187)),
                    TextWrapping = TextWrapping.Wrap,
                    Margin       = new Thickness(0, 2, 0, 0),
                };

                tb.Inlines.Add(new System.Windows.Documents.Run("  ·  "));
                SetChangelogTextWithMentions(tb, change, clear: false);

                card.Children.Add(tb);
                isFirstChange = false;
            }

            ChangelogPanel.Children.Add(card);
        }
    }

    private void SetChangelogTextWithMentions(TextBlock tb, string text, bool clear = true)
    {
        if (clear) { tb.Text = null; tb.Inlines.Clear(); }
        var parts = System.Text.RegularExpressions.Regex.Split(text, @"(@\w+)");
        foreach (var part in parts)
        {
            if (part.StartsWith("@") && part.Length > 1)
                tb.Inlines.Add(MakeMentionInline(part));
            else if (part.Length > 0)
                tb.Inlines.Add(new System.Windows.Documents.Run(part));
        }
    }

    private System.Windows.Documents.InlineUIContainer MakeMentionInline(string mention)
    {
        var tag = new Border
        {
            Padding         = new Thickness(5, 1, 5, 1),
            Margin          = new Thickness(2, 0, 2, 0),
            CornerRadius    = new CornerRadius(3),
            Background      = new SolidColorBrush(Color.FromArgb(30, 88, 101, 242)),
            BorderBrush     = new SolidColorBrush(Color.FromArgb(80, 88, 101, 242)),
            BorderThickness = new Thickness(1),
            Cursor          = Cursors.Hand,
            Child = new TextBlock
            {
                Text       = mention,
                FontFamily = new System.Windows.Media.FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize   = 10,
                FontWeight = FontWeights.Bold,
                Foreground = new SolidColorBrush(Color.FromArgb(255, 88, 101, 242)),
            }
        };
        var discordId = ChangelogDiscordUsers.GetValueOrDefault(mention[1..]);
        if (discordId != null)
            tag.MouseDown += (_, _) => Process.Start(new ProcessStartInfo
                { FileName = $"https://discord.com/users/{discordId}", UseShellExecute = true });
        tag.MouseEnter += (_, _) => tag.Background = new SolidColorBrush(Color.FromArgb(50, 88, 101, 242));
        tag.MouseLeave += (_, _) => tag.Background = new SolidColorBrush(Color.FromArgb(30, 88, 101, 242));
        return new System.Windows.Documents.InlineUIContainer(tag)
            { BaselineAlignment = BaselineAlignment.Center };
    }

    // ── Bug report ────────────────────────────────────────────────────────────

    private void BugReportButton_Click(object sender, RoutedEventArgs e)
    {
        BugETitleBox.Text           = "";
        BugDescBox.Text             = "";
        _bugImagePath               = null;
        _bugReportDiscordUser       = null;
        BugImageFileName.Text       = Loc.Get("bug_report_image_none");
        BugImageFileName.Foreground = new SolidColorBrush(Color.FromArgb(160, 42, 16, 80));
        BugStatusText.Visibility    = Visibility.Collapsed;
        SendBugReportBtnText.Text   = Loc.Get("bug_report_send_btn");
        SendBugReportBtn.IsEnabled  = true;
        var cached = Services.DiscordOAuthService.LoadCached();
        if (cached.HasValue)
        {
            _bugReportDiscordUser          = cached;
            BugDiscordUsername.Text        = cached.Value.Username;
            BugDiscordConnected.Visibility = Visibility.Visible;
            BugDiscordConnectRow.Visibility = Visibility.Collapsed;
        }
        else
        {
            BugDiscordConnected.Visibility  = Visibility.Collapsed;
            BugDiscordConnectRow.Visibility = Visibility.Visible;
        }
        BugReportOverlay.Visibility = Visibility.Visible;
    }

    private void BugImageBtn_Click(object sender, RoutedEventArgs e)
    {
        var picker = new OpenFileDialog
        {
            Filter = "Images|*.jpg;*.jpeg;*.png;*.gif;*.webp;*.bmp",
        };
        if (picker.ShowDialog(this) != true) return;
        _bugImagePath = picker.FileName;
        BugImageFileName.Text       = IOPath.GetFileName(_bugImagePath);
        BugImageFileName.Foreground = new SolidColorBrush(Color.FromArgb(220, 170, 140, 220));
    }

    private void CloseBugReportBtn_Click(object sender, RoutedEventArgs e) =>
        BugReportOverlay.Visibility = Visibility.Collapsed;

    private async void BugDiscordConnectBtn_Click(object sender, RoutedEventArgs e)
    {
        _discordAuthCts?.Cancel();
        _discordAuthCts = new CancellationTokenSource();

        BugDiscordConnectRow.Visibility = Visibility.Collapsed;
        BugDiscordWaiting.Visibility    = Visibility.Visible;

        var user = await Services.DiscordOAuthService.AuthenticateAsync(_discordAuthCts.Token);

        BugDiscordWaiting.Visibility = Visibility.Collapsed;

        if (user.HasValue)
        {
            _bugReportDiscordUser           = user;
            BugDiscordUsername.Text         = user.Value.Username;
            BugDiscordConnected.Visibility  = Visibility.Visible;
            Services.DiscordOAuthService.SaveCached(user.Value.Id, user.Value.Username);
        }
        else
        {
            BugDiscordConnectRow.Visibility = Visibility.Visible;
        }
    }

    private void BugDiscordCancelAuthBtn_Click(object sender, RoutedEventArgs e)
    {
        _discordAuthCts?.Cancel();
        BugDiscordWaiting.Visibility    = Visibility.Collapsed;
        BugDiscordConnectRow.Visibility = Visibility.Visible;
    }

    private void BugDiscordDisconnectBtn_Click(object sender, RoutedEventArgs e)
    {
        Services.DiscordOAuthService.ClearCached();
        _bugReportDiscordUser           = null;
        BugDiscordConnected.Visibility  = Visibility.Collapsed;
        BugDiscordConnectRow.Visibility = Visibility.Visible;
    }

    private void BugDiscordInfoBtn_Click(object sender, RoutedEventArgs e)
    {
        DiscordInfoWhyText.Text       = Loc.Get("bug_report_discord_info_why_text");
        DiscordInfoOverlay.Visibility = Visibility.Visible;
    }

    private void DiscordInfoCloseBtn_Click(object sender, RoutedEventArgs e) =>
        DiscordInfoOverlay.Visibility = Visibility.Collapsed;

    /// <summary>Shows the "Discord required" explainer dialog — why it's needed (per-caller
    /// <paramref name="introTextKey"/>), the identify-only permission it requests, and what it
    /// can't see — used to gate any Discord-required Send flow, and starts the OAuth flow via
    /// <paramref name="onConnect"/> if the user chooses to continue. Shared by the bug-report and
    /// hand-mod-submission Send flows, which only differ in their intro line.</summary>
    private async Task ShowDiscordRequiredDialogAsync(string introTextKey, Action onConnect)
    {
        var content = new StackPanel();
        content.Children.Add(new TextBlock
        {
            Text         = Loc.Get(introTextKey),
            TextWrapping = TextWrapping.Wrap,
            FontFamily   = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize     = 12,
            Foreground   = new SolidColorBrush(Color.FromArgb(255, 160, 180, 200)),
            Margin       = new Thickness(0, 0, 0, 16),
        });
        content.Children.Add(new TextBlock
        {
            Text       = Loc.Get("bug_report_discord_info_perms_title"),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize   = 10, FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(Color.FromRgb(0x99, 0x44, 0xCC)),
            Margin     = new Thickness(0, 0, 0, 8),
        });

        var permRow = new Border
        {
            Background      = new SolidColorBrush(Color.FromArgb(40, 119, 85, 204)),
            BorderBrush     = new SolidColorBrush(Color.FromArgb(90, 119, 85, 204)),
            BorderThickness = new Thickness(1),
            CornerRadius    = new CornerRadius(4),
            Padding         = new Thickness(10, 8, 10, 8),
            Margin          = new Thickness(0, 0, 0, 10),
        };
        var permStack = new StackPanel();
        permStack.Children.Add(new TextBlock
        {
            Text       = Loc.Get("bug_report_discord_info_perm_name"),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize   = 10, FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(Color.FromRgb(119, 85, 204)),
        });
        permStack.Children.Add(new TextBlock
        {
            Text         = Loc.Get("bug_report_discord_info_perm_desc"),
            FontFamily   = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize     = 9,
            Foreground   = new SolidColorBrush(Color.FromRgb(74, 90, 122)),
            TextWrapping = TextWrapping.Wrap,
            Margin       = new Thickness(0, 3, 0, 0),
        });
        permRow.Child = permStack;
        content.Children.Add(permRow);

        content.Children.Add(new TextBlock
        {
            Text         = Loc.Get("bug_report_discord_info_no_access"),
            FontFamily   = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize     = 9,
            Foreground   = new SolidColorBrush(Color.FromRgb(51, 68, 68)),
            TextWrapping = TextWrapping.Wrap,
        });

        var discordResult = await WpfDialog.ShowAsync(this,
            Loc.Get("bug_report_discord_required_title"), content,
            primaryText: Loc.Get("bug_report_discord_connect_btn"),
            closeText:   Loc.Get("back"));

        if (discordResult == WpfDialogResult.Primary) onConnect();
    }

    private async void SendBugReportBtn_Click(object sender, RoutedEventArgs e)
    {
        var title = BugETitleBox.Text.Trim();
        var desc  = BugDescBox.Text.Trim();

        if (string.IsNullOrEmpty(title) || string.IsNullOrEmpty(desc))
        {
            BugStatusText.Text       = Loc.Get("bug_report_err_empty");
            BugStatusText.Foreground = new SolidColorBrush(Color.FromArgb(255, 200, 60, 60));
            BugStatusText.Visibility = Visibility.Visible;
            return;
        }

        if (_bugReportDiscordUser is null)
        {
            await ShowDiscordRequiredDialogAsync("bug_report_err_discord_required",
                () => BugDiscordConnectBtn_Click(sender, e));
            return;
        }

        SendBugReportBtn.IsEnabled = false;
        SendBugReportBtnText.Text  = Loc.Get("bug_report_sending");
        BugStatusText.Visibility   = Visibility.Collapsed;

        var ok = await SendDiscordBugReportAsync(_bugReportDiscordUser.Value, title, desc, _bugImagePath);

        if (ok)
        {
            BugStatusText.Text       = Loc.Get("bug_report_success");
            BugStatusText.Foreground = new SolidColorBrush(Color.FromArgb(255, 0, 200, 140));
            BugStatusText.Visibility = Visibility.Visible;
            SendBugReportBtnText.Text = Loc.Get("bug_report_send_btn");
            await Task.Delay(2000);
            BugReportOverlay.Visibility = Visibility.Collapsed;
        }
        else
        {
            BugStatusText.Text         = Loc.Get("bug_report_err_send");
            BugStatusText.Foreground   = new SolidColorBrush(Color.FromArgb(255, 200, 60, 60));
            BugStatusText.Visibility   = Visibility.Visible;
            SendBugReportBtnText.Text  = Loc.Get("bug_report_send_btn");
            SendBugReportBtn.IsEnabled = true;
        }
    }

    private static async Task<bool> SendDiscordBugReportAsync(
        (string Id, string Username) discordUser, string title, string description, string? imagePath)
    {
        const string WebhookUrl =
            "WebhookUrl";

        try
        {
            using var client = new System.Net.Http.HttpClient { Timeout = TimeSpan.FromSeconds(30) };

            var authorName = $"{discordUser.Username}  ·  ID: {discordUser.Id}";
            var mention    = $"<@{discordUser.Id}>";
            var footer     = $"Playtime Speed Launcher  ·  {AppVersion.GetDisplayVersion()}";

            string json;
            if (imagePath != null && File.Exists(imagePath))
            {
                var fileName = IOPath.GetFileName(imagePath);
                json = System.Text.Json.JsonSerializer.Serialize(new
                {
                    content = mention,
                    embeds = new[] { new
                    {
                        author      = new { name = authorName },
                        title,
                        description,
                        color       = 0x7B2FBE,
                        image       = new { url = $"attachment://{fileName}" },
                        footer      = new { text = footer },
                        timestamp   = DateTime.UtcNow.ToString("o"),
                    }}
                });

                using var form = new System.Net.Http.MultipartFormDataContent();
                form.Add(new System.Net.Http.StringContent(
                    json, System.Text.Encoding.UTF8, "application/json"), "payload_json");

                var bytes       = await File.ReadAllBytesAsync(imagePath);
                var fileContent = new System.Net.Http.ByteArrayContent(bytes);
                fileContent.Headers.ContentType =
                    new System.Net.Http.Headers.MediaTypeHeaderValue(GetImageMimeType(imagePath));
                form.Add(fileContent, "files[0]", fileName);

                var resp = await client.PostAsync(WebhookUrl, form);
                return resp.IsSuccessStatusCode;
            }
            else
            {
                json = System.Text.Json.JsonSerializer.Serialize(new
                {
                    content = mention,
                    embeds = new[] { new
                    {
                        author      = new { name = authorName },
                        title,
                        description,
                        color       = 0x7B2FBE,
                        footer      = new { text = footer },
                        timestamp   = DateTime.UtcNow.ToString("o"),
                    }}
                });

                var content = new System.Net.Http.StringContent(
                    json, System.Text.Encoding.UTF8, "application/json");
                var resp = await client.PostAsync(WebhookUrl, content);
                return resp.IsSuccessStatusCode;
            }
        }
        catch { return false; }
    }

    private static string GetImageMimeType(string path) =>
        IOPath.GetExtension(path).ToLowerInvariant() switch
        {
            ".jpg" or ".jpeg" => "image/jpeg",
            ".png"            => "image/png",
            ".gif"            => "image/gif",
            ".webp"           => "image/webp",
            ".bmp"            => "image/bmp",
            _                 => "application/octet-stream",
        };

    // ── UE4SS ─────────────────────────────────────────────────────────────────

    private void Ue4ssCardBtn_Click(object sender, RoutedEventArgs e)
    {
        _ue4ssTargetChapter = (int)((Button)sender).Tag;

        var chapter = _chapters.FirstOrDefault(c => c.Number == _ue4ssTargetChapter);
        _ue4ssWin64Dir = null;
        _ue4ssZipPath  = null;

        if (chapter != null)
        {
            var exePath = GetActiveExePath(chapter);
            if (!string.IsNullOrEmpty(exePath))
                _ue4ssWin64Dir = FindWin64Dir(IOPath.GetDirectoryName(exePath)!);

            _ue4ssZipPath = chapter.Number >= 5
                ? IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Tools", "Chapter 5", "Ue4ss.zip")
                : IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Tools", "Chapter 1 - 4", "Ue4ss.zip");
        }

        // UE4SS installed by the Load Manip system is a different build (see
        // LoadManipFilesService.IsUe4ssFromLoadManip) — don't report it here as the
        // standalone UE4SS install, since removing/replacing it must go through Load Manip.
        bool installedViaLoadManip = _ue4ssWin64Dir != null && LoadManipFilesService.IsUe4ssFromLoadManip(_ue4ssWin64Dir);
        bool installed = _ue4ssWin64Dir != null && IsUe4ssInstalled(_ue4ssWin64Dir) && !installedViaLoadManip;
        _ue4ssTargetInstalledViaLoadManip = installedViaLoadManip;

        if (installedViaLoadManip)
        {
            bool fullBrightAlsoInstalled = (_ue4ssTargetChapter == 1 || _ue4ssTargetChapter == 5)
                && _ue4ssWin64Dir != null && FullBrightFilesService.IsInstalled(_ue4ssWin64Dir);
            Ue4ssPopupQuestion.Text = fullBrightAlsoInstalled
                ? "Load Manip, FullBright and their UE4SS files must be\nremoved before installing the original UE4SS."
                : "Load Manip and its UE4SS files must be\nremoved before installing the original UE4SS.";
            Ue4ssYesBtn.Visibility    = Visibility.Collapsed;
            Ue4ssDeleteBtn.Visibility = Visibility.Visible;
            Ue4ssDeleteBtn.IsEnabled  = true;
            Ue4ssDeleteBtn.Opacity    = 1.0;
        }
        else if (installed)
        {
            Ue4ssPopupQuestion.Text   = "Do you want to remove UE4SS\nfrom this version?";
            Ue4ssYesBtn.Visibility    = Visibility.Collapsed;
            Ue4ssDeleteBtn.Visibility = Visibility.Visible;
            Ue4ssDeleteBtn.IsEnabled  = true;
            Ue4ssDeleteBtn.Opacity    = 1.0;
        }
        else
        {
            Ue4ssPopupQuestion.Text   = "Do you want to add UE4SS\nto this version?";
            Ue4ssYesBtn.Visibility    = Visibility.Visible;
            Ue4ssDeleteBtn.Visibility = Visibility.Collapsed;
        }

        Ue4ssOverlay.Opacity    = 0;
        Ue4ssOverlay.Visibility = Visibility.Visible;
        var ease = new CubicEase { EasingMode = EasingMode.EaseOut };
        Ue4ssOverlay.BeginAnimation(UIElement.OpacityProperty,
            new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(220)));
        Ue4ssPopupScale.BeginAnimation(ScaleTransform.ScaleXProperty,
            new DoubleAnimation(0.85, 1, TimeSpan.FromMilliseconds(260)) { EasingFunction = ease });
        Ue4ssPopupScale.BeginAnimation(ScaleTransform.ScaleYProperty,
            new DoubleAnimation(0.85, 1, TimeSpan.FromMilliseconds(260)) { EasingFunction = ease });
    }

    private void Ue4ssNoBtn_Click(object sender, RoutedEventArgs e)
        => CloseUe4ssOverlay();

    private async void Ue4ssYesBtn_Click(object sender, RoutedEventArgs e)
    {
        CloseUe4ssOverlay();

        if (_ue4ssWin64Dir is null)
        {
            ShowUe4ssDialog("No game path found for this chapter.\nPlease set up the game installation first.");
            return;
        }

        if (_ue4ssZipPath is null || !File.Exists(_ue4ssZipPath))
        {
            ShowUe4ssDialog("UE4SS zip not found. Try restarting the launcher.");
            return;
        }

        try
        {
            await Task.Run(() => ZipFile.ExtractToDirectory(_ue4ssZipPath, _ue4ssWin64Dir, overwriteFiles: true));
            RefreshUe4ssBtnStates();
            ShowUe4ssDialog($"UE4SS installed successfully!\n\n{_ue4ssWin64Dir}", success: true);
        }
        catch (Exception ex)
        {
            ShowUe4ssDialog($"Error installing UE4SS:\n{ex.Message}");
        }
    }

    private async void Ue4ssDeleteBtn_Click(object sender, RoutedEventArgs e)
    {
        CloseUe4ssOverlay();

        if (_ue4ssWin64Dir is null)
            return;

        if (_ue4ssTargetInstalledViaLoadManip)
        {
            var win64        = _ue4ssWin64Dir;
            var paksDir      = LoadManipFilesService.GetPaksDir(win64);
            var loadManipZip = LoadManipFilesService.GetZipPath(_ue4ssTargetChapter);
            var ue4ssZipPath = LoadManipFilesService.GetUe4ssZipPath(_ue4ssTargetChapter);
            var markerZipPath = LoadManipFilesService.GetPlaytimeMarkerZipPath(_ue4ssTargetChapter);

            bool fullBrightInstalled = (_ue4ssTargetChapter == 1 || _ue4ssTargetChapter == 5)
                && FullBrightFilesService.IsInstalled(win64);
            var fullBrightZip        = fullBrightInstalled ? FullBrightFilesService.GetZipPath(_ue4ssTargetChapter) : null;
            var fullBrightUe4ssZip   = fullBrightInstalled ? FullBrightFilesService.GetUe4ssZipPath(_ue4ssTargetChapter) : null;
            var fullBrightMarkerZip  = fullBrightInstalled ? FullBrightFilesService.GetPlaytimeMarkerZipPath(_ue4ssTargetChapter) : null;

            if (_ue4ssZipPath is null || !File.Exists(_ue4ssZipPath))
            {
                ShowUe4ssDialog("UE4SS zip not found. Try restarting the launcher.");
                return;
            }
            var originalUe4ssZip = _ue4ssZipPath;

            try
            {
                await Task.Run(() =>
                {
                    // FullBright's pak/UE4SS build overwrote Load Manip's own files, so it must
                    // be removed first — otherwise its extra files (fullbright.playtime, the
                    // FullBright/CheatManagerEnablerMod ue4ss Mods) would be left orphaned once
                    // the plain UE4SS build is extracted over everything.
                    if (fullBrightInstalled && paksDir != null
                        && fullBrightZip != null && File.Exists(fullBrightZip)
                        && fullBrightUe4ssZip != null && File.Exists(fullBrightUe4ssZip)
                        && fullBrightMarkerZip != null && File.Exists(fullBrightMarkerZip))
                    {
                        // Keep config.ini (holds the user's chosen keybinds) even though it
                        // ships in the config zip — only pak/UE4SS/marker get removed here.
                        FullBrightFilesService.Uninstall(paksDir, win64, fullBrightZip, fullBrightUe4ssZip, fullBrightMarkerZip);
                    }

                    if (paksDir != null && loadManipZip != null && File.Exists(loadManipZip))
                        LoadManipFilesService.Uninstall(paksDir, loadManipZip);
                    if (ue4ssZipPath != null && File.Exists(ue4ssZipPath))
                        LoadManipFilesService.UninstallUe4ss(win64, ue4ssZipPath, markerZipPath);

                    ZipFile.ExtractToDirectory(originalUe4ssZip, win64, overwriteFiles: true);
                });
                RefreshUe4ssBtnStates();
                RefreshLoadManipBtnStates();
                RefreshFullBrightBtnStates();
                RefreshFullBrightKeysUI();
                RefreshChapter5FullBrightUI();
                RefreshChapter1UI();
                RefreshChapter5LoadManipUI();
                ShowUe4ssDialog($"UE4SS installed successfully!\n\n{win64}", success: true);
            }
            catch (Exception ex)
            {
                ShowUe4ssDialog($"Error installing UE4SS:\n{ex.Message}");
            }
            return;
        }

        if (_ue4ssZipPath is null || !File.Exists(_ue4ssZipPath))
            return;

        try
        {
            var win64   = _ue4ssWin64Dir;
            var zipPath = _ue4ssZipPath;
            await Task.Run(() => LoadManipFilesService.UninstallUe4ss(win64, zipPath));
            RefreshUe4ssBtnStates();
            ShowUe4ssDialog("UE4SS removed successfully!", success: true);
        }
        catch (Exception ex)
        {
            ShowUe4ssDialog($"Error removing UE4SS:\n{ex.Message}");
        }
    }

    private void CloseUe4ssOverlay()
    {
        var ease = new CubicEase { EasingMode = EasingMode.EaseIn };
        var fade = new DoubleAnimation(1, 0, TimeSpan.FromMilliseconds(180)) { EasingFunction = ease };
        fade.Completed += (_, _) => Ue4ssOverlay.Visibility = Visibility.Collapsed;
        Ue4ssOverlay.BeginAnimation(UIElement.OpacityProperty, fade);
    }

    private void ShowUe4ssDialog(string message, bool success = false)
    {
        WpfDialog.Show(this, "UE4SS", new TextBlock
        {
            Text         = message,
            FontFamily   = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize     = 12,
            Foreground   = new SolidColorBrush(success
                ? Color.FromArgb(200, 0, 204, 170)
                : Color.FromArgb(200, 160, 180, 200)),
            TextWrapping = TextWrapping.Wrap,
            MaxWidth     = 360,
        }, closeText: "OK");
    }

    private static string? FindWin64Dir(string startDir)
    {
        if (IOPath.GetFileName(startDir).Equals("Win64", StringComparison.OrdinalIgnoreCase))
            return startDir;
        try
        {
            return Directory.EnumerateDirectories(startDir, "*", SearchOption.AllDirectories)
                .FirstOrDefault(d => IOPath.GetFileName(d).Equals("Win64", StringComparison.OrdinalIgnoreCase));
        }
        catch { return null; }
    }

    /// <summary>Resolves a chapter's real shipping-binary process name from disk (cached per exe
    /// path, since it requires a one-time directory walk via <see cref="FindWin64Dir"/>) — needed
    /// because a chapter's launcher-facing stub exe doesn't always share a name with the UE
    /// project it launches (e.g. Chapter 4's stub is "Playtime_Chapter4.exe" but its shipping
    /// binary is "ch4_pro-Win64-Shipping.exe"), so guessing "{stubName}-Win64-Shipping" only
    /// happens to work for chapters where the two names coincide. Returns null if no
    /// "*-Win64-Shipping.exe" is found (or the install layout doesn't have a Win64 folder at all).</summary>
    private string? ResolveShippingExeName(string exePath)
    {
        if (_shippingExeNameCache.TryGetValue(exePath, out var cached)) return cached;

        string? name = null;
        try
        {
            var exeDir = IOPath.GetDirectoryName(exePath);
            var win64  = exeDir != null ? FindWin64Dir(exeDir) : null;
            if (win64 != null)
            {
                name = Directory.EnumerateFiles(win64, "*-Win64-Shipping.exe")
                    .Select(IOPath.GetFileNameWithoutExtension)
                    .FirstOrDefault();
            }
        }
        catch { }

        _shippingExeNameCache[exePath] = name;
        return name;
    }

    private static bool IsUe4ssInstalled(string win64Dir) =>
        File.Exists(IOPath.Combine(win64Dir, "dwmapi.dll")) ||
        Directory.Exists(IOPath.Combine(win64Dir, "ue4ss"));

    private bool IsUe4ssActiveForChapter(ChapterInfo ch)
    {
        var exePath = GetActiveExePath(ch);
        if (string.IsNullOrEmpty(exePath)) return false;
        var win64 = FindWin64Dir(IOPath.GetDirectoryName(exePath)!);
        return win64 != null && IsUe4ssInstalled(win64) && !LoadManipFilesService.IsUe4ssFromLoadManip(win64);
    }

    private void ApplyUe4ssTempRemap(string exe)
    {
        if (_ue4ssTempRemap) return;

        var hwnd = new System.Windows.Interop.WindowInteropHelper(this).Handle;

        _savedHotkeyMod = _hotkeyModifiers;
        _savedHotkeyVk  = _hotkeyVk;
        _savedTutMod    = _tutorialHotkeyModifiers;
        _savedTutVk     = _tutorialHotkeyVk;

        UnregisterHotKey(hwnd, HOTKEY_ID);
        UnregisterHotKey(hwnd, TUTORIAL_HOTKEY_ID);

        _hotkeyModifiers         = 0;
        _hotkeyVk                = VK_F2;
        _tutorialHotkeyModifiers = 0;
        _tutorialHotkeyVk        = VK_F1;

        RegisterHotKey(hwnd, HOTKEY_ID,          _hotkeyModifiers,        _hotkeyVk);
        RegisterHotKey(hwnd, TUTORIAL_HOTKEY_ID, _tutorialHotkeyModifiers, _tutorialHotkeyVk);

        _ue4ssTempRemap    = true;
        _ue4ssTempRemapExe = exe;

        ShowUe4ssRemapToast();
    }

    private void RestoreUe4ssHotkeys()
    {
        if (!_ue4ssTempRemap) return;

        var hwnd = new System.Windows.Interop.WindowInteropHelper(this).Handle;

        UnregisterHotKey(hwnd, HOTKEY_ID);
        UnregisterHotKey(hwnd, TUTORIAL_HOTKEY_ID);

        _hotkeyModifiers         = _savedHotkeyMod;
        _hotkeyVk                = _savedHotkeyVk;
        _tutorialHotkeyModifiers = _savedTutMod;
        _tutorialHotkeyVk        = _savedTutVk;

        RegisterHotKey(hwnd, HOTKEY_ID,          _hotkeyModifiers,        _hotkeyVk);
        RegisterHotKey(hwnd, TUTORIAL_HOTKEY_ID, _tutorialHotkeyModifiers, _tutorialHotkeyVk);

        _ue4ssTempRemap    = false;
        _ue4ssTempRemapExe = null;
        _ue4ssRemapToast?.Close();
    }

    private void ShowUe4ssRemapToast()
    {
        _ue4ssRemapToast?.Close();

        const double W        = 340;
        const double Duration = 8;

        var progressFg = new Border
        {
            Background          = new SolidColorBrush(Color.FromArgb(255, 0, 204, 170)),
            Height              = 3,
            HorizontalAlignment = HorizontalAlignment.Left,
            Width               = W - 2,
        };

        var progressGrid = new Grid { Height = 3 };
        progressGrid.Children.Add(new Border
        {
            Background = new SolidColorBrush(Color.FromArgb(40, 0, 204, 170)),
        });
        progressGrid.Children.Add(progressFg);

        var textStack = new StackPanel { Margin = new Thickness(14, 12, 14, 10) };
        textStack.Children.Add(new TextBlock
        {
            Text       = Loc.Get("ue4ss_remap_hint"),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize   = 10,
            Foreground = new SolidColorBrush(Color.FromArgb(180, 160, 190, 210)),
        });
        textStack.Children.Add(new TextBlock
        {
            Text       = Loc.Get("ue4ss_remap_keys"),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize   = 12,
            FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(Color.FromArgb(255, 0, 204, 170)),
            Margin     = new Thickness(0, 4, 0, 0),
        });

        var innerGrid = new Grid();
        innerGrid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        innerGrid.RowDefinitions.Add(new RowDefinition { Height = new GridLength(3) });
        Grid.SetRow(textStack,    0);
        Grid.SetRow(progressGrid, 1);
        innerGrid.Children.Add(textStack);
        innerGrid.Children.Add(progressGrid);

        var outerBorder = new Border
        {
            Background      = new SolidColorBrush(Color.FromArgb(240, 9, 20, 30)),
            BorderBrush     = new SolidColorBrush(Color.FromArgb(255, 21, 48, 72)),
            BorderThickness = new Thickness(1),
            CornerRadius    = new CornerRadius(6),
            ClipToBounds    = true,
            Child           = innerGrid,
        };

        var screen = SystemParameters.WorkArea;
        var toast = new Window
        {
            WindowStyle        = WindowStyle.None,
            AllowsTransparency = true,
            Background         = Brushes.Transparent,
            ResizeMode         = ResizeMode.NoResize,
            ShowInTaskbar      = false,
            Topmost            = true,
            Width              = W,
            SizeToContent      = SizeToContent.Height,
            Left               = screen.Left + 20,
            Top                = screen.Top + 20,
            Opacity            = 0,
            Content            = outerBorder,
        };

        toast.Loaded += (_, _) =>
        {
            toast.BeginAnimation(UIElement.OpacityProperty,
                new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(250)));

            var fullW = progressGrid.ActualWidth;
            progressFg.Width = fullW;
            progressFg.BeginAnimation(FrameworkElement.WidthProperty,
                new DoubleAnimation(fullW, 0, TimeSpan.FromSeconds(Duration)));

            var fadeTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(Duration - 1) };
            fadeTimer.Tick += (_, _) =>
            {
                fadeTimer.Stop();
                toast.BeginAnimation(UIElement.OpacityProperty,
                    new DoubleAnimation(1, 0, TimeSpan.FromMilliseconds(600)));
            };
            fadeTimer.Start();

            var closeTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(Duration) };
            closeTimer.Tick += (_, _) => { closeTimer.Stop(); toast.Close(); };
            closeTimer.Start();
        };

        _ue4ssRemapToast = toast;
        toast.Closed += (_, _) => { if (ReferenceEquals(_ue4ssRemapToast, toast)) _ue4ssRemapToast = null; };
        toast.Show();
    }

    private void RefreshUe4ssBtnStates()
    {
        for (int i = 0; i < _chapters.Count && i < _ue4ssBtns.Count; i++)
        {
            var exePath = GetActiveExePath(_chapters[i]);
            bool installed = false;
            if (!string.IsNullOrEmpty(exePath))
            {
                var win64 = FindWin64Dir(IOPath.GetDirectoryName(exePath)!);
                if (win64 != null)
                    installed = IsUe4ssInstalled(win64) && !LoadManipFilesService.IsUe4ssFromLoadManip(win64);
            }
            _ue4ssBtns[i].Opacity = installed ? 1.0 : 0.3;
        }
    }

    // ── Load Manip files ─────────────────────────────────────────────────────

    private void LoadManipCardBtn_Click(object sender, RoutedEventArgs e)
    {
        _loadManipTargetChapter = (int)((Button)sender).Tag;
        _loadManipUe4ssInstalledThisSession = false;

        if (!ComputeLoadManipTargets())
        {
            ShowLoadManipDialog("No game path found for this chapter.\nPlease set up the game installation first.");
            return;
        }

        UpdateLoadManipPopupState();
        OpenLoadManipOverlay();
    }

    /// <summary>Resolves the win64/paks/zip paths for the currently targeted chapter.
    /// Returns false (and leaves the targets null) when no game path is found.</summary>
    private bool ComputeLoadManipTargets()
    {
        var chapter = _chapters.FirstOrDefault(c => c.Number == _loadManipTargetChapter);
        _loadManipWin64Dir     = null;
        _loadManipPaksDir      = null;
        _loadManipZipPath      = null;
        _loadManipUe4ssZipPath = null;
        _loadManipMarkerZipPath = null;

        if (chapter != null)
        {
            var exePath = GetActiveExePath(chapter);
            if (!string.IsNullOrEmpty(exePath))
                _loadManipWin64Dir = FindWin64Dir(IOPath.GetDirectoryName(exePath)!);

            _loadManipZipPath      = LoadManipFilesService.GetZipPath(chapter.Number);
            _loadManipUe4ssZipPath = LoadManipFilesService.GetUe4ssZipPath(chapter.Number);
            _loadManipMarkerZipPath = LoadManipFilesService.GetPlaytimeMarkerZipPath(chapter.Number);
            if (_loadManipWin64Dir != null)
                _loadManipPaksDir = LoadManipFilesService.GetPaksDir(_loadManipWin64Dir);
        }

        return _loadManipWin64Dir != null;
    }

    /// <summary>Refreshes the popup's text/buttons for the current target without
    /// touching its visibility/animation, so callers can swap state in place.</summary>
    private void UpdateLoadManipPopupState()
    {
        bool needsUe4ss = (_loadManipTargetChapter == 1 || _loadManipTargetChapter == 5)
            && _loadManipWin64Dir != null && !IsUe4ssInstalled(_loadManipWin64Dir);
        bool installed  = !needsUe4ss
            && _loadManipPaksDir != null && _loadManipZipPath != null && File.Exists(_loadManipZipPath)
            && LoadManipFilesService.IsInstalled(_loadManipPaksDir, _loadManipZipPath);

        LoadManipYesBtn.Visibility           = Visibility.Collapsed;
        LoadManipDeleteBtn.Visibility        = Visibility.Collapsed;
        LoadManipInstallUe4ssBtn.Visibility  = Visibility.Collapsed;
        LoadManipWarningText.Visibility      = Visibility.Collapsed;

        if (needsUe4ss)
        {
            LoadManipPopupQuestion.Text = $"UE4SS is required before adding\nLoad Manip files for Chapter {_loadManipTargetChapter}.";
            LoadManipInstallUe4ssBtn.Visibility = Visibility.Visible;
            if (_loadManipTargetChapter == 1)
            {
                LoadManipWarningText.Text = "⚠ Only compatible with Patch 1.3";
                LoadManipWarningText.Visibility = Visibility.Visible;
            }
        }
        else if (installed)
        {
            bool fullBrightAlsoInstalled = (_loadManipTargetChapter == 1 || _loadManipTargetChapter == 5)
                && _loadManipWin64Dir != null && FullBrightFilesService.IsInstalled(_loadManipWin64Dir);
            LoadManipPopupQuestion.Text = "Do you want to remove Load Manip files\nfrom this version?";
            LoadManipDeleteBtn.Visibility = Visibility.Visible;
            if (fullBrightAlsoInstalled)
            {
                LoadManipWarningText.Text = "⚠ FullBright is installed on top of Load Manip\nand will be removed as well.";
                LoadManipWarningText.Visibility = Visibility.Visible;
            }
        }
        else
        {
            LoadManipPopupQuestion.Text = "Do you want to add Load Manip files\nto this version?";
            LoadManipYesBtn.Visibility = Visibility.Visible;
            if (_loadManipTargetChapter == 1)
            {
                LoadManipWarningText.Text = "⚠ Only compatible with Patch 1.3";
                LoadManipWarningText.Visibility = Visibility.Visible;
            }
        }
    }

    private void OpenLoadManipOverlay()
    {
        LoadManipOverlay.Opacity    = 0;
        LoadManipOverlay.Visibility = Visibility.Visible;
        var ease = new CubicEase { EasingMode = EasingMode.EaseOut };
        LoadManipOverlay.BeginAnimation(UIElement.OpacityProperty,
            new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(220)));
        LoadManipPopupScale.BeginAnimation(ScaleTransform.ScaleXProperty,
            new DoubleAnimation(0.85, 1, TimeSpan.FromMilliseconds(260)) { EasingFunction = ease });
        LoadManipPopupScale.BeginAnimation(ScaleTransform.ScaleYProperty,
            new DoubleAnimation(0.85, 1, TimeSpan.FromMilliseconds(260)) { EasingFunction = ease });
    }

    private void LoadManipNoBtn_Click(object sender, RoutedEventArgs e)
        => CloseLoadManipOverlay();

    private async void LoadManipYesBtn_Click(object sender, RoutedEventArgs e)
    {
        CloseLoadManipOverlay();

        if (_loadManipPaksDir is null || _loadManipWin64Dir is null)
        {
            ShowLoadManipDialog("No game path found for this chapter.\nPlease set up the game installation first.");
            return;
        }

        if (_loadManipZipPath is null || !File.Exists(_loadManipZipPath))
        {
            ShowLoadManipDialog("Load Manip zip not found. Try restarting the launcher.");
            return;
        }

        var win64Dir     = _loadManipWin64Dir;
        var paksDir      = _loadManipPaksDir;
        var zipPath      = _loadManipZipPath;
        var ue4ssZipPath = _loadManipUe4ssZipPath;
        var markerZipPath = _loadManipMarkerZipPath;

        if (_loadManipTargetChapter == 1 || _loadManipTargetChapter == 5)
        {
            if (ue4ssZipPath is null || !File.Exists(ue4ssZipPath))
            {
                ShowLoadManipDialog("Load Manip UE4SS zip not found. Try restarting the launcher.");
                return;
            }

            // If UE4SS was already present before this flow installed it (e.g. via the
            // standalone UE4SS card), its files must be replaced with the Load Manip build.
            if (!_loadManipUe4ssInstalledThisSession && IsUe4ssInstalled(win64Dir))
            {
                var confirmContent = new TextBlock
                {
                    Text         = "This requires deleting the current UE4SS files.\nDo you want to delete them?",
                    FontFamily   = new FontFamily("Cascadia Code, Consolas, Courier New"),
                    FontSize     = 12,
                    Foreground   = new SolidColorBrush(Color.FromArgb(200, 160, 180, 200)),
                    TextWrapping = TextWrapping.Wrap,
                    MaxWidth     = 360,
                };
                var confirmResult = WpfDialog.Show(this, "LOAD MANIP", confirmContent,
                    primaryText: "YES", closeText: "CANCEL");
                if (confirmResult != WpfDialogResult.Primary) return;

                try
                {
                    // Chapter 5 runs a newer engine build than Chapters 1-4, so it needs its
                    // own generic UE4SS build (see Ue4ssCardBtn_Click).
                    var genericUe4ssZip = _loadManipTargetChapter >= 5
                        ? IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Tools", "Chapter 5", "Ue4ss.zip")
                        : IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Tools", "Chapter 1 - 4", "Ue4ss.zip");
                    if (File.Exists(genericUe4ssZip))
                        await Task.Run(() => LoadManipFilesService.UninstallUe4ss(win64Dir, genericUe4ssZip));
                }
                catch (Exception ex)
                {
                    ShowLoadManipDialog($"Error deleting existing UE4SS files:\n{ex.Message}");
                    return;
                }
            }
        }

        try
        {
            await Task.Run(() =>
            {
                LoadManipFilesService.Install(paksDir, zipPath, LoadManipFilesService.GetConfigZipPath(_loadManipTargetChapter));
                if ((_loadManipTargetChapter == 1 || _loadManipTargetChapter == 5) && ue4ssZipPath != null)
                    LoadManipFilesService.InstallUe4ss(win64Dir, ue4ssZipPath, markerZipPath);
            });
            RefreshLoadManipBtnStates();
            RefreshUe4ssBtnStates();
            RefreshChapter1UI();
            RefreshChapter5LoadManipUI();
            ShowLoadManipDialog($"Load Manip files installed successfully!\n\n{paksDir}", success: true);
        }
        catch (Exception ex)
        {
            ShowLoadManipDialog($"Error installing Load Manip files:\n{ex.Message}");
        }
    }

    private async void LoadManipDeleteBtn_Click(object sender, RoutedEventArgs e)
    {
        CloseLoadManipOverlay();

        if (_loadManipPaksDir is null || _loadManipZipPath is null || !File.Exists(_loadManipZipPath))
            return;

        try
        {
            var paksDir       = _loadManipPaksDir;
            var zipPath       = _loadManipZipPath;
            var win64Dir      = _loadManipWin64Dir;
            var ue4ssZipPath  = _loadManipUe4ssZipPath;
            var markerZipPath = _loadManipMarkerZipPath;

            bool fullBrightInstalled = (_loadManipTargetChapter == 1 || _loadManipTargetChapter == 5)
                && win64Dir != null && FullBrightFilesService.IsInstalled(win64Dir);
            var fullBrightZip       = fullBrightInstalled ? FullBrightFilesService.GetZipPath(_loadManipTargetChapter) : null;
            var fullBrightUe4ssZip  = fullBrightInstalled ? FullBrightFilesService.GetUe4ssZipPath(_loadManipTargetChapter) : null;
            var fullBrightMarkerZip = fullBrightInstalled ? FullBrightFilesService.GetPlaytimeMarkerZipPath(_loadManipTargetChapter) : null;

            await Task.Run(() =>
            {
                // FullBright can't exist without Load Manip — its pak/UE4SS overwrote Load
                // Manip's own files (same paths). Remove it first, otherwise its
                // fullbright.playtime marker survives and keeps reporting FullBright as
                // "installed" even after Load Manip (and most of its files) are gone.
                if (fullBrightInstalled && win64Dir != null
                    && fullBrightZip != null && File.Exists(fullBrightZip)
                    && fullBrightUe4ssZip != null && File.Exists(fullBrightUe4ssZip)
                    && fullBrightMarkerZip != null && File.Exists(fullBrightMarkerZip))
                {
                    FullBrightFilesService.Uninstall(paksDir, win64Dir, fullBrightZip, fullBrightUe4ssZip, fullBrightMarkerZip);
                }

                LoadManipFilesService.Uninstall(paksDir, zipPath);

                // Also remove the UE4SS build Load Manip installed, so the standalone
                // UE4SS card correctly reports "not installed" afterward.
                if (win64Dir != null && ue4ssZipPath != null && File.Exists(ue4ssZipPath)
                    && LoadManipFilesService.IsUe4ssFromLoadManip(win64Dir))
                {
                    LoadManipFilesService.UninstallUe4ss(win64Dir, ue4ssZipPath, markerZipPath);
                }
            });
            RefreshLoadManipBtnStates();
            RefreshUe4ssBtnStates();
            RefreshFullBrightBtnStates();
            RefreshFullBrightKeysUI();
            RefreshChapter5FullBrightUI();
            RefreshChapter1UI();
            RefreshChapter5LoadManipUI();
            ShowLoadManipDialog(fullBrightInstalled
                ? "Load Manip and FullBright files removed successfully!"
                : "Load Manip files removed successfully!", success: true);
        }
        catch (Exception ex)
        {
            ShowLoadManipDialog($"Error removing Load Manip files:\n{ex.Message}");
        }
    }

    private async void LoadManipInstallUe4ssBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_loadManipWin64Dir is null)
        {
            CloseLoadManipOverlay();
            ShowLoadManipDialog("No game path found for this chapter.\nPlease set up the game installation first.");
            return;
        }

        // Load Manip's UE4SS dependency applies to Chapters 1 and 5. Chapter 5 runs a
        // newer engine build than Chapters 1-4, so it needs its own generic UE4SS build
        // (see Ue4ssCardBtn_Click).
        var ue4ssZipPath = _loadManipTargetChapter >= 5
            ? IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Tools", "Chapter 5", "Ue4ss.zip")
            : IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Tools", "Chapter 1 - 4", "Ue4ss.zip");
        if (!File.Exists(ue4ssZipPath))
        {
            CloseLoadManipOverlay();
            ShowLoadManipDialog("UE4SS zip not found. Try restarting the launcher.");
            return;
        }

        LoadManipInstallUe4ssBtn.IsEnabled = false;
        try
        {
            var win64 = _loadManipWin64Dir;
            await Task.Run(() => ZipFile.ExtractToDirectory(ue4ssZipPath, win64, overwriteFiles: true));
            _loadManipUe4ssInstalledThisSession = true;
            RefreshUe4ssBtnStates();
            // Swap straight to the "add Load Manip files" state in the same popup
            // instead of closing it, since UE4SS is now installed.
            UpdateLoadManipPopupState();
        }
        catch (Exception ex)
        {
            CloseLoadManipOverlay();
            ShowLoadManipDialog($"Error installing UE4SS:\n{ex.Message}");
        }
        finally
        {
            LoadManipInstallUe4ssBtn.IsEnabled = true;
        }
    }

    private void CloseLoadManipOverlay()
    {
        var ease = new CubicEase { EasingMode = EasingMode.EaseIn };
        var fade = new DoubleAnimation(1, 0, TimeSpan.FromMilliseconds(180)) { EasingFunction = ease };
        fade.Completed += (_, _) => LoadManipOverlay.Visibility = Visibility.Collapsed;
        LoadManipOverlay.BeginAnimation(UIElement.OpacityProperty, fade);
    }

    private void ShowLoadManipDialog(string message, bool success = false)
    {
        WpfDialog.Show(this, "LOAD MANIP", new TextBlock
        {
            Text         = message,
            FontFamily   = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize     = 12,
            Foreground   = new SolidColorBrush(success
                ? Color.FromArgb(200, 0, 204, 170)
                : Color.FromArgb(200, 160, 180, 200)),
            TextWrapping = TextWrapping.Wrap,
            MaxWidth     = 360,
        }, closeText: "OK");
    }

    private void RefreshLoadManipBtnStates()
    {
        foreach (var (chapterNumber, btn) in _loadManipBtns)
        {
            var chapter = _chapters.FirstOrDefault(c => c.Number == chapterNumber);
            var exePath = chapter != null ? GetActiveExePath(chapter) : null;
            bool installed = false;
            if (!string.IsNullOrEmpty(exePath))
            {
                var win64 = FindWin64Dir(IOPath.GetDirectoryName(exePath)!);
                var zipPath = LoadManipFilesService.GetZipPath(chapterNumber);
                if (win64 != null && zipPath != null)
                {
                    var paksDir = LoadManipFilesService.GetPaksDir(win64);
                    if (paksDir != null)
                        installed = LoadManipFilesService.IsInstalled(paksDir, zipPath);
                }
            }
            btn.Opacity = installed ? 1.0 : 0.3;
        }
    }

    // ── FullBright (Chapters 1 and 5) ─────────────────────────────────────────

    private void FullBrightCardBtn_Click(object sender, RoutedEventArgs e)
    {
        _fullBrightTargetChapter = (int)((Button)sender).Tag;

        if (!ComputeFullBrightTargets())
        {
            ShowFullBrightDialog("No game path found for this chapter.\nPlease set up the game installation first.");
            return;
        }

        UpdateFullBrightPopupState();
        OpenFullBrightOverlay();
    }

    /// <summary>Resolves the win64/paks/zip paths for the currently targeted chapter.
    /// Returns false (and leaves the targets null) when no game path is found.</summary>
    private bool ComputeFullBrightTargets()
    {
        var chapter = _chapters.FirstOrDefault(c => c.Number == _fullBrightTargetChapter);
        _fullBrightWin64Dir      = null;
        _fullBrightPaksDir       = null;
        _fullBrightZipPath       = null;
        _fullBrightUe4ssZipPath  = null;
        _fullBrightMarkerZipPath = null;
        _fullBrightConfigZipPath = null;

        if (chapter != null)
        {
            var exePath = GetActiveExePath(chapter);
            if (!string.IsNullOrEmpty(exePath))
                _fullBrightWin64Dir = FindWin64Dir(IOPath.GetDirectoryName(exePath)!);

            _fullBrightZipPath       = FullBrightFilesService.GetZipPath(chapter.Number);
            _fullBrightUe4ssZipPath  = FullBrightFilesService.GetUe4ssZipPath(chapter.Number);
            _fullBrightMarkerZipPath = FullBrightFilesService.GetPlaytimeMarkerZipPath(chapter.Number);
            _fullBrightConfigZipPath = FullBrightFilesService.GetConfigZipPath(chapter.Number);
            if (_fullBrightWin64Dir != null)
                _fullBrightPaksDir = LoadManipFilesService.GetPaksDir(_fullBrightWin64Dir);
        }

        return _fullBrightWin64Dir != null;
    }

    private void UpdateFullBrightPopupState()
    {
        bool installed = _fullBrightWin64Dir != null && FullBrightFilesService.IsInstalled(_fullBrightWin64Dir);

        FullBrightYesBtn.Visibility    = installed ? Visibility.Collapsed : Visibility.Visible;
        FullBrightDeleteBtn.Visibility = installed ? Visibility.Visible : Visibility.Collapsed;
        FullBrightWarningText.Visibility = Visibility.Collapsed;

        FullBrightPopupQuestion.Text = installed
            ? "Do you want to remove FullBright files\nfrom this version?"
            : "Do you want to add FullBright files\nto this version?";

        if (installed)
        {
            FullBrightWarningText.Text = "⚠ Removing FullBright also removes Load Manip's\noverwritten files — Load Manip will be reinstalled automatically.";
            FullBrightWarningText.Visibility = Visibility.Visible;
        }
        else if (_fullBrightTargetChapter == 1)
        {
            FullBrightWarningText.Text = "⚠ Only compatible with Patch 1.3";
            FullBrightWarningText.Visibility = Visibility.Visible;
        }
    }

    private void OpenFullBrightOverlay()
    {
        FullBrightOverlay.Opacity    = 0;
        FullBrightOverlay.Visibility = Visibility.Visible;
        var ease = new CubicEase { EasingMode = EasingMode.EaseOut };
        FullBrightOverlay.BeginAnimation(UIElement.OpacityProperty,
            new DoubleAnimation(0, 1, TimeSpan.FromMilliseconds(220)));
        FullBrightPopupScale.BeginAnimation(ScaleTransform.ScaleXProperty,
            new DoubleAnimation(0.85, 1, TimeSpan.FromMilliseconds(260)) { EasingFunction = ease });
        FullBrightPopupScale.BeginAnimation(ScaleTransform.ScaleYProperty,
            new DoubleAnimation(0.85, 1, TimeSpan.FromMilliseconds(260)) { EasingFunction = ease });
    }

    private void CloseFullBrightOverlay()
    {
        var ease = new CubicEase { EasingMode = EasingMode.EaseIn };
        var fade = new DoubleAnimation(1, 0, TimeSpan.FromMilliseconds(180)) { EasingFunction = ease };
        fade.Completed += (_, _) => FullBrightOverlay.Visibility = Visibility.Collapsed;
        FullBrightOverlay.BeginAnimation(UIElement.OpacityProperty, fade);
    }

    private void FullBrightNoBtn_Click(object sender, RoutedEventArgs e)
        => CloseFullBrightOverlay();

    private void ShowFullBrightDialog(string message, bool success = false)
    {
        WpfDialog.Show(this, "FULLBRIGHT", new TextBlock
        {
            Text         = message,
            FontFamily   = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize     = 12,
            Foreground   = new SolidColorBrush(success
                ? Color.FromArgb(200, 0, 204, 170)
                : Color.FromArgb(200, 160, 180, 200)),
            TextWrapping = TextWrapping.Wrap,
            MaxWidth     = 360,
        }, closeText: "OK");
    }

    /// <summary>Ensures Load Manip (for <see cref="_fullBrightTargetChapter"/>) is fully installed
    /// — pak, UE4SS build, and launcher.playtime marker — before FullBright is laid on top of it.
    /// Duplicates the relevant steps of LoadManipYesBtn_Click rather than calling into it, so that
    /// method's own code path stays completely untouched. Returns false (after showing a dialog,
    /// if appropriate) on failure or if the user cancels the "delete existing UE4SS" confirm.</summary>
    private async Task<bool> EnsureLoadManipInstalledAsync(string win64Dir, string paksDir)
    {
        var chapterNumber = _fullBrightTargetChapter;
        var zipPath       = LoadManipFilesService.GetZipPath(chapterNumber);
        var ue4ssZipPath  = LoadManipFilesService.GetUe4ssZipPath(chapterNumber);
        var markerZipPath = LoadManipFilesService.GetPlaytimeMarkerZipPath(chapterNumber);

        if (zipPath is null || !File.Exists(zipPath) || ue4ssZipPath is null || !File.Exists(ue4ssZipPath))
        {
            ShowFullBrightDialog("Load Manip zip not found. Try restarting the launcher.");
            return false;
        }

        if (LoadManipFilesService.IsInstalled(paksDir, zipPath))
            return true;

        // Same "existing non-Load-Manip UE4SS must be confirmed-deleted first" branch as
        // LoadManipYesBtn_Click, duplicated so that method is never modified.
        if (IsUe4ssInstalled(win64Dir) && !LoadManipFilesService.IsUe4ssFromLoadManip(win64Dir))
        {
            var confirmContent = new TextBlock
            {
                Text         = "This requires deleting the current UE4SS files.\nDo you want to delete them?",
                FontFamily   = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize     = 12,
                Foreground   = new SolidColorBrush(Color.FromArgb(200, 160, 180, 200)),
                TextWrapping = TextWrapping.Wrap,
                MaxWidth     = 360,
            };
            var confirmResult = WpfDialog.Show(this, "FULLBRIGHT", confirmContent,
                primaryText: "YES", closeText: "CANCEL");
            if (confirmResult != WpfDialogResult.Primary) return false;

            try
            {
                // Chapter 5 runs a newer engine build than Chapters 1-4, so it needs its
                // own generic UE4SS build (see Ue4ssCardBtn_Click).
                var genericUe4ssZip = chapterNumber >= 5
                    ? IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Tools", "Chapter 5", "Ue4ss.zip")
                    : IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Tools", "Chapter 1 - 4", "Ue4ss.zip");
                if (File.Exists(genericUe4ssZip))
                    await Task.Run(() => LoadManipFilesService.UninstallUe4ss(win64Dir, genericUe4ssZip));
            }
            catch (Exception ex)
            {
                ShowFullBrightDialog($"Error deleting existing UE4SS files:\n{ex.Message}");
                return false;
            }
        }

        try
        {
            await Task.Run(() =>
            {
                LoadManipFilesService.Install(paksDir, zipPath, LoadManipFilesService.GetConfigZipPath(chapterNumber));
                LoadManipFilesService.InstallUe4ss(win64Dir, ue4ssZipPath, markerZipPath);
            });
            RefreshLoadManipBtnStates();
            RefreshUe4ssBtnStates();
            return true;
        }
        catch (Exception ex)
        {
            ShowFullBrightDialog($"Error installing Load Manip files:\n{ex.Message}");
            return false;
        }
    }

    private async void FullBrightYesBtn_Click(object sender, RoutedEventArgs e)
    {
        CloseFullBrightOverlay();

        if (_fullBrightPaksDir is null || _fullBrightWin64Dir is null)
        {
            ShowFullBrightDialog("No game path found for this chapter.\nPlease set up the game installation first.");
            return;
        }

        if (_fullBrightZipPath is null || !File.Exists(_fullBrightZipPath)
            || _fullBrightUe4ssZipPath is null || !File.Exists(_fullBrightUe4ssZipPath)
            || _fullBrightMarkerZipPath is null || !File.Exists(_fullBrightMarkerZipPath))
        {
            ShowFullBrightDialog("FullBright zip not found. Try restarting the launcher.");
            return;
        }

        var win64Dir      = _fullBrightWin64Dir;
        var paksDir       = _fullBrightPaksDir;
        var zipPath       = _fullBrightZipPath;
        var ue4ssZipPath  = _fullBrightUe4ssZipPath;
        var markerZipPath = _fullBrightMarkerZipPath;
        var configZipPath = _fullBrightConfigZipPath;

        var loadManipPaksDir = LoadManipFilesService.GetPaksDir(win64Dir);
        if (loadManipPaksDir is null)
        {
            ShowFullBrightDialog("No game path found for this chapter.\nPlease set up the game installation first.");
            return;
        }

        var loadManipZipPath = LoadManipFilesService.GetZipPath(_fullBrightTargetChapter);
        bool loadManipInstalled = loadManipZipPath != null && File.Exists(loadManipZipPath)
            && LoadManipFilesService.IsInstalled(loadManipPaksDir, loadManipZipPath);

        if (!loadManipInstalled)
        {
            var confirmContent = new TextBlock
            {
                Text         = "FullBright requires Load Manip.\nInstall Load Manip now?",
                FontFamily   = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize     = 12,
                Foreground   = new SolidColorBrush(Color.FromArgb(200, 160, 180, 200)),
                TextWrapping = TextWrapping.Wrap,
                MaxWidth     = 360,
            };
            var confirmResult = WpfDialog.Show(this, "FULLBRIGHT", confirmContent,
                primaryText: "YES", closeText: "CANCEL");
            if (confirmResult != WpfDialogResult.Primary) return;

            if (!await EnsureLoadManipInstalledAsync(win64Dir, loadManipPaksDir))
                return;
        }

        try
        {
            await Task.Run(() =>
                FullBrightFilesService.Install(paksDir, win64Dir, zipPath, ue4ssZipPath, markerZipPath, configZipPath));
            RefreshFullBrightBtnStates();
            RefreshFullBrightKeysUI();
            RefreshChapter5FullBrightUI();
            RefreshLoadManipBtnStates();
            RefreshUe4ssBtnStates();
            RefreshChapter1UI();
            RefreshChapter5LoadManipUI();
            ShowFullBrightDialog($"FullBright files installed successfully!\n\n{paksDir}", success: true);
        }
        catch (Exception ex)
        {
            ShowFullBrightDialog($"Error installing FullBright files:\n{ex.Message}");
        }
    }

    private async void FullBrightDeleteBtn_Click(object sender, RoutedEventArgs e)
    {
        CloseFullBrightOverlay();

        if (_fullBrightPaksDir is null || _fullBrightWin64Dir is null
            || _fullBrightZipPath is null || !File.Exists(_fullBrightZipPath)
            || _fullBrightUe4ssZipPath is null || !File.Exists(_fullBrightUe4ssZipPath)
            || _fullBrightMarkerZipPath is null || !File.Exists(_fullBrightMarkerZipPath))
            return;

        var win64Dir      = _fullBrightWin64Dir;
        var paksDir       = _fullBrightPaksDir;
        var zipPath       = _fullBrightZipPath;
        var ue4ssZipPath  = _fullBrightUe4ssZipPath;
        var markerZipPath = _fullBrightMarkerZipPath;

        var loadManipZipPath      = LoadManipFilesService.GetZipPath(_fullBrightTargetChapter);
        var loadManipUe4ssZipPath = LoadManipFilesService.GetUe4ssZipPath(_fullBrightTargetChapter);
        var loadManipMarkerZipPath = LoadManipFilesService.GetPlaytimeMarkerZipPath(_fullBrightTargetChapter);

        try
        {
            await Task.Run(() =>
            {
                // Keep config.ini (holds the user's chosen keybinds) even though it ships in
                // the config zip — only pak/UE4SS/marker get removed here.
                FullBrightFilesService.Uninstall(paksDir, win64Dir, zipPath, ue4ssZipPath, markerZipPath);

                // FullBright's pak/UE4SS overwrote Load Manip's own files (same paths), so
                // removing FullBright also removed them — restore Load Manip's base files
                // so it stays installed/functional afterward.
                if (loadManipZipPath != null && File.Exists(loadManipZipPath)
                    && loadManipUe4ssZipPath != null && File.Exists(loadManipUe4ssZipPath))
                {
                    LoadManipFilesService.Install(paksDir, loadManipZipPath, LoadManipFilesService.GetConfigZipPath(_fullBrightTargetChapter));
                    LoadManipFilesService.InstallUe4ss(win64Dir, loadManipUe4ssZipPath, loadManipMarkerZipPath);
                }
            });
            RefreshFullBrightBtnStates();
            RefreshFullBrightKeysUI();
            RefreshChapter5FullBrightUI();
            RefreshLoadManipBtnStates();
            RefreshUe4ssBtnStates();
            RefreshChapter1UI();
            RefreshChapter5LoadManipUI();
            ShowFullBrightDialog("FullBright files removed and Load Manip restored successfully!", success: true);
        }
        catch (Exception ex)
        {
            ShowFullBrightDialog($"Error removing FullBright files:\n{ex.Message}");
        }
    }

    private void RefreshFullBrightBtnStates()
    {
        foreach (var (chapterNumber, btn) in _fullBrightBtns)
        {
            var chapter = _chapters.FirstOrDefault(c => c.Number == chapterNumber);
            var exePath = chapter != null ? GetActiveExePath(chapter) : null;
            bool installed = false;
            if (!string.IsNullOrEmpty(exePath))
            {
                var win64 = FindWin64Dir(IOPath.GetDirectoryName(exePath)!);
                if (win64 != null)
                    installed = FullBrightFilesService.IsInstalled(win64);
            }
            btn.Opacity = installed ? 1.0 : 0.3;
        }
    }

    /// <summary>Resolves the live, already-installed FullBright config.ini for the currently
    /// active install of the given chapter, or null if FullBright isn't installed there.</summary>
    private string? GetActiveFullBrightConfigPath(int chapterNumber = 1)
    {
        var chapter = _chapters.FirstOrDefault(c => c.Number == chapterNumber);
        var exePath = chapter != null ? GetActiveExePath(chapter) : null;
        if (string.IsNullOrEmpty(exePath)) return null;

        var win64 = FindWin64Dir(IOPath.GetDirectoryName(exePath)!);
        if (win64 is null) return null;

        var projectRoot = FullBrightFilesService.GetProjectRoot(win64);
        if (projectRoot is null) return null;

        var path = IOPath.Combine(projectRoot, FullBrightFilesService.ConfigFileName);
        return File.Exists(path) ? path : null;
    }

    /// <summary>Shows editable Unlit/Lit key rows reading from the live installed config.ini
    /// when FullBright is installed for the active Chapter 1 exe, or "(fullbright not installed)"
    /// otherwise.</summary>
    private void RefreshFullBrightKeysUI()
    {
        if (_capturingFullBrightKey) CancelFullBrightCapture();

        var path = GetActiveFullBrightConfigPath();
        var (unlit, lit) = path != null ? ParseFullBrightConfig(path) : (null, null);
        bool editable = unlit != null && lit != null;

        Chapter1FullbrightUnlitRow.Visibility         = editable ? Visibility.Visible : Visibility.Collapsed;
        Chapter1FullbrightLitRow.Visibility           = editable ? Visibility.Visible : Visibility.Collapsed;
        Chapter1FullbrightNotInstalledText.Visibility = editable ? Visibility.Collapsed : Visibility.Visible;
        Chapter1FullbrightNotInstalledText.Text       = Loc.Get("chapter1_fullbright_not_installed");

        var normalBrush = new SolidColorBrush(Color.FromArgb(255, 26, 58, 85));
        var normalFg    = new SolidColorBrush(Color.FromArgb(255, 138, 170, 187));
        Chapter1FullbrightUnlitBtn.BorderBrush = normalBrush;
        Chapter1FullbrightLitBtn.BorderBrush   = normalBrush;
        Chapter1FullbrightUnlitText.Foreground = normalFg;
        Chapter1FullbrightLitText.Foreground   = normalFg;

        if (editable)
        {
            Chapter1FullbrightUnlitText.Text = LoadManipKeyDisplayName(unlit!);
            Chapter1FullbrightLitText.Text   = LoadManipKeyDisplayName(lit!);
        }
    }

    private static (string? unlit, string? lit) ParseFullBrightConfig(string path)
    {
        string? unlit = null, lit = null;
        foreach (var line in File.ReadAllLines(path))
        {
            var t = line.Trim();
            if (t.Length == 0 || t.StartsWith("#") || t.StartsWith("[") || !t.Contains('='))
                continue;

            var parts = t.Split('=', 2);
            var key   = parts[0].Trim();
            var value = parts[1].Trim();
            if (key.Equals("KeyUnlit", StringComparison.OrdinalIgnoreCase)) unlit = value;
            else if (key.Equals("KeyLit", StringComparison.OrdinalIgnoreCase)) lit = value;
        }
        return (unlit, lit);
    }

    // ── FullBright key capture (overlay-only — writes directly to config.ini, no OS hook) ────

    private void Chapter1FullbrightUnlitBtn_Click(object sender, RoutedEventArgs e) =>
        StartFullBrightCapture("KeyUnlit", Chapter1FullbrightUnlitBtn, Chapter1FullbrightUnlitText);
    private void Chapter1FullbrightLitBtn_Click(object sender, RoutedEventArgs e) =>
        StartFullBrightCapture("KeyLit", Chapter1FullbrightLitBtn, Chapter1FullbrightLitText);

    private void StartFullBrightCapture(string configKey, Button btn, TextBlock text)
    {
        if (_capturingFullBrightKey)
        {
            var wasThis = _fullBrightCaptureTarget == configKey;
            CancelFullBrightCapture();
            RefreshFullBrightKeysUI();
            if (wasThis) return;
        }

        _capturingFullBrightKey  = true;
        _fullBrightCaptureTarget = configKey;
        text.Text       = Loc.Get("f11_remap_press_input");
        text.Foreground = new SolidColorBrush(Teal);
        btn.BorderBrush = new SolidColorBrush(Teal);

        _fullBrightKeyCapture = CaptureFullBrightKeyDown;
        AddHandler(UIElement.PreviewKeyDownEvent, _fullBrightKeyCapture, true);

        _fullBrightMouseCapture = CaptureFullBrightMouseDown;
        AddHandler(UIElement.PreviewMouseDownEvent, _fullBrightMouseCapture, true);
    }

    private void CancelFullBrightCapture()
    {
        _capturingFullBrightKey = false;
        if (_fullBrightKeyCapture != null)
        {
            RemoveHandler(UIElement.PreviewKeyDownEvent, _fullBrightKeyCapture);
            _fullBrightKeyCapture = null;
        }
        if (_fullBrightMouseCapture != null)
        {
            RemoveHandler(UIElement.PreviewMouseDownEvent, _fullBrightMouseCapture);
            _fullBrightMouseCapture = null;
        }
    }

    private void CaptureFullBrightKeyDown(object sender, KeyEventArgs e)
    {
        var key = e.Key == Key.System ? e.SystemKey : e.Key;

        if (key is Key.LeftCtrl or Key.RightCtrl
                or Key.LeftShift or Key.RightShift
                or Key.LeftAlt or Key.RightAlt
                or Key.LWin or Key.RWin
                or Key.None)
            return;

        var target = _fullBrightCaptureTarget!;
        CancelFullBrightCapture();

        if (key == Key.Escape)
        {
            RefreshFullBrightKeysUI();
            e.Handled = true;
            return;
        }

        var name = WpfKeyToUnrealKeyName(key);
        if (name != null) ApplyFullBrightKey(target, name);
        else RefreshFullBrightKeysUI();
        e.Handled = true;
    }

    private void CaptureFullBrightMouseDown(object sender, MouseButtonEventArgs e)
    {
        var name = e.ChangedButton switch
        {
            MouseButton.Middle   => "MIDDLE_MOUSE_BUTTON",
            MouseButton.XButton1 => "XBUTTON_ONE",
            MouseButton.XButton2 => "XBUTTON_TWO",
            _ => null,
        };
        if (name is null) return;

        var target = _fullBrightCaptureTarget!;
        CancelFullBrightCapture();
        ApplyFullBrightKey(target, name);
        e.Handled = true;
    }

    private void ApplyFullBrightKey(string configKey, string value)
    {
        var path = GetActiveFullBrightConfigPath();
        if (path != null)
            FullBrightFilesService.UpdateConfigKey(path, configKey, value);
        RefreshFullBrightKeysUI();
    }

    // ── Chapter 5 FullBright key capture (mirrors the block above for Chapter 1) ────────────

    /// <summary>Shows editable Unlit/Lit key rows reading from the live installed config.ini
    /// when FullBright is installed for the active Chapter 5 exe, or "(fullbright not installed)"
    /// otherwise. Mirrors <see cref="RefreshFullBrightKeysUI"/>.</summary>
    private void RefreshChapter5FullBrightUI()
    {
        if (_capturingChapter5FullBrightKey) CancelChapter5FullBrightCapture();

        var path = GetActiveFullBrightConfigPath(5);
        var (unlit, lit) = path != null ? ParseFullBrightConfig(path) : (null, null);
        bool editable = unlit != null && lit != null;

        Chapter5FullbrightUnlitRow.Visibility         = editable ? Visibility.Visible : Visibility.Collapsed;
        Chapter5FullbrightLitRow.Visibility           = editable ? Visibility.Visible : Visibility.Collapsed;
        Chapter5FullbrightNotInstalledText.Visibility = editable ? Visibility.Collapsed : Visibility.Visible;
        Chapter5FullbrightNotInstalledText.Text       = Loc.Get("chapter5_fullbright_not_installed");

        var normalBrush = new SolidColorBrush(Color.FromArgb(255, 26, 58, 85));
        var normalFg    = new SolidColorBrush(Color.FromArgb(255, 138, 170, 187));
        Chapter5FullbrightUnlitBtn.BorderBrush = normalBrush;
        Chapter5FullbrightLitBtn.BorderBrush   = normalBrush;
        Chapter5FullbrightUnlitText.Foreground = normalFg;
        Chapter5FullbrightLitText.Foreground   = normalFg;

        if (editable)
        {
            Chapter5FullbrightUnlitText.Text = LoadManipKeyDisplayName(unlit!);
            Chapter5FullbrightLitText.Text   = LoadManipKeyDisplayName(lit!);
        }
    }

    private void Chapter5FullbrightUnlitBtn_Click(object sender, RoutedEventArgs e) =>
        StartChapter5FullBrightCapture("KeyUnlit", Chapter5FullbrightUnlitBtn, Chapter5FullbrightUnlitText);
    private void Chapter5FullbrightLitBtn_Click(object sender, RoutedEventArgs e) =>
        StartChapter5FullBrightCapture("KeyLit", Chapter5FullbrightLitBtn, Chapter5FullbrightLitText);

    private void StartChapter5FullBrightCapture(string configKey, Button btn, TextBlock text)
    {
        if (_capturingChapter5FullBrightKey)
        {
            var wasThis = _chapter5FullBrightCaptureTarget == configKey;
            CancelChapter5FullBrightCapture();
            RefreshChapter5FullBrightUI();
            if (wasThis) return;
        }

        _capturingChapter5FullBrightKey  = true;
        _chapter5FullBrightCaptureTarget = configKey;
        text.Text       = Loc.Get("f11_remap_press_input");
        text.Foreground = new SolidColorBrush(Teal);
        btn.BorderBrush = new SolidColorBrush(Teal);

        _chapter5FullBrightKeyCapture = CaptureChapter5FullBrightKeyDown;
        AddHandler(UIElement.PreviewKeyDownEvent, _chapter5FullBrightKeyCapture, true);

        _chapter5FullBrightMouseCapture = CaptureChapter5FullBrightMouseDown;
        AddHandler(UIElement.PreviewMouseDownEvent, _chapter5FullBrightMouseCapture, true);
    }

    private void CancelChapter5FullBrightCapture()
    {
        _capturingChapter5FullBrightKey = false;
        if (_chapter5FullBrightKeyCapture != null)
        {
            RemoveHandler(UIElement.PreviewKeyDownEvent, _chapter5FullBrightKeyCapture);
            _chapter5FullBrightKeyCapture = null;
        }
        if (_chapter5FullBrightMouseCapture != null)
        {
            RemoveHandler(UIElement.PreviewMouseDownEvent, _chapter5FullBrightMouseCapture);
            _chapter5FullBrightMouseCapture = null;
        }
    }

    private void CaptureChapter5FullBrightKeyDown(object sender, KeyEventArgs e)
    {
        var key = e.Key == Key.System ? e.SystemKey : e.Key;

        if (key is Key.LeftCtrl or Key.RightCtrl
                or Key.LeftShift or Key.RightShift
                or Key.LeftAlt or Key.RightAlt
                or Key.LWin or Key.RWin
                or Key.None)
            return;

        var target = _chapter5FullBrightCaptureTarget!;
        CancelChapter5FullBrightCapture();

        if (key == Key.Escape)
        {
            RefreshChapter5FullBrightUI();
            e.Handled = true;
            return;
        }

        var name = WpfKeyToUnrealKeyName(key);
        if (name != null) ApplyChapter5FullBrightKey(target, name);
        else RefreshChapter5FullBrightUI();
        e.Handled = true;
    }

    private void CaptureChapter5FullBrightMouseDown(object sender, MouseButtonEventArgs e)
    {
        var name = e.ChangedButton switch
        {
            MouseButton.Middle   => "MIDDLE_MOUSE_BUTTON",
            MouseButton.XButton1 => "XBUTTON_ONE",
            MouseButton.XButton2 => "XBUTTON_TWO",
            _ => null,
        };
        if (name is null) return;

        var target = _chapter5FullBrightCaptureTarget!;
        CancelChapter5FullBrightCapture();
        ApplyChapter5FullBrightKey(target, name);
        e.Handled = true;
    }

    private void ApplyChapter5FullBrightKey(string configKey, string value)
    {
        var path = GetActiveFullBrightConfigPath(5);
        if (path != null)
            FullBrightFilesService.UpdateConfigKey(path, configKey, value);
        RefreshChapter5FullBrightUI();
    }

    /// <summary>Maps a WPF key to the key-name string FullBright's config.ini (and Unreal's
    /// input system) expects — e.g. Key.K -&gt; "K", Key.F5 -&gt; "F5", Key.NumPad1 -&gt; "NumPadOne".
    /// Returns null for keys with no sensible mapping.</summary>
    private static string? WpfKeyToUnrealKeyName(Key key)
    {
        if (key >= Key.A && key <= Key.Z) return key.ToString();
        if (key >= Key.F1 && key <= Key.F24) return key.ToString();

        return key switch
        {
            Key.D0 => "Zero", Key.D1 => "One", Key.D2 => "Two", Key.D3 => "Three", Key.D4 => "Four",
            Key.D5 => "Five", Key.D6 => "Six", Key.D7 => "Seven", Key.D8 => "Eight", Key.D9 => "Nine",

            Key.NumPad0 => "NumPadZero", Key.NumPad1 => "NumPadOne", Key.NumPad2 => "NumPadTwo",
            Key.NumPad3 => "NumPadThree", Key.NumPad4 => "NumPadFour", Key.NumPad5 => "NumPadFive",
            Key.NumPad6 => "NumPadSix", Key.NumPad7 => "NumPadSeven", Key.NumPad8 => "NumPadEight",
            Key.NumPad9 => "NumPadNine",
            Key.Multiply => "Multiply", Key.Add => "Add", Key.Subtract => "Subtract",
            Key.Divide => "Divide", Key.Decimal => "Decimal",

            Key.Left => "Left", Key.Right => "Right", Key.Up => "Up", Key.Down => "Down",

            Key.Space => "SpaceBar", Key.Enter => "Enter", Key.Tab => "Tab", Key.Back => "BackSpace",
            Key.Delete => "Delete", Key.Insert => "Insert", Key.Home => "Home", Key.End => "End",
            Key.PageUp => "PageUp", Key.PageDown => "PageDown", Key.CapsLock => "CapsLock",
            Key.NumLock => "NumLock", Key.Scroll => "ScrollLock", Key.Pause => "Pause",

            Key.OemComma => "Comma", Key.OemPeriod => "Period", Key.OemQuestion => "Slash",
            Key.OemSemicolon => "Semicolon", Key.OemQuotes => "Quote",
            Key.OemOpenBrackets => "LeftBracket", Key.OemCloseBrackets => "RightBracket",
            Key.OemPipe => "Backslash", Key.OemMinus => "Hyphen", Key.OemPlus => "Equals",
            Key.OemTilde => "Tilde",

            _ => null,
        };
    }

    // ── Hand Mods (per-chapter hand-skin paks fetched live from GitHub) ─────────
    //
    // Flow: HandModsMenuButton opens straight to a chapter picker (Screen 0 — HandModsHubScroll),
    // then pick which installed version to target (Screen 1 — HandModsVersionList), then pick a
    // mod for that version (Screen 2 — HandModsList, showing real install state plus an info
    // button for each mod's declared hand color(s)), then a dedicated progress screen
    // (Screen 3 — HandModsInstallPanel) drives the download/install.
    private void HandModsChapter1Btn_Click(object sender, RoutedEventArgs e) => OpenHandModsOverlay(1);
    private void HandModsChapter2Btn_Click(object sender, RoutedEventArgs e) => OpenHandModsOverlay(2);
    private void HandModsChapter3Btn_Click(object sender, RoutedEventArgs e) => OpenHandModsOverlay(3);
    private void HandModsChapter4Btn_Click(object sender, RoutedEventArgs e) => OpenHandModsOverlay(4);
    private void HandModsChapter5Btn_Click(object sender, RoutedEventArgs e) => OpenHandModsOverlay(5);

    private void OpenHandModsHub()
    {
        ShowHandModsScreenHub();
        HandModsOverlay.Visibility = Visibility.Visible;
    }

    private void OpenHandModsOverlay(int chapterNumber)
    {
        _handModsTargetChapter = chapterNumber;
        _handModsWin64Dir = null;
        _handModsPaksDir  = null;
        _handModsList     = null;

        BuildHandModsVersionList();
        ShowHandModsScreenVersions();
        HandModsOverlay.Visibility = Visibility.Visible;
    }

    private void CloseHandModsBtn_Click(object sender, RoutedEventArgs e) =>
        HandModsOverlay.Visibility = Visibility.Collapsed;

    // Single back button shared by every non-root screen — walks back exactly one step:
    // the mods list returns to the version picker, the version picker returns to the hub.
    private void HandModsBackBtn_Click(object sender, RoutedEventArgs e)
    {
        if (HandModsListScroll.Visibility == Visibility.Visible)
            ShowHandModsScreenVersions();
        else
            ShowHandModsScreenHub();
    }

    private void HandModsInstallDoneBtn_Click(object sender, RoutedEventArgs e)
    {
        BuildHandModsList();
        ShowHandModsScreenMods();
    }

    private void ShowHandModsScreenHub()
    {
        HandModsBackBtn.Visibility        = Visibility.Collapsed;
        HandModsHubScroll.Visibility      = Visibility.Visible;
        HandModsVersionScroll.Visibility  = Visibility.Collapsed;
        HandModsListScroll.Visibility     = Visibility.Collapsed;
        HandModsSubmitScroll.Visibility   = Visibility.Collapsed;
        HandModsInstallPanel.Visibility   = Visibility.Collapsed;
        HandModsHeader.Text = "✋ Hand Mods";
    }

    private void ShowHandModsScreenVersions()
    {
        HandModsBackBtn.Visibility        = Visibility.Visible;
        HandModsHubScroll.Visibility      = Visibility.Collapsed;
        HandModsVersionScroll.Visibility  = Visibility.Visible;
        HandModsListScroll.Visibility     = Visibility.Collapsed;
        HandModsSubmitScroll.Visibility   = Visibility.Collapsed;
        HandModsInstallPanel.Visibility   = Visibility.Collapsed;
        HandModsHeader.Text = $"✋ Hand Mods — Chapter {_handModsTargetChapter}";
    }

    private void ShowHandModsScreenMods()
    {
        HandModsBackBtn.Visibility        = Visibility.Visible;
        HandModsHubScroll.Visibility      = Visibility.Collapsed;
        HandModsVersionScroll.Visibility  = Visibility.Collapsed;
        HandModsListScroll.Visibility     = Visibility.Visible;
        HandModsSubmitScroll.Visibility   = Visibility.Collapsed;
        HandModsInstallPanel.Visibility   = Visibility.Collapsed;
        HandModsHeader.Text = "✋ Hand Mods — Select a Mod";
    }

    private void ShowHandModsScreenSubmit()
    {
        HandModsBackBtn.Visibility        = Visibility.Visible;
        HandModsHubScroll.Visibility      = Visibility.Collapsed;
        HandModsVersionScroll.Visibility  = Visibility.Collapsed;
        HandModsListScroll.Visibility     = Visibility.Collapsed;
        HandModsSubmitScroll.Visibility   = Visibility.Visible;
        HandModsInstallPanel.Visibility   = Visibility.Collapsed;
        HandModsHeader.Text = Loc.Get("handmods_submit_header");
    }

    private void ShowHandModsScreenInstalling(string modName)
    {
        HandModsBackBtn.Visibility        = Visibility.Collapsed;
        HandModsHubScroll.Visibility      = Visibility.Collapsed;
        HandModsVersionScroll.Visibility  = Visibility.Collapsed;
        HandModsListScroll.Visibility     = Visibility.Collapsed;
        HandModsSubmitScroll.Visibility   = Visibility.Collapsed;
        HandModsInstallPanel.Visibility   = Visibility.Visible;
        HandModsHeader.Text = "✋ Installing…";

        HandModsInstallModName.Text       = modName;
        HandModsProgressBar.Value         = 0;
        HandModsProgressText.Text         = "Downloading… 0%";
        HandModsProgressText.Foreground   = new SolidColorBrush(Color.FromArgb(255, 0, 204, 170));
        HandModsInstallDoneBtn.Visibility = Visibility.Collapsed;
    }

    // ── Hand Mods submission (players contribute a new hand mod for review) ─────

    private void HandModsSubmitEntryBtn_Click(object sender, RoutedEventArgs e)
    {
        _handModsSubmitChapter = _handModsTargetChapter > 0 ? _handModsTargetChapter : 1;
        _handModsSubmitColors.Clear();
        _handModsSubmitFiles.Clear();

        HandModsSubmitNameBox.Text          = "";
        HandModsSubmitFilesText.Text        = Loc.Get("handmods_submit_no_files");
        HandModsSubmitStatusText.Visibility = Visibility.Collapsed;
        HandModsSubmitSendBtn.IsEnabled     = true;
        HandModsSubmitSendBtnText.Text      = Loc.Get("handmods_submit_send_btn");

        HandModsSubmitDiscordWaiting.Visibility = Visibility.Collapsed;
        var cachedDiscord = Services.DiscordOAuthService.LoadCached();
        if (cachedDiscord.HasValue)
        {
            HandModsSubmitDiscordUsername.Text         = cachedDiscord.Value.Username;
            HandModsSubmitDiscordConnected.Visibility  = Visibility.Visible;
            HandModsSubmitDiscordConnectRow.Visibility = Visibility.Collapsed;
        }
        else
        {
            HandModsSubmitDiscordConnected.Visibility  = Visibility.Collapsed;
            HandModsSubmitDiscordConnectRow.Visibility = Visibility.Visible;
        }

        BuildHandModsSubmitChapterChips();
        BuildHandModsSubmitColorChips();

        ShowHandModsScreenSubmit();
    }

    private void BuildHandModsSubmitChapterChips()
    {
        HandModsSubmitChapterPanel.Children.Clear();
        _handModsSubmitChapterChips.Clear();
        for (int n = 1; n <= 5; n++)
        {
            var chapterNum = n; // for-loop variables are shared across iterations in C#, unlike foreach — capture a fresh copy per closure
            var chip = MakeSmallButton($"CH {chapterNum}", Teal);
            chip.Margin = new Thickness(0, 0, 6, 6);
            chip.Tag    = chapterNum;
            chip.Click += (_, _) => { _handModsSubmitChapter = chapterNum; RefreshHandModsSubmitChips(); };
            _handModsSubmitChapterChips.Add(chip);
            HandModsSubmitChapterPanel.Children.Add(chip);
        }
        RefreshHandModsSubmitChips();
    }

    private void BuildHandModsSubmitColorChips()
    {
        HandModsSubmitColorsPanel.Children.Clear();
        _handModsSubmitColorChips.Clear();
        foreach (var color in Services.HandModSubmissionService.Colors)
        {
            var chip = MakeSmallButton(color, Teal);
            chip.Margin = new Thickness(0, 0, 6, 6);
            chip.Tag    = color;
            chip.Click += (_, _) =>
            {
                if (!_handModsSubmitColors.Remove(color)) _handModsSubmitColors.Add(color);
                RefreshHandModsSubmitChips();
            };
            _handModsSubmitColorChips.Add(chip);
            HandModsSubmitColorsPanel.Children.Add(chip);
        }
        RefreshHandModsSubmitChips();
    }

    private void RefreshHandModsSubmitChips()
    {
        foreach (var chip in _handModsSubmitChapterChips)
        {
            var selected     = chip.Tag is int n && n == _handModsSubmitChapter;
            chip.Background  = new SolidColorBrush(selected ? Color.FromArgb(255, 0, 120, 100) : Color.FromArgb(255, 8, 30, 55));
            chip.BorderBrush = new SolidColorBrush(selected ? Color.FromArgb(255, 0, 204, 170) : Color.FromArgb(180, 0, 120, 100));
        }
        foreach (var chip in _handModsSubmitColorChips)
        {
            var selected     = chip.Tag is string c && _handModsSubmitColors.Contains(c);
            chip.Background  = new SolidColorBrush(selected ? Color.FromArgb(255, 0, 120, 100) : Color.FromArgb(255, 8, 30, 55));
            chip.BorderBrush = new SolidColorBrush(selected ? Color.FromArgb(255, 0, 204, 170) : Color.FromArgb(180, 0, 120, 100));
        }
    }

    private void HandModsSubmitFilesBtn_Click(object sender, RoutedEventArgs e)
    {
        var picker = new OpenFileDialog
        {
            Filter      = "Mod files|*.zip;*.pak;*.ucas;*.utoc",
            Multiselect = true,
        };
        if (picker.ShowDialog() != true) return;

        _handModsSubmitFiles.Clear();
        _handModsSubmitFiles.AddRange(picker.FileNames);
        HandModsSubmitFilesText.Text = _handModsSubmitFiles.Count == 0
            ? Loc.Get("handmods_submit_no_files")
            : string.Join(", ", _handModsSubmitFiles.Select(IOPath.GetFileName));
    }

    private async void HandModsSubmitDiscordConnectBtn_Click(object sender, RoutedEventArgs e)
    {
        _discordAuthCts?.Cancel();
        _discordAuthCts = new CancellationTokenSource();

        HandModsSubmitDiscordConnectRow.Visibility = Visibility.Collapsed;
        HandModsSubmitDiscordWaiting.Visibility    = Visibility.Visible;

        var user = await Services.DiscordOAuthService.AuthenticateAsync(_discordAuthCts.Token);

        HandModsSubmitDiscordWaiting.Visibility = Visibility.Collapsed;

        if (user.HasValue)
        {
            HandModsSubmitDiscordUsername.Text        = user.Value.Username;
            HandModsSubmitDiscordConnected.Visibility = Visibility.Visible;
            Services.DiscordOAuthService.SaveCached(user.Value.Id, user.Value.Username);
        }
        else
        {
            HandModsSubmitDiscordConnectRow.Visibility = Visibility.Visible;
        }
    }

    private void HandModsSubmitDiscordCancelAuthBtn_Click(object sender, RoutedEventArgs e)
    {
        _discordAuthCts?.Cancel();
        HandModsSubmitDiscordWaiting.Visibility    = Visibility.Collapsed;
        HandModsSubmitDiscordConnectRow.Visibility = Visibility.Visible;
    }

    private void HandModsSubmitDiscordDisconnectBtn_Click(object sender, RoutedEventArgs e)
    {
        Services.DiscordOAuthService.ClearCached();
        HandModsSubmitDiscordConnected.Visibility  = Visibility.Collapsed;
        HandModsSubmitDiscordConnectRow.Visibility = Visibility.Visible;
    }

    private void HandModsSubmitDiscordInfoBtn_Click(object sender, RoutedEventArgs e)
    {
        DiscordInfoWhyText.Text       = Loc.Get("handmods_submit_discord_info_why_text");
        DiscordInfoOverlay.Visibility = Visibility.Visible;
    }

    private async void HandModsSubmitSendBtn_Click(object sender, RoutedEventArgs e)
    {
        var name = HandModsSubmitNameBox.Text.Trim();

        if (string.IsNullOrEmpty(name))
        {
            ShowHandModsSubmitError(Loc.Get("handmods_submit_err_name"));
            return;
        }
        if (_handModsSubmitFiles.Count == 0)
        {
            ShowHandModsSubmitError(Loc.Get("handmods_submit_err_files"));
            return;
        }
        if (Services.DiscordOAuthService.LoadCached() is null)
        {
            await ShowDiscordRequiredDialogAsync("handmods_submit_err_discord_required",
                () => HandModsSubmitDiscordConnectBtn_Click(sender, e));
            return;
        }

        HandModsSubmitSendBtn.IsEnabled     = false;
        HandModsSubmitSendBtnText.Text      = Loc.Get("handmods_submit_sending");
        HandModsSubmitStatusText.Visibility = Visibility.Collapsed;

        var cached        = Services.DiscordOAuthService.LoadCached();
        var submitterName = cached is { } u ? $"{u.Username} · ID: {u.Id}" : null;
        var submitterId   = cached?.Id;

        var (ok, error) = await Services.HandModSubmissionService.SubmitAsync(
            name, _handModsSubmitChapter, _handModsSubmitColors, _handModsSubmitFiles, submitterName, submitterId);

        if (ok)
        {
            HandModsSubmitStatusText.Text       = Loc.Get("handmods_submit_success");
            HandModsSubmitStatusText.Foreground = new SolidColorBrush(Color.FromArgb(255, 0, 200, 140));
            HandModsSubmitStatusText.Visibility = Visibility.Visible;
            HandModsSubmitSendBtnText.Text       = Loc.Get("handmods_submit_send_btn");
            await Task.Delay(2000);
            ShowHandModsScreenHub();
        }
        else
        {
            ShowHandModsSubmitError(error ?? Loc.Get("handmods_submit_err_send"));
            HandModsSubmitSendBtnText.Text  = Loc.Get("handmods_submit_send_btn");
            HandModsSubmitSendBtn.IsEnabled = true;
        }
    }

    private void ShowHandModsSubmitError(string message)
    {
        HandModsSubmitStatusText.Text       = message;
        HandModsSubmitStatusText.Foreground = new SolidColorBrush(Color.FromArgb(255, 200, 60, 60));
        HandModsSubmitStatusText.Visibility = Visibility.Visible;
    }

    /// <summary>Lists every install of the targeted chapter that actually exists on disk
    /// (Auto + each custom) so the user picks which one to browse/install hand mods into
    /// before ever seeing the mod list.</summary>
    private void BuildHandModsVersionList()
    {
        HandModsVersionList.Children.Clear();

        var chapter = _chapters.FirstOrDefault(c => c.Number == _handModsTargetChapter);
        if (chapter is null) return;

        var autoExe = _epicService.IsEnabled ? _epicService.GetExePath(chapter.Number) : chapter.GameExePath;
        if (!string.IsNullOrEmpty(autoExe) && File.Exists(autoExe))
            HandModsVersionList.Children.Add(MakeHandModsVersionRow(Loc.Get("auto_name"), autoExe));

        foreach (var custom in _store.GetCustoms(chapter.Number))
        {
            if (!File.Exists(custom.ExePath)) continue;
            HandModsVersionList.Children.Add(MakeHandModsVersionRow(custom.Name, custom.ExePath));
        }

        if (HandModsVersionList.Children.Count == 0)
        {
            HandModsVersionList.Children.Add(new TextBlock
            {
                Text = "No installed version found for this chapter.",
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize = 11, Foreground = new SolidColorBrush(Color.FromArgb(160, 160, 180, 200)),
                TextWrapping = TextWrapping.Wrap, Margin = new Thickness(4, 12, 4, 0),
            });
        }
    }

    private Border MakeHandModsVersionRow(string name, string exePath)
    {
        var info = new StackPanel { VerticalAlignment = VerticalAlignment.Center };
        info.Children.Add(new TextBlock
        {
            Text = name, FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 13, FontWeight = FontWeights.SemiBold,
            Foreground = new SolidColorBrush(Color.FromArgb(255, 210, 220, 230)),
        });
        info.Children.Add(new TextBlock
        {
            Text = exePath, FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 10, Foreground = new SolidColorBrush(Color.FromArgb(255, 45, 90, 120)),
            TextTrimming = TextTrimming.CharacterEllipsis,
        });

        var normalBg = new SolidColorBrush(Color.FromArgb(12, 255, 255, 255));
        var hoverBg  = new SolidColorBrush(Color.FromArgb(28, 255, 255, 255));
        var row = new Border
        {
            Background   = normalBg,
            CornerRadius = new CornerRadius(4),
            Padding      = new Thickness(10, 8, 10, 8),
            Child        = info,
            Margin       = new Thickness(0, 0, 0, 2),
            Cursor       = Cursors.Hand,
        };
        row.MouseEnter += (_, _) => row.Background = hoverBg;
        row.MouseLeave += (_, _) => row.Background = normalBg;
        row.MouseDown  += (_, _) =>
        {
            var win64 = FindWin64Dir(IOPath.GetDirectoryName(exePath)!);
            if (win64 is null)
            {
                ShowHandModsDialog("No Content\\Paks folder found for this version.");
                return;
            }
            _handModsWin64Dir = win64;
            _handModsPaksDir  = LoadManipFilesService.GetPaksDir(win64);
            ShowHandModsScreenMods();
            _ = LoadHandModsAsync();
        };
        return row;
    }

    /// <summary>Fetches (or reuses this session's cache of) the chapter's mod list and renders
    /// Screen 2 — each row's install state is checked against the version picked in Screen 1.</summary>
    private async Task LoadHandModsAsync()
    {
        var chapterNumber = _handModsTargetChapter;

        HandModsList.Children.Clear();
        HandModsList.Children.Add(new TextBlock
        {
            Text = "Fetching mods…",
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 11, Foreground = new SolidColorBrush(TealDim), Margin = new Thickness(4, 12, 4, 0),
        });

        if (!_handModsCache.TryGetValue(chapterNumber, out var mods))
        {
            try
            {
                mods = await HandModsService.GetModsAsync(chapterNumber);
                _handModsCache[chapterNumber] = mods;
            }
            catch (Exception ex)
            {
                if (_handModsTargetChapter != chapterNumber) return; // overlay moved on while awaiting
                HandModsList.Children.Clear();
                HandModsList.Children.Add(new TextBlock
                {
                    Text = $"Failed to load mods:\n{ex.Message}",
                    FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                    FontSize = 11, Foreground = new SolidColorBrush(Color.FromArgb(220, 204, 51, 51)),
                    TextWrapping = TextWrapping.Wrap, Margin = new Thickness(4, 12, 4, 0),
                });
                return;
            }
        }

        if (_handModsTargetChapter != chapterNumber) return; // overlay moved on while awaiting
        _handModsList = mods;
        BuildHandModsList();
    }

    private void BuildHandModsList()
    {
        HandModsList.Children.Clear();
        if (_handModsList is null) return;

        if (_handModsList.Count == 0)
        {
            HandModsList.Children.Add(new TextBlock
            {
                Text = "No hand mods available for this chapter yet.",
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize = 11, Foreground = new SolidColorBrush(Color.FromArgb(160, 160, 180, 200)),
                TextWrapping = TextWrapping.Wrap, Margin = new Thickness(4, 12, 4, 0),
            });
            return;
        }

        foreach (var mod in _handModsList)
            HandModsList.Children.Add(MakeHandModRow(mod));
    }

    private Border MakeHandModRow(HandModsService.HandMod mod)
    {
        var info = new StackPanel { VerticalAlignment = VerticalAlignment.Center };
        info.Children.Add(new TextBlock
        {
            Text = mod.Name, FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 13, FontWeight = FontWeights.SemiBold,
            Foreground = new SolidColorBrush(Color.FromArgb(255, 210, 220, 230)),
        });
        info.Children.Add(new TextBlock
        {
            Text = HandModsService.FormatFileSize(mod.Size),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 10, Foreground = new SolidColorBrush(Color.FromArgb(255, 45, 90, 120)),
        });

        var grid = new Grid();
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        Grid.SetColumn(info, 0);
        grid.Children.Add(info);

        var paksDir   = _handModsPaksDir;
        var installed = paksDir != null && HandModsService.IsInstalled(paksDir, mod.BaseName);

        var actions = new StackPanel
        {
            Orientation = Orientation.Horizontal, VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(8, 0, 0, 0),
        };
        Grid.SetColumn(actions, 1);
        grid.Children.Add(actions);

        var infoBtn = new Button
        {
            Width = 26, Height = 26,
            Background = new SolidColorBrush(Color.FromArgb(40, 100, 130, 160)),
            BorderThickness = new Thickness(0), Padding = new Thickness(0),
            Margin = new Thickness(0, 0, 6, 0),
            Content = new TextBlock
            {
                FontFamily = new FontFamily("Segoe MDL2 Assets"), Text = "",
                FontSize = 13, Foreground = new SolidColorBrush(Color.FromArgb(220, 160, 180, 200)),
                HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center,
            },
        };
        ButtonHelper.SetCornerRadius(infoBtn, new CornerRadius(3));
        infoBtn.Click += async (_, _) => await HandModInfo_Click(mod, infoBtn);
        actions.Children.Add(infoBtn);

        var previewBtn = new Button
        {
            Width = 26, Height = 26,
            Background = new SolidColorBrush(Color.FromArgb(40, 100, 130, 160)),
            BorderThickness = new Thickness(0), Padding = new Thickness(0),
            Margin = new Thickness(0, 0, 6, 0),
            Content = new TextBlock
            {
                FontFamily = new FontFamily("Segoe MDL2 Assets"), Text = "",
                FontSize = 13, Foreground = new SolidColorBrush(Color.FromArgb(220, 160, 180, 200)),
                HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center,
            },
        };
        ButtonHelper.SetCornerRadius(previewBtn, new CornerRadius(3));
        previewBtn.Click += async (_, _) => await HandModPreview_Click(mod, previewBtn);
        actions.Children.Add(previewBtn);

        var extrasBtn = new Button
        {
            Width = 26, Height = 26,
            Background = new SolidColorBrush(Color.FromArgb(40, 100, 130, 160)),
            BorderThickness = new Thickness(0), Padding = new Thickness(0),
            Margin = new Thickness(0, 0, 6, 0),
            Content = new TextBlock
            {
                FontFamily = new FontFamily("Segoe MDL2 Assets"), Text = "",
                FontSize = 13, Foreground = new SolidColorBrush(Color.FromArgb(220, 160, 180, 200)),
                HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center,
            },
        };
        ButtonHelper.SetCornerRadius(extrasBtn, new CornerRadius(3));
        extrasBtn.Click += async (_, _) => await HandModExtras_Click(mod, extrasBtn);
        actions.Children.Add(extrasBtn);

        if (installed)
        {
            var uninstallBtn = MakeSmallButton("Uninstall", Color.FromArgb(200, 204, 51, 51));
            uninstallBtn.MinWidth = 90;
            uninstallBtn.Click += async (_, _) => await HandModUninstall_Click(mod);
            actions.Children.Add(uninstallBtn);
        }
        else if (paksDir is null)
        {
            actions.Children.Add(new TextBlock
            {
                Text = "unavailable", FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize = 10, Foreground = new SolidColorBrush(Color.FromArgb(140, 160, 180, 200)),
                VerticalAlignment = VerticalAlignment.Center,
            });
        }
        else
        {
            var installBtn = MakeSmallButton("Install", Teal);
            installBtn.MinWidth = 90;
            var capWin64 = _handModsWin64Dir!; var capPaks = paksDir;
            installBtn.Click += (_, _) => StartHandModInstall(mod, capWin64, capPaks);
            actions.Children.Add(installBtn);
        }

        return new Border
        {
            Background   = new SolidColorBrush(installed ? Color.FromArgb(30, 0, 204, 170) : Color.FromArgb(12, 255, 255, 255)),
            CornerRadius = new CornerRadius(4),
            Padding      = new Thickness(10, 8, 10, 8),
            Child        = grid,
            Margin       = new Thickness(0, 0, 0, 2),
        };
    }

    /// <summary>Downloads (or reuses the cached copy of) a mod's zip purely to read its declared
    /// hand color(s) out of hand.txt and show them — doesn't touch install state.</summary>
    private async Task HandModInfo_Click(HandModsService.HandMod mod, Button infoBtn)
    {
        infoBtn.IsEnabled = false;
        try
        {
            var zipPath = await HandModsService.DownloadModAsync(mod, _handModsTargetChapter);
            var hands = HandModsService.ReadDeclaredHands(zipPath);
            ShowHandModsDialog(hands != null
                ? $"{mod.Name} changes: {string.Join(", ", hands)}"
                : $"{mod.Name} hasn't declared its hand color yet.");
        }
        catch (Exception ex)
        {
            ShowHandModsDialog($"Couldn't check {mod.Name}'s info:\n{ex.Message}");
        }
        finally
        {
            infoBtn.IsEnabled = true;
        }
    }

    /// <summary>Downloads (or reuses the cached copy of) a mod's zip purely to read its hand
    /// preview image(s) — named "{name}_{color}.ext" inside the zip — and show them, or a
    /// "no preview available" message if it ships none.</summary>
    private async Task HandModPreview_Click(HandModsService.HandMod mod, Button previewBtn)
    {
        previewBtn.IsEnabled = false;
        try
        {
            var zipPath = await HandModsService.DownloadModAsync(mod, _handModsTargetChapter);
            var images  = HandModsService.ReadHandImages(zipPath);
            ShowHandModsPreviewDialog(mod.Name, images);
        }
        catch (Exception ex)
        {
            ShowHandModsDialog($"Couldn't load {mod.Name}'s preview:\n{ex.Message}");
        }
        finally
        {
            previewBtn.IsEnabled = true;
        }
    }

    private void ShowHandModsPreviewDialog(string modName, List<HandModsService.HandImage> images)
    {
        UIElement content;
        if (images.Count == 0)
        {
            content = new TextBlock
            {
                Text = "No preview available.",
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize = 12, Foreground = new SolidColorBrush(Color.FromArgb(200, 160, 180, 200)),
                TextWrapping = TextWrapping.Wrap, MaxWidth = 360,
            };
        }
        else
        {
            var panel = new StackPanel { Orientation = Orientation.Horizontal };
            foreach (var image in images)
            {
                var bitmap = new BitmapImage();
                using (var ms = new MemoryStream(image.Data))
                {
                    bitmap.BeginInit();
                    bitmap.CacheOption = BitmapCacheOption.OnLoad;
                    bitmap.StreamSource = ms;
                    bitmap.EndInit();
                }
                bitmap.Freeze();

                var card = new StackPanel { Margin = new Thickness(8, 0, 8, 0) };
                card.Children.Add(new Border
                {
                    Width = 260, Height = 260, CornerRadius = new CornerRadius(6),
                    Background = new SolidColorBrush(Color.FromArgb(255, 14, 42, 78)),
                    Child = new Image { Source = bitmap, Stretch = Stretch.UniformToFill },
                });
                card.Children.Add(new TextBlock
                {
                    Text = image.Color, FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                    FontSize = 12, Foreground = new SolidColorBrush(Color.FromArgb(200, 160, 180, 200)),
                    HorizontalAlignment = HorizontalAlignment.Center, Margin = new Thickness(0, 6, 0, 0),
                });
                panel.Children.Add(card);
            }
            content = panel;
        }

        WpfDialog.Show(this, $"{modName} preview", content, closeText: "OK");
    }

    /// <summary>Downloads (or reuses the cached copy of) a mod's zip purely to check whether it
    /// ships an "Extras" folder of individually-installable per-color variants, and if so opens
    /// <see cref="ShowHandModsExtrasDialog"/> to browse and install them one at a time.</summary>
    private async Task HandModExtras_Click(HandModsService.HandMod mod, Button extrasBtn)
    {
        extrasBtn.IsEnabled = false;
        try
        {
            var zipPath = await HandModsService.DownloadModAsync(mod, _handModsTargetChapter);
            var extras  = HandModsService.ReadExtras(zipPath);
            if (extras.Count == 0)
            {
                ShowHandModsDialog($"{mod.Name} doesn't ship any individual hand options.");
                return;
            }

            var images = HandModsService.ReadHandImages(zipPath);
            ShowHandModsExtrasDialog(mod, zipPath, extras, images);
        }
        catch (Exception ex)
        {
            ShowHandModsDialog($"Couldn't load {mod.Name}'s extras:\n{ex.Message}");
        }
        finally
        {
            extrasBtn.IsEnabled = true;
        }
    }

    private void ShowHandModsExtrasDialog(HandModsService.HandMod mod, string zipPath,
        List<HandModsService.HandModExtra> extras, List<HandModsService.HandImage> images)
    {
        var panel = new StackPanel { Width = 380 };
        RefreshHandModsExtrasPanel(panel, mod, zipPath, extras, images);

        var scroll = new ScrollViewer
        {
            Content = panel, MaxHeight = 420,
            VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
        };
        WpfDialog.Show(this, $"{mod.Name} — Individual Hands", scroll, closeText: "Close");
    }

    /// <summary>Rebuilds every row of the extras dialog from scratch — same pattern as
    /// <see cref="BuildHandModsList"/> for the main list — so an install/uninstall inside the
    /// dialog (which can evict a sibling extra or the parent mod itself) is immediately
    /// reflected across every row, not just the one that was clicked.</summary>
    private void RefreshHandModsExtrasPanel(StackPanel panel, HandModsService.HandMod mod, string zipPath,
        List<HandModsService.HandModExtra> extras, List<HandModsService.HandImage> images)
    {
        panel.Children.Clear();
        foreach (var extra in extras)
            panel.Children.Add(MakeHandModExtraRow(panel, mod, zipPath, extra, extras, images));
    }

    private Border MakeHandModExtraRow(StackPanel panel, HandModsService.HandMod mod, string zipPath,
        HandModsService.HandModExtra extra, List<HandModsService.HandModExtra> extras,
        List<HandModsService.HandImage> images)
    {
        var paksDir = _handModsPaksDir;
        var installed = paksDir != null && HandModsService.IsInstalled(paksDir, extra.BaseName);

        var thumb = new Border
        {
            Width = 44, Height = 44, CornerRadius = new CornerRadius(4),
            Background = new SolidColorBrush(Color.FromArgb(255, 14, 42, 78)),
            Margin = new Thickness(0, 0, 10, 0),
        };
        var image = images.FirstOrDefault(i => i.Color.Equals(extra.Color, StringComparison.OrdinalIgnoreCase));
        if (image != null)
        {
            var bitmap = new BitmapImage();
            using (var ms = new MemoryStream(image.Data))
            {
                bitmap.BeginInit();
                bitmap.CacheOption = BitmapCacheOption.OnLoad;
                bitmap.StreamSource = ms;
                bitmap.EndInit();
            }
            bitmap.Freeze();
            thumb.Child = new Image { Source = bitmap, Stretch = Stretch.UniformToFill };
        }

        var info = new StackPanel { VerticalAlignment = VerticalAlignment.Center };
        info.Children.Add(new TextBlock
        {
            Text = extra.Color, FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 13, FontWeight = FontWeights.SemiBold,
            Foreground = new SolidColorBrush(Color.FromArgb(255, 210, 220, 230)),
        });
        info.Children.Add(new TextBlock
        {
            Text = HandModsService.FormatFileSize(extra.Size),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 10, Foreground = new SolidColorBrush(Color.FromArgb(255, 45, 90, 120)),
        });

        var grid = new Grid();
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        Grid.SetColumn(thumb, 0); grid.Children.Add(thumb);
        Grid.SetColumn(info, 1);  grid.Children.Add(info);

        var actions = new StackPanel
        {
            Orientation = Orientation.Horizontal, VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(8, 0, 0, 0),
        };
        Grid.SetColumn(actions, 2);
        grid.Children.Add(actions);

        if (installed)
        {
            var uninstallBtn = MakeSmallButton("Uninstall", Color.FromArgb(200, 204, 51, 51));
            uninstallBtn.MinWidth = 90;
            uninstallBtn.Click += async (_, _) => await HandModExtraUninstall_Click(
                paksDir!, extra, panel, mod, zipPath, extras, images);
            actions.Children.Add(uninstallBtn);
        }
        else if (paksDir is null)
        {
            actions.Children.Add(new TextBlock
            {
                Text = "unavailable", FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize = 10, Foreground = new SolidColorBrush(Color.FromArgb(140, 160, 180, 200)),
                VerticalAlignment = VerticalAlignment.Center,
            });
        }
        else
        {
            var installBtn = MakeSmallButton("Install", Teal);
            installBtn.MinWidth = 90;
            installBtn.Click += async (_, _) => await HandModExtraInstall_Click(
                paksDir, extra, panel, mod, zipPath, extras, images);
            actions.Children.Add(installBtn);
        }

        return new Border
        {
            Background   = new SolidColorBrush(installed ? Color.FromArgb(30, 0, 204, 170) : Color.FromArgb(12, 255, 255, 255)),
            CornerRadius = new CornerRadius(4),
            Padding      = new Thickness(10, 8, 10, 8),
            Child        = grid,
            Margin       = new Thickness(0, 0, 0, 6),
        };
    }

    /// <summary>Installs one of <paramref name="mod"/>'s Extras per-color variants — extracts the
    /// nested zip out of the already-downloaded parent zip, resolves conflicts the same way
    /// <see cref="StartHandModInstall"/> does for a normal mod (except scanning every installed
    /// mod's hand marker directly via <see cref="HandModsService.GetAllInstalledHandMarkers"/>,
    /// since a variant's base name isn't part of any chapter's known mod list — it could conflict
    /// with its own parent mod, a sibling variant, or an unrelated mod, all the same way), then
    /// installs it with its color as the declared hand — the nested zip carries no hand.txt of
    /// its own.</summary>
    private async Task HandModExtraInstall_Click(string paksDir, HandModsService.HandModExtra extra,
        StackPanel panel, HandModsService.HandMod mod, string zipPath,
        List<HandModsService.HandModExtra> extras, List<HandModsService.HandImage> images)
    {
        try
        {
            var extraZipPath = await Task.Run(() =>
                HandModsService.ExtractExtra(zipPath, extra, _handModsTargetChapter));
            var newHands = new List<string> { extra.Color };

            var conflicting = HandModsService.GetAllInstalledHandMarkers(paksDir)
                .Where(m => !m.BaseName.Equals(extra.BaseName, StringComparison.OrdinalIgnoreCase))
                .Where(m => HandModsService.HandsConflict(newHands, m.Hands))
                .ToList();

            if (conflicting.Count > 0)
            {
                var names = string.Join(", ", conflicting.Select(m => m.BaseName));
                var confirmContent = new TextBlock
                {
                    Text = $"⚠ {extra.Color} changes the {extra.Color} hand.\n{names} will be removed first.",
                    FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                    FontSize = 12, Foreground = new SolidColorBrush(Color.FromArgb(200, 160, 180, 200)),
                    TextWrapping = TextWrapping.Wrap, MaxWidth = 360,
                };
                var confirmResult = WpfDialog.Show(this, "Hand Conflict", confirmContent,
                    primaryText: "Continue", closeText: "Cancel");
                if (confirmResult != WpfDialogResult.Primary) return;

                await Task.Run(() =>
                {
                    foreach (var c in conflicting)
                        HandModsService.UninstallByBaseName(paksDir, c.BaseName);
                });
            }

            await Task.Run(() => HandModsService.Install(paksDir, extraZipPath, extra.BaseName, newHands));
        }
        catch (Exception ex)
        {
            ShowHandModsDialog($"Error installing {extra.Color}:\n{ex.Message}");
        }
        finally
        {
            RefreshHandModsExtrasPanel(panel, mod, zipPath, extras, images);
            BuildHandModsList();
        }
    }

    private async Task HandModExtraUninstall_Click(string paksDir, HandModsService.HandModExtra extra,
        StackPanel panel, HandModsService.HandMod mod, string zipPath,
        List<HandModsService.HandModExtra> extras, List<HandModsService.HandImage> images)
    {
        try
        {
            await Task.Run(() => HandModsService.UninstallByBaseName(paksDir, extra.BaseName));
        }
        catch (Exception ex)
        {
            ShowHandModsDialog($"Error removing {extra.Color}:\n{ex.Message}");
        }
        finally
        {
            RefreshHandModsExtrasPanel(panel, mod, zipPath, extras, images);
            BuildHandModsList();
        }
    }

    /// <summary>Drives Screen 3: downloads <paramref name="mod"/>, reads which hand color(s) it
    /// declares (its zip's hand.txt — defaults to a single <see cref="HandModsService.UnknownHand"/>
    /// entry if the mod predates that convention), warns and auto-removes any other installed mod
    /// that shares a color, then installs it. Mods declaring entirely *different* colors (e.g. one
    /// Blue, one Green) are left alone and can coexist.</summary>
    private async void StartHandModInstall(HandModsService.HandMod mod, string win64Dir, string paksDir)
    {
        _handModsWin64Dir = win64Dir;
        _handModsPaksDir  = paksDir;

        ShowHandModsScreenInstalling(mod.Name);

        try
        {
            var sw = Stopwatch.StartNew();
            var progress = new Progress<int>(p =>
            {
                HandModsProgressBar.Value = p;
                HandModsProgressText.Text = $"Downloading… {p}%";
            });
            var zipPath = await HandModsService.DownloadModAsync(mod, _handModsTargetChapter, progress);

            // Hand-mod zips are tiny (a few MB) and are often already cached, so the real
            // download can finish almost instantly — pad the bar up to a floor duration with a
            // smooth animated fill so this step actually reads as progress instead of flashing by.
            var downloadFloor = DownloadFloorDuration - sw.Elapsed;
            await AnimateHandModsProgressAsync(HandModsProgressBar.Value, 100,
                downloadFloor > TimeSpan.Zero ? downloadFloor : TimeSpan.Zero,
                p => $"Downloading… {p}%");

            var newHands = HandModsService.ReadDeclaredHands(zipPath) ?? [HandModsService.UnknownHand];

            // Scans every installed mod's hand marker on disk rather than just this chapter's
            // known mod list, so this also catches conflicts against an installed Extras variant
            // (e.g. "Popiass11_Green") — its base name lives only inside its parent's zip, not
            // in any manifest, so it would otherwise be invisible to this check.
            var conflicting = HandModsService.GetAllInstalledHandMarkers(paksDir)
                .Where(m => !m.BaseName.Equals(mod.BaseName, StringComparison.OrdinalIgnoreCase))
                .Where(m => HandModsService.HandsConflict(newHands, m.Hands))
                .ToList();

            if (conflicting.Count > 0)
            {
                var names  = string.Join(", ", conflicting.Select(m => m.BaseName));
                var shared = newHands.Where(h => conflicting.Any(c =>
                        c.Hands.Any(oh => oh.Equals(h, StringComparison.OrdinalIgnoreCase))))
                    .ToList();
                var affected = shared.Count > 0 ? shared : newHands;
                var colors   = string.Join(", ", affected);
                var confirmContent = new TextBlock
                {
                    Text = $"⚠ {mod.Name} changes the {colors} hand{(affected.Count == 1 ? "" : "s")}.\n"
                         + $"{names} will be removed first.",
                    FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                    FontSize = 12, Foreground = new SolidColorBrush(Color.FromArgb(200, 160, 180, 200)),
                    TextWrapping = TextWrapping.Wrap, MaxWidth = 360,
                };
                var confirmResult = WpfDialog.Show(this, "Hand Conflict", confirmContent,
                    primaryText: "Continue", closeText: "Cancel");
                if (confirmResult != WpfDialogResult.Primary)
                {
                    BuildHandModsList();
                    ShowHandModsScreenMods();
                    return;
                }

                await Task.Run(() =>
                {
                    foreach (var c in conflicting)
                        HandModsService.UninstallByBaseName(paksDir, c.BaseName);
                });
            }

            HandModsProgressText.Text = "Installing files…";
            await Task.Run(() => HandModsService.Install(paksDir, zipPath, mod.BaseName));
            await Task.Delay(InstallHoldDuration); // extraction is near-instant — hold so it's readable

            HandModsProgressBar.Value = 100;
            HandModsProgressText.Text = $"✔ {mod.Name} installed successfully.";
            HandModsInstallDoneBtn.Visibility = Visibility.Visible;
        }
        catch (Exception ex)
        {
            HandModsProgressText.Foreground   = new SolidColorBrush(Color.FromArgb(220, 204, 51, 51));
            HandModsProgressText.Text         = $"Error installing mod:\n{ex.Message}";
            HandModsInstallDoneBtn.Visibility = Visibility.Visible;
        }
    }

    private static readonly TimeSpan DownloadFloorDuration = TimeSpan.FromMilliseconds(1500);
    private static readonly TimeSpan InstallHoldDuration    = TimeSpan.FromMilliseconds(700);

    /// <summary>Smoothly steps <see cref="HandModsProgressBar"/> from <paramref name="from"/> to
    /// <paramref name="to"/> over <paramref name="duration"/>, updating the status text via
    /// <paramref name="labelFormat"/> along the way — used to pad fast/cached downloads up to a
    /// minimum visible duration instead of the bar jumping straight to 100%.</summary>
    private async Task AnimateHandModsProgressAsync(double from, double to, TimeSpan duration, Func<int, string> labelFormat)
    {
        const int frameMs = 30;
        var steps = duration > TimeSpan.Zero ? Math.Max(1, (int)(duration.TotalMilliseconds / frameMs)) : 1;
        for (int i = 1; i <= steps; i++)
        {
            var value = from + (to - from) * i / steps;
            HandModsProgressBar.Value = value;
            HandModsProgressText.Text = labelFormat((int)value);
            if (duration > TimeSpan.Zero) await Task.Delay(frameMs);
        }
    }

    private async Task HandModUninstall_Click(HandModsService.HandMod mod)
    {
        if (_handModsPaksDir is null) return;
        var paksDir = _handModsPaksDir;

        try
        {
            await Task.Run(() => HandModsService.UninstallByBaseName(paksDir, mod.BaseName));
            BuildHandModsList();
        }
        catch (Exception ex)
        {
            ShowHandModsDialog($"Error removing mod:\n{ex.Message}");
        }
    }

    private void ShowHandModsDialog(string message)
    {
        WpfDialog.Show(this, "Hand Mods", new TextBlock
        {
            Text = message,
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 12, Foreground = new SolidColorBrush(Color.FromArgb(200, 160, 180, 200)),
            TextWrapping = TextWrapping.Wrap, MaxWidth = 360,
        }, closeText: "OK");
    }
}
