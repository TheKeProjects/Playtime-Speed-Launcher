using System.Diagnostics;
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
using IOPath = System.IO.Path;
using Loc = SpeedrunLauncher.Services.LocalizationService;

namespace SpeedrunLauncher;

public partial class MainWindow : Window
{
    private readonly List<ChapterInfo>                            _chapters          = ChapterInfo.GetAll();
    private readonly List<Border>                                 _cards             = [];
    private readonly InstallationsStore                           _store             = InstallationsStore.Load();
    private readonly Dictionary<string, CancellationTokenSource> _activePolls       = [];
    private readonly Dictionary<string, List<string>>            _downloadLogs      = [];
    private readonly Dictionary<string, TextBlock>               _downloadLogBlocks = [];
    private int   _selected        = 0;
    private int   _versionsChapter = 0;
    private bool  _hidePresetRows  = true;
    private float _sfxVolume       = 0.5f;
    private static readonly Dictionary<string, string> LangNames = new()
    {
        ["es"] = "Español",
        ["en"] = "English",
    };

    private bool _popupWasOpen = false;

    // ── Update system ─────────────────────────────────────────────────────────
    private readonly UpdateService _updateService = new();
    private UpdateInfo?   _updateInfo   = null;
    private GbUpdateInfo? _gbUpdateInfo = null;
    private bool _isDownloading      = false;
    private bool _showingInstallView  = false;
    private bool _isGbInstall         = false;

    // ── Palette ───────────────────────────────────────────────────────────────
    private static readonly Color Teal       = Color.FromArgb(255,   0, 204, 170);
    private static readonly Color TealDim    = Color.FromArgb(120,   0, 204, 170);
    private static readonly Color CardBorder = Color.FromArgb( 50,  13,  42,  59);
    private static readonly Color Overlay0   = Color.FromArgb(  0,   0,   0,   0);
    private static readonly Color Overlay1   = Color.FromArgb(210,   5,  10,  18);

    public MainWindow()
    {
        Loc.LoadSaved();
        InitializeComponent();
        InitLangSelector();
        ApplyLanguage();
        SetupWindow();
        _ = DetectVersionsAsync();
        _ = DetectUpdatesAsync();
        PlayIntro();
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
        LanguageLabel.Text        = Loc.Get("language_label");
        ToolTipService.SetToolTip(SettingsButton, Loc.Get("settings_tooltip"));

        UpdatesHeaderText.Text         = Loc.Get("updates_header");
        UpdateCurrentVersionLabel.Text = Loc.Get("updates_current_version");
        UpdateCurrentVersionText.Text  = AppVersion.GetDisplayVersion();
        UpdateCheckHint.Text           = Loc.Get("updates_check_hint");
        UpdateDetailsLabel.Text        = Loc.Get("updates_details_label");
        UpdateLatestVersionLabel.Text  = Loc.Get("updates_latest_version");
        UpdateFileNameLabel.Text       = Loc.Get("updates_file_name");
        UpdateFileSizeLabel.Text       = Loc.Get("updates_file_size");
        WhatsNewLabel.Text             = Loc.Get("updates_whats_new");
        AcceptInstallBtnText.Text      = Loc.Get("updates_download_btn");
        CancelInstallBtnText.Text      = Loc.Get("updates_cancel_btn");
        CloseUpdatesBtnText.Text       = Loc.Get("updates_close");

        CardsPanel.Children.Clear();
        _cards.Clear();
        BuildCards();
        SelectChapter(_selected);

        if (VersionsOverlay.Visibility == Visibility.Visible)
            BuildInstallationsList();
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
    }

    private void IntroOverlay_MouseDown(object sender, MouseButtonEventArgs e) => HideIntro();

    // ── Window ────────────────────────────────────────────────────────────────

    private void SetupWindow()
    {
        Title = "Poppy Playtime — Speedrun Launcher";
        WindowState = WindowState.Maximized;
        WindowStyle = WindowStyle.None;
    }

    // ── Sound ─────────────────────────────────────────────────────────────────

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

    private void BuildCards()
    {
        var bannerDir = IOPath.Combine(Services.ResourceExtractor.TempDir, "Assets", "Banners");
        for (int i = 0; i < _chapters.Count; i++)
        {
            var chapter = _chapters[i];
            var card = MakeCard(chapter, IOPath.Combine(bannerDir, $"Chapter {i + 1}.jpg"));
            var idx = i;
            card.MouseDown   += (_, _) => SelectChapter(idx);
            card.MouseEnter  += (_, _) => { OnCardHover(idx, true); PlaySfx("OpcionMover.WAV", noStop: true); };
            card.MouseLeave  += (_, _) => OnCardHover(idx, false);
            _cards.Add(card);
            CardsPanel.Children.Add(card);
        }
    }

    private Border MakeCard(ChapterInfo chapter, string bannerPath)
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
        grid.Children.Add(bottom);

        if (!chapter.IsAvailable)
        {
            var overlay = new Border { Background = new SolidColorBrush(Color.FromArgb(170, 5, 10, 18)) };
            overlay.Child = new TextBlock
            {
                Text = Loc.Get("coming_soon"),
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize = 9, Foreground = new SolidColorBrush(TealDim),
                HorizontalAlignment = HorizontalAlignment.Center,
                VerticalAlignment   = VerticalAlignment.Center,
                TextWrapping = TextWrapping.Wrap, TextAlignment = TextAlignment.Center,
            };
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

    private void RefreshInfo()
    {
        var ch     = _chapters[_selected];
        var selPath = _store.GetSelectedPath(ch.Number);

        TitleText.Text       = Loc.Get($"ch{ch.Number}_title");
        DescriptionText.Text = Loc.Get($"ch{ch.Number}_desc");

        if (selPath != null)
        {
            var custom = _store.GetCustoms(ch.Number).FirstOrDefault(x => x.ExePath == selPath);
            VersionText.Text = Loc.Get("version_prefix") + " " + (custom?.Name ?? IOPath.GetFileNameWithoutExtension(selPath));
        }
        else
        {
            VersionText.Text = ch.IsInstalled
                ? Loc.Get("version_prefix") + " " + Loc.Get("version_auto_steam")
                : ch.IsAvailable
                    ? Loc.Get("version_prefix") + " " + Loc.Get("version_not_installed")
                    : Loc.Get("version_prefix") + " " + Loc.Get("version_none");
        }

        var canPlay = ch.IsInstalled || (selPath != null && File.Exists(selPath));
        StatusText.Text      = ch.IsInstalled ? Loc.Get("status_installed") : ch.IsAvailable ? Loc.Get("status_not_found") : Loc.Get("status_coming_soon");
        PlayButton.IsEnabled = canPlay;
        PlayButton.Opacity   = canPlay ? 1.0 : 0.35;
    }

    private async Task DetectVersionsAsync()
    {
        await Task.Run(() => SteamDetector.DetectAll(_chapters));
        Dispatcher.BeginInvoke(new Action(RefreshInfo));
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
        TogglePresetsBtn.Content = new TextBlock
        {
            Text = _hidePresetRows ? Loc.Get("show_installers") : Loc.Get("hide_installers"),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 11, FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(Color.FromArgb(255, 45, 90, 120)),
        };

        InstallsList.Children.Add(
            MakeInstallRow(Loc.Get("auto_name"), Loc.Get("auto_subtitle"),
                isAuto: true, isSelected: sel is null, chapterNum: chNum,
                exePath: ch.GameExePath ?? ""));

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
            foreach (var inst in customs)
                InstallsList.Children.Add(
                    MakeInstallRow(inst.Name, inst.ExePath,
                        isAuto: false, isSelected: sel == inst.ExePath,
                        chapterNum: chNum, exePath: inst.ExePath, inst: inst));
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
        InstallationInfo? inst = null)
    {
        var grid = new Grid();
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(46) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

        var iconBorder = new Border
        {
            Width = 34, Height = 34, CornerRadius = new CornerRadius(4),
            Background = new SolidColorBrush(isAuto ? Color.FromArgb(255, 18, 60, 110) : Color.FromArgb(255, 20, 38, 55)),
            HorizontalAlignment = HorizontalAlignment.Left, VerticalAlignment = VerticalAlignment.Center,
        };
        var customIcon = inst?.IconPath;
        var steamImg   = IOPath.Combine(Services.ResourceExtractor.TempDir, "Assets", "Images", "Steam.jpg");
        var chapterImg = IOPath.Combine(Services.ResourceExtractor.TempDir, "Assets", "Images", $"Chapter {chapterNum}.png");
        iconBorder.Child =
            !isAuto && customIcon is not null && File.Exists(customIcon)
                ? new Image { Source = new BitmapImage(new Uri(customIcon)), Stretch = Stretch.UniformToFill }
            : isAuto && File.Exists(steamImg)
                ? (UIElement)new Image { Source = new BitmapImage(new Uri(steamImg)), Stretch = Stretch.UniformToFill }
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
                try { await StartPresetInstallAsync(capPreset, capCh); }
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
        _ = ShowSteamGuardPopupAsync();

        var progress = new Progress<string>(_ => { });

        try
        {
            await SteamCmdRunner.RunAsync(steamcmdPath, username, password,
                preset.AppId, preset.DepotId, preset.ManifestId,
                progress, cts.Token);
        }
        catch (OperationCanceledException) { }
        catch (Exception ex) { ShowErrorAsync($"{Loc.Get("error_steamcmd_msg")}\n{ex.Message}"); }
        finally
        {
            ticker.Stop();
            _activePolls.Remove(preset.ManifestId);
            _downloadLogBlocks.Remove(preset.ManifestId);
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
        var userTb = new TextBox
        {
            Text = suggestedUser,
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            Background = new SolidColorBrush(Color.FromRgb(10, 20, 32)),
            Foreground = new SolidColorBrush(Color.FromRgb(200, 210, 220)),
            BorderBrush = new SolidColorBrush(Color.FromRgb(30, 60, 90)),
            CaretBrush = new SolidColorBrush(Color.FromRgb(0, 204, 170)),
        };
        var passPb = new PasswordBox
        {
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            Background = new SolidColorBrush(Color.FromRgb(10, 20, 32)),
            Foreground = new SolidColorBrush(Color.FromRgb(200, 210, 220)),
            BorderBrush = new SolidColorBrush(Color.FromRgb(30, 60, 90)),
        };

        var panel = new StackPanel { MaxWidth = 440 };
        panel.Children.Add(new TextBlock
        {
            Text = Loc.Get("credentials_version_label"),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"), FontSize = 11,
            Foreground = new SolidColorBrush(Color.FromArgb(160, 120, 160, 190)),
            Margin = new Thickness(0, 0, 0, 6),
        });
        panel.Children.Add(new Border
        {
            Background = new SolidColorBrush(Color.FromArgb(50, 0, 204, 170)),
            CornerRadius = new CornerRadius(4), Padding = new Thickness(10, 5, 10, 5),
            Margin = new Thickness(0, 0, 0, 8),
            Child = new TextBlock
            {
                Text = preset.Command,
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize = 10, Foreground = new SolidColorBrush(Teal),
            },
        });
        if (!string.IsNullOrEmpty(suggestedUser))
            panel.Children.Add(new TextBlock
            {
                Text = Loc.Get("detected_user"),
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"), FontSize = 10,
                Foreground = new SolidColorBrush(Color.FromArgb(160, 0, 204, 170)),
                Margin = new Thickness(0, 0, 0, 4),
            });
        panel.Children.Add(new TextBlock { Text = Loc.Get("username_placeholder") ?? "Username", Foreground = new SolidColorBrush(Color.FromArgb(120, 150, 180, 200)), FontSize = 9, FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"), Margin = new Thickness(0, 0, 0, 2) });
        panel.Children.Add(userTb);
        panel.Children.Add(new TextBlock { Text = Loc.Get("password_placeholder") ?? "Password", Foreground = new SolidColorBrush(Color.FromArgb(120, 150, 180, 200)), FontSize = 9, FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"), Margin = new Thickness(0, 8, 0, 2) });
        panel.Children.Add(passPb);

        var result = await WpfDialog.ShowAsync(this,
            Loc.Get("install_dialog_title", TranslatePresetName(preset.Name)), panel,
            primaryText: Loc.Get("start_download"),
            closeText:   Loc.Get("cancel"));

        if (result != WpfDialogResult.Primary) return null;
        var user = userTb.Text.Trim();
        if (string.IsNullOrEmpty(user)) return null;
        return (user, passPb.Password.Length > 0 ? passPb.Password : null);
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

    // ── Main buttons ──────────────────────────────────────────────────────────

    private void PlayButton_Click(object sender, RoutedEventArgs e)
    {
        var ch  = _chapters[_selected];
        var exe = _store.GetSelectedPath(ch.Number) ?? ch.GameExePath;
        if (string.IsNullOrEmpty(exe) || !File.Exists(exe)) return;

        try
        {
            Process.Start(new ProcessStartInfo
            {
                FileName = exe, WorkingDirectory = IOPath.GetDirectoryName(exe), UseShellExecute = true,
            });
        }
        catch { }
    }

    private void SettingsButton_Click(object sender, RoutedEventArgs e) =>
        SettingsOverlay.Visibility = Visibility.Visible;

    private void CloseSettingsBtn_Click(object sender, RoutedEventArgs e) =>
        SettingsOverlay.Visibility = Visibility.Collapsed;

    private void VersionBtn_Click(object sender, RoutedEventArgs e) => OpenVersionsOverlay();

    private void VolumeSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        _sfxVolume = (float)(e.NewValue / 100.0);
        if (VolumeValueText is not null)
            VolumeValueText.Text = $"{(int)e.NewValue}%";
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

    // ── Updates ───────────────────────────────────────────────────────────────

    private async Task DetectUpdatesAsync()
    {
        _updateInfo   = await _updateService.CheckForUpdatesAsync();
        _gbUpdateInfo = await _updateService.CheckGameBananaUpdateAsync();

        Dispatcher.BeginInvoke(new Action(() =>
        {
            UpdatesTopBtnText.Text = AppVersion.GetDisplayVersion();

            if (_updateInfo.IsUpdateAvailable || (_gbUpdateInfo?.IsUpdateAvailable ?? false))
            {
                UpdatesTopBtnText.Text       = $"{AppVersion.GetDisplayVersion()}  ↑  UPDATE";
                UpdatesTopBtnText.Foreground = new SolidColorBrush(Teal);
            }

            if (UpdatesOverlay.Visibility == Visibility.Visible)
                RefreshUpdateCheckView();
        }));
    }

    private void UpdatesTopBtn_Click(object sender, RoutedEventArgs e)
    {
        ShowUpdateCheckView();
        UpdatesOverlay.Visibility = Visibility.Visible;
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
            ChangelogText.Text       = string.IsNullOrWhiteSpace(_gbUpdateInfo.Changelog)
                ? "—" : _gbUpdateInfo.Changelog;
        }
        else if (_updateInfo != null)
        {
            InstallTitleText.Text    = Loc.Get("updates_install_ready", $"v{_updateInfo.LatestVersion}");
            InstallSubtitleText.Text = Loc.Get("updates_install_subtitle",
                _updateInfo.FileName, UpdateService.FormatFileSize(_updateInfo.FileSize));
            ChangelogText.Text       = string.IsNullOrWhiteSpace(_updateInfo.Changelog)
                ? "—" : _updateInfo.Changelog;
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
            Dispatcher.BeginInvoke(new Action(() =>
            {
                UpdateCheckHint.Text = Loc.Get("updates_check_hint");
                RefreshUpdateCheckView();
                if (_updateInfo.IsUpdateAvailable || (_gbUpdateInfo?.IsUpdateAvailable ?? false))
                {
                    UpdatesTopBtnText.Text       = $"{AppVersion.GetDisplayVersion()}  ↑  UPDATE";
                    UpdatesTopBtnText.Foreground = new SolidColorBrush(Teal);
                }
                else
                {
                    UpdatesTopBtnText.Text       = AppVersion.GetDisplayVersion();
                    UpdatesTopBtnText.Foreground = new SolidColorBrush(Color.FromArgb(255, 26, 58, 85));
                }
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
}
