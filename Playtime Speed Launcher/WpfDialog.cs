using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

namespace SpeedrunLauncher;

public enum WpfDialogResult { Primary, Secondary, Close }

/// <summary>
/// Dark-themed modal dialog — replacement for WinUI ContentDialog.
/// Call Show() for synchronous/blocking use, or ShowAsync() from async code on the UI thread.
/// ShowDialog() pumps a nested message loop so the UI stays responsive.
/// </summary>
public class WpfDialog : Window
{
    public WpfDialogResult Result { get; private set; } = WpfDialogResult.Close;

    public WpfDialog(Window owner, string title, UIElement content,
        string? primaryText = null, string? secondaryText = null, string? closeText = null)
    {
        Owner = owner;
        WindowStartupLocation = WindowStartupLocation.CenterOwner;
        WindowStyle = WindowStyle.None;
        ResizeMode = ResizeMode.NoResize;
        SizeToContent = SizeToContent.WidthAndHeight;
        ShowInTaskbar = false;
        Background = new SolidColorBrush(Color.FromRgb(9, 20, 30));

        var grid = new Grid();
        grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        grid.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
        grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });

        // ── Title bar ──────────────────────────────────────────────────────────
        var titleBar = new Border
        {
            Background = new SolidColorBrush(Color.FromRgb(6, 15, 24)),
            BorderBrush = new SolidColorBrush(Color.FromRgb(13, 32, 48)),
            BorderThickness = new Thickness(0, 0, 0, 1),
            Padding = new Thickness(20, 0, 20, 0),
            Height = 48,
        };
        titleBar.Child = new TextBlock
        {
            Text = title,
            FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
            FontSize = 14,
            FontWeight = FontWeights.Bold,
            Foreground = new SolidColorBrush(Color.FromRgb(0, 204, 170)),
            VerticalAlignment = VerticalAlignment.Center,
        };
        Grid.SetRow(titleBar, 0);
        grid.Children.Add(titleBar);

        // ── Content ────────────────────────────────────────────────────────────
        var contentBorder = new Border { Padding = new Thickness(20, 16, 20, 16) };
        contentBorder.Child = content;
        Grid.SetRow(contentBorder, 1);
        grid.Children.Add(contentBorder);

        // ── Button row ─────────────────────────────────────────────────────────
        bool hasButtons = primaryText != null || secondaryText != null ||
                          (closeText != null && closeText != "");
        if (hasButtons)
        {
            var btnBar = new Border
            {
                Background = new SolidColorBrush(Color.FromRgb(6, 15, 24)),
                BorderBrush = new SolidColorBrush(Color.FromRgb(13, 32, 48)),
                BorderThickness = new Thickness(0, 1, 0, 0),
                Padding = new Thickness(14, 0, 14, 0),
                Height = 52,
            };
            var btnPanel = new StackPanel
            {
                Orientation = Orientation.Horizontal,
                HorizontalAlignment = HorizontalAlignment.Right,
                VerticalAlignment = VerticalAlignment.Center,
            };

            if (closeText != null && closeText != "")
            {
                var b = MakeButton(closeText, false);
                b.Click += (_, _) => { Result = WpfDialogResult.Close; Close(); };
                btnPanel.Children.Add(b);
            }
            if (secondaryText != null)
            {
                var b = MakeButton(secondaryText, false);
                b.Margin = new Thickness(8, 0, 0, 0);
                b.Click += (_, _) => { Result = WpfDialogResult.Secondary; Close(); };
                btnPanel.Children.Add(b);
            }
            if (primaryText != null)
            {
                var b = MakeButton(primaryText, true);
                b.Margin = new Thickness(8, 0, 0, 0);
                b.Click += (_, _) => { Result = WpfDialogResult.Primary; Close(); };
                btnPanel.Children.Add(b);
            }

            btnBar.Child = btnPanel;
            Grid.SetRow(btnBar, 2);
            grid.Children.Add(btnBar);
        }

        // Outer border (gives visual edge since WindowStyle=None)
        Content = new Border
        {
            BorderBrush = new SolidColorBrush(Color.FromRgb(21, 48, 72)),
            BorderThickness = new Thickness(1),
            Child = grid,
        };
    }

    private static Button MakeButton(string text, bool isPrimary)
    {
        var btn = new Button
        {
            Height = 36,
            MinWidth = 100,
            Background = new SolidColorBrush(isPrimary
                ? Color.FromRgb(0, 80, 60)
                : Color.FromRgb(10, 24, 37)),
            BorderBrush = new SolidColorBrush(isPrimary
                ? Color.FromArgb(180, 0, 180, 130)
                : Color.FromRgb(13, 37, 53)),
            BorderThickness = new Thickness(1),
            Padding = new Thickness(12, 0, 12, 0),
            Content = new TextBlock
            {
                Text = text,
                FontFamily = new FontFamily("Cascadia Code, Consolas, Courier New"),
                FontSize = 11,
                FontWeight = FontWeights.Bold,
                Foreground = new SolidColorBrush(isPrimary
                    ? Color.FromRgb(0, 204, 170)
                    : Color.FromArgb(200, 58, 106, 138)),
            },
        };
        ButtonHelper.SetCornerRadius(btn, new CornerRadius(3));
        return btn;
    }

    // ── Static helpers ─────────────────────────────────────────────────────────

    public static WpfDialogResult Show(Window owner, string title, UIElement content,
        string? primaryText = null, string? secondaryText = null, string? closeText = null)
    {
        var dlg = new WpfDialog(owner, title, content, primaryText, secondaryText, closeText);
        dlg.ShowDialog();
        return dlg.Result;
    }

    // Async wrapper — ShowDialog() runs a nested message loop so this is safe from async code on the UI thread.
    public static Task<WpfDialogResult> ShowAsync(Window owner, string title, UIElement content,
        string? primaryText = null, string? secondaryText = null, string? closeText = null)
        => Task.FromResult(Show(owner, title, content, primaryText, secondaryText, closeText));
}
