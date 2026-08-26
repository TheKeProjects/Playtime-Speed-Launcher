using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace SpeedrunLauncher.Services;

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

    private readonly List<Key> _konamiBuffer     = [];
    private readonly List<Key> _ganeBuffer       = [];
    private readonly List<Key> _raulBecasBuffer  = [];

    private readonly MediaElement _player;
    private readonly UIElement    _overlay;

    public bool Playing { get; private set; }

    public EasterEggService(MediaElement player, UIElement overlay)
    {
        _player  = player;
        _overlay = overlay;
    }

    public void HandleKeyDown(KeyEventArgs e)
    {
        if (e.Key == Key.Escape && Playing)
        {
            Hide();
            return;
        }

        var key = e.Key == Key.System ? e.SystemKey : e.Key;

        TrackSequence(_konamiBuffer,    KonamiSequence,    key, "Torrente.mp4");
        TrackSequence(_ganeBuffer,      GaneSequence,      key, "Gane.mp4");
        TrackSequence(_raulBecasBuffer, RaulBecasSequence, key, "RaulBecas.mp4");
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

        _player.Source       = new Uri(videoPath);
        _overlay.Visibility  = Visibility.Visible;
        _player.Play();
    }

    public void Hide()
    {
        _player.Stop();
        _player.Source      = null;
        _overlay.Visibility = Visibility.Collapsed;
        Playing              = false;
    }
}
