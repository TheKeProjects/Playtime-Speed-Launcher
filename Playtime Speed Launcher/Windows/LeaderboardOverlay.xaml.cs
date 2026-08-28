using System.Diagnostics;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Media;
using SpeedrunLauncher.Models;
using SpeedrunLauncher.Services;
using SpeedrunLauncher.Services.Community;

namespace SpeedrunLauncher;

public partial class LeaderboardOverlay : Window
{
    private readonly List<ChapterInfo> _chapters = ChapterInfo.GetAll();
    private readonly SpeedrunComService _speedrunService = new();
    private readonly Dictionary<int, List<SpeedrunRunType>> _runTypesCache = new();
    private readonly Dictionary<string, List<SpeedrunLeaderboardEntry>> _runnersCache = new();
    private int _loadToken;

    public LeaderboardOverlay()
    {
        InitializeComponent();
        Closed += (_, _) => _speedrunService.Dispose();

        BuildChapterList();
        _ = LoadChapterAsync(_chapters[0].Number);
    }

    private void BuildChapterList()
    {
        ChapterListPanel.Children.Clear();
        foreach (var chapter in _chapters)
        {
            var btn = new Button
            {
                Tag                     = chapter.Number,
                HorizontalContentAlignment = HorizontalAlignment.Left,
                BorderThickness         = new Thickness(1),
                Padding                 = new Thickness(12, 10, 12, 10),
                Margin                  = new Thickness(0, 0, 0, 6),
            };
            ButtonHelper.SetCornerRadius(btn, new CornerRadius(4));

            var stack = new StackPanel();
            stack.Children.Add(new TextBlock
            {
                Text       = $"CAPÍTULO {chapter.Number}",
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize   = 9, FontWeight = FontWeights.Bold,
                Foreground = new SolidColorBrush(Color.FromRgb(0, 204, 170)),
            });
            stack.Children.Add(new TextBlock
            {
                Text         = chapter.SubTitle.Contains(':') ? chapter.SubTitle.Split(':')[1].Trim() : chapter.SubTitle,
                FontFamily   = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize     = 12, FontWeight = FontWeights.SemiBold,
                Foreground   = new SolidColorBrush(Color.FromRgb(58, 106, 138)),
                TextWrapping = TextWrapping.Wrap,
                Margin       = new Thickness(0, 3, 0, 0),
            });

            btn.Content = stack;
            btn.Click  += ChapterButton_Click;
            ChapterListPanel.Children.Add(btn);
        }

        HighlightSelectedChapter(_chapters[0].Number);
    }

    private async void ChapterButton_Click(object sender, RoutedEventArgs e)
    {
        if (sender is Button { Tag: int number })
            await LoadChapterAsync(number);
    }

    private void HighlightSelectedChapter(int chapterNumber)
    {
        foreach (var child in ChapterListPanel.Children)
        {
            if (child is not Button { Tag: int number } btn) continue;
            var selected = number == chapterNumber;
            btn.Background  = new SolidColorBrush(selected ? Color.FromArgb(200, 0, 60, 50) : Color.FromArgb(180, 9, 20, 30));
            btn.BorderBrush = new SolidColorBrush(selected ? Color.FromRgb(0, 204, 170) : Color.FromArgb(140, 17, 34, 51));
        }
    }

    private async Task LoadChapterAsync(int chapterNumber)
    {
        var token = ++_loadToken;
        HighlightSelectedChapter(chapterNumber);

        RunTypesPanel.Children.Clear();
        RunTypesPanel.Children.Add(StatusText("Cargando categorías desde speedrun.com...", Color.FromRgb(58, 106, 138)));

        if (!_runTypesCache.TryGetValue(chapterNumber, out var runTypes))
        {
            runTypes = await _speedrunService.GetRunTypesAsync(chapterNumber);
            _runTypesCache[chapterNumber] = runTypes;
        }

        if (token != _loadToken) return;

        RunTypesPanel.Children.Clear();

        if (runTypes.Count == 0)
        {
            RunTypesPanel.Children.Add(StatusText("No se pudieron cargar los datos de speedrun.com.", Color.FromRgb(204, 51, 51)));
            return;
        }

        foreach (var runType in runTypes)
            RunTypesPanel.Children.Add(BuildRunTypeCard(runType, token));
    }

    private async Task RefreshRunnersAsync(SpeedrunRunType runType, StackPanel runnersPanel, int token)
    {
        runnersPanel.Children.Clear();
        runnersPanel.Children.Add(StatusText("Cargando...", Color.FromRgb(58, 106, 138), 11));

        var cacheKey = $"{runType.GameId}|{runType.CategoryId}|{SpeedrunComService.BuildVarQuery(runType)}";
        if (!_runnersCache.TryGetValue(cacheKey, out var runners))
        {
            runners = await _speedrunService.GetTopRunnersAsync(runType);
            _runnersCache[cacheKey] = runners;
        }

        if (token != _loadToken) return;

        runnersPanel.Children.Clear();

        if (runners.Count == 0)
        {
            runnersPanel.Children.Add(StatusText("Sin runs registradas todavía.", Color.FromRgb(58, 106, 138), 11));
            return;
        }

        foreach (var entry in runners)
            runnersPanel.Children.Add(BuildRunnerRow(entry));
    }

    private Border BuildRunTypeCard(SpeedrunRunType runType, int token)
    {
        var card = new Border
        {
            Background      = new SolidColorBrush(Color.FromArgb(160, 9, 20, 30)),
            BorderBrush     = new SolidColorBrush(Color.FromArgb(120, 17, 34, 51)),
            BorderThickness = new Thickness(1),
            CornerRadius    = new CornerRadius(6),
            Margin          = new Thickness(0, 0, 0, 12),
            Padding         = new Thickness(14, 10, 14, 12),
        };

        var outer = new StackPanel();
        outer.Children.Add(new TextBlock
        {
            Text       = runType.Label,
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize   = 13, FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(Color.FromRgb(0, 204, 170)),
            Margin     = new Thickness(0, 0, 0, 8),
        });

        var runnersPanel = new StackPanel();

        if (runType.SecondaryVariables.Count > 0)
        {
            var selectorsRow = new WrapPanel { Margin = new Thickness(0, 0, 0, 10) };
            foreach (var variable in runType.SecondaryVariables)
            {
                selectorsRow.Children.Add(BuildVariableSelector(variable,
                    () => { _ = RefreshRunnersAsync(runType, runnersPanel, token); }));
            }
            outer.Children.Add(selectorsRow);
        }

        runnersPanel.Children.Add(StatusText("Cargando...", Color.FromRgb(58, 106, 138), 11));
        outer.Children.Add(runnersPanel);
        card.Child = outer;

        _ = RefreshRunnersAsync(runType, runnersPanel, token);

        return card;
    }

    /// <summary>A themed Button+Popup dropdown (native ComboBox doesn't match the app's dark UI) for one subcategory variable.</summary>
    private FrameworkElement BuildVariableSelector(SpeedrunVariable variable, Action onChanged)
    {
        var container = new StackPanel
        {
            Orientation       = Orientation.Horizontal,
            Margin            = new Thickness(0, 0, 10, 6),
            VerticalAlignment = VerticalAlignment.Center,
        };

        container.Children.Add(new TextBlock
        {
            Text              = variable.Name.ToUpperInvariant() + ":",
            FontFamily        = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize          = 9, FontWeight = FontWeights.Bold,
            Foreground        = new SolidColorBrush(Color.FromRgb(58, 106, 138)),
            VerticalAlignment = VerticalAlignment.Center,
            Margin            = new Thickness(0, 0, 6, 0),
        });

        var selectedLabel = variable.Values.FirstOrDefault(v => v.Id == variable.SelectedValueId)?.Label
                             ?? variable.Values.FirstOrDefault()?.Label ?? "";

        var btnText = new TextBlock
        {
            Text              = selectedLabel,
            FontFamily        = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize          = 10, FontWeight = FontWeights.SemiBold,
            Foreground        = new SolidColorBrush(Color.FromRgb(138, 170, 187)),
            VerticalAlignment = VerticalAlignment.Center,
        };

        var btnContent = new StackPanel { Orientation = Orientation.Horizontal };
        btnContent.Children.Add(btnText);
        btnContent.Children.Add(new TextBlock
        {
            FontFamily        = new FontFamily("Segoe MDL2 Assets"),
            Text              = "",
            FontSize          = 8,
            Foreground        = new SolidColorBrush(Color.FromRgb(42, 90, 122)),
            Margin            = new Thickness(6, 0, 0, 0),
            VerticalAlignment = VerticalAlignment.Center,
        });

        var btn = new Button
        {
            Height          = 24,
            Padding         = new Thickness(8, 0, 8, 0),
            Background      = new SolidColorBrush(Color.FromRgb(10, 24, 37)),
            BorderBrush     = new SolidColorBrush(Color.FromRgb(26, 58, 85)),
            BorderThickness = new Thickness(1),
            Content         = btnContent,
        };
        ButtonHelper.SetCornerRadius(btn, new CornerRadius(4));

        var popup = new Popup
        {
            PlacementTarget = btn,
            Placement       = PlacementMode.Bottom,
            StaysOpen       = false,
        };

        var optionsPanel = new StackPanel();
        foreach (var value in variable.Values)
        {
            var optBtn = new Button
            {
                Content                    = value.Label,
                Background                 = Brushes.Transparent,
                BorderThickness            = new Thickness(0),
                HorizontalContentAlignment = HorizontalAlignment.Left,
                Padding                    = new Thickness(10, 6, 20, 6),
                FontFamily                 = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize                   = 11,
                Foreground                 = new SolidColorBrush(Color.FromRgb(138, 170, 187)),
            };
            optBtn.Click += (_, _) =>
            {
                variable.SelectedValueId = value.Id;
                btnText.Text = value.Label;
                popup.IsOpen = false;
                onChanged();
            };
            optionsPanel.Children.Add(optBtn);
        }

        popup.Child = new Border
        {
            Background      = new SolidColorBrush(Color.FromRgb(9, 20, 30)),
            BorderBrush     = new SolidColorBrush(Color.FromRgb(21, 48, 72)),
            BorderThickness = new Thickness(1),
            CornerRadius    = new CornerRadius(4),
            MinWidth        = 120,
            Child           = optionsPanel,
        };

        btn.Click += (_, _) => popup.IsOpen = !popup.IsOpen;

        container.Children.Add(btn);
        container.Children.Add(popup);
        return container;
    }

    private Border BuildRunnerRow(SpeedrunLeaderboardEntry entry)
    {
        var rankColor = entry.Place switch
        {
            1 => Color.FromRgb(255, 210, 77),
            2 => Color.FromRgb(200, 210, 220),
            3 => Color.FromRgb(205, 140, 95),
            _ => Color.FromRgb(58, 106, 138),
        };

        var row = new Border
        {
            BorderBrush     = new SolidColorBrush(Color.FromArgb(60, 17, 34, 51)),
            BorderThickness = new Thickness(0, 0, 0, 1),
            Padding         = new Thickness(0, 5, 0, 5),
        };

        var grid = new Grid();
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(32) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });

        var rankText = new TextBlock
        {
            Text              = $"#{entry.Place}",
            FontFamily        = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize          = 11, FontWeight = FontWeights.Bold,
            Foreground        = new SolidColorBrush(rankColor),
            VerticalAlignment = VerticalAlignment.Center,
        };
        Grid.SetColumn(rankText, 0);

        var nameText = new TextBlock
        {
            Text               = entry.PlayerName,
            FontFamily         = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize           = 12,
            Foreground         = new SolidColorBrush(Color.FromRgb(220, 230, 240)),
            VerticalAlignment  = VerticalAlignment.Center,
            TextTrimming       = TextTrimming.CharacterEllipsis,
            Margin             = new Thickness(6, 0, 6, 0),
        };
        Grid.SetColumn(nameText, 1);

        var timeText = new TextBlock
        {
            Text              = SpeedrunComService.FormatTime(entry.TimeSeconds),
            FontFamily        = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize          = 12, FontWeight = FontWeights.SemiBold,
            Foreground        = new SolidColorBrush(Color.FromRgb(0, 204, 170)),
            VerticalAlignment = VerticalAlignment.Center,
            Margin            = new Thickness(0, 0, 10, 0),
        };
        Grid.SetColumn(timeText, 2);

        var linkBtn = new Button
        {
            Width           = 22, Height = 22,
            Background      = Brushes.Transparent,
            BorderThickness = new Thickness(0),
            Tag             = entry.RunWeblink,
            ToolTip         = "Ver run en speedrun.com",
            Content = new TextBlock
            {
                FontFamily          = new FontFamily("Segoe MDL2 Assets"),
                Text                = "",
                FontSize            = 12,
                Foreground          = new SolidColorBrush(Color.FromRgb(58, 106, 138)),
                HorizontalAlignment = HorizontalAlignment.Center,
                VerticalAlignment   = VerticalAlignment.Center,
            },
        };
        linkBtn.Click += RunLink_Click;
        Grid.SetColumn(linkBtn, 3);

        grid.Children.Add(rankText);
        grid.Children.Add(nameText);
        grid.Children.Add(timeText);
        grid.Children.Add(linkBtn);

        row.Child = grid;
        return row;
    }

    private static TextBlock StatusText(string text, Color color, double fontSize = 12) => new()
    {
        Text       = text,
        FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
        FontSize   = fontSize,
        Foreground = new SolidColorBrush(color),
        Margin     = new Thickness(4, 10, 0, 0),
    };

    private void RunLink_Click(object sender, RoutedEventArgs e)
    {
        if (sender is not Button { Tag: string url } || string.IsNullOrEmpty(url)) return;
        try { Process.Start(new ProcessStartInfo { FileName = url, UseShellExecute = true }); }
        catch { }
    }

    private void Window_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Escape) Close();
    }

    private void Backdrop_MouseDown(object sender, MouseButtonEventArgs e) => Close();

    private void CloseBtn_Click(object sender, RoutedEventArgs e) => Close();
}
