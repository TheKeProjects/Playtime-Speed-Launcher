using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net.Http;
using System.Text.Json;
using System.Threading.Tasks;

namespace SpeedrunLauncher.Services;

public class UpdateInfo
{
    public string LatestVersion      { get; set; } = string.Empty;
    public string Changelog          { get; set; } = string.Empty;
    public string DownloadUrl        { get; set; } = string.Empty;
    public string FileName           { get; set; } = string.Empty;
    public long   FileSize           { get; set; }
    public bool   IsUpdateAvailable  { get; set; }
    public string ReleaseUrl         { get; set; } = string.Empty;
}

public class GbUpdateInfo
{
    public string LatestVersion     { get; set; } = string.Empty;
    public string DownloadUrl       { get; set; } = string.Empty;
    public string FileName          { get; set; } = string.Empty;
    public long   FileSize          { get; set; }
    public bool   IsUpdateAvailable { get; set; }
    public string Changelog         { get; set; } = string.Empty;
}

public class UpdateService : IDisposable
{
    private readonly HttpClient _httpClient;
    private bool _disposed = false;

    public UpdateService()
    {
        _httpClient = new HttpClient();
        _httpClient.DefaultRequestHeaders.Add("User-Agent", "PlaytimeTurbo");
        _httpClient.Timeout = TimeSpan.FromSeconds(30);
    }

    public async Task<UpdateInfo> CheckForUpdatesAsync()
    {
        var repos = new[] { AppVersion.GITHUB_REPO, AppVersion.GITHUB_REPO_LEGACY };

        foreach (var repo in repos)
        {
            var result = await TryCheckRepoAsync(repo);
            if (result != null)
                return result;
        }

        return new UpdateInfo
        {
            LatestVersion     = AppVersion.CURRENT_VERSION,
            IsUpdateAvailable = false
        };
    }

    private async Task<UpdateInfo?> TryCheckRepoAsync(string repo)
    {
        try
        {
            var versionUrl    = $"https://raw.githubusercontent.com/{AppVersion.GITHUB_OWNER}/{repo}/{AppVersion.GITHUB_BRANCH}/version.txt";
            var latestVersion = (await _httpClient.GetStringAsync(versionUrl))
                                    .Replace("\r", "").Replace("\n", "").Replace("\t", "").Trim();

            if (string.IsNullOrWhiteSpace(latestVersion))
                return null;

            var cleanParts = new string(latestVersion.Where(c => char.IsDigit(c) || c == '.').ToArray())
                                .Split('.').Where(p => !string.IsNullOrEmpty(p)).ToArray();
            if (cleanParts.Length < 2 || !cleanParts.All(p => int.TryParse(p, out _)))
                return null;

            var updateInfo = new UpdateInfo
            {
                LatestVersion     = latestVersion,
                IsUpdateAvailable = IsNewerVersion(AppVersion.CURRENT_VERSION, latestVersion)
            };

            try
            {
                var changelogUrl     = $"https://raw.githubusercontent.com/{AppVersion.GITHUB_OWNER}/{repo}/{AppVersion.GITHUB_BRANCH}/changelog.txt";
                updateInfo.Changelog = (await _httpClient.GetStringAsync(changelogUrl)).Trim();
            }
            catch { }

            if (updateInfo.IsUpdateAvailable)
            {
                var fileName    = $"PTTurbov{latestVersion.Replace(".", "-")}.zip";
                var downloadUrl = $"https://github.com/{AppVersion.GITHUB_OWNER}/{repo}/releases/latest/download/{fileName}";

                updateInfo.DownloadUrl = downloadUrl;
                updateInfo.FileName    = fileName;
                updateInfo.ReleaseUrl  = $"https://github.com/{AppVersion.GITHUB_OWNER}/{repo}/releases/latest";

                try
                {
                    using var head = new HttpRequestMessage(HttpMethod.Head, downloadUrl);
                    using var res  = await _httpClient.SendAsync(head);
                    if (res.IsSuccessStatusCode && res.Content.Headers.ContentLength.HasValue)
                        updateInfo.FileSize = res.Content.Headers.ContentLength.Value;
                }
                catch { }
            }

            return updateInfo;
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[UpdateService] GitHub check failed for {repo}: {ex.Message}");
            return null;
        }
    }

    public async Task<GbUpdateInfo> CheckGameBananaUpdateAsync()
    {
        var info = new GbUpdateInfo();
        if (AppVersion.GB_TOOL_ID <= 0) return info;

        try
        {
            var url  = $"https://api.gamebanana.com/Core/Item/Data?itemtype=Tool&itemid={AppVersion.GB_TOOL_ID}&fields=name,Files().aFiles()";
            var json = await _httpClient.GetStringAsync(url);

            using var doc  = JsonDocument.Parse(json);
            var root = doc.RootElement;

            if (root.ValueKind != JsonValueKind.Array || root.GetArrayLength() < 2) return info;
            var filesElement = root[1];
            if (filesElement.ValueKind != JsonValueKind.Object) return info;

            foreach (var fileProp in filesElement.EnumerateObject())
            {
                var f         = fileProp.Value;
                var gbVersion = GbGetString(f, "_sVersion")?.Trim() ?? string.Empty;
                info.LatestVersion     = gbVersion;
                info.IsUpdateAvailable = IsNewerVersion(AppVersion.CURRENT_VERSION, gbVersion);

                if (info.IsUpdateAvailable)
                {
                    info.DownloadUrl = GbGetString(f, "_sDownloadUrl") ?? string.Empty;
                    info.FileName    = GbGetString(f, "_sFile")        ?? string.Empty;
                    info.FileSize    = GbGetLong(f,   "_nFilesize")    ?? 0;

                    foreach (var repo in new[] { AppVersion.GITHUB_REPO, AppVersion.GITHUB_REPO_LEGACY })
                    {
                        try
                        {
                            var changelogUrl = $"https://raw.githubusercontent.com/{AppVersion.GITHUB_OWNER}/{repo}/{AppVersion.GITHUB_BRANCH}/changelog.txt";
                            info.Changelog   = (await _httpClient.GetStringAsync(changelogUrl)).Trim();
                            break;
                        }
                        catch { }
                    }
                }
                break;
            }
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[UpdateService] GameBanana check failed: {ex.Message}");
        }

        return info;
    }

    public async Task<bool> DownloadAndInstallUpdateAsync(UpdateInfo updateInfo, IProgress<int>? progress = null)
    {
        if (string.IsNullOrWhiteSpace(updateInfo.DownloadUrl)) return false;

        string? tempDir = null;
        string? zipPath = null;

        try
        {
            tempDir = Path.Combine(Path.GetTempPath(), "PlaytimeTurbo_Update");
            if (Directory.Exists(tempDir)) Directory.Delete(tempDir, true);
            Directory.CreateDirectory(tempDir);

            zipPath = Path.Combine(tempDir, updateInfo.FileName);

            using (var response = await _httpClient.GetAsync(updateInfo.DownloadUrl, HttpCompletionOption.ResponseHeadersRead))
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
            Directory.CreateDirectory(extractDir);
            ZipFile.ExtractToDirectory(zipPath, extractDir);

            var newExePath = FindExecutable(extractDir);
            if (string.IsNullOrEmpty(newExePath)) return false;

            var currentExePath  = Environment.ProcessPath ?? Path.Combine(AppContext.BaseDirectory, "PlaytimeTurbo.exe");
            var currentDir      = Path.GetDirectoryName(currentExePath) ?? AppContext.BaseDirectory;
            var extractedExeDir = Path.GetDirectoryName(newExePath) ?? extractDir;
            var restartExePath  = Path.Combine(currentDir, Path.GetFileName(newExePath));
            var currentExeName  = Path.GetFileName(currentExePath);
            var newExeName      = Path.GetFileName(newExePath);

            var renamePatch = "";
            if (!currentExeName.Equals(newExeName, StringComparison.OrdinalIgnoreCase))
            {
                var oldPath = Path.Combine(currentDir, currentExeName);
                renamePatch = $@"
del /F ""{oldPath}"" 2>nul
copy /Y ""{restartExePath}"" ""{oldPath}"" >nul 2>&1";
            }

            var batchPath    = Path.Combine(tempDir, "update.bat");
            var batchContent = $@"@echo off
chcp 65001 >nul
timeout /t 3 /nobreak >nul
xcopy ""{extractedExeDir}\*"" ""{currentDir}"" /E /Y /I /Q
if %ERRORLEVEL% NEQ 0 (
    timeout /t 3 /nobreak >nul
    xcopy ""{extractedExeDir}\*"" ""{currentDir}"" /E /Y /I /Q
)
if %ERRORLEVEL% NEQ 0 (pause & exit /b 1){renamePatch}
start """" ""{restartExePath}""
rd /s /q ""{tempDir}""
exit";

            await File.WriteAllTextAsync(batchPath, batchContent);
            Process.Start(new ProcessStartInfo
            {
                FileName        = "cmd.exe",
                Arguments       = $"/c \"{batchPath}\"",
                UseShellExecute = true,
                WindowStyle     = ProcessWindowStyle.Hidden
            });

            await Task.Delay(1000);
            Environment.Exit(0);
            return true;
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[UpdateService] Install failed: {ex.Message}");
            try
            {
                if (zipPath != null && File.Exists(zipPath)) File.Delete(zipPath);
                if (tempDir != null && Directory.Exists(tempDir)) Directory.Delete(tempDir, true);
            }
            catch { }
            return false;
        }
    }

    public async Task<bool> DownloadAndInstallGbUpdateAsync(GbUpdateInfo gbInfo, IProgress<int>? progress = null)
    {
        var mapped = new UpdateInfo
        {
            DownloadUrl       = gbInfo.DownloadUrl,
            FileName          = string.IsNullOrWhiteSpace(gbInfo.FileName) ? "gamebanana_update.zip" : gbInfo.FileName,
            IsUpdateAvailable = true,
            LatestVersion     = gbInfo.LatestVersion
        };
        return await DownloadAndInstallUpdateAsync(mapped, progress);
    }

    private string FindExecutable(string directory)
    {
        var names = new List<string> { "PlaytimeTurbo.exe" };
        var processName = Path.GetFileName(Environment.ProcessPath);
        if (!string.IsNullOrEmpty(processName)) names.Insert(0, processName);

        foreach (var name in names)
        {
            var found = Directory.GetFiles(directory, name, SearchOption.AllDirectories).FirstOrDefault();
            if (found != null) return found;
        }

        return Directory.GetFiles(directory, "*.exe", SearchOption.AllDirectories).FirstOrDefault() ?? "";
    }

    private bool IsNewerVersion(string currentVersion, string newVersion)
    {
        try
        {
            var cur = ParseVersion(currentVersion);
            var lat = ParseVersion(newVersion);
            if (lat.major != cur.major) return lat.major > cur.major;
            if (lat.minor != cur.minor) return lat.minor > cur.minor;
            return lat.patch > cur.patch;
        }
        catch { return false; }
    }

    private static (int major, int minor, int patch) ParseVersion(string version)
    {
        var clean = new string(version.Where(c => char.IsDigit(c) || c == '.').ToArray());
        var parts = clean.Split('.');
        return (
            parts.Length > 0 && int.TryParse(parts[0], out int m) ? m : 0,
            parts.Length > 1 && int.TryParse(parts[1], out int n) ? n : 0,
            parts.Length > 2 && int.TryParse(parts[2], out int p) ? p : 0
        );
    }

    private static string? GbGetString(JsonElement el, string key)
        => el.TryGetProperty(key, out var p) && p.ValueKind == JsonValueKind.String ? p.GetString() : null;

    private static long? GbGetLong(JsonElement el, string key)
        => el.TryGetProperty(key, out var p) && p.TryGetInt64(out var v) ? v : null;

    public static string FormatFileSize(long bytes)
    {
        if (bytes <= 0)   return "—";
        if (bytes < 1024) return $"{bytes} B";
        if (bytes < 1024 * 1024) return $"{bytes / 1024.0:F1} KB";
        return $"{bytes / (1024.0 * 1024):F2} MB";
    }

    public void Dispose()
    {
        if (!_disposed) { _httpClient?.Dispose(); _disposed = true; }
    }
}
