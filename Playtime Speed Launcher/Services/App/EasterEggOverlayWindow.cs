using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Threading;

namespace SpeedrunLauncher.Services.App;

/// <summary>
/// Always-on-top window that plays an easter egg video over the game while it's in the
/// foreground. Click-through and never activated, so the game keeps keyboard focus and
/// nothing gets minimized. Built in code like <see cref="Fps.FpsOverlayWindow"/>.
/// </summary>
public sealed class EasterEggOverlayWindow : Window
{
    private readonly MediaElement _player;

    // Borderless-fullscreen games keep re-asserting HWND_TOPMOST; re-assert ours too so the
    // video doesn't get buried behind the game (same approach as the FPS overlay).
    private readonly DispatcherTimer _topmostTimer = new() { Interval = TimeSpan.FromSeconds(1) };

    /// <summary>Raised when the video ends or fails to play.</summary>
    public event Action? Finished;

    public EasterEggOverlayWindow(string videoPath)
    {
        Title              = "Easter egg";
        WindowStyle        = WindowStyle.None;
        AllowsTransparency = true;
        Background         = new SolidColorBrush(Color.FromArgb(0xCC, 0, 0, 0));
        ShowInTaskbar      = false;
        ShowActivated      = false;
        Topmost            = true;
        ResizeMode         = ResizeMode.NoResize;
        Left               = 0;
        Top                = 0;
        Width              = SystemParameters.PrimaryScreenWidth;
        Height             = SystemParameters.PrimaryScreenHeight;

        _player = new MediaElement
        {
            LoadedBehavior   = MediaState.Manual,
            UnloadedBehavior = MediaState.Stop,
            Stretch          = Stretch.Uniform,
            Source           = new Uri(videoPath),
        };
        _player.MediaEnded  += (_, _) => Finished?.Invoke();
        _player.MediaFailed += (_, _) => Finished?.Invoke();

        Content = new Border
        {
            Width               = Math.Min(960, Width  * 0.8),
            Height              = Math.Min(570, Height * 0.8),
            Background          = new SolidColorBrush(Color.FromRgb(0x09, 0x0C, 0x1E)),
            BorderBrush         = new SolidColorBrush(Color.FromRgb(0x2A, 0x10, 0x50)),
            BorderThickness     = new Thickness(1),
            CornerRadius        = new CornerRadius(4),
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment   = VerticalAlignment.Center,
            Child               = _player,
        };

        Loaded += (_, _) => _player.Play();
    }

    protected override void OnSourceInitialized(EventArgs e)
    {
        base.OnSourceInitialized(e);

        var hwnd = new WindowInteropHelper(this).Handle;
        var ex   = NativeMethods.GetWindowLong(hwnd, NativeMethods.GWL_EXSTYLE);
        NativeMethods.SetWindowLong(hwnd, NativeMethods.GWL_EXSTYLE,
            ex | NativeMethods.WS_EX_TRANSPARENT | NativeMethods.WS_EX_LAYERED
               | NativeMethods.WS_EX_NOACTIVATE  | NativeMethods.WS_EX_TOOLWINDOW);

        _topmostTimer.Tick += (_, _) => NativeMethods.SetWindowPos(hwnd, NativeMethods.HWND_TOPMOST,
            0, 0, 0, 0, NativeMethods.SWP_NOMOVE | NativeMethods.SWP_NOSIZE | NativeMethods.SWP_NOACTIVATE);
        _topmostTimer.Start();
    }

    protected override void OnClosed(EventArgs e)
    {
        _topmostTimer.Stop();
        _player.Stop();
        _player.Source = null;
        base.OnClosed(e);
    }

    private static class NativeMethods
    {
        public const int GWL_EXSTYLE       = -20;
        public const int WS_EX_TRANSPARENT = 0x20;
        public const int WS_EX_TOOLWINDOW  = 0x80;
        public const int WS_EX_LAYERED     = 0x80000;
        public const int WS_EX_NOACTIVATE  = 0x8000000;

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
