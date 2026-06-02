using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Threading;
using SpeedrunLauncher.Services;

namespace SpeedrunLauncher;

public partial class VideoTutorialOverlay : Window
{
    private TutorialVideo?          _currentVideo;
    private TutorialVideo?          _downloadingVideo;
    private bool                    _isPlaying;
    private DiscordPresenceService? _discord;
    private bool             _isDragging;
    private bool             _updatingSlider;
    private bool             _isMuted;
    private double           _volumeBeforeMute = 0.8;
    private Border?          _selectedItem;
    private DispatcherTimer? _progressTimer;

    private string? _chapterFilter;
    private string? _categoryFilter;
    private string? _runCategoryFilter;



    private const string IconPlay  = "";
    private const string IconPause = "";
    private const string IconVol   = "";
    private const string IconMute  = "";

    private static readonly Color NormalBg   = Color.FromArgb( 12, 255, 255, 255);
    private static readonly Color HoverBg    = Color.FromArgb( 28, 255, 255, 255);
    private static readonly Color ActiveBg   = Color.FromArgb( 50,   0, 204, 170);
    private static readonly Color PillActive = Color.FromArgb( 40,   0, 204, 170);
    private static readonly Color PillNormal = Color.FromArgb( 20, 255, 255, 255);

    public VideoTutorialOverlay(DiscordPresenceService? discord = null)
    {
        _discord = discord;
        InitializeComponent();

        Player.Volume = VolumeSlider.Value;

        _progressTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(250) };
        _progressTimer.Tick += ProgressTimer_Tick;
        _progressTimer.Start();

        ProgressSlider.AddHandler(
            Thumb.DragStartedEvent,
            new DragStartedEventHandler((_, _) => _isDragging = true));
        ProgressSlider.AddHandler(
            Thumb.DragCompletedEvent,
            new DragCompletedEventHandler((_, _) => { _isDragging = false; SeekToSlider(); }));

        Closed += (_, _) =>
        {
            Player.Stop();
            _progressTimer.Stop();
        };

        SaveExplainText.Text = LocalizationService.Get("tutorial_save_explain");

        SaveIcon.Foreground = new SolidColorBrush(VideoTutorialService.SaveEnabled
            ? Color.FromArgb(255,   0, 204, 170)
            : Color.FromArgb(100,  26,  58,  85));

        BuildFilters();
        BuildVideoList();
    }

    // ── Filters ───────────────────────────────────────────────────────────────

    private void BuildFilters()
    {
        FiltersPanel.Children.Clear();

        var allChapters = VideoTutorialService.Videos
            .Select(v => v.Chapter).Where(c => !string.IsNullOrEmpty(c))
            .Distinct().OrderBy(c => c).ToList();

        var categoriesInScope = VideoTutorialService.Videos
            .Where(v => _chapterFilter == null || v.Chapter == _chapterFilter)
            .Select(v => v.Category).Where(c => !string.IsNullOrEmpty(c))
            .Distinct().OrderBy(c => c).ToList();

        if (allChapters.Count >= 1)
        {
            FiltersPanel.Children.Add(MakeFilterLabel("CHAPTER"));
            var row = new WrapPanel { Margin = new Thickness(0, 3, 0, 8) };
            row.Children.Add(MakePill("All", _chapterFilter == null, () =>
            {
                _chapterFilter = null; _categoryFilter = null; _runCategoryFilter = null;
                BuildFilters(); BuildVideoList();
            }));
            foreach (var ch in allChapters)
            {
                var cap = ch;
                row.Children.Add(MakePill(cap, _chapterFilter == cap, () =>
                {
                    _chapterFilter = cap; _categoryFilter = null; _runCategoryFilter = null;
                    BuildFilters(); BuildVideoList();
                }));
            }
            FiltersPanel.Children.Add(row);
        }

        if (!VideoTutorialService.FlatList && categoriesInScope.Count > 1)
        {
            FiltersPanel.Children.Add(MakeFilterLabel("CATEGORY"));
            var row = new WrapPanel { Margin = new Thickness(0, 3, 0, 4) };
            row.Children.Add(MakePill("All", _categoryFilter == null, () =>
            {
                _categoryFilter = null;
                BuildFilters(); BuildVideoList();
            }));
            foreach (var cat in categoriesInScope)
            {
                var cap = cat;
                row.Children.Add(MakePill(cap, _categoryFilter == cap, () =>
                {
                    _categoryFilter = cap;
                    BuildFilters(); BuildVideoList();
                }));
            }
            FiltersPanel.Children.Add(row);
        }

        var runCategories = VideoTutorialService.FlatList ? [] : VideoTutorialService.Videos
            .Where(v => _chapterFilter == null || v.Chapter == _chapterFilter)
            .SelectMany(v => ParseRunCategories(v.Description))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .OrderBy(c => c)
            .ToList();

        if (runCategories.Count > 0)
        {
            FiltersPanel.Children.Add(MakeFilterLabel("RUN CATEGORY"));
            var runRow = new WrapPanel { Margin = new Thickness(0, 3, 0, 4) };
            runRow.Children.Add(MakePill("All", _runCategoryFilter == null, () =>
            {
                _runCategoryFilter = null;
                BuildFilters(); BuildVideoList();
            }));
            foreach (var rc in runCategories)
            {
                var cap = rc;
                runRow.Children.Add(MakePill(cap, _runCategoryFilter == cap, () =>
                {
                    _runCategoryFilter = cap;
                    BuildFilters(); BuildVideoList();
                }));
            }
            FiltersPanel.Children.Add(runRow);
        }
    }

    private static IEnumerable<string> ParseRunCategories(string description)
    {
        var main = description.Split(" — ", 2)[0].Split(" – ", 2)[0];
        return main.Split(',')
                   .Select(s => s.Trim())
                   .Where(s => s.Length > 0 &&
                               !s.StartsWith("All Categor", StringComparison.OrdinalIgnoreCase));
    }

    private static TextBlock MakeFilterLabel(string text) => new()
    {
        Text       = text,
        FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
        FontSize   = 8, FontWeight = FontWeights.Bold,
        Foreground = new SolidColorBrush(Color.FromArgb(100, 0, 204, 170)),
        Margin     = new Thickness(0, 6, 0, 0)
    };

    private static Border MakePill(string label, bool active, Action onClick)
    {
        var text = new TextBlock
        {
            Text              = label,
            FontFamily        = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize          = 9, FontWeight = FontWeights.Bold,
            Foreground        = new SolidColorBrush(active
                                    ? Color.FromArgb(255,   0, 204, 170)
                                    : Color.FromArgb(150, 100, 140, 160)),
            VerticalAlignment = VerticalAlignment.Center
        };
        var pill = new Border
        {
            Padding         = new Thickness(7, 3, 7, 3),
            Margin          = new Thickness(0, 0, 4, 4),
            CornerRadius    = new CornerRadius(10),
            Background      = new SolidColorBrush(active ? PillActive : PillNormal),
            BorderBrush     = new SolidColorBrush(active
                                ? Color.FromArgb(160,   0, 204, 170)
                                : Color.FromArgb( 35, 100, 140, 160)),
            BorderThickness = new Thickness(1),
            Child           = text,
            Cursor          = Cursors.Hand
        };
        if (!active)
        {
            pill.MouseEnter += (_, _) => pill.Background = new SolidColorBrush(Color.FromArgb(30, 255, 255, 255));
            pill.MouseLeave += (_, _) => pill.Background = new SolidColorBrush(PillNormal);
        }
        pill.MouseDown += (_, _) => onClick();
        return pill;
    }

    // ── Video list ────────────────────────────────────────────────────────────

    private void BuildVideoList()
    {
        VideoListPanel.Children.Clear();

        var search = SearchBox.Text.Trim();

        var filtered = VideoTutorialService.Videos
            .Where(v => (_chapterFilter  == null || v.Chapter  == _chapterFilter)  &&
                        (_categoryFilter == null || v.Category == _categoryFilter) &&
                        (_runCategoryFilter == null ||
                            v.Description.Contains(_runCategoryFilter, StringComparison.OrdinalIgnoreCase) ||
                            v.Description.Contains("All Categor", StringComparison.OrdinalIgnoreCase)) &&
                        (string.IsNullOrEmpty(search) ||
                            v.Title.Contains(search, StringComparison.OrdinalIgnoreCase)       ||
                            v.Category.Contains(search, StringComparison.OrdinalIgnoreCase)    ||
                            v.Chapter.Contains(search, StringComparison.OrdinalIgnoreCase)     ||
                            v.Description.Contains(search, StringComparison.OrdinalIgnoreCase)))
            .ToList();

        if (filtered.Count == 0)
        {
            VideoListPanel.Children.Add(MakeEmptyLabel(
                VideoTutorialService.Videos.Count == 0
                    ? "No videos configured.\nAdd URLs in VideoTutorialService.cs"
                    : "No results."));
            return;
        }

        bool multiChapter = _chapterFilter == null &&
                            filtered.Select(v => v.Chapter).Distinct().Count() > 1;

        if (multiChapter)
        {
            bool firstChapter = true;
            foreach (var chGroup in filtered.GroupBy(v => v.Chapter).OrderBy(g => g.Key))
            {
                if (!firstChapter)
                    VideoListPanel.Children.Add(MakeDivider(10));
                VideoListPanel.Children.Add(new TextBlock
                {
                    Text       = chGroup.Key.ToUpperInvariant(),
                    FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                    FontSize   = 10, FontWeight = FontWeights.Bold,
                    Foreground = new SolidColorBrush(Color.FromArgb(255, 255, 170, 0)),
                    Margin     = new Thickness(0, 4, 0, 6)
                });
                if (VideoTutorialService.FlatList)
                    foreach (var v in chGroup) VideoListPanel.Children.Add(MakeVideoItem(v));
                else
                    AddCategoryGroups(chGroup.ToList(), indented: true);
                firstChapter = false;
            }
        }
        else
        {
            if (VideoTutorialService.FlatList)
                foreach (var v in filtered) VideoListPanel.Children.Add(MakeVideoItem(v));
            else
                AddCategoryGroups(filtered, indented: false);
        }
    }

    private void AddCategoryGroups(List<TutorialVideo> videos, bool indented)
    {
        bool firstCat = true;
        foreach (var catGroup in videos.GroupBy(v => v.Category).OrderBy(g => g.Key))
        {
            if (!firstCat)
                VideoListPanel.Children.Add(MakeDivider(indented ? 6 : 8));
            VideoListPanel.Children.Add(new TextBlock
            {
                Text       = catGroup.Key.ToUpperInvariant(),
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize   = 9, FontWeight = FontWeights.Bold,
                Foreground = new SolidColorBrush(Color.FromArgb(180, 0, 204, 170)),
                Margin     = indented ? new Thickness(8, 2, 0, 5) : new Thickness(0, 4, 0, 6)
            });
            foreach (var video in catGroup)
                VideoListPanel.Children.Add(MakeVideoItem(video));
            firstCat = false;
        }
    }

    private static Border MakeDivider(double vMargin) => new()
    {
        BorderBrush     = new SolidColorBrush(Color.FromArgb(50, 13, 32, 48)),
        BorderThickness = new Thickness(0, 1, 0, 0),
        Margin          = new Thickness(0, vMargin, 0, vMargin)
    };

    private static TextBlock MakeEmptyLabel(string text) => new()
    {
        Text         = text,
        FontFamily   = new FontFamily("Cascadia Code, Consolas, Courier New"),
        FontSize     = 10,
        Foreground   = new SolidColorBrush(Color.FromArgb(120, 100, 140, 160)),
        TextWrapping = TextWrapping.Wrap,
        Margin       = new Thickness(0, 8, 0, 0)
    };

    private Border MakeVideoItem(TutorialVideo video)
    {
        bool isDownloading = _downloadingVideo?.Id == video.Id;
        bool isDownloaded  = video.IsDownloaded;

        // Status dot
        var dotColor = isDownloading ? Color.FromArgb(255, 255, 170,   0)   // gold = downloading
                     : isDownloaded  ? Color.FromArgb(255,   0, 204, 170)   // teal  = ready
                                     : Color.FromArgb( 60, 100, 130, 150);  // gray  = not yet

        var dot = new System.Windows.Shapes.Ellipse
        {
            Width             = 6, Height = 6,
            Fill              = new SolidColorBrush(dotColor),
            VerticalAlignment = VerticalAlignment.Top,
            Margin            = new Thickness(0, 4, 8, 0)
        };

        var titleLabel = new TextBlock
        {
            Text         = video.Title,
            FontFamily   = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize     = 11, FontWeight = FontWeights.Bold,
            Foreground   = new SolidColorBrush(isDownloaded || isDownloading
                               ? Color.FromArgb(255, 200, 216, 228)
                               : Color.FromArgb(130, 160, 180, 200)),
            TextWrapping = TextWrapping.Wrap
        };

        var textStack = new StackPanel();
        textStack.Children.Add(titleLabel);

        if (isDownloading)
            textStack.Children.Add(new TextBlock
            {
                Text       = "Downloading...",
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize   = 9,
                Foreground = new SolidColorBrush(Color.FromArgb(200, 255, 170, 0)),
                Margin     = new Thickness(0, 2, 0, 0)
            });

        var row = new StackPanel { Orientation = Orientation.Horizontal };
        row.Children.Add(dot);
        row.Children.Add(textStack);

        var isSelected = _currentVideo?.Id == video.Id || _downloadingVideo?.Id == video.Id;
        var border = new Border
        {
            Padding      = new Thickness(8),
            Margin       = new Thickness(0, 2, 0, 2),
            CornerRadius = new CornerRadius(4),
            Child        = row,
            // All items are clickable — not-yet-downloaded ones trigger a download
            Cursor       = isDownloading ? Cursors.Wait : Cursors.Hand,
            Background   = new SolidColorBrush(isSelected ? ActiveBg : NormalBg),
            Tag          = video
        };

        if (isSelected)
            _selectedItem = border;

        if (!isDownloading)
        {
            border.MouseEnter += (_, _) => { if (_selectedItem != border) border.Background = new SolidColorBrush(HoverBg); };
            border.MouseLeave += (_, _) => { if (_selectedItem != border) border.Background = new SolidColorBrush(NormalBg); };
            border.MouseDown  += (_, _) => SelectVideo(border, video);
        }

        return border;
    }

    // ── Selection & download-on-demand ────────────────────────────────────────

    private void SelectVideo(Border item, TutorialVideo video)
    {
        // Ignore clicks while another video is downloading
        if (_downloadingVideo != null) return;

        if (_selectedItem != null)
            _selectedItem.Background = new SolidColorBrush(NormalBg);
        _selectedItem   = item;
        item.Background = new SolidColorBrush(ActiveBg);

        VideoTitle.Text         = video.Title;
        VideoCategoryLabel.Text = $"{video.Chapter}  ·  {video.Category}".ToUpperInvariant();
        VideoDescription.Text   = video.Description;

        if (video.IsDownloaded)
            PlayVideoFile(video);
        else
            _ = DownloadAndPlayAsync(video);
    }

    private void PlayVideoFile(TutorialVideo video)
    {
        _currentVideo = video;
        Player.Source = new Uri(video.PlayablePath);
        Player.Play();
        _isPlaying = true;
        _discord?.SetWatchingTutorial(video.Title, video.Chapter);

        PlayPauseIcon.Text          = IconPause;
        PlayPauseBtn.IsEnabled      = true;
        ProgressSlider.IsEnabled    = true;
        EmptyState.Visibility       = Visibility.Collapsed;
        DownloadBarTrack.Visibility = Visibility.Collapsed;
        EmptyStateLabel.Text        = "Select a video";
    }

    private async Task DownloadAndPlayAsync(TutorialVideo video)
    {
        _downloadingVideo = video;

        // Show download state in the player area
        Player.Stop();
        Player.Source            = null;
        _isPlaying               = false;
        PlayPauseBtn.IsEnabled   = false;
        ProgressSlider.IsEnabled = false;
        EmptyState.Visibility    = Visibility.Visible;
        EmptyStateLabel.Text        = "Downloading...  0%";
        DownloadBarFill.Width       = 0;
        DownloadBarTrack.Visibility = Visibility.Visible;

        BuildVideoList(); // show gold dot + "Downloading..." on the item

        var progress = new Progress<double>(pct =>
        {
            DownloadBarFill.Width  = 220.0 * pct / 100.0;
            EmptyStateLabel.Text   = $"Downloading...  {pct:F0}%";
        });

        var ok = await VideoTutorialService.DownloadAsync(video, progress);

        _downloadingVideo           = null;
        DownloadBarTrack.Visibility = Visibility.Collapsed;
        BuildVideoList(); // refresh dot color

        if (ok)
        {
            PlayVideoFile(video);
        }
        else
        {
            EmptyStateLabel.Text = "Download failed. Check your connection.";
        }
    }

    // ── Progress timer ────────────────────────────────────────────────────────

    private void ProgressTimer_Tick(object? sender, EventArgs e)
    {
        if (_isDragging || _currentVideo == null) return;
        if (!Player.NaturalDuration.HasTimeSpan) return;

        var pos   = Player.Position;
        var total = Player.NaturalDuration.TimeSpan;
        if (total.TotalSeconds <= 0) return;

        _updatingSlider      = true;
        ProgressSlider.Value = pos.TotalSeconds / total.TotalSeconds * 100.0;
        _updatingSlider      = false;

        TimeLabel.Text = $"{FormatTime(pos)} / {FormatTime(total)}";
    }

    private static string FormatTime(TimeSpan t)
        => t.TotalHours >= 1
            ? $"{(int)t.TotalHours}:{t.Minutes:D2}:{t.Seconds:D2}"
            : $"{t.Minutes}:{t.Seconds:D2}";

    private void SeekToSlider()
    {
        if (!Player.NaturalDuration.HasTimeSpan) return;
        var total = Player.NaturalDuration.TimeSpan;
        Player.Position = TimeSpan.FromSeconds(ProgressSlider.Value / 100.0 * total.TotalSeconds);
    }

    // ── Player event handlers ─────────────────────────────────────────────────

    private void Player_MediaOpened(object sender, RoutedEventArgs e)
    {
        _updatingSlider      = true;
        ProgressSlider.Value = 0;
        _updatingSlider      = false;

        if (Player.NaturalDuration.HasTimeSpan)
            TimeLabel.Text = $"0:00 / {FormatTime(Player.NaturalDuration.TimeSpan)}";
    }

    private void Player_MediaEnded(object sender, RoutedEventArgs e)
    {
        _isPlaying         = false;
        PlayPauseIcon.Text = IconPlay;

        _updatingSlider      = true;
        ProgressSlider.Value = 0;
        _updatingSlider      = false;

        Player.Position = TimeSpan.Zero;
    }

    private void PlayPause_Click(object sender, RoutedEventArgs e)
    {
        if (_isPlaying)
        {
            Player.Pause();
            _isPlaying         = false;
            PlayPauseIcon.Text = IconPlay;
        }
        else
        {
            Player.Play();
            _isPlaying         = true;
            PlayPauseIcon.Text = IconPause;
        }
    }

    private void ProgressSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (_updatingSlider || _isDragging) return;
        SeekToSlider();
    }

    private void VolumeSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        Player.Volume   = e.NewValue;
        VolumeIcon.Text = e.NewValue == 0 ? IconMute : IconVol;
    }

    private void VolumeIcon_MouseDown(object sender, MouseButtonEventArgs e)
    {
        if (_isMuted)
        {
            VolumeSlider.Value = _volumeBeforeMute;
            _isMuted           = false;
        }
        else
        {
            _volumeBeforeMute  = VolumeSlider.Value;
            VolumeSlider.Value = 0;
            _isMuted           = true;
        }
    }

    private void SearchBox_TextChanged(object sender, TextChangedEventArgs e)
    {
        SearchPlaceholder.Visibility = string.IsNullOrEmpty(SearchBox.Text)
            ? Visibility.Visible
            : Visibility.Collapsed;
        BuildVideoList();
    }

    private async void Save_Click(object sender, RoutedEventArgs e)
    {
        SaveBtn.IsEnabled = false;
        await VideoTutorialService.ToggleSaveAsync();
        SaveIcon.Foreground = new SolidColorBrush(VideoTutorialService.SaveEnabled
            ? Color.FromArgb(255,   0, 204, 170)
            : Color.FromArgb(100,  26,  58,  85));
        SaveBtn.IsEnabled = true;
        BuildVideoList();
    }

    private void Close_Click(object sender, RoutedEventArgs e)             => Close();
    private void Backdrop_MouseDown(object sender, MouseButtonEventArgs e) => Close();

    private void Window_KeyDown(object sender, KeyEventArgs e)
    {
        switch (e.Key)
        {
            case Key.Escape:
                Close();
                break;
            case Key.Space when _currentVideo != null:
                PlayPause_Click(sender, e);
                e.Handled = true;
                break;
        }
    }
}
