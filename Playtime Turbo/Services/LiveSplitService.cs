using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net.Http;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace SpeedrunLauncher.Services;

public class LiveSplitInfo
{
    public string  LatestVersion     { get; set; } = string.Empty;
    public string  DownloadUrl       { get; set; } = string.Empty;
    public string  FileName          { get; set; } = string.Empty;
    public long    FileSize          { get; set; }
    public bool    IsUpdateAvailable { get; set; }
    public bool    IsInstalled       { get; set; }
    public string  InstalledVersion  { get; set; } = string.Empty;
    public string? InstallPath       { get; set; }
}

public class LiveSplitService : IDisposable
{
    private readonly HttpClient _httpClient;
    private bool _disposed = false;

    private static readonly string DataDir =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "SpeedrunLauncher");
    private static readonly string ConfigFile =
        Path.Combine(DataDir, "livesplit.cfg");
    public static readonly string DefaultInstallDir =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "SpeedrunLauncher", "LiveSplit");

    public LiveSplitService()
    {
        _httpClient = new HttpClient();
        _httpClient.DefaultRequestHeaders.Add("User-Agent", "PlaytimeTurbo");
        _httpClient.Timeout = TimeSpan.FromSeconds(30);
    }

    public (string version, string path) LoadInstalled()
    {
        if (!File.Exists(ConfigFile)) return (string.Empty, string.Empty);
        var lines   = File.ReadAllLines(ConfigFile);
        var version = lines.FirstOrDefault(l => l.StartsWith("version="))?["version=".Length..] ?? string.Empty;
        var path    = lines.FirstOrDefault(l => l.StartsWith("path="))?["path=".Length..]    ?? string.Empty;
        return (version, path);
    }

    public void SaveInstalled(string version, string path)
    {
        Directory.CreateDirectory(DataDir);
        File.WriteAllLines(ConfigFile, [$"version={version}", $"path={path}"]);
    }

    public async Task<LiveSplitInfo> CheckAsync()
    {
        var info = new LiveSplitInfo();
        var (installedVersion, installPath) = LoadInstalled();

        if (!string.IsNullOrEmpty(installPath))
        {
            info.IsInstalled      = File.Exists(Path.Combine(installPath, "LiveSplit.exe"));
            info.InstalledVersion = installedVersion;
            info.InstallPath      = installPath;
        }

        try
        {
            var html  = await _httpClient.GetStringAsync("https://livesplit.org/downloads/");

            // The page has: <h3 ...><a href="https://github.com/LiveSplit/LiveSplit/releases/download/1.8.37/LiveSplit_1.8.37.zip">
            var match = Regex.Match(html,
                @"href=""(https://github\.com/LiveSplit/LiveSplit/releases/download/([\d.]+)/LiveSplit_([\d.]+)\.zip)""",
                RegexOptions.IgnoreCase);

            if (match.Success)
            {
                info.DownloadUrl   = match.Groups[1].Value;
                info.LatestVersion = match.Groups[2].Value;
                info.FileName      = $"LiveSplit_{info.LatestVersion}.zip";
            }

            if (!string.IsNullOrEmpty(info.LatestVersion) && info.IsInstalled)
                info.IsUpdateAvailable = IsNewerVersion(installedVersion, info.LatestVersion);
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[LiveSplitService] Check failed: {ex.Message}");
        }

        return info;
    }

    public async Task<bool> DownloadAndInstallAsync(LiveSplitInfo info, string installDir, IProgress<int>? progress = null)
    {
        if (string.IsNullOrWhiteSpace(info.DownloadUrl)) return false;

        string? tempDir = null;
        string? zipPath = null;

        try
        {
            tempDir = Path.Combine(Path.GetTempPath(), "PlaytimeTurbo_LiveSplit");
            if (Directory.Exists(tempDir)) Directory.Delete(tempDir, true);
            Directory.CreateDirectory(tempDir);

            zipPath = Path.Combine(tempDir, string.IsNullOrEmpty(info.FileName) ? "livesplit.zip" : info.FileName);

            using (var response = await _httpClient.GetAsync(info.DownloadUrl, HttpCompletionOption.ResponseHeadersRead))
            {
                response.EnsureSuccessStatusCode();
                var totalBytes = response.Content.Headers.ContentLength ?? 0;
                var buffer     = new byte[8192];
                var bytesRead  = 0L;

                using var contentStream = await response.Content.ReadAsStreamAsync();
                using var fileStream    = new FileStream(zipPath, FileMode.Create, FileAccess.Write, FileShare.None, 8192, true);

                int read;
                while ((read = await contentStream.ReadAsync(buffer, 0, buffer.Length)) > 0)
                {
                    await fileStream.WriteAsync(buffer, 0, read);
                    bytesRead += read;
                    if (totalBytes > 0)
                        progress?.Report((int)((bytesRead * 100) / totalBytes));
                }
            }

            var extractDir = Path.Combine(tempDir, "extracted");
            ZipFile.ExtractToDirectory(zipPath, extractDir);

            // The zip may extract into a single subdirectory
            var sourceDir = extractDir;
            var subDirs   = Directory.GetDirectories(extractDir);
            if (subDirs.Length == 1 && !File.Exists(Path.Combine(extractDir, "LiveSplit.exe")))
                sourceDir = subDirs[0];

            if (Directory.Exists(installDir)) Directory.Delete(installDir, true);
            Directory.CreateDirectory(installDir);

            foreach (var file in Directory.GetFiles(sourceDir, "*", SearchOption.AllDirectories))
            {
                var rel  = Path.GetRelativePath(sourceDir, file);
                var dest = Path.Combine(installDir, rel);
                Directory.CreateDirectory(Path.GetDirectoryName(dest)!);
                File.Copy(file, dest, true);
            }

            SaveInstalled(info.LatestVersion, installDir);
            try { Directory.Delete(tempDir, true); } catch { }
            return true;
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[LiveSplitService] Install failed: {ex.Message}");
            try
            {
                if (zipPath != null && File.Exists(zipPath)) File.Delete(zipPath);
                if (tempDir != null && Directory.Exists(tempDir)) Directory.Delete(tempDir, true);
            }
            catch { }
            return false;
        }
    }

    public static void Launch(string installPath)
    {
        var exe = Path.Combine(installPath, "LiveSplit.exe");
        if (File.Exists(exe))
            Process.Start(new ProcessStartInfo { FileName = exe, UseShellExecute = true });
    }

    private static bool IsNewerVersion(string current, string latest)
    {
        try
        {
            static (int, int, int) Parse(string v)
            {
                var clean = new string(v.Where(c => char.IsDigit(c) || c == '.').ToArray());
                var parts = clean.Split('.');
                return (
                    parts.Length > 0 && int.TryParse(parts[0], out int a) ? a : 0,
                    parts.Length > 1 && int.TryParse(parts[1], out int b) ? b : 0,
                    parts.Length > 2 && int.TryParse(parts[2], out int c) ? c : 0
                );
            }
            var (cm, cn, cp) = Parse(current);
            var (lm, ln, lp) = Parse(latest);
            if (lm != cm) return lm > cm;
            if (ln != cn) return ln > cn;
            return lp > cp;
        }
        catch { return false; }
    }

    public static string FormatFileSize(long bytes)
    {
        if (bytes <= 0)          return "—";
        if (bytes < 1024)        return $"{bytes} B";
        if (bytes < 1024 * 1024) return $"{bytes / 1024.0:F1} KB";
        return $"{bytes / (1024.0 * 1024):F2} MB";
    }

    public void Dispose()
    {
        if (!_disposed) { _httpClient?.Dispose(); _disposed = true; }
    }
}
