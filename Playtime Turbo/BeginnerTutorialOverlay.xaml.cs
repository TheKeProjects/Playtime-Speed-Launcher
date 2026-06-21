using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Threading;
using SpeedrunLauncher.Services;

namespace SpeedrunLauncher;

public partial class BeginnerTutorialOverlay : Window
{
    private static readonly Dictionary<string, TutorialVideo> _videos = new()
    {
        ["mNz257eeAec"] = new() { Id = "mNz257eeAec", Title = "ADD A INSTALLED VERSION",       Url = "https://www.youtube.com/watch?v=mNz257eeAec" },
        ["FAOt2QUJIv0"] = new() { Id = "FAOt2QUJIv0", Title = "INSTALL A DEPOT AUTOMATICALLY", Url = "https://www.youtube.com/watch?v=FAOt2QUJIv0" },
        ["Nc4OVsv1cs0"] = new() { Id = "Nc4OVsv1cs0", Title = "INSTALL A DEPOT MANUAL",        Url = "https://www.youtube.com/watch?v=Nc4OVsv1cs0" },
        ["JwE_bhk91Rc"] = new() { Id = "JwE_bhk91Rc", Title = "OPEN THE CHECKPOINT LOADER",    Url = "https://www.youtube.com/watch?v=JwE_bhk91Rc" },
    };

    private static readonly Color NormalBg = Color.FromRgb(10, 26, 40);
    private static readonly Color HoverBg  = Color.FromRgb(16, 38, 58);

    private TutorialVideo?   _currentVideo;
    private bool             _isPlaying;
    private bool             _isDragging;
    private bool             _updatingSlider;
    private bool             _isMuted;
    private double           _volumeBeforeMute = 0.8;
    private DispatcherTimer? _progressTimer;

    private const string IconPlay  = "";
    private const string IconPause = "";
    private const string IconVol   = "";
    private const string IconMute  = "";

    public BeginnerTutorialOverlay()
    {
        InitializeComponent();

        Player.Volume = VolumeSlider.Value;

        _progressTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(250) };
        _progressTimer.Tick += ProgressTimer_Tick;
        _progressTimer.Start();

        ProgressSlider.AddHandler(Thumb.DragStartedEvent,
            new DragStartedEventHandler((_, _) => _isDragging = true));
        ProgressSlider.AddHandler(Thumb.DragCompletedEvent,
            new DragCompletedEventHandler((_, _) => { _isDragging = false; SeekToSlider(); }));

        Closed += (_, _) => { Player.Stop(); _progressTimer.Stop(); };
    }

    // ── View switching ────────────────────────────────────────────────────────

    private void ShowCards()
    {
        Player.Stop();
        Player.Source            = null;
        _currentVideo            = null;
        _isPlaying               = false;
        PlayPauseBtn.IsEnabled   = false;
        ProgressSlider.IsEnabled = false;
        ProgressSlider.Value     = 0;
        TimeLabel.Text           = "0:00 / 0:00";

        CardsPanel.Visibility  = Visibility.Visible;
        PlayerPanel.Visibility = Visibility.Collapsed;
        BackBtn.Visibility     = Visibility.Collapsed;
        HeaderTitle.Text       = "BEGINNER TUTORIALS";
    }

    private void ShowPlayer(TutorialVideo video)
    {
        CardsPanel.Visibility  = Visibility.Collapsed;
        PlayerPanel.Visibility = Visibility.Visible;
        BackBtn.Visibility     = Visibility.Visible;
        HeaderTitle.Text       = video.Title;

        EmptyState.Visibility      = Visibility.Visible;
        EmptyStateLabel.Text       = "LOADING, PLEASE WAIT";
        EmptyStateLabel.Foreground = new SolidColorBrush(Color.FromArgb(200, 0, 204, 170));
        LoadingBarTrack.Visibility = Visibility.Visible;

        _ = FetchAndPlayAsync(video);
    }

    // ── Card interactions ─────────────────────────────────────────────────────

    private void Card_MouseDown(object sender, MouseButtonEventArgs e)
    {
        if (sender is Border card && card.Tag is string id && _videos.TryGetValue(id, out var video))
            ShowPlayer(video);
    }

    private void Card_MouseEnter(object sender, MouseEventArgs e)
    {
        if (sender is Border card) card.Background = new SolidColorBrush(HoverBg);
    }

    private void Card_MouseLeave(object sender, MouseEventArgs e)
    {
        if (sender is Border card) card.Background = new SolidColorBrush(NormalBg);
    }

    private void BackBtn_Click(object sender, RoutedEventArgs e) => ShowCards();

    // ── Stream fetch + playback ───────────────────────────────────────────────

    private async Task FetchAndPlayAsync(TutorialVideo video)
    {
        var (url, error) = await VideoTutorialService.GetStreamUrlAsync(video);

        LoadingBarTrack.Visibility = Visibility.Collapsed;

        if (url != null)
        {
            _currentVideo            = video;
            Player.Source            = new Uri(url);
            Player.Play();
            _isPlaying               = true;
            PlayPauseIcon.Text       = IconPause;
            PlayPauseBtn.IsEnabled   = true;
            ProgressSlider.IsEnabled = true;
            EmptyState.Visibility    = Visibility.Collapsed;
        }
        else
        {
            EmptyStateLabel.Text       = error ?? "Could not load video.";
            EmptyStateLabel.Foreground = new SolidColorBrush(Color.FromArgb(200, 200, 80, 60));
        }
    }

    // ── Player controls ───────────────────────────────────────────────────────

    private void ProgressTimer_Tick(object? sender, EventArgs e)
    {
        if (_isDragging || _currentVideo == null || !Player.NaturalDuration.HasTimeSpan) return;
        var pos   = Player.Position;
        var total = Player.NaturalDuration.TimeSpan;
        if (total.TotalSeconds <= 0) return;
        _updatingSlider = true;
        ProgressSlider.Value = pos.TotalSeconds / total.TotalSeconds * 100.0;
        _updatingSlider = false;
        TimeLabel.Text = $"{FormatTime(pos)} / {FormatTime(total)}";
    }

    private static string FormatTime(TimeSpan t)
        => t.TotalHours >= 1 ? $"{(int)t.TotalHours}:{t.Minutes:D2}:{t.Seconds:D2}" : $"{t.Minutes}:{t.Seconds:D2}";

    private void SeekToSlider()
    {
        if (!Player.NaturalDuration.HasTimeSpan) return;
        Player.Position = TimeSpan.FromSeconds(ProgressSlider.Value / 100.0 * Player.NaturalDuration.TimeSpan.TotalSeconds);
    }

    private void Player_MediaOpened(object sender, RoutedEventArgs e)
    {
        _updatingSlider = true;
        ProgressSlider.Value = 0;
        _updatingSlider = false;
        if (Player.NaturalDuration.HasTimeSpan)
            TimeLabel.Text = $"0:00 / {FormatTime(Player.NaturalDuration.TimeSpan)}";
    }

    private void Player_MediaEnded(object sender, RoutedEventArgs e)
    {
        _updatingSlider = true;
        ProgressSlider.Value = 0;
        _updatingSlider = false;
        Player.Position = TimeSpan.Zero;
        Player.Play();
        _isPlaying = true;
        PlayPauseIcon.Text = IconPause;
    }

    private void PlayPause_Click(object sender, RoutedEventArgs e)
    {
        if (_isPlaying) { Player.Pause(); _isPlaying = false; PlayPauseIcon.Text = IconPlay; }
        else            { Player.Play();  _isPlaying = true;  PlayPauseIcon.Text = IconPause; }
    }

    private void ProgressSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (_updatingSlider || _isDragging) return;
        SeekToSlider();
    }

    private void VolumeSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        Player.Volume   = e.NewValue;
        VolumeIcon.Text = e.NewValue == 0 ? IconMute : IconVol;
    }

    private void VolumeIcon_MouseDown(object sender, MouseButtonEventArgs e)
    {
        if (_isMuted) { VolumeSlider.Value = _volumeBeforeMute; _isMuted = false; }
        else          { _volumeBeforeMute = VolumeSlider.Value; VolumeSlider.Value = 0; _isMuted = true; }
    }

    // ── Window ────────────────────────────────────────────────────────────────

    private void Close_Click(object sender, RoutedEventArgs e)             => Close();
    private void Backdrop_MouseDown(object sender, MouseButtonEventArgs e) => Close();

    private void Window_KeyDown(object sender, KeyEventArgs e)
    {
        switch (e.Key)
        {
            case Key.Escape when PlayerPanel.Visibility == Visibility.Visible:
                ShowCards();
                e.Handled = true;
                break;
            case Key.Escape:
                Close();
                break;
            case Key.Space when _currentVideo != null:
                PlayPause_Click(sender, e);
                e.Handled = true;
                break;
        }
    }
}
