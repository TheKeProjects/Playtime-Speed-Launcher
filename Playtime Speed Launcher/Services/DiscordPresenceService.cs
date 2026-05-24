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

public sealed class DiscordPresenceService : IDisposable
{
    // ── Replace with your Discord Application ID ───────────────────────────────
    private static readonly string AppId = "YourDiscordApplicationID";
    // ──────────────────────────────────────────────────────────────────────────

    private DiscordRpcClient? _client;
    private readonly DateTime  _sessionStart = DateTime.UtcNow;

    public DiscordPresenceService()
    {
        if (!long.TryParse(AppId, out _)) return;

        try
        {
            _client = new DiscordRpcClient(AppId) { Logger = new NullLogger() };
            _client.Initialize();
        }
        catch
        {
            _client = null;
        }
    }

    // ── Public state setters ───────────────────────────────────────────────────

    public void SetBrowsing()
    {
        Set(new RichPresence
        {
            Details    = "Speedrun Launcher",
            State      = "Browsing chapters",
            Timestamps = new Timestamps(_sessionStart),
            Assets     = new Assets
            {
                LargeImageKey  = "launcher",
                LargeImageText = "Playtime Speed Launcher",
            },
        });
    }

    public void SetChapterSelected(ChapterInfo chapter, string version)
    {
        Set(new RichPresence
        {
            Details    = chapter.SubTitle,
            State      = version,
            Timestamps = new Timestamps(_sessionStart),
            Assets     = new Assets
            {
                LargeImageKey  = "launcher",
                LargeImageText = "Playtime Speed Launcher",
                SmallImageKey  = $"chapter_{chapter.Number}",
                SmallImageText = chapter.SubTitle,
            },
        });
    }

    public void SetInstalling(ChapterInfo chapter, string presetName)
    {
        Set(new RichPresence
        {
            Details    = $"Installing {chapter.SubTitle}",
            State      = presetName,
            Timestamps = new Timestamps(DateTime.UtcNow),
            Assets     = new Assets
            {
                LargeImageKey  = $"chapter_{chapter.Number}",
                LargeImageText = chapter.SubTitle,
                SmallImageKey  = "launcher",
                SmallImageText = "Installing via SteamCMD",
            },
        });
    }

    public void SetGameRunning(ChapterInfo chapter, string version)
    {
        Set(new RichPresence
        {
            Details    = $"Speedrunning {chapter.SubTitle}",
            State      = version,
            Timestamps = new Timestamps(DateTime.UtcNow),
            Assets     = new Assets
            {
                LargeImageKey  = "launcher",
                LargeImageText = "Playtime Speed Launcher",
                SmallImageKey  = $"chapter_{chapter.Number}",
                SmallImageText = chapter.SubTitle,
            },
        });
    }

    // ── Internals ──────────────────────────────────────────────────────────────

    private void Set(RichPresence presence)
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
        _client = null;
    }
}
