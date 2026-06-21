using DiscordRPC;
using DiscordRPC.Logging;
using SpeedrunLauncher.Models;

namespace SpeedrunLauncher.Services;

// ── Discord Rich Presence ──────────────────────────────────────────────────────
//
// SETUP (one-time):
//   1. Go to https://discord.com/developers/applications → New Application.
//   2. Copy the "Application ID" and replace the AppId constant below.
//   3. Under Rich Presence → Art Assets, upload images with these exact keys:
//        "launcher"   – launcher logo (shown as small icon during gameplay)
//        "chapter_1"  – Chapter 1 banner
//        "chapter_2"  – Chapter 2 banner
//        "chapter_3"  – Chapter 3 banner
//        "chapter_4"  – Chapter 4 banner
//        "chapter_5"  – Chapter 5 banner
//
// PRIORITY NOTE:
//   The launcher connects to Discord's IPC pipe on startup and keeps its presence
//   active for the whole session — including while a Poppy Playtime chapter runs.
//   Discord will display this presence instead of its automatic game detection,
//   because this RPC client holds the active connection.
//   (If presence ever stops showing, have the user disable "Display currently
//    running game as a status message" in Discord Settings → Activity Privacy.)
//
// LAYOUT:
//   Details (upper line) = activity/chapter name only
//   State   (lower line) = version  ·  ⏱ timer  (controlled by DiscordPresenceSettings)

public sealed class DiscordPresenceService : IDisposable
{
    // ── Replace with YourDiscordApplicationID ─────────────────────────────────
    private static readonly string AppId = "YourDiscordApplicationID";
    // ──────────────────────────────────────────────────────────────────────────

    private DiscordRpcClient? _client;
    private readonly DateTime  _sessionStart = DateTime.UtcNow;

    private string        _liveSplitLine   = "LiveSplit not connected";
    private string        _versionLine     = "";
    private RichPresence? _currentPresence;

    private (string Id, string Username)? _connectedUser;

    /// <summary>Discord user retrieved from the OnReady handshake, or null if not connected.</summary>
    public (string Id, string Username)? ConnectedUser => _connectedUser;

    // ── User settings ──────────────────────────────────────────────────────────
    private bool _showActivity  = true;
    private bool _showVersion   = true;
    private bool _showLiveSplit = false;

    /// <summary>
    /// Applies the three Discord presence settings. Call this on startup and
    /// whenever the user changes a toggle.
    /// </summary>
    public void ApplySettings(bool showActivity, bool showVersion, bool showLiveSplit)
    {
        _showActivity  = showActivity;
        _showVersion   = showVersion;
        _showLiveSplit = showLiveSplit;

        if (!_showActivity)
        {
            if (_client is null) return;
            try { _client.ClearPresence(); } catch { }
            return;
        }

        // Re-send the current presence with refreshed State
        if (_currentPresence is null) return;
        _currentPresence.State = BuildState();
        Send(_currentPresence);
    }

    // ── Discord rate limit: ~5 updates per 20 s ────────────────────────────────
    private DateTime _lastLiveSplitSend = DateTime.MinValue;
    private static readonly TimeSpan MinLiveSplitInterval = TimeSpan.FromSeconds(4);

    public DiscordPresenceService()
    {
        if (!long.TryParse(AppId, out _)) return;

        try
        {
            _client = new DiscordRpcClient(AppId) { Logger = new NullLogger() };
            _client.OnReady            += (_, msg) => _connectedUser = (msg.User.ID.ToString(), msg.User.Username);
            _client.OnConnectionFailed += (_, _)   => _connectedUser = null;
            _client.Initialize();
        }
        catch
        {
            _client = null;
        }
    }

    // ── LiveSplit line ─────────────────────────────────────────────────────────

    /// <summary>
    /// Called every second by the poller. Always stores the latest timer value;
    /// only pushes to Discord when ShowLiveSplit is on and the rate limit allows.
    /// </summary>
    public void UpdateLiveSplitLine(string line)
    {
        if (line == _liveSplitLine) return;
        _liveSplitLine = line;
        if (!_showActivity || !_showLiveSplit || _currentPresence is null) return;
        var now = DateTime.UtcNow;
        if (now - _lastLiveSplitSend < MinLiveSplitInterval) return;
        _lastLiveSplitSend = now;
        _currentPresence.State = BuildState();
        Send(_currentPresence);
    }

    // ── Public state setters ───────────────────────────────────────────────────

    public void SetBrowsing()
    {
        Set(new RichPresence
        {
            Details    = "Speedrun Launcher",
            Timestamps = new Timestamps(_sessionStart),
            Assets     = new Assets
            {
                LargeImageKey  = AppVersion.LauncherImageKey,
                LargeImageText = "Playtime Turbo!",
            },
        }, version: "");
    }

    public void SetChapterSelected(ChapterInfo chapter, string version)
    {
        Set(new RichPresence
        {
            Details    = chapter.SubTitle,
            Timestamps = new Timestamps(_sessionStart),
            Assets     = new Assets
            {
                LargeImageKey  = AppVersion.LauncherImageKey,
                LargeImageText = "Playtime Turbo!",
                SmallImageKey  = $"chapter_{chapter.Number}",
                SmallImageText = chapter.SubTitle,
            },
        }, version: version);
    }

    public void SetInstalling(ChapterInfo chapter, string presetName)
    {
        Set(new RichPresence
        {
            Details    = $"Installing {chapter.SubTitle}",
            Timestamps = new Timestamps(DateTime.UtcNow),
            Assets     = new Assets
            {
                LargeImageKey  = $"chapter_{chapter.Number}",
                LargeImageText = chapter.SubTitle,
                SmallImageKey  = AppVersion.LauncherImageKey,
                SmallImageText = "Installing via SteamCMD",
            },
        }, version: presetName);
    }

    public void SetGameRunning(ChapterInfo chapter, string version)
    {
        Set(new RichPresence
        {
            Details    = $"Running {chapter.SubTitle}",
            Timestamps = new Timestamps(DateTime.UtcNow),
            Assets     = new Assets
            {
                LargeImageKey  = AppVersion.LauncherImageKey,
                LargeImageText = "Playtime Turbo!",
                SmallImageKey  = $"chapter_{chapter.Number}",
                SmallImageText = chapter.SubTitle,
            },
        }, version: version);
    }

    public void SetWatchingTutorial(string videoTitle, string chapter)
    {
        Set(new RichPresence
        {
            Details    = $"Watching tutorial  ·  {chapter}",
            Timestamps = new Timestamps(DateTime.UtcNow),
            Assets     = new Assets
            {
                LargeImageKey  = AppVersion.LauncherImageKey,
                LargeImageText = "Playtime Turbo!",
                SmallImageKey  = "checkpoint",
                SmallImageText = videoTitle,
            },
        }, version: videoTitle);
    }

    public void SetSelectingCheckpoint(ChapterInfo chapter, string version)
    {
        Set(new RichPresence
        {
            Details    = $"Selecting checkpoint  ·  {chapter.SubTitle}",
            Timestamps = new Timestamps(DateTime.UtcNow),
            Assets     = new Assets
            {
                LargeImageKey  = AppVersion.LauncherImageKey,
                LargeImageText = "Playtime Turbo!",
                SmallImageKey  = "checkpoint",
                SmallImageText = chapter.SubTitle,
            },
        }, version: version);
    }

    // ── Internals ──────────────────────────────────────────────────────────────

    private string BuildState()
    {
        var v = (_showVersion   && !string.IsNullOrEmpty(_versionLine))   ? _versionLine   : null;
        var l = (_showLiveSplit && !string.IsNullOrEmpty(_liveSplitLine)) ? _liveSplitLine : null;
        return (v, l) switch
        {
            (null, null) => "",
            ({ } s, null) => s,
            (null, { } s) => s,
            ({ } a, { } b) => $"{a}  ·  {b}",
        };
    }

    private void Set(RichPresence presence, string version)
    {
        _versionLine     = version;
        presence.State   = BuildState();
        _currentPresence = presence;
        if (_showActivity) Send(presence);
    }

    private void Send(RichPresence presence)
    {
        if (_client is null) return;
        try { _client.SetPresence(presence); }
        catch { }
    }

    public void Dispose()
    {
        if (_client is null) return;
        try
        {
            _client.ClearPresence();
            _client.Dispose();
        }
        catch { }
        _client        = null;
        _connectedUser = null;
    }
}
