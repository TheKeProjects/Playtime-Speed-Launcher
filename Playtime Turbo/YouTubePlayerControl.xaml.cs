using System.Globalization;
using System.IO;
using System.Text.Json;
using System.Windows.Controls;
using System.Windows.Threading;
using Microsoft.Web.WebView2.Core;

namespace SpeedrunLauncher;

// Embeds YouTube's own official IFrame player (via WebView2) instead of scraping a raw CDN
// stream URL out of YouTube. That scraping approach (YoutubeExplode) is what YouTube's
// anti-bot/rate-limiting targets; the IFrame embed is YouTube's sanctioned third-party
// playback mechanism, so it isn't subject to the same breakage.
public partial class YouTubePlayerControl : UserControl, IDisposable
{
    public event EventHandler?            MediaOpened;
    public event EventHandler?            MediaEnded;
    public event EventHandler<string>?    PlaybackError;
    public event EventHandler<bool>?      PlayStateChanged;

    private bool    _coreReady;
    private bool    _playerReady;
    private string? _pendingVideoId;
    private double? _duration;
    private double  _currentTime;
    private double  _volume = 0.8;
    private bool    _hasFiredOpened;
    private DispatcherTimer? _loadTimeoutTimer;

    public bool     HasDuration => _duration is > 0;
    public TimeSpan Duration    => TimeSpan.FromSeconds(_duration ?? 0);

    public TimeSpan Position
    {
        get => TimeSpan.FromSeconds(_currentTime);
        set
        {
            _currentTime = value.TotalSeconds;
            RunScript($"seekTo({Inv(value.TotalSeconds)})");
        }
    }

    public double Volume
    {
        get => _volume;
        set
        {
            _volume = value;
            RunScript($"setVolume({Inv(value)})");
        }
    }

    public YouTubePlayerControl()
    {
        InitializeComponent();
        Loaded += async (_, _) => await InitializeAsync();
    }

    private const string VirtualHost = "playtimeturbo.local";

    private static string Inv(double d) => d.ToString(CultureInfo.InvariantCulture);

    private async Task InitializeAsync()
    {
        if (_coreReady) return;
        try
        {
            var appDataDir = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "PlaytimeSpeedLauncher");
            var userDataFolder = Path.Combine(appDataDir, "WebView2");
            var env = await CoreWebView2Environment.CreateAsync(userDataFolder: userDataFolder);
            await WebView.EnsureCoreWebView2Async(env);

            // YouTube's IFrame API validates the embedding page's origin and rejects the
            // null/opaque origin that NavigateToString produces (surfaces as player error 153).
            // Serving the page from a real virtual-host origin fixes that.
            var siteDir = Path.Combine(appDataDir, "WebView2Player");
            Directory.CreateDirectory(siteDir);
            File.WriteAllText(Path.Combine(siteDir, "player.html"), PlayerHtml);
            WebView.CoreWebView2.SetVirtualHostNameToFolderMapping(
                VirtualHost, siteDir, CoreWebView2HostResourceAccessKind.Allow);

            WebView.CoreWebView2.Settings.AreDefaultContextMenusEnabled = false;
            WebView.CoreWebView2.Settings.AreDevToolsEnabled            = false;
            WebView.CoreWebView2.WebMessageReceived += CoreWebView2_WebMessageReceived;

            // The YouTube embed (a cross-origin iframe inside our page) draws its own chrome —
            // title bar, watermark, and the "suggested videos" pause card — on top of the video.
            // playerVars can't turn that off; it has to be hidden with injected CSS, re-applied
            // every time that frame's document loads (this SDK version has no per-frame
            // "run on document created" hook, so DOMContentLoaded is the next best thing).
            WebView.CoreWebView2.FrameCreated += (_, fe) =>
                fe.Frame.DOMContentLoaded += (_, _) =>
                {
                    try { _ = fe.Frame.ExecuteScriptAsync(HideYouTubeChromeScript); }
                    catch { /* frame may already be gone */ }
                };

            WebView.CoreWebView2.Navigate($"https://{VirtualHost}/player.html");
            _coreReady = true;
        }
        catch (Exception ex)
        {
            PlaybackError?.Invoke(this,
                $"Could not start the embedded video player. Make sure the WebView2 Runtime is installed. ({ex.Message})");
        }
    }

    private void CoreWebView2_WebMessageReceived(object? sender, CoreWebView2WebMessageReceivedEventArgs e)
    {
        string? json;
        try { json = e.TryGetWebMessageAsString(); }
        catch { return; }
        if (json == null) return;

        JsonElement root;
        try { root = JsonDocument.Parse(json).RootElement; }
        catch { return; }

        switch (root.GetProperty("type").GetString())
        {
            case "ready":
                _playerReady = true;
                if (_pendingVideoId != null)
                {
                    var id = _pendingVideoId;
                    _pendingVideoId = null;
                    RunScript($"loadVideo('{id}')");
                }
                RunScript($"setVolume({Inv(_volume)})");
                break;

            case "time":
                _currentTime = root.GetProperty("current").GetDouble();
                var newDuration = root.GetProperty("duration").GetDouble();
                var state        = root.GetProperty("state").GetInt32();

                if (newDuration > 0 && _duration != newDuration)
                {
                    _duration = newDuration;
                    if (!_hasFiredOpened)
                    {
                        _hasFiredOpened = true;
                        StopLoadTimeout();
                        MediaOpened?.Invoke(this, EventArgs.Empty);
                    }
                }

                if (state == 1)      PlayStateChanged?.Invoke(this, true);
                else if (state == 2) PlayStateChanged?.Invoke(this, false);
                break;

            case "state":
                if (root.GetProperty("state").GetInt32() == 0)
                    MediaEnded?.Invoke(this, EventArgs.Empty);
                break;

            case "error":
                StopLoadTimeout();
                PlaybackError?.Invoke(this, DescribeError(root.GetProperty("code").GetInt32()));
                break;
        }
    }

    private static string DescribeError(int code) => code switch
    {
        2          => "This video's link looks invalid.",
        5          => "This video can't be played in an embedded player.",
        100        => "This video was removed or made private.",
        101 or 150 => "The video owner disabled playback outside of YouTube.",
        _          => $"Playback error ({code})."
    };

    private void RunScript(string script)
    {
        if (!_playerReady && !script.StartsWith("loadVideo")) return;
        try { _ = WebView.CoreWebView2?.ExecuteScriptAsync(script); }
        catch { /* control is closing */ }
    }

    public void LoadVideo(string videoId)
    {
        _hasFiredOpened = false;
        _duration       = null;
        _currentTime    = 0;
        StartLoadTimeout();

        if (!_playerReady)
        {
            _pendingVideoId = videoId;
            return;
        }
        _pendingVideoId = null;
        RunScript($"loadVideo('{videoId}')");
    }

    public void Play()  => RunScript("playVideo()");
    public void Pause() => RunScript("pauseVideo()");

    private void StartLoadTimeout()
    {
        StopLoadTimeout();
        _loadTimeoutTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(15) };
        _loadTimeoutTimer.Tick += (_, _) =>
        {
            StopLoadTimeout();
            PlaybackError?.Invoke(this, "Timed out loading this video. Try again or pick another one.");
        };
        _loadTimeoutTimer.Start();
    }

    private void StopLoadTimeout()
    {
        _loadTimeoutTimer?.Stop();
        _loadTimeoutTimer = null;
    }

    public void Stop()
    {
        _hasFiredOpened = false;
        _duration       = null;
        _currentTime    = 0;
        StopLoadTimeout();
        RunScript("stopVideo()");
    }

    public void Dispose()
    {
        StopLoadTimeout();
        WebView.Dispose();
    }

    // Selectors cover two different YouTube embed player builds seen during development —
    // the classic one (.ytp-* classes) and a newer web-components rewrite (custom elements
    // like <player-fullscreen-controls>/<player-top-controls>) that YouTube appears to have
    // rolled out mid-development. querySelectorAll on a selector that matches nothing is a
    // harmless no-op, so listing both keeps this working across whichever build is served.
    private const string HideYouTubeChromeScript = """
        (function() {
          // Pause card / share+suggested-video overlay: hidden entirely.
          var s = document.createElement('style');
          s.textContent =
            'player-fullscreen-controls, ' +
            '.ytp-pause-overlay, .ytp-ce-element, .ytp-ce-video, .ytp-ce-playlist, ' +
            '.ytp-ce-channel, .ytp-endscreen-content, .ytp-cards-teaser, .ytp-gradient-bottom ' +
            '{ display: none !important; }';
          (document.head || document.documentElement).appendChild(s);

          // Title bar/channel avatar and watermark: shown normally, but on our own faster
          // idle timer instead of YouTube's (~3s delay, then its own fade). Polls instead of
          // using a single scheduled timeout, because these elements are built by YouTube's
          // player JS well after this script's DOMContentLoaded fires — a one-shot timer can
          // fire before they exist and then never re-check. Drives visibility via inline
          // styles (which always win over stylesheet rules) instead of trying to override
          // YouTube's own, more specific autohide CSS.
          var IDLE_MS = 900;
          var lastActive = Date.now();
          document.addEventListener('mousemove', function () { lastActive = Date.now(); }, true);
          document.addEventListener('mousedown', function () { lastActive = Date.now(); }, true);

          setInterval(function () {
            var idle = (Date.now() - lastActive) >= IDLE_MS;
            var els = document.querySelectorAll(
              'player-top-controls, player-middle-controls, ' +
              '.ytp-chrome-top, .ytp-title, .ytp-watermark, .ytp-gradient-top, .ytp-large-play-button');
            for (var i = 0; i < els.length; i++) {
              els[i].style.setProperty('transition', 'opacity 0.08s linear', 'important');
              if (idle) els[i].style.setProperty('opacity', '0', 'important');
              else els[i].style.removeProperty('opacity');
            }
          }, 200);
        })();
        """;

    private const string PlayerHtml = """
        <!DOCTYPE html>
        <html><head><style>html,body{margin:0;padding:0;background:#000;overflow:hidden;height:100%;}#player{width:100%;height:100%;}</style></head>
        <body>
        <div id="player"></div>
        <script>
          var tag = document.createElement('script');
          tag.src = "https://www.youtube.com/iframe_api";
          document.body.appendChild(tag);

          var player = null;
          var ready = false;
          var pendingVideoId = null;

          function post(obj) { window.chrome.webview.postMessage(JSON.stringify(obj)); }

          function onYouTubeIframeAPIReady() {
            player = new YT.Player('player', {
              width: '100%', height: '100%',
              playerVars: {
                autoplay: 1, playsinline: 1, modestbranding: 1,
                rel: 0, controls: 0, fs: 0, disablekb: 1, iv_load_policy: 3,
                origin: 'https://playtimeturbo.local'
              },
              events: {
                onReady: function() {
                  ready = true;
                  post({ type: 'ready' });
                  if (pendingVideoId) { player.loadVideoById(pendingVideoId); pendingVideoId = null; }
                },
                onStateChange: function(e) { post({ type: 'state', state: e.data }); },
                onError: function(e) { post({ type: 'error', code: e.data }); }
              }
            });
          }

          function loadVideo(id) {
            if (ready && player && player.loadVideoById) player.loadVideoById(id);
            else pendingVideoId = id;
          }
          function playVideo()  { if (ready) player.playVideo(); }
          function pauseVideo() { if (ready) player.pauseVideo(); }
          function stopVideo()  { if (ready) player.stopVideo(); }
          function seekTo(sec)  { if (ready) player.seekTo(sec, true); }
          function setVolume(v) { if (ready) player.setVolume(Math.round(v * 100)); }

          setInterval(function() {
            if (!ready || !player || !player.getCurrentTime) return;
            try { post({ type: 'time', current: player.getCurrentTime(), duration: player.getDuration(), state: player.getPlayerState() }); }
            catch (e) {}
          }, 250);
        </script>
        </body></html>
        """;
}
