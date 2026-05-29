using System.Diagnostics;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using IOPath = System.IO.Path;
using Loc = SpeedrunLauncher.Services.LocalizationService;

namespace SpeedrunLauncher;

public partial class HotkeyOverlay : Window
{
    private record CheckpointItem(string Source, string Dest, string? DeletePattern = null);
    private record DeleteInfo(string Dest, string? DeletePattern);

    private CheckpointItem? _selected;
    private Border?         _selectedBorder;
    private DeleteInfo?     _deleteInfo;

    private static readonly Color NormalBg = Color.FromArgb( 12, 255, 255, 255);
    private static readonly Color HoverBg  = Color.FromArgb( 28, 255, 255, 255);
    private static readonly Color SelBg    = Color.FromArgb( 50,   0, 204, 170);

    public HotkeyOverlay(string?[] chapterExePaths)
    {
        InitializeComponent();
        HintText.Text      = Loc.Get("hotkey_hint");
        CloseBtnText.Text  = Loc.Get("back");
        DeleteBtnText.Text = Loc.Get("hotkey_delete_btn");
        PopulateAll(chapterExePaths);
    }

    // Returns 1-based chapter number of the first running chapter exe, or 0 if none.
    private static int DetectRunningChapter(string?[] exePaths)
    {
        for (int i = 0; i < exePaths.Length; i++)
        {
            var path = exePaths[i];
            if (string.IsNullOrEmpty(path)) continue;
            var processName = IOPath.GetFileNameWithoutExtension(path);
            try
            {
                if (Process.GetProcessesByName(processName).Length > 0)
                    return i + 1;
            }
            catch { }
        }
        return 0;
    }

    private void PopulateAll(string?[] exePaths)
    {
        var detected = DetectRunningChapter(exePaths);

        var localApp  = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var savesBase = IOPath.Combine(Services.ResourceExtractor.TempDir, "Assets", "Saves");

        var chapters = new (int Num, string SavesDir, string? SaveFile, string Dest, string? DeletePattern)[]
        {
            (1, IOPath.Combine(savesBase, "Chapter 1"), "Chap1Checkpoint.sav",
             IOPath.Combine(localApp, "Poppy_Playtime",      "Saved", "SaveGames", "Chap1Checkpoint.sav"), null),
            (2, IOPath.Combine(savesBase, "Chapter 2"), "Chap2Checkpoint.sav",
             IOPath.Combine(localApp, "Playtime_Prototype4", "Saved", "SaveGames", "Chap2Checkpoint.sav"), null),
            (3, IOPath.Combine(savesBase, "Chapter 3"), "Playtime.sav",
             IOPath.Combine(localApp, "Playtime_Chapter3",   "Saved", "SaveGames", "Playtime.sav"), null),
            (4, IOPath.Combine(savesBase, "Chapter 4"), null,
             IOPath.Combine(localApp, "ch4_pro",            "Saved", "SaveGames"), "SaveGame_*"),
            (5, IOPath.Combine(savesBase, "Chapter 5"), null,
             IOPath.Combine(localApp, "ch5_pro",            "Saved", "SaveGames"), "PoppySave_*"),
        };

        if (detected > 0)
        {
            // Show only the running chapter — narrow panel, chapter name as header
            ContentPanel.Width = 480;
            HeaderText.Text    = $"[ {Loc.Get($"ch{detected}_title")} ]";
            LoadBtnText.Text   = Loc.Get(detected >= 4 ? "hotkey_load_btn" : "hotkey_load_all_btn");

            var ch = chapters.FirstOrDefault(c => c.Num == detected);
            if (ch.Num > 0)
            {
                _deleteInfo          = new DeleteInfo(ch.Dest, ch.DeletePattern);
                DeleteBtn.Visibility = Visibility.Visible;

                if (ch.DeletePattern != null)
                {
                    // ch4/5: no checkpoint list — pre-arm the load button with the full saves folder
                    _selected         = new CheckpointItem(ch.SavesDir, ch.Dest, ch.DeletePattern);
                    LoadBtn.IsEnabled = true;
                    return;
                }
            }
        }
        else
        {
            // No chapter detected — show ch1-3 only with section labels
            ContentPanel.Width = 520;
            HeaderText.Text    = Loc.Get("hotkey_header");
            LoadBtnText.Text   = Loc.Get("hotkey_load_all_btn");
        }

        bool firstSection = true;
        foreach (var ch in chapters)
        {
            if (detected > 0 && ch.Num != detected) continue;
            if (detected == 0 && ch.Num > 3) continue;
            AddSection(ch.Num, ch.SavesDir, ch.SaveFile, ch.Dest, ch.DeletePattern,
                       firstSection, showSectionLabel: detected == 0);
            firstSection = false;
        }
    }

    private void AddSection(int chNum, string savesDir, string? saveFile, string dest, string? deletePattern,
                            bool firstSection, bool showSectionLabel)
    {
        if (!Directory.Exists(savesDir)) return;

        var entries = Directory.GetDirectories(savesDir)
            .OrderBy(d =>
            {
                var m = System.Text.RegularExpressions.Regex.Match(IOPath.GetFileName(d), @"^\[(\d+)\]");
                return m.Success ? int.Parse(m.Groups[1].Value) : 999;
            })
            .Select(d => saveFile != null
                ? (name: IOPath.GetFileName(d), src: IOPath.Combine(d, saveFile), isDir: false)
                : (name: IOPath.GetFileName(d), src: d, isDir: true))
            .Where(e => e.isDir ? (Directory.Exists(e.src) && Directory.GetFiles(e.src).Length > 0) : File.Exists(e.src))
            .ToList();

        if (entries.Count == 0) return;

        // Divider between sections (only for the all-chapters view)
        if (!firstSection && showSectionLabel)
        {
            ChaptersPanel.Children.Add(new Border
            {
                BorderBrush     = new SolidColorBrush(Color.FromArgb(60, 13, 32, 48)),
                BorderThickness = new Thickness(0, 1, 0, 0),
                Margin          = new Thickness(0, 10, 0, 10),
            });
        }

        // Section label when showing all chapters
        if (showSectionLabel)
        {
            ChaptersPanel.Children.Add(new TextBlock
            {
                Text       = Loc.Get($"ch{chNum}_title"),
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize   = 9, FontWeight = FontWeights.Bold,
                Foreground = new SolidColorBrush(Color.FromArgb(255, 0, 204, 170)),
                Margin     = new Thickness(0, 0, 0, 6),
            });
        }

        bool first = true;
        foreach (var (name, src, _) in entries)
        {
            ChaptersPanel.Children.Add(MakeRow(name, new CheckpointItem(src, dest, deletePattern), first));
            first = false;
        }
    }

    private Border MakeRow(string name, CheckpointItem item, bool first)
    {
        var icon = new TextBlock
        {
            FontFamily        = new FontFamily("Segoe MDL2 Assets"),
            Text              = "",
            FontSize          = 11,
            Foreground        = new SolidColorBrush(Color.FromArgb(200, 0, 204, 170)),
            VerticalAlignment = VerticalAlignment.Center,
        };
        var label = new TextBlock
        {
            Text              = name,
            FontFamily        = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize          = 10,
            FontWeight        = FontWeights.Bold,
            Foreground        = new SolidColorBrush(Color.FromArgb(255, 200, 210, 220)),
            VerticalAlignment = VerticalAlignment.Center,
            TextWrapping      = TextWrapping.Wrap,
            Margin            = new Thickness(8, 0, 0, 0),
        };
        var content = new StackPanel { Orientation = Orientation.Horizontal };
        content.Children.Add(icon);
        content.Children.Add(label);

        var row = new Border
        {
            Background   = new SolidColorBrush(NormalBg),
            CornerRadius = new CornerRadius(3),
            Padding      = new Thickness(10, 7, 10, 7),
            Child        = content,
            Margin       = first ? new Thickness(0) : new Thickness(0, 3, 0, 0),
            Cursor       = Cursors.Hand,
        };

        row.MouseEnter += (_, _) => { if (_selectedBorder != row) row.Background = new SolidColorBrush(HoverBg); };
        row.MouseLeave += (_, _) => { if (_selectedBorder != row) row.Background = new SolidColorBrush(NormalBg); };
        row.MouseDown  += (_, _) => SelectRow(row, item);

        return row;
    }

    private void SelectRow(Border row, CheckpointItem item)
    {
        if (_selectedBorder != null)
            _selectedBorder.Background = new SolidColorBrush(NormalBg);

        _selected         = item;
        _selectedBorder   = row;
        row.Background    = new SolidColorBrush(SelBg);
        LoadBtn.IsEnabled = true;
    }

    private void LoadBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_selected == null) return;
        try
        {
            if (_selected.DeletePattern != null)
            {
                Directory.CreateDirectory(_selected.Dest);
                foreach (var f in Directory.GetFiles(_selected.Dest, _selected.DeletePattern))
                    File.Delete(f);
                foreach (var f in Directory.GetFiles(_selected.Source))
                    File.Copy(f, IOPath.Combine(_selected.Dest, IOPath.GetFileName(f)), overwrite: true);
            }
            else
            {
                Directory.CreateDirectory(IOPath.GetDirectoryName(_selected.Dest)!);
                File.Copy(_selected.Source, _selected.Dest, overwrite: true);
            }
        }
        catch { }
        Close();
    }

    private void DeleteBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_deleteInfo == null) return;
        try
        {
            if (_deleteInfo.DeletePattern != null)
            {
                if (Directory.Exists(_deleteInfo.Dest))
                    foreach (var f in Directory.GetFiles(_deleteInfo.Dest, _deleteInfo.DeletePattern))
                        File.Delete(f);
            }
            else
            {
                if (File.Exists(_deleteInfo.Dest))
                    File.Delete(_deleteInfo.Dest);
            }
        }
        catch { }
        Close();
    }

    private void CloseBtn_Click(object sender, RoutedEventArgs e) => Close();

    private void Window_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Escape) Close();
    }

    private void Backdrop_MouseDown(object sender, MouseButtonEventArgs e) => Close();
}
