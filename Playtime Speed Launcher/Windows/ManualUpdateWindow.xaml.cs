using System.Diagnostics;
using System.Windows;
using System.Windows.Media;
using SpeedrunLauncher.Services;
using SpeedrunLauncher.Services.App;
using Loc = SpeedrunLauncher.Services.App.LocalizationService;

namespace SpeedrunLauncher;

// Fallback for when the automatic updater (see MainWindow's Updates overlay) doesn't
// detect or install a new version. Always fetches the latest published release —
// regardless of what the local version comparison thinks — so the user can force a
// reinstall, or fall back to downloading it manually in a browser.
public partial class ManualUpdateWindow : Window
{
    private readonly UpdateService _updateService;
    private UpdateInfo? _releaseInfo;
    private bool _isDownloading;

    public ManualUpdateWindow(UpdateService updateService)
    {
        InitializeComponent();
        _updateService = updateService;

        HeaderText.Text     = Loc.Get("manual_update_header");
        IntroText.Text      = Loc.Get("manual_update_intro");
        DownloadBtnText.Text = Loc.Get("manual_update_download_btn");
        BrowserBtnText.Text  = Loc.Get("manual_update_browser_btn");

        DownloadBtn.IsEnabled = false;
        SetStatus(Loc.Get("manual_update_checking"), "", Color.FromRgb(0x00, 0xCC, 0xAA));

        Loaded += async (_, _) => await CheckAsync();
    }

    private async Task CheckAsync()
    {
        _releaseInfo = await _updateService.GetLatestReleaseInfoAsync();

        if (!string.IsNullOrWhiteSpace(_releaseInfo.DownloadUrl))
        {
            SetStatus(
                Loc.Get("manual_update_ready", $"v{_releaseInfo.LatestVersion}"),
                $"{_releaseInfo.FileName}  |  {UpdateService.FormatFileSize(_releaseInfo.FileSize)}",
                Color.FromRgb(0x00, 0xCC, 0xAA));
            DownloadBtn.IsEnabled = true;
        }
        else
        {
            SetStatus(Loc.Get("manual_update_error"), "", Color.FromRgb(0xE6, 0x51, 0x00));
        }
    }

    private void SetStatus(string title, string message, Color color)
    {
        StatusTitleText.Text      = title;
        StatusMessageText.Text    = message;
        var brush = new SolidColorBrush(color);
        StatusIcon.Foreground     = brush;
        StatusTitleText.Foreground = brush;
    }

    private async void DownloadBtn_Click(object sender, RoutedEventArgs e)
    {
        if (_isDownloading || _releaseInfo == null) return;
        _isDownloading = true;

        DownloadBtn.IsEnabled = false;
        BrowserBtn.IsEnabled  = false;
        DownloadProgressPanel.Visibility = Visibility.Visible;

        var progress = new Progress<int>(pct =>
            Dispatcher.BeginInvoke(new Action(() =>
            {
                DownloadProgressBar.Value = pct;
                DownloadProgressText.Text = Loc.Get("updates_downloading", pct);
            })));

        var ok = await _updateService.DownloadAndInstallLauncherExeAsync(progress);

        if (!ok)
        {
            _isDownloading = false;
            DownloadProgressPanel.Visibility = Visibility.Collapsed;
            DownloadBtn.IsEnabled = true;
            BrowserBtn.IsEnabled  = true;
            SetStatus(Loc.Get("manual_update_error"), "", Color.FromRgb(0xE6, 0x51, 0x00));
        }
    }

    private void BrowserBtn_Click(object sender, RoutedEventArgs e)
    {
        var url = string.IsNullOrWhiteSpace(_releaseInfo?.ReleaseUrl)
            ? $"{AppVersion.GetGitHubRepoUrl()}/releases/latest"
            : _releaseInfo.ReleaseUrl;

        try
        {
            Process.Start(new ProcessStartInfo { FileName = url, UseShellExecute = true });
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[ManualUpdateWindow] Failed to open browser: {ex.Message}");
        }
    }

    private void Close_Click(object sender, RoutedEventArgs e) => Close();
}
