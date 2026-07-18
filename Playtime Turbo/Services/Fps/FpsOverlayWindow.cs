using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Interop;
using System.Windows.Media;

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
        Left = corner is "top-right" or "bottom-right"
            ? workArea.Right - Width - margin
            : workArea.Left + margin;
        Top = corner is "bottom-left" or "bottom-right"
            ? workArea.Bottom - Height - margin
            : workArea.Top + margin;
    }

    protected override void OnSourceInitialized(EventArgs e)
    {
        base.OnSourceInitialized(e);

        var hwnd = new WindowInteropHelper(this).Handle;
        var ex   = NativeMethods.GetWindowLong(hwnd, NativeMethods.GWL_EXSTYLE);
        NativeMethods.SetWindowLong(hwnd, NativeMethods.GWL_EXSTYLE,
            ex | NativeMethods.WS_EX_TRANSPARENT | NativeMethods.WS_EX_LAYERED);
    }

    private static class NativeMethods
    {
        public const int GWL_EXSTYLE     = -20;
        public const int WS_EX_LAYERED   = 0x80000;
        public const int WS_EX_TRANSPARENT = 0x20;

        [DllImport("user32.dll")]
        public static extern int GetWindowLong(nint hWnd, int nIndex);

        [DllImport("user32.dll")]
        public static extern int SetWindowLong(nint hWnd, int nIndex, int dwNewLong);
    }
}
