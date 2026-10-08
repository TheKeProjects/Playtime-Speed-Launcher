using System.Runtime.CompilerServices;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Shapes;
using System.Windows.Threading;

namespace SpeedrunLauncher.Seasonal;

/// <summary>
/// Re-tints the launcher's classic teal/blue UI (settings, overlays, dialogs inside the main
/// window) into the active season's colors. The UI hard-codes its colors in XAML and in
/// code-behind, so instead of rewriting every brush this walks the element tree and swaps
/// locally-set brushes whose color belongs to the classic palette:
///   teal accents   → the season's accent hue (orange / amber / gold)
///   blue neutrals  → the season's neutral hue (purple / indigo / pine green)
/// Lightness, saturation and alpha are kept, so contrast stays the same as the classic look.
/// The original brush is remembered, so switching back to "classic" restores it exactly.
/// Brushes assigned later by code-behind (tab switches, toggles...) are picked up by a pass
/// that runs after every click/key press and on a slow timer.
/// </summary>
public static class SeasonalRecolor
{
    /// <summary>Classic UI colors (0xRRGGBB) collected from MainWindow.xaml / MainWindow.xaml.cs.</summary>
    private static readonly HashSet<uint> ClassicColors =
    [
        0x00CCAA, 0x3A6A8A, 0x060F18, 0x8AAABB, 0x0D2535, 0x0A1825, 0x1A3A55, 0x0D2030, 0x09141E, 0xA0B4C8,
        0x153048, 0x2A5A7A, 0x00503C, 0x00B482, 0x00281E, 0xD2DCE6, 0x1E3C5A, 0x112233, 0x00CC88, 0x2D5A78,
        0x007864, 0x5A8AA0, 0x07101A, 0xA0BED2, 0x5082C8, 0x0A1420, 0xC8D2DC, 0x1A4060, 0x060C12, 0xA0C8D8,
        0x50AAE6, 0x081E37, 0x0E2A4E, 0x00C88C, 0x78A0BE, 0xA0BEDC, 0x0A283C, 0x6482A0, 0x08111A, 0x007755,
        0x1A3A50, 0x2A4A6A, 0x080C18, 0x4A5A7A, 0x334444, 0x050A12, 0x142637, 0x50A0E6, 0xA0C8E6, 0x96B4C8,
        0x00785A, 0x070C12, 0x00CC66, 0x003322, 0x050B12, 0x0A1D2E, 0x123049, 0x0F2438, 0xCCEEFF, 0xAABBCC,
        0x060B14, 0x1A2A38, 0x5A7A9A, 0x0D2A3B, 0xE6F0FF, 0x123C6E, 0x008CC8, 0x64AADC, 0xC8D2E1, 0x3C78A0,
        0x64A0DC, 0x00C896, 0x005064, 0x648CA0, 0x64A0C8, 0x8CAAC8, 0x1565C0, 0xB4D2E6,
    ];

    /// <summary>Colors used inside XAML styles/templates; exposed as "Seasonal.RRGGBB" DynamicResources.</summary>
    private static readonly uint[] ResourceColors = [0x00CCAA, 0x0A1825, 0x8AAABB, 0x123049, 0x0F2438, 0x1A3A55];

    private static readonly DependencyProperty[] BrushProperties =
    [
        Panel.BackgroundProperty, Control.BackgroundProperty, Border.BackgroundProperty, Border.BorderBrushProperty,
        Control.BorderBrushProperty, TextElement.ForegroundProperty, TextBlock.BackgroundProperty,
        Shape.FillProperty, Shape.StrokeProperty, TextBoxBase.CaretBrushProperty, TextBoxBase.SelectionBrushProperty,
    ];
    // (Control.Background/Foreground share their DependencyProperty with Panel.Background / TextElement.Foreground.)

    /// <summary>Seasonal brush → the classic brush it replaced.</summary>
    private static readonly ConditionalWeakTable<Brush, Brush> Originals = new();
    private static readonly Dictionary<(string, uint), SolidColorBrush> Cache = new();

    private static readonly List<WeakReference<FrameworkElement>> Roots = [];
    private static readonly DispatcherTimer SafetyTimer = new() { Interval = TimeSpan.FromSeconds(1.5) };
    private static bool _scheduled;
    private static bool _initialized;

    /// <summary>Keeps <paramref name="root"/> (and everything inside it) tinted for the active season.</summary>
    public static void Attach(FrameworkElement root)
    {
        if (!_initialized)
        {
            _initialized = true;
            SafetyTimer.Tick += (_, _) => Schedule();
            SeasonalThemeService.Instance.SeasonChanged += (_, _) => Refresh();
        }
        Roots.Add(new WeakReference<FrameworkElement>(root));
        root.AddHandler(UIElement.PreviewMouseUpEvent, new MouseButtonEventHandler((_, _) => Schedule()), true);
        root.AddHandler(UIElement.PreviewKeyUpEvent, new KeyEventHandler((_, _) => Schedule()), true);
        root.Loaded += (_, _) => Refresh();
        Refresh();
    }

    /// <summary>Re-applies the season everywhere right away (e.g. after the theme changes).</summary>
    public static void Refresh()
    {
        UpdateResources();
        if (Active) SafetyTimer.Start(); else SafetyTimer.Stop();
        RunPass();
    }

    private static bool Active => SeasonalThemeService.Instance.ActiveSeason != "classic";

    /// <summary>Queues a pass after the current input has been handled (so code-behind has set its brushes).</summary>
    private static void Schedule()
    {
        if (_scheduled) return;
        if (!Active && Cache.Count == 0) return; // never tinted anything: nothing to restore
        _scheduled = true;
        Dispatcher.CurrentDispatcher.BeginInvoke(DispatcherPriority.Background, () =>
        {
            _scheduled = false;
            RunPass();
        });
    }

    private static void UpdateResources()
    {
        if (Application.Current == null) return;
        foreach (var rgb in ResourceColors)
        {
            var classic = Color.FromRgb((byte)(rgb >> 16), (byte)(rgb >> 8), (byte)rgb);
            var brush = new SolidColorBrush(Map(classic, SeasonalThemeService.Instance.ActiveSeason));
            brush.Freeze();
            Application.Current.Resources[$"Seasonal.{rgb:X6}"] = brush;
        }
    }

    private static void RunPass()
    {
        var season = SeasonalThemeService.Instance.ActiveSeason;
        var visited = new HashSet<DependencyObject>();
        for (int i = Roots.Count - 1; i >= 0; i--)
        {
            if (!Roots[i].TryGetTarget(out var root)) { Roots.RemoveAt(i); continue; }
            Walk(root, season, visited);
        }
    }

    private static void Walk(DependencyObject node, string season, HashSet<DependencyObject> visited)
    {
        if (!visited.Add(node)) return;
        if (node is SeasonalBackdrop or SeasonalOverlay) return;

        foreach (var dp in BrushProperties)
        {
            // Only local values: brushes from styles/triggers/bindings/DynamicResources are left alone
            // (overriding them would break hover effects and the like).
            if (node.ReadLocalValue(dp) is not Brush current) continue;

            var original = Originals.TryGetValue(current, out var o) ? o : current;
            var target = season == "classic" ? original : Tint(original, season);
            if (!ReferenceEquals(target, current)) node.SetValue(dp, target);
        }

        // Logical children cover collapsed/untemplated content, visual children cover templates.
        foreach (var child in LogicalTreeHelper.GetChildren(node))
            if (child is DependencyObject d) Walk(d, season, visited);
        if (node is Visual or System.Windows.Media.Media3D.Visual3D)
        {
            var count = VisualTreeHelper.GetChildrenCount(node);
            for (int i = 0; i < count; i++) Walk(VisualTreeHelper.GetChild(node, i), season, visited);
        }
    }

    /// <summary>Seasonal version of a classic brush, or the same brush if it isn't part of the classic palette.</summary>
    private static Brush Tint(Brush original, string season)
    {
        switch (original)
        {
            case SolidColorBrush solid:
            {
                if (!IsClassic(solid.Color)) return original;
                var key = (season, ((uint)solid.Color.A << 24) | Rgb(solid.Color));
                if (solid.Opacity != 1 || !Cache.TryGetValue(key, out var tinted))
                {
                    tinted = new SolidColorBrush(Map(solid.Color, season)) { Opacity = solid.Opacity };
                    tinted.Freeze();
                    if (solid.Opacity == 1) Cache[key] = tinted;
                }
                Register(tinted, original);
                return tinted;
            }
            case GradientBrush gradient when gradient.GradientStops.Any(s => IsClassic(s.Color)):
            {
                var clone = gradient.CloneCurrentValue();
                foreach (var stop in clone.GradientStops) stop.Color = Map(stop.Color, season);
                clone.Freeze();
                Register(clone, original);
                return clone;
            }
            default:
                return original;
        }
    }

    private static void Register(Brush tinted, Brush original)
    {
        Originals.AddOrUpdate(tinted, original);
    }

    private static uint Rgb(Color c) => ((uint)c.R << 16) | ((uint)c.G << 8) | c.B;
    private static bool IsClassic(Color c) => ClassicColors.Contains(Rgb(c));

    /// <summary>Maps a classic color into the season's hues (identity for classic / non-palette colors).</summary>
    public static Color Map(Color c, string season)
    {
        if (season == "classic" || !IsClassic(c)) return c;

        RgbToHsl(c, out var h, out var s, out var l);
        var teal = h < 185;
        double nh, ns = s, nl = l;
        switch (season)
        {
            case "halloween":
                if (teal) { nh = 26; nl = Math.Min(0.72, l * 1.12); }
                else      { nh = 272 + (h - 205) * 0.4; ns = Math.Min(1, s * 1.05); }
                break;
            case "summer":
                if (teal) { nh = 40; nl = Math.Min(0.72, l * 1.15); }
                else      { nh = 248 + (h - 205) * 0.4; }
                break;
            case "christmas":
                if (teal) { nh = 45; nl = Math.Min(0.72, l * 1.15); }
                else      { nh = 152 + (h - 205) * 0.3; ns = s * 0.9; }
                break;
            default:
                return c;
        }
        var mapped = HslToRgb((nh % 360 + 360) % 360, ns, nl);
        return Color.FromArgb(c.A, mapped.R, mapped.G, mapped.B);
    }

    private static void RgbToHsl(Color c, out double h, out double s, out double l)
    {
        double r = c.R / 255.0, g = c.G / 255.0, b = c.B / 255.0;
        double max = Math.Max(r, Math.Max(g, b)), min = Math.Min(r, Math.Min(g, b));
        l = (max + min) / 2;
        if (max == min) { h = s = 0; return; }
        var d = max - min;
        s = l > 0.5 ? d / (2 - max - min) : d / (max + min);
        if (max == r) h = (g - b) / d + (g < b ? 6 : 0);
        else if (max == g) h = (b - r) / d + 2;
        else h = (r - g) / d + 4;
        h *= 60;
    }

    private static Color HslToRgb(double h, double s, double l)
    {
        double Hue(double p, double q, double t)
        {
            if (t < 0) t += 1;
            if (t > 1) t -= 1;
            if (t < 1.0 / 6) return p + (q - p) * 6 * t;
            if (t < 0.5) return q;
            if (t < 2.0 / 3) return p + (q - p) * (2.0 / 3 - t) * 6;
            return p;
        }
        double r, g, b;
        if (s == 0) r = g = b = l;
        else
        {
            var q = l < 0.5 ? l * (1 + s) : l + s - l * s;
            var p = 2 * l - q;
            var hk = h / 360;
            r = Hue(p, q, hk + 1.0 / 3);
            g = Hue(p, q, hk);
            b = Hue(p, q, hk - 1.0 / 3);
        }
        return Color.FromRgb((byte)Math.Round(r * 255), (byte)Math.Round(g * 255), (byte)Math.Round(b * 255));
    }
}
