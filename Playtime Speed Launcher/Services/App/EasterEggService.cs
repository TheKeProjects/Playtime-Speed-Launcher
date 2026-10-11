using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace SpeedrunLauncher.Services.App;

/// <summary>
/// Tracks key-sequence easter eggs (Konami code, typing "gane", ...) and plays
/// the matching video in the overlay <see cref="MediaElement"/> passed in.
/// </summary>
public sealed class EasterEggService
{
    private static readonly Key[] KonamiSequence =
        [Key.Up, Key.Up, Key.Down, Key.Down, Key.Left, Key.Right, Key.Left, Key.Right, Key.B, Key.A, Key.Enter];
    private static readonly Key[] GaneSequence = 
        [Key.G, Key.A, Key.N, Key.E];
    private static readonly Key[] RaulBecasSequence =
        [Key.R, Key.A, Key.U, Key.L, Key.B, Key.E, Key.C, Key.A, Key.S];
    private static readonly Key[] TomhSequence =
        [Key.T, Key.O, Key.M, Key.H];
    private static readonly Key[] PichaSequence =
        [Key.P, Key.I, Key.C, Key.H, Key.A];
    private static readonly Key[] GokuSequence =
        [Key.G, Key.O, Key.K, Key.U];

    private readonly List<Key> _konamiBuffer     = [];
    private readonly List<Key> _ganeBuffer       = [];
    private readonly List<Key> _raulBecasBuffer  = [];
    private readonly List<Key> _tomhBuffer       = [];
    private readonly List<Key> _pichaBuffer      = [];
    private readonly List<Key> _gokuBuffer       = [];

    private readonly MediaElement _player;
    private readonly UIElement    _overlay;
    private readonly Func<bool>   _isGameForeground;

    // Set while a video is playing over the game instead of inside the launcher.
    private EasterEggOverlayWindow? _gameOverlay;

    public bool Playing { get; private set; }

    /// <summary>True while the video is being shown over the game.</summary>
    public bool PlayingInGame => _gameOverlay != null;

    public EasterEggService(MediaElement player, UIElement overlay, Func<bool> isGameForeground)
    {
        _player           = player;
        _overlay          = overlay;
        _isGameForeground = isGameForeground;
    }

    public void HandleKeyDown(KeyEventArgs e) =>
        HandleKey(e.Key == Key.System ? e.SystemKey : e.Key);

    /// <summary>
    /// Feeds one key press. Also called with keys read from the launcher's low-level
    /// keyboard hooks, since WPF key events don't arrive reliably while those are installed.
    /// </summary>
    public void HandleKey(Key key)
    {
        if (key == Key.Escape && Playing)
        {
            Hide();
            return;
        }

        TrackSequence(_konamiBuffer,    KonamiSequence,    key, "Torrente.mp4");
        TrackSequence(_ganeBuffer,      GaneSequence,      key, "Gane.mp4");
        TrackSequence(_raulBecasBuffer, RaulBecasSequence, key, "RaulBecas.mp4");
        TrackSequence(_tomhBuffer,      TomhSequence,      key, "Tomh.mp4");
        TrackSequence(_pichaBuffer,     PichaSequence,     key, "Picha.mp4");
        TrackSequence(_gokuBuffer,      GokuSequence,      key, "Goku.mp4");
    }

    private void TrackSequence(List<Key> buffer, Key[] sequence, Key key, string fileName)
    {
        if (key != sequence[buffer.Count])
        {
            buffer.Clear();
            if (key == sequence[0]) buffer.Add(key);
            return;
        }

        buffer.Add(key);
        if (buffer.Count < sequence.Length) return;

        buffer.Clear();
        Show(fileName);
    }

    private void Show(string fileName)
    {
        if (Playing) return;

        var videoPath = Path.Combine(ResourceExtractor.TempDir, "Assets", "Videos", "EasterEggs", fileName);
        if (!File.Exists(videoPath)) return;

        Playing = true;

        if (_isGameForeground())
        {
            try
            {
                _gameOverlay = new EasterEggOverlayWindow(videoPath);
                _gameOverlay.Finished += Hide;
                _gameOverlay.Show();
            }
            catch
            {
                Hide();
            }
            return;
        }

        _player.Source       = new Uri(videoPath);
        _overlay.Visibility  = Visibility.Visible;
        _player.Play();
    }

    public void Hide()
    {
        if (_gameOverlay != null)
        {
            var window = _gameOverlay;
            _gameOverlay = null;
            window.Finished -= Hide;
            window.Close();
        }

        _player.Stop();
        _player.Source      = null;
        _overlay.Visibility = Visibility.Collapsed;
        Playing              = false;
    }
}
