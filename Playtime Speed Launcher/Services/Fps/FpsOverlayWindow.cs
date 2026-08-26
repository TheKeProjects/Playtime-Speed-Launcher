using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Threading;

namespace SpeedrunLauncher.Services.Fps;

/// <summary>
/// Tiny always-on-top HUD pinned to a screen corner, showing the FPS reading GameFpsService
/// reports for whichever game process is currently being tracked. Click-through so it never
/// intercepts input meant for the game underneath. Built entirely in code (no XAML) so this
/// feature stays a self-contained addition under Services/Fps.
/// </summary>
public sealed class FpsOverlayWindow : Window
{
    // The launcher ships "Poppy Playtime.ttf" under Assets/Fonts (extracted to disk at startup
    // by ResourceExtractor) — its internal family name is "VCR OSD Mono", not "Poppy Playtime",
    // so that's what WPF's "./#<family>" file-font syntax has to reference.
    public static readonly FontFamily PoppyPlaytimeFont = LoadPoppyPlaytimeFont();
    public static readonly FontFamily MonospaceFont = new("Cascadia Code, Consolas, Courier New");

    private static FontFamily LoadPoppyPlaytimeFont()
    {
        try
        {
            var fontDir = Path.Combine(ResourceExtractor.TempDir, "Assets", "Fonts");
            return new FontFamily(new Uri(fontDir + "/", UriKind.Absolute), "./#VCR OSD Mono");
        }
        catch
        {
            return MonospaceFont;
        }
    }

    private readonly TextBlock _text;

    // Some games (especially borderless-fullscreen) periodically re-assert their own window as
    // HWND_TOPMOST to stay above the taskbar. That wins the z-order fight against WPF's
    // Topmost=true — which is only (re)applied when this window activates — and can permanently
    // bury the overlay behind the game even though both are nominally "topmost". Re-asserting
    // our own topmost position on a timer keeps this overlay on top no matter how often the game
    // re-asserts its own.
    private readonly DispatcherTimer _topmostTimer = new() { Interval = TimeSpan.FromSeconds(1.5) };

    // Last corner placement request, re-applied every time the window's actual size changes
    // (not just once after Show()) — the custom file-based font can finish loading a moment
    // after the first layout pass, which resizes the window slightly and would otherwise leave
    // a one-shot position calculation stale.
    private Rect   _workArea;
    private string _corner = "top-right";
    private double _margin = 20;

    public FpsOverlayWindow()
    {
        Title              = "FPS";
        WindowStyle        = WindowStyle.None;
        AllowsTransparency = true;
        Background         = Brushes.Transparent;
        ShowInTaskbar      = false;
        Topmost            = true;
        ResizeMode         = ResizeMode.NoResize;
        SizeToContent      = SizeToContent.WidthAndHeight;

        _text = new TextBlock
        {
            Text       = "FPS: --",
            FontFamily = PoppyPlaytimeFont,
            FontSize   = 18,
            FontWeight = FontWeights.Bold,
            Foreground = Brushes.White,
            Padding    = new Thickness(10, 5, 10, 5),
        };

        Content = new Border
        {
            Background   = new SolidColorBrush(Color.FromArgb(170, 0, 0, 0)),
            CornerRadius = new CornerRadius(6),
            Child        = _text,
        };

        SizeChanged += (_, _) => Reposition();
    }

    public void SetFps(double fps) => _text.Text = $"FPS: {fps:F0}";

    public void SetFont(FontFamily font) => _text.FontFamily = font;

    /// <summary>Sets the text size and scales the surrounding padding to match.</summary>
    public void SetSize(double fontSize)
    {
        _text.FontSize = fontSize;
        var pad = fontSize * 0.55;
        _text.Padding = new Thickness(pad * 1.8, pad, pad * 1.8, pad);
    }

    /// <summary>Positions the overlay in a corner of the given screen work area.</summary>
    public void PlaceInCorner(Rect workArea, string corner, double margin = 20)
    {
        _workArea = workArea;
        _corner   = corner;
        _margin   = margin;
        Reposition();
    }

    private void Reposition()
    {
        if (_workArea.IsEmpty) return;

        // ActualWidth/ActualHeight reflect the current rendered size even if this fires from a
        // late re-layout (e.g. the custom font finishing its load) — Width/Height alone can lag.
        double w = ActualWidth  > 0 ? ActualWidth  : Width;
        double h = ActualHeight > 0 ? ActualHeight : Height;

        Left = _corner is "top-right" or "bottom-right"
            ? _workArea.Right - w - _margin
            : _workArea.Left + _margin;
        Top = _corner is "bottom-left" or "bottom-right"
            ? _workArea.Bottom - h - _margin
            : _workArea.Top + _margin;
    }

    protected override void OnSourceInitialized(EventArgs e)
    {
        base.OnSourceInitialized(e);

        var hwnd = new WindowInteropHelper(this).Handle;
        var ex   = NativeMethods.GetWindowLong(hwnd, NativeMethods.GWL_EXSTYLE);
        NativeMethods.SetWindowLong(hwnd, NativeMethods.GWL_EXSTYLE,
            ex | NativeMethods.WS_EX_TRANSPARENT | NativeMethods.WS_EX_LAYERED);

        _topmostTimer.Tick += (_, _) => NativeMethods.SetWindowPos(hwnd, NativeMethods.HWND_TOPMOST,
            0, 0, 0, 0, NativeMethods.SWP_NOMOVE | NativeMethods.SWP_NOSIZE | NativeMethods.SWP_NOACTIVATE);
        _topmostTimer.Start();
    }

    protected override void OnClosed(EventArgs e)
    {
        _topmostTimer.Stop();
        base.OnClosed(e);
    }

    private static class NativeMethods
    {
        public const int GWL_EXSTYLE     = -20;
        public const int WS_EX_LAYERED   = 0x80000;
        public const int WS_EX_TRANSPARENT = 0x20;

        public static readonly nint HWND_TOPMOST = -1;
        public const uint SWP_NOSIZE     = 0x0001;
        public const uint SWP_NOMOVE     = 0x0002;
        public const uint SWP_NOACTIVATE = 0x0010;

        [DllImport("user32.dll")]
        public static extern int GetWindowLong(nint hWnd, int nIndex);

        [DllImport("user32.dll")]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool SetWindowPos(nint hWnd, nint hWndInsertAfter,
            int x, int y, int cx, int cy, uint uFlags);

        [DllImport("user32.dll")]
        public static extern int SetWindowLong(nint hWnd, int nIndex, int dwNewLong);
    }
}
