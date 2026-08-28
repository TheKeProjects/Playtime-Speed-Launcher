using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using SpeedrunLauncher.Services;
using SpeedrunLauncher.Services.App;
using SpeedrunLauncher.Services.Community;
using SpeedrunLauncher.Services.Discord;

namespace SpeedrunLauncher;

public partial class VideoTutorialOverlay : Window
{
    private DiscordPresenceService? _discord;
    private TutorialVideo?          _currentVideo;
    private TutorialVideo?          _loadingVideo;
    private bool                    _isPlaying;
    private bool                    _isDragging;
    private bool                    _updatingSlider;
    private bool                    _isMuted;
    private double                  _volumeBeforeMute = 0.8;
    private Border?                 _selectedItem;
    private string?                 _selectedVideoId;
    private DispatcherTimer?        _progressTimer;
    private double                  _scrollTarget;
    private DispatcherTimer?        _scrollTimer;

    private string? _chapterFilter;
    private string? _categoryFilter;
    private string? _runCategoryFilter;
    private string? _versionFilter;
    private string? _routeFilter;
    private string? _restrictionsFilter;

    private static readonly Color NormalBg   = Color.FromArgb( 12, 255, 255, 255);
    private static readonly Color HoverBg    = Color.FromArgb( 28, 255, 255, 255);
    private static readonly Color ActiveBg   = Color.FromArgb( 50,   0, 204, 170);
    private static readonly Color PillActive = Color.FromArgb( 40,   0, 204, 170);
    private static readonly Color PillNormal = Color.FromArgb( 20, 255, 255, 255);

    private const string IconPlay  = "";
    private const string IconPause = "";
    private const string IconVol   = "";
    private const string IconMute  = "";

    public VideoTutorialOverlay(DiscordPresenceService? discord = null)
    {
        _discord = discord;
        InitializeComponent();

        Player.Volume = VolumeSlider.Value;
        Player.PlaybackError    += Player_PlaybackError;
        Player.PlayStateChanged += Player_PlayStateChanged;

        _progressTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(250) };
        _progressTimer.Tick += ProgressTimer_Tick;
        _progressTimer.Start();

        ProgressSlider.AddHandler(Thumb.DragStartedEvent,
            new DragStartedEventHandler((_, _) => _isDragging = true));
        ProgressSlider.AddHandler(Thumb.DragCompletedEvent,
            new DragCompletedEventHandler((_, _) => { _isDragging = false; SeekToSlider(); }));

        _scrollTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(16) };
        _scrollTimer.Tick += ScrollTimer_Tick;

        Closed += (_, _) => { Player.Stop(); Player.Dispose(); _progressTimer.Stop(); _scrollTimer.Stop(); };

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

        if (allChapters.Count >= 1)
        {
            FiltersPanel.Children.Add(MakeFilterLabel("CHAPTER"));
            var row = new WrapPanel { Margin = new Thickness(0, 3, 0, 8) };
            row.Children.Add(MakePill("All", _chapterFilter == null, () =>
            {
                _chapterFilter = null; _categoryFilter = null; _runCategoryFilter = null;
                _versionFilter = null; _routeFilter = null; _restrictionsFilter = null;
                BuildFilters(); BuildVideoList();
            }));
            foreach (var ch in allChapters)
            {
                var cap = ch;
                row.Children.Add(MakePill(cap, _chapterFilter == cap, () =>
                {
                    _chapterFilter = cap; _categoryFilter = null; _runCategoryFilter = null;
                    _versionFilter = null; _routeFilter = null; _restrictionsFilter = null;
                    BuildFilters(); BuildVideoList();
                }));
            }
            FiltersPanel.Children.Add(row);
        }

        bool showStructured = _chapterFilter != null &&
            VideoTutorialService.Videos
                .Where(v => v.Chapter == _chapterFilter)
                .Any(v => v.RunCategories.Length > 0);

        if (showStructured)
        {
            var scopedVideos = VideoTutorialService.Videos
                .Where(v => v.Chapter == _chapterFilter).ToList();

            var runCats = scopedVideos.SelectMany(v => v.RunCategories)
                .Distinct(StringComparer.OrdinalIgnoreCase).OrderBy(c => c).ToList();

            var versions = scopedVideos
                .Where(v => !string.IsNullOrEmpty(v.Version))
                .SelectMany(v => v.Version.Split(',').Select(s => s.Trim()))
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .OrderBy(v => v.StartsWith("<") ? "0" + v : v).ToList();

            var routes = scopedVideos.SelectMany(v => v.Routes)
                .Distinct(StringComparer.OrdinalIgnoreCase).OrderBy(r => r).ToList();

            // Special categories = Category values that aren't standard trick-group labels
            var skipGroups = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
                { "Major Skips", "Small Tricks", "Legacy", "General" };
            var specialCats = scopedVideos
                .Select(v => v.Category)
                .Where(c => !string.IsNullOrEmpty(c) && !skipGroups.Contains(c))
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .OrderBy(c => c).ToList();

            if (specialCats.Count > 0)
            {
                FiltersPanel.Children.Add(MakeFilterLabel("CATEGORY"));
                var row = new WrapPanel { Margin = new Thickness(0, 3, 0, 4) };
                row.Children.Add(MakePill("All", _categoryFilter == null, () =>
                {
                    _categoryFilter = null;
                    BuildFilters(); BuildVideoList();
                }));
                foreach (var sc in specialCats)
                {
                    var c = sc;
                    row.Children.Add(MakePill(c, _categoryFilter == c, () =>
                    {
                        _categoryFilter = c; _runCategoryFilter = null;
                        BuildFilters(); BuildVideoList();
                    }));
                }
                FiltersPanel.Children.Add(row);
            }

            if (runCats.Count > 1)
            {
                FiltersPanel.Children.Add(MakeFilterLabel(specialCats.Count > 0 ? "RUN CATEGORIES" : "CATEGORY"));
                var row = new WrapPanel { Margin = new Thickness(0, 3, 0, 4) };
                row.Children.Add(MakePill("All", _runCategoryFilter == null, () => { _runCategoryFilter = null; BuildFilters(); BuildVideoList(); }));
                foreach (var rc in runCats) { var c = rc; row.Children.Add(MakePill(c, _runCategoryFilter == c, () => { _runCategoryFilter = c; _categoryFilter = null; BuildFilters(); BuildVideoList(); })); }
                FiltersPanel.Children.Add(row);
            }

            if (versions.Count > 1)
            {
                FiltersPanel.Children.Add(MakeFilterLabel("VERSION"));
                var row = new WrapPanel { Margin = new Thickness(0, 3, 0, 4) };
                row.Children.Add(MakePill("All", _versionFilter == null, () => { _versionFilter = null; BuildFilters(); BuildVideoList(); }));
                foreach (var ver in versions) { var v = ver; row.Children.Add(MakePill(v, _versionFilter == v, () => { _versionFilter = v; BuildFilters(); BuildVideoList(); })); }
                FiltersPanel.Children.Add(row);
            }

            if (routes.Count > 1)
            {
                FiltersPanel.Children.Add(MakeFilterLabel("ROUTE"));
                var row = new WrapPanel { Margin = new Thickness(0, 3, 0, 4) };
                row.Children.Add(MakePill("All", _routeFilter == null, () => { _routeFilter = null; BuildFilters(); BuildVideoList(); }));
                foreach (var rt in routes) { var r = rt; row.Children.Add(MakePill(r, _routeFilter == r, () => { _routeFilter = r; BuildFilters(); BuildVideoList(); })); }
                FiltersPanel.Children.Add(row);
            }

            var restrictions = scopedVideos
                .Where(v => !string.IsNullOrEmpty(v.Restrictions))
                .SelectMany(v => v.Restrictions.Split(',').Select(s => s.Trim()))
                .Distinct(StringComparer.OrdinalIgnoreCase).OrderBy(r => r).ToList();

            if (restrictions.Count > 0)
            {
                FiltersPanel.Children.Add(MakeFilterLabel("RESTRICTIONS"));
                var row = new WrapPanel { Margin = new Thickness(0, 3, 0, 4) };
                row.Children.Add(MakePill("All", _restrictionsFilter == null, () => { _restrictionsFilter = null; BuildFilters(); BuildVideoList(); }));
                foreach (var rs in restrictions) { var r = rs; row.Children.Add(MakePill(r, _restrictionsFilter == r, () => { _restrictionsFilter = r; BuildFilters(); BuildVideoList(); })); }
                FiltersPanel.Children.Add(row);
            }
        }
        else
        {
            var categoriesInScope = VideoTutorialService.Videos
                .Where(v => _chapterFilter == null || v.Chapter == _chapterFilter)
                .Select(v => v.Category).Where(c => !string.IsNullOrEmpty(c))
                .Distinct().OrderBy(c => c).ToList();

            if (!VideoTutorialService.FlatList && categoriesInScope.Count > 1)
            {
                FiltersPanel.Children.Add(MakeFilterLabel("CATEGORY"));
                var row = new WrapPanel { Margin = new Thickness(0, 3, 0, 4) };
                row.Children.Add(MakePill("All", _categoryFilter == null, () => { _categoryFilter = null; BuildFilters(); BuildVideoList(); }));
                foreach (var cat in categoriesInScope) { var c = cat; row.Children.Add(MakePill(c, _categoryFilter == c, () => { _categoryFilter = c; BuildFilters(); BuildVideoList(); })); }
                FiltersPanel.Children.Add(row);
            }

            var runCategories = VideoTutorialService.FlatList ? [] : VideoTutorialService.Videos
                .Where(v => _chapterFilter == null || v.Chapter == _chapterFilter)
                .SelectMany(v => ParseRunCategories(v.Description))
                .Distinct(StringComparer.OrdinalIgnoreCase).OrderBy(c => c).ToList();

            if (runCategories.Count > 0)
            {
                FiltersPanel.Children.Add(MakeFilterLabel("RUN CATEGORY"));
                var row = new WrapPanel { Margin = new Thickness(0, 3, 0, 4) };
                row.Children.Add(MakePill("All", _runCategoryFilter == null, () => { _runCategoryFilter = null; BuildFilters(); BuildVideoList(); }));
                foreach (var rc in runCategories) { var r = rc; row.Children.Add(MakePill(r, _runCategoryFilter == r, () => { _runCategoryFilter = r; BuildFilters(); BuildVideoList(); })); }
                FiltersPanel.Children.Add(row);
            }
        }
    }

    private static IEnumerable<string> ParseRunCategories(string description)
    {
        var main = description.Split(" — ", 2)[0].Split(" – ", 2)[0];
        return main.Split(',').Select(s => s.Trim())
            .Where(s => s.Length > 0 && !s.StartsWith("All Categor", StringComparison.OrdinalIgnoreCase));
    }

    private static TextBlock MakeFilterLabel(string text) => new()
    {
        Text = text, FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
        FontSize = 8, FontWeight = FontWeights.Bold,
        Foreground = new SolidColorBrush(Color.FromArgb(100, 0, 204, 170)),
        Margin = new Thickness(0, 6, 0, 0)
    };

    private static Border MakePill(string label, bool active, Action onClick)
    {
        var text = new TextBlock
        {
            Text = label, FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 9, FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(active ? Color.FromArgb(255, 0, 204, 170) : Color.FromArgb(150, 100, 140, 160)),
            VerticalAlignment = VerticalAlignment.Center
        };
        var pill = new Border
        {
            Padding = new Thickness(7, 3, 7, 3), Margin = new Thickness(0, 0, 4, 4),
            CornerRadius = new CornerRadius(10),
            Background = new SolidColorBrush(active ? PillActive : PillNormal),
            BorderBrush = new SolidColorBrush(active ? Color.FromArgb(160, 0, 204, 170) : Color.FromArgb(35, 100, 140, 160)),
            BorderThickness = new Thickness(1), Child = text, Cursor = Cursors.Hand
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
        var rawSearch   = SearchBox.Text.Trim();
        var authorOnly  = rawSearch.StartsWith('@');
        var search      = authorOnly ? rawSearch[1..] : rawSearch;

        var filtered = VideoTutorialService.Videos
            .Where(v => (_chapterFilter  == null || v.Chapter  == _chapterFilter) &&
                        (_categoryFilter == null || v.Category == _categoryFilter) &&
                        (_runCategoryFilter == null || (
                            v.RunCategories.Length > 0
                                ? v.RunCategories.Contains(_runCategoryFilter, StringComparer.OrdinalIgnoreCase)
                                : v.Description.Contains(_runCategoryFilter, StringComparison.OrdinalIgnoreCase) ||
                                  v.Description.Contains("All Categor", StringComparison.OrdinalIgnoreCase))) &&
                        (_versionFilter == null || string.IsNullOrEmpty(v.Version) ||
                            v.Version.Split(',').Select(s => s.Trim())
                                     .Contains(_versionFilter, StringComparer.OrdinalIgnoreCase)) &&
                        (_routeFilter == null || v.Routes.Length == 0 ||
                            v.Routes.Contains(_routeFilter, StringComparer.OrdinalIgnoreCase)) &&
                        (_restrictionsFilter == null || string.IsNullOrEmpty(v.Restrictions) ||
                            v.Restrictions.Split(',').Select(s => s.Trim())
                                         .Contains(_restrictionsFilter, StringComparer.OrdinalIgnoreCase)) &&
                        (string.IsNullOrEmpty(search) ||
                            (authorOnly
                                ? v.Author.Contains(search, StringComparison.OrdinalIgnoreCase)
                                : v.Title.Contains(search, StringComparison.OrdinalIgnoreCase)    ||
                                  v.Author.Contains(search, StringComparison.OrdinalIgnoreCase)   ||
                                  v.Chapter.Contains(search, StringComparison.OrdinalIgnoreCase)  ||
                                  v.Category.Contains(search, StringComparison.OrdinalIgnoreCase) ||
                                  v.Description.Contains(search, StringComparison.OrdinalIgnoreCase))))
            .ToList();

        if (filtered.Count == 0)
        {
            VideoListPanel.Children.Add(new TextBlock
            {
                Text = VideoTutorialService.Videos.Count == 0 ? "No videos configured." : "No results.",
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"), FontSize = 10,
                Foreground = new SolidColorBrush(Color.FromArgb(120, 100, 140, 160)),
                TextWrapping = TextWrapping.Wrap, Margin = new Thickness(0, 8, 0, 0)
            });
            return;
        }

        bool multiChapter = _chapterFilter == null && filtered.Select(v => v.Chapter).Distinct().Count() > 1;

        if (multiChapter)
        {
            bool first = true;
            foreach (var g in filtered.GroupBy(v => v.Chapter).OrderBy(g => g.Key))
            {
                if (!first) VideoListPanel.Children.Add(MakeDivider(10));
                VideoListPanel.Children.Add(new TextBlock
                {
                    Text = g.Key.ToUpperInvariant(),
                    FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                    FontSize = 10, FontWeight = FontWeights.Bold,
                    Foreground = new SolidColorBrush(Color.FromArgb(255, 255, 170, 0)),
                    Margin = new Thickness(0, 4, 0, 6)
                });
                if (VideoTutorialService.FlatList)
                    foreach (var v in g) VideoListPanel.Children.Add(MakeVideoItem(v));
                else
                    AddCategoryGroups(g.ToList(), indented: true);
                first = false;
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
        bool first = true;
        foreach (var g in videos.GroupBy(v => v.Category).OrderBy(g => g.Key))
        {
            if (!first) VideoListPanel.Children.Add(MakeDivider(indented ? 6 : 8));
            VideoListPanel.Children.Add(new TextBlock
            {
                Text = g.Key.ToUpperInvariant(),
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize = 9, FontWeight = FontWeights.Bold,
                Foreground = new SolidColorBrush(Color.FromArgb(180, 0, 204, 170)),
                Margin = indented ? new Thickness(8, 2, 0, 5) : new Thickness(0, 4, 0, 6)
            });
            foreach (var v in g) VideoListPanel.Children.Add(MakeVideoItem(v));
            first = false;
        }
    }

    private static Border MakeItemTag(string label, Color fg, Color bg) => new()
    {
        Padding = new Thickness(5, 1, 5, 1),
        Margin = new Thickness(0, 0, 4, 0),
        CornerRadius = new CornerRadius(3),
        Background = new SolidColorBrush(bg),
        BorderBrush = new SolidColorBrush(Color.FromArgb((byte)(fg.A / 3), fg.R, fg.G, fg.B)),
        BorderThickness = new Thickness(1),
        Child = new TextBlock
        {
            Text = label,
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 8, FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(fg)
        }
    };

    private static Border MakeDivider(double vMargin) => new()
    {
        BorderBrush = new SolidColorBrush(Color.FromArgb(50, 13, 32, 48)),
        BorderThickness = new Thickness(0, 1, 0, 0),
        Margin = new Thickness(0, vMargin, 0, vMargin)
    };

    private Border MakeVideoItem(TutorialVideo video)
    {
        bool isLoading = _loadingVideo?.Id == video.Id;
        bool isProac   = string.Equals(video.Author, "proac", StringComparison.OrdinalIgnoreCase);

        var normalBg = isProac ? Color.FromArgb( 18, 218, 165, 32) : NormalBg;
        var hoverBg  = isProac ? Color.FromArgb( 35, 218, 165, 32) : HoverBg;

        var dot = new System.Windows.Shapes.Ellipse
        {
            Width = 6, Height = 6,
            Fill = new SolidColorBrush(
                isLoading                     ? Color.FromArgb(255, 255, 170,   0) :
                _currentVideo?.Id == video.Id ? Color.FromArgb(255,   0, 204, 170) :
                                                Color.FromArgb( 60, 100, 130, 150)),
            VerticalAlignment = VerticalAlignment.Top,
            Margin = new Thickness(0, 4, 8, 0)
        };

        var title = new TextBlock
        {
            Text = video.Title,
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 11, FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(
                isLoading || _currentVideo?.Id == video.Id
                    ? Color.FromArgb(255, 200, 216, 228)
                    : Color.FromArgb(130, 160, 180, 200)),
            TextWrapping = TextWrapping.Wrap
        };

        var stack = new StackPanel();
        stack.Children.Add(title);

        if (!string.IsNullOrEmpty(video.Version) || video.Routes.Length > 0)
        {
            var metaRow = new WrapPanel { Margin = new Thickness(0, 3, 0, 0) };

            if (!string.IsNullOrEmpty(video.Version))
                metaRow.Children.Add(MakeItemTag(video.Version, Color.FromArgb(180, 0, 204, 170), Color.FromArgb(30, 0, 204, 170)));

            foreach (var route in video.Routes)
            {
                metaRow.Children.Add(MakeItemTag(route, Color.FromArgb(140, 160, 180, 200), Color.FromArgb(15, 160, 180, 200)));
            }

            stack.Children.Add(metaRow);
        }

        if (isLoading)
            stack.Children.Add(new TextBlock
            {
                Text = "Loading...",
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize = 9,
                Foreground = new SolidColorBrush(Color.FromArgb(200, 255, 170, 0)),
                Margin = new Thickness(0, 2, 0, 0)
            });

        var leftPart = new StackPanel { Orientation = Orientation.Horizontal };
        leftPart.Children.Add(dot);
        leftPart.Children.Add(stack);

        var row = new Grid();
        row.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        row.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        Grid.SetColumn(leftPart, 0);
        row.Children.Add(leftPart);

        if (isProac)
        {
            const double avatarSize = 28;
            var avatarPath = Path.Combine(ResourceExtractor.TempDir, "Assets", "Images", "proacventure.png");
            Brush? fill = null;
            if (File.Exists(avatarPath))
            {
                var bmp = new BitmapImage();
                bmp.BeginInit();
                bmp.UriSource   = new Uri(avatarPath);
                bmp.CacheOption = BitmapCacheOption.OnLoad;
                bmp.EndInit();
                fill = new ImageBrush { ImageSource = bmp, Stretch = Stretch.UniformToFill };
            }
            var avatarEllipse = new System.Windows.Shapes.Ellipse
            {
                Width             = avatarSize,
                Height            = avatarSize,
                Fill              = fill ?? Brushes.Transparent,
                Stroke            = new SolidColorBrush(Color.FromArgb(180, 218, 165, 32)),
                StrokeThickness   = 1.5,
                VerticalAlignment = VerticalAlignment.Center,
                Margin            = new Thickness(8, 0, 0, 0)
            };
            Grid.SetColumn(avatarEllipse, 1);
            row.Children.Add(avatarEllipse);
        }

        var isSelected = _selectedVideoId == video.Id;
        var border = new Border
        {
            Padding = new Thickness(8), Margin = new Thickness(0, 2, 0, 2),
            CornerRadius = new CornerRadius(4), Child = row,
            Cursor = isLoading ? Cursors.Wait : Cursors.Hand,
            Background = new SolidColorBrush(isSelected ? ActiveBg : normalBg)
        };

        if (isProac)
        {
            border.BorderBrush     = new SolidColorBrush(Color.FromArgb(90, 218, 165, 32));
            border.BorderThickness = new Thickness(1);
            border.Tag             = normalBg;
        }

        if (isSelected) _selectedItem = border;

        if (!isLoading)
        {
            border.MouseEnter += (_, _) => { if (_selectedItem != border) border.Background = new SolidColorBrush(hoverBg); };
            border.MouseLeave += (_, _) => { if (_selectedItem != border) border.Background = new SolidColorBrush(normalBg); };
            border.MouseDown  += (_, _) => SelectVideo(border, video);
        }

        return border;
    }

    // ── Selection ─────────────────────────────────────────────────────────────

    private void SelectVideo(Border item, TutorialVideo video)
    {
        if (_loadingVideo != null) return;

        if (_selectedItem != null)
        {
            var prevNormal = _selectedItem.Tag is Color tagColor ? tagColor : NormalBg;
            _selectedItem.Background = new SolidColorBrush(prevNormal);
        }
        _selectedItem    = item;
        _selectedVideoId = video.Id;
        item.Background  = new SolidColorBrush(ActiveBg);

        VideoTitle.Text = video.Title;
        if (video.RunCategories.Length > 0)
        {
            var ver = string.IsNullOrEmpty(video.Version) ? "" : $"  ·  {video.Version}";
            var rest = string.IsNullOrEmpty(video.Restrictions) ? "" : $"  ·  {video.Restrictions}";
            VideoCategoryLabel.Text = $"{video.Chapter}  ·  {string.Join(", ", video.RunCategories)}{ver}{rest}".ToUpperInvariant();
            VideoDescription.Text   = string.IsNullOrEmpty(video.Author)     ? video.Description
                                    : string.IsNullOrEmpty(video.Description) ? $"Video by {video.Author}"
                                    : $"Video by {video.Author}  ·  {video.Description}";
        }
        else
        {
            VideoCategoryLabel.Text = $"{video.Chapter}  ·  {video.Category}".ToUpperInvariant();
            VideoDescription.Text   = video.Description;
        }

        PlayVideo(video);
    }

    private void PlayVideo(TutorialVideo video)
    {
        var videoId = VideoTutorialService.ExtractVideoId(video.Url);

        _loadingVideo = video;

        // Not calling Player.Stop() here: YouTube's stopVideo() followed immediately by
        // loadVideoById() can leave the embedded player stuck (no further state/time
        // updates ever arrive, so the UI hangs on "LOADING, PLEASE WAIT" forever).
        // loadVideoById() already replaces whatever is currently loaded on its own.
        _isPlaying               = false;
        _currentVideo            = null;
        PlayPauseBtn.IsEnabled   = false;
        ProgressSlider.IsEnabled = false;
        EmptyState.Visibility      = Visibility.Visible;
        LoadingBarTrack.Visibility = Visibility.Visible;
        EmptyStateLabel.Text       = "LOADING, PLEASE WAIT";
        EmptyStateLabel.Foreground = new SolidColorBrush(Color.FromArgb(200, 0, 204, 170));

        BuildVideoList();

        if (videoId == null)
        {
            Player_PlaybackError(this, "Could not read this video's URL.");
            return;
        }

        _currentVideo = video;
        // Visible as soon as loading starts, not only once MediaOpened fires: WebView2's
        // underlying window is effectively hidden while Collapsed, and Chromium throttles/
        // pauses JS timers on hidden content — including the polling loop that detects the
        // video is ready — so staying Collapsed through the whole load can prevent it from
        // ever finishing (video hangs on "LOADING, PLEASE WAIT" forever). The trade-off is
        // seeing YouTube's own brief buffering frame instead of our loading text/bar.
        Player.Visibility = Visibility.Visible;
        Player.LoadVideo(videoId);
        _discord?.SetWatchingTutorial(video.Title, video.Chapter);
    }

    // ── Player ────────────────────────────────────────────────────────────────

    private void ProgressTimer_Tick(object? sender, EventArgs e)
    {
        if (_isDragging || _currentVideo == null || !Player.HasDuration) return;
        var pos = Player.Position;
        var total = Player.Duration;
        if (total.TotalSeconds <= 0) return;
        _updatingSlider = true;
        ProgressSlider.Value = pos.TotalSeconds / total.TotalSeconds * 100.0;
        _updatingSlider = false;
        TimeLabel.Text = $"{FormatTime(pos)} / {FormatTime(total)}";
    }

    private static string FormatTime(TimeSpan t)
        => t.TotalHours >= 1 ? $"{(int)t.TotalHours}:{t.Minutes:D2}:{t.Seconds:D2}" : $"{t.Minutes}:{t.Seconds:D2}";

    private void SeekToSlider()
    {
        if (!Player.HasDuration) return;
        Player.Position = TimeSpan.FromSeconds(ProgressSlider.Value / 100.0 * Player.Duration.TotalSeconds);
    }

    private void Player_MediaOpened(object? sender, EventArgs e)
    {
        _loadingVideo              = null;
        LoadingBarTrack.Visibility = Visibility.Collapsed;
        EmptyState.Visibility      = Visibility.Collapsed;
        Player.Visibility          = Visibility.Visible;
        PlayPauseBtn.IsEnabled     = true;
        ProgressSlider.IsEnabled   = true;
        PlayPauseIcon.Text         = IconPause;
        _isPlaying                 = true;

        _updatingSlider = true;
        ProgressSlider.Value = 0;
        _updatingSlider = false;
        TimeLabel.Text = $"0:00 / {FormatTime(Player.Duration)}";

        BuildVideoList();
    }

    private void Player_MediaEnded(object? sender, EventArgs e)
    {
        _updatingSlider = true;
        ProgressSlider.Value = 0;
        _updatingSlider = false;
        Player.Position = TimeSpan.Zero;
        Player.Play();
        _isPlaying = true;
        PlayPauseIcon.Text = IconPause;
    }

    private void Player_PlaybackError(object? sender, string message)
    {
        _loadingVideo              = null;
        _currentVideo              = null;
        LoadingBarTrack.Visibility = Visibility.Collapsed;
        Player.Visibility          = Visibility.Collapsed;
        EmptyStateLabel.Foreground = new SolidColorBrush(Color.FromArgb(255, 26, 58, 85));
        EmptyStateLabel.Text       = message;
        EmptyState.Visibility      = Visibility.Visible;
        PlayPauseBtn.IsEnabled     = false;
        ProgressSlider.IsEnabled   = false;
        BuildVideoList();
    }

    private void Player_PlayStateChanged(object? sender, bool playing)
    {
        _isPlaying = playing;
        PlayPauseIcon.Text = playing ? IconPause : IconPlay;
    }

    private void PlayPause_Click(object sender, RoutedEventArgs e)
    {
        if (_isPlaying) { Player.Pause(); _isPlaying = false; PlayPauseIcon.Text = IconPlay; }
        else            { Player.Play();  _isPlaying = true;  PlayPauseIcon.Text = IconPause; }
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
        if (_isMuted) { VolumeSlider.Value = _volumeBeforeMute; _isMuted = false; }
        else          { _volumeBeforeMute = VolumeSlider.Value; VolumeSlider.Value = 0; _isMuted = true; }
    }

    // ── Input ─────────────────────────────────────────────────────────────────

    private void ScrollTimer_Tick(object? sender, EventArgs e)
    {
        var current = VideoListScroller.VerticalOffset;
        var diff = _scrollTarget - current;
        if (Math.Abs(diff) < 0.5)
        {
            VideoListScroller.ScrollToVerticalOffset(_scrollTarget);
            _scrollTimer!.Stop();
            return;
        }
        VideoListScroller.ScrollToVerticalOffset(current + diff * 0.2);
    }

    private void VideoList_PreviewMouseWheel(object sender, MouseWheelEventArgs e)
    {
        _scrollTarget = Math.Max(0, Math.Min(
            _scrollTarget - e.Delta * 0.5,
            VideoListScroller.ScrollableHeight));
        if (!_scrollTimer!.IsEnabled) _scrollTimer.Start();
        e.Handled = true;
    }

    private void SearchBox_TextChanged(object sender, TextChangedEventArgs e)
    {
        SearchPlaceholder.Visibility = string.IsNullOrEmpty(SearchBox.Text) ? Visibility.Visible : Visibility.Collapsed;
        BuildVideoList();
    }

    private void ReportSkip_Click(object sender, RoutedEventArgs e) =>
        new ReportSkipWindow { Owner = this }.ShowDialog();

    private void Close_Click(object sender, RoutedEventArgs e)             => Close();
    private void Backdrop_MouseDown(object sender, MouseButtonEventArgs e) => Close();

    private void Window_KeyDown(object sender, KeyEventArgs e)
    {
        switch (e.Key)
        {
            case Key.Escape: Close(); break;
            case Key.Space when _currentVideo != null:
                PlayPause_Click(sender, e);
                e.Handled = true;
                break;
        }
    }
}
