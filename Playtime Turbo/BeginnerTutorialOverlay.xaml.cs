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
        Player.PlaybackError    += Player_PlaybackError;
        Player.PlayStateChanged += Player_PlayStateChanged;

        _progressTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(250) };
        _progressTimer.Tick += ProgressTimer_Tick;
        _progressTimer.Start();

        ProgressSlider.AddHandler(Thumb.DragStartedEvent,
            new DragStartedEventHandler((_, _) => _isDragging = true));
        ProgressSlider.AddHandler(Thumb.DragCompletedEvent,
            new DragCompletedEventHandler((_, _) => { _isDragging = false; SeekToSlider(); }));

        Closed += (_, _) => { Player.Stop(); Player.Dispose(); _progressTimer.Stop(); };
    }

    // ── View switching ────────────────────────────────────────────────────────

    private void ShowCards()
    {
        Player.Stop();
        Player.Visibility        = Visibility.Collapsed;
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
        // Visible immediately, not Collapsed: WebView2's underlying window is effectively
        // hidden while Collapsed, and Chromium throttles/pauses JS timers on hidden content
        // — including the polling loop that detects the video is ready — so staying
        // Collapsed through the load can prevent it from ever finishing.
        Player.Visibility          = Visibility.Visible;
        EmptyStateLabel.Text       = "LOADING, PLEASE WAIT";
        EmptyStateLabel.Foreground = new SolidColorBrush(Color.FromArgb(200, 0, 204, 170));
        LoadingBarTrack.Visibility = Visibility.Visible;

        FetchAndPlayAsync(video);
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

    private void FetchAndPlayAsync(TutorialVideo video)
    {
        var videoId = VideoTutorialService.ExtractVideoId(video.Url);

        if (videoId == null)
        {
            Player_PlaybackError(this, "Could not read this video's URL.");
            return;
        }

        _currentVideo = video;
        Player.LoadVideo(videoId);
    }

    private void Player_PlaybackError(object? sender, string message)
    {
        _currentVideo = null;
        LoadingBarTrack.Visibility = Visibility.Collapsed;
        Player.Visibility          = Visibility.Collapsed;
        EmptyState.Visibility      = Visibility.Visible;
        EmptyStateLabel.Text       = message;
        EmptyStateLabel.Foreground = new SolidColorBrush(Color.FromArgb(200, 200, 80, 60));
        PlayPauseBtn.IsEnabled     = false;
        ProgressSlider.IsEnabled   = false;
    }

    private void Player_PlayStateChanged(object? sender, bool playing)
    {
        _isPlaying = playing;
        PlayPauseIcon.Text = playing ? IconPause : IconPlay;
    }

    // ── Player controls ───────────────────────────────────────────────────────

    private void ProgressTimer_Tick(object? sender, EventArgs e)
    {
        if (_isDragging || _currentVideo == null || !Player.HasDuration) return;
        var pos   = Player.Position;
        var total = Player.Duration;
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
        if (!Player.HasDuration) return;
        Player.Position = TimeSpan.FromSeconds(ProgressSlider.Value / 100.0 * Player.Duration.TotalSeconds);
    }

    private void Player_MediaOpened(object? sender, EventArgs e)
    {
        LoadingBarTrack.Visibility = Visibility.Collapsed;
        EmptyState.Visibility      = Visibility.Collapsed;
        Player.Visibility          = Visibility.Visible;
        PlayPauseBtn.IsEnabled     = true;
        ProgressSlider.IsEnabled   = true;
        PlayPauseIcon.Text         = IconPause;
        _isPlaying                 = true;

        _updatingSlider = true;
        ProgressSlider.Value = 0;
        _updatingSlider = false;
        TimeLabel.Text = $"0:00 / {FormatTime(Player.Duration)}";
    }

    private void Player_MediaEnded(object? sender, EventArgs e)
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
