using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using SpeedrunLauncher.Services;
using SpeedrunLauncher.Services.Community;

namespace SpeedrunLauncher;

public partial class ReportSkipWindow : Window
{
    private static readonly Color PillActive = Color.FromArgb(40, 0, 204, 170);
    private static readonly Color PillNormal = Color.FromArgb(20, 255, 255, 255);

    private readonly HashSet<string> _selectedRunCategories = new(StringComparer.OrdinalIgnoreCase);
    private readonly HashSet<string> _selectedRoutes        = new(StringComparer.OrdinalIgnoreCase);
    private readonly HashSet<string> _selectedRestrictions  = new(StringComparer.OrdinalIgnoreCase);
    private readonly HashSet<string> _selectedVersions      = new(StringComparer.OrdinalIgnoreCase);
    private readonly List<string>    _customRunCategories   = new();
    private string? _selectedChapter;
    private bool    _addingCustomRunCategory;

    public ReportSkipWindow()
    {
        InitializeComponent();
        BuildChapterPills();
        BuildRunCategoryPanel();
        RefreshChapterScopedPanels();

        var restrictionOptions = VideoTutorialService.Videos
            .Where(v => !string.IsNullOrEmpty(v.Restrictions))
            .SelectMany(v => v.Restrictions.Split(',').Select(s => s.Trim()))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .OrderBy(r => r).ToList();
        BuildTogglePills(RestrictionsPanel, restrictionOptions, _selectedRestrictions);
    }

    // Chapter is a single-choice field — picking one clears any other selection,
    // and clicking the active one again clears it. Version/Route are scoped to
    // whichever chapter is selected (or the whole catalog when none is).
    private void BuildChapterPills()
    {
        var chapterOptions = VideoTutorialService.Videos
            .Select(v => v.Chapter)
            .Where(c => !string.IsNullOrEmpty(c))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .OrderBy(c => c).ToList();

        ChapterPanel.Children.Clear();
        foreach (var option in chapterOptions)
        {
            var opt = option;
            ChapterPanel.Children.Add(MakeTogglePill(opt, _selectedChapter == opt, () =>
            {
                _selectedChapter = _selectedChapter == opt ? null : opt;
                BuildChapterPills();
                RefreshChapterScopedPanels();
            }));
        }
    }

    private void RefreshChapterScopedPanels()
    {
        var scoped = string.IsNullOrEmpty(_selectedChapter)
            ? VideoTutorialService.Videos
            : VideoTutorialService.Videos.Where(v => v.Chapter == _selectedChapter);
        scoped = scoped.ToList();

        var versionOptions = scoped
            .Where(v => !string.IsNullOrEmpty(v.Version))
            .SelectMany(v => v.Version.Split(',').Select(s => s.Trim()))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .OrderBy(v => v.StartsWith('<') ? "0" + v : v).ToList();

        var routeOptions = scoped
            .SelectMany(v => v.Routes)
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .OrderBy(r => r).ToList();

        _selectedVersions.Clear();
        _selectedRoutes.Clear();

        BuildTogglePills(VersionPanel, versionOptions, _selectedVersions);
        BuildTogglePills(RoutePanel, routeOptions, _selectedRoutes);
    }

    // Run Category also allows adding a custom tag that isn't in the catalog yet
    // via the trailing "+" pill.
    private void BuildRunCategoryPanel()
    {
        var options = VideoTutorialService.Videos
            .SelectMany(v => v.RunCategories)
            .Concat(_customRunCategories)
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .OrderBy(c => c).ToList();

        RunCategoryPanel.Children.Clear();
        foreach (var option in options)
        {
            var opt = option;
            RunCategoryPanel.Children.Add(MakeTogglePill(opt, _selectedRunCategories.Contains(opt), () =>
            {
                if (!_selectedRunCategories.Add(opt)) _selectedRunCategories.Remove(opt);
                BuildRunCategoryPanel();
            }));
        }

        if (_addingCustomRunCategory)
            RunCategoryPanel.Children.Add(MakeCustomRunCategoryInput());
        else
            RunCategoryPanel.Children.Add(MakeAddPill(() =>
            {
                _addingCustomRunCategory = true;
                BuildRunCategoryPanel();
            }));
    }

    private TextBox MakeCustomRunCategoryInput()
    {
        var box = new TextBox
        {
            Width = 90, Margin = new Thickness(0, 0, 4, 4),
            Background = new SolidColorBrush(Color.FromArgb(30, 255, 255, 255)),
            BorderBrush = new SolidColorBrush(Color.FromArgb(90, 0, 204, 170)),
            BorderThickness = new Thickness(1),
            Foreground = new SolidColorBrush(Color.FromRgb(0xC8, 0xD8, 0xE4)),
            CaretBrush = new SolidColorBrush(Color.FromRgb(0, 0xCC, 0xAA)),
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 9, Padding = new Thickness(5, 3, 5, 3),
            MaxLength = 30,
        };

        void Commit()
        {
            var value = box.Text.Trim();
            _addingCustomRunCategory = false;
            if (!string.IsNullOrEmpty(value))
            {
                _customRunCategories.Add(value);
                _selectedRunCategories.Add(value);
            }
            BuildRunCategoryPanel();
        }

        box.KeyDown += (_, e) =>
        {
            if (e.Key == Key.Enter) { Commit(); e.Handled = true; }
            else if (e.Key == Key.Escape) { _addingCustomRunCategory = false; BuildRunCategoryPanel(); e.Handled = true; }
        };
        box.LostFocus += (_, _) => { if (_addingCustomRunCategory) Commit(); };

        Dispatcher.BeginInvoke(new Action(() => { box.Focus(); Keyboard.Focus(box); }),
            System.Windows.Threading.DispatcherPriority.Input);

        return box;
    }

    private static Border MakeAddPill(Action onClick)
    {
        var text = new TextBlock
        {
            Text = "+", FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 11, FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(Color.FromArgb(200, 0, 204, 170)),
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center
        };
        var pill = new Border
        {
            Width = 24, Height = 24, Margin = new Thickness(0, 0, 4, 4),
            CornerRadius = new CornerRadius(12),
            Background = new SolidColorBrush(PillNormal),
            BorderBrush = new SolidColorBrush(Color.FromArgb(90, 0, 204, 170)),
            BorderThickness = new Thickness(1), Child = text, Cursor = Cursors.Hand,
            ToolTip = "Add a custom run category"
        };
        pill.MouseEnter += (_, _) => pill.Background = new SolidColorBrush(Color.FromArgb(30, 255, 255, 255));
        pill.MouseLeave += (_, _) => pill.Background = new SolidColorBrush(PillNormal);
        pill.MouseDown  += (_, _) => onClick();
        return pill;
    }

    private void BuildTogglePills(WrapPanel panel, List<string> options, HashSet<string> selected)
    {
        panel.Children.Clear();
        foreach (var option in options)
        {
            var opt = option;
            panel.Children.Add(MakeTogglePill(opt, selected.Contains(opt), () =>
            {
                if (!selected.Add(opt)) selected.Remove(opt);
                BuildTogglePills(panel, options, selected);
            }));
        }
    }

    private static Border MakeTogglePill(string label, bool active, Action onClick)
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

    private void Close_Click(object sender, RoutedEventArgs e) => Close();

    private async void SendButton_Click(object sender, RoutedEventArgs e)
    {
        string name        = SkipNameBox.Text.Trim();
        string chapter     = _selectedChapter ?? "";
        string version     = string.Join(", ", _selectedVersions);
        string videoUrl    = VideoUrlBox.Text.Trim();
        string author      = AuthorBox.Text.Trim();
        string description = DescriptionBox.Text.Trim();
        bool   cores       = CoresCheckBox.IsChecked == true;

        if (string.IsNullOrWhiteSpace(name))
        {
            ShowStatus("Please enter the skip's name.", Colors.Orange);
            return;
        }

        if (!SkipReportService.IsConfigured)
        {
            ShowStatus("Reporting isn't configured yet. Try again later.", Colors.Orange);
            return;
        }

        SendButton.IsEnabled = false;
        SendButtonText.Text  = "Sending...";
        ShowStatus("Sending...", Colors.Gray);

        var (success, error) = await SkipReportService.SendReportAsync(
            name, chapter, author, videoUrl, description,
            version, _selectedRunCategories, _selectedRoutes, _selectedRestrictions, cores);

        if (success)
        {
            ShowStatus("Report sent! Thank you.", Color.FromRgb(0, 204, 170));
            await Task.Delay(1800);
            Close();
        }
        else
        {
            ShowStatus(error ?? "Failed to send. Try again.", Colors.Red);
            SendButton.IsEnabled = true;
            SendButtonText.Text  = "Send Report";
        }
    }

    private void ShowStatus(string message, Color color)
    {
        StatusText.Text       = message;
        StatusText.Foreground = new SolidColorBrush(color);
        StatusText.Visibility = Visibility.Visible;
    }
}
