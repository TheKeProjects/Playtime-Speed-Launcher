using System.Diagnostics;
using System.IO.Compression;
using System.Text.RegularExpressions;
using Microsoft.Win32;

namespace SpeedrunLauncher.Services;

public static class SteamCmdRunner
{
    // AppData\Local is always ASCII-safe; SteamCMD (2013 binary) crashes on unicode paths.
    private static readonly string LocalDir =
        Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "steamcmd");

    private static readonly string[] SearchPaths =
    [
        Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "steamcmd", "steamcmd.exe"),
        @"C:\Program Files (x86)\Steam\steamcmd.exe",
        @"C:\steamcmd\steamcmd.exe",
        @"C:\SteamCMD\steamcmd.exe",
    ];

    public static string? Find()
    {
        foreach (var p in SearchPaths)
            if (File.Exists(p)) return p;
        return null;
    }

    /// <summary>Returns the Steam installation directory from the Windows Registry.</summary>
    public static string? GetSteamInstallPath()
    {
        try
        {
            // 64-bit Windows stores Steam under WOW6432Node
            using var key = Registry.LocalMachine.OpenSubKey(
                @"SOFTWARE\WOW6432Node\Valve\Steam");
            if (key?.GetValue("InstallPath") is string path64 && Directory.Exists(path64))
                return path64;

            // Fallback: HKCU (older installs)
            using var keyU = Registry.CurrentUser.OpenSubKey(@"SOFTWARE\Valve\Steam");
            if (keyU?.GetValue("SteamPath") is string pathU && Directory.Exists(pathU))
                return pathU;
        }
        catch { }
        return null;
    }

    /// <summary>
    /// Reads Steam's loginusers.vdf and returns the AccountName of the most-recently
    /// logged-in user, or null if it cannot be determined.
    /// </summary>
    public static string? GetLoggedInUsername()
    {
        var steamDir = GetSteamInstallPath();
        if (steamDir is null) return null;

        var vdf = Path.Combine(steamDir, "config", "loginusers.vdf");
        if (!File.Exists(vdf)) return null;

        try
        {
            var content = File.ReadAllText(vdf);

            // Split into per-user blocks; each block starts with a quoted 64-bit SteamID
            var blocks = Regex.Split(content, @"""\d{17}""");

            foreach (var block in blocks)
            {
                // Look for MostRecent "1" inside this block
                if (!Regex.IsMatch(block, @"""MostRecent""\s+""1""")) continue;

                var m = Regex.Match(block, @"""AccountName""\s+""([^""]+)""");
                if (m.Success) return m.Groups[1].Value;
            }
        }
        catch { }
        return null;
    }

    /// <summary>
    /// Copies Steam's config.vdf into SteamCMD's config directory so that
    /// SteamCMD can reuse the existing login token without a password prompt.
    /// </summary>
    public static void CopyCredentials(string steamCmdPath)
    {
        var steamDir = GetSteamInstallPath();
        if (steamDir is null) return;

        var src = Path.Combine(steamDir, "config", "config.vdf");
        if (!File.Exists(src)) return;

        try
        {
            var steamCmdDir    = Path.GetDirectoryName(steamCmdPath)!;
            var configDir      = Path.Combine(steamCmdDir, "config");
            Directory.CreateDirectory(configDir);
            File.Copy(src, Path.Combine(configDir, "config.vdf"), overwrite: true);
        }
        catch { }
    }

    /// <summary>Downloads and extracts steamcmd to the launcher's own folder.</summary>
    public static async Task<string> DownloadAsync(IProgress<string>? progress = null)
    {
        Directory.CreateDirectory(LocalDir);
        var zipPath = Path.Combine(LocalDir, "steamcmd.zip");
        var exePath = Path.Combine(LocalDir, "steamcmd.exe");

        progress?.Report("Descargando SteamCMD…");
        using var http = new HttpClient { Timeout = TimeSpan.FromMinutes(2) };
        var bytes = await http.GetByteArrayAsync(
            "https://steamcdn-a.akamaihd.net/client/installer/steamcmd.zip");
        await File.WriteAllBytesAsync(zipPath, bytes);

        progress?.Report("Extrayendo…");
        ZipFile.ExtractToDirectory(zipPath, LocalDir, overwriteFiles: true);
        File.Delete(zipPath);

        if (!File.Exists(exePath))
            throw new FileNotFoundException("No se encontró steamcmd.exe tras la extracción.");

        return exePath;
    }

    /// <summary>
    /// Runs SteamCMD hidden and streams each output line via <paramref name="lineOutput"/>.
    /// When SteamCMD outputs a Steam Guard / 2FA prompt, <paramref name="promptAsync"/>
    /// is called with the prompt text; return the code string or null to skip.
    /// </summary>
    public static async Task RunAsync(
        string steamCmdPath,
        string username,
        string? password,
        int appId, int depotId, string manifestId,
        IProgress<string> lineOutput,
        CancellationToken ct = default)
    {
        var login = string.IsNullOrEmpty(password)
            ? $"+login {username}"
            : $"+login {username} \"{password}\"";

        var args = $"{login} +download_depot {appId} {depotId} {manifestId} +quit";

        var psi = new ProcessStartInfo
        {
            FileName               = steamCmdPath,
            Arguments              = args,
            WorkingDirectory       = Path.GetDirectoryName(steamCmdPath)!,
            UseShellExecute        = false,
            RedirectStandardOutput = true,
            RedirectStandardError  = true,
            RedirectStandardInput  = true,
            CreateNoWindow         = true,
        };

        using var proc = Process.Start(psi)
            ?? throw new InvalidOperationException("No se pudo iniciar SteamCMD.");

        proc.OutputDataReceived += (_, e) =>
        {
            if (!string.IsNullOrWhiteSpace(e.Data))
                lineOutput.Report(e.Data);
        };

        proc.ErrorDataReceived += (_, e) =>
        {
            if (!string.IsNullOrWhiteSpace(e.Data))
                lineOutput.Report(e.Data);
        };

        proc.BeginOutputReadLine();
        proc.BeginErrorReadLine();

        await proc.WaitForExitAsync(ct);
    }
}
