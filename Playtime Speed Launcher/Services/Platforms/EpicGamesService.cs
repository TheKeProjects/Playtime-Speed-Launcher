using System.Text.Json;

namespace SpeedrunLauncher.Services.Platforms;

public class EpicGamesService
{
    private static readonly string ConfigFile = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "SpeedrunLauncher", "epic.json");

    // Relative paths inside the PoppyPlaytimeChapterOne folder to each chapter's exe.
    // BasePath always points to the PoppyPlaytimeChapterOne folder itself.
    private static readonly Dictionary<int, string> ChapterSubPaths = new()
    {
        [1] = @"Chapter1\Poppy_Playtime.exe",
        [2] = @"Chapter2\Playtime_Prototype4.exe",
        [3] = @"Chapter3\Playtime_Chapter3.exe",
        [4] = @"Chapter4\Playtime_Chapter4.exe",
        [5] = @"Chapter5\ch5_pro.exe",
    };

    public bool    IsEnabled { get; private set; }
    public string? BasePath  { get; private set; }

    public EpicGamesService() => Load();

    /// <summary>Returns the exe path for a chapter, or null if not found.</summary>
    public string? GetExePath(int chapter)
    {
        if (BasePath is null || !ChapterSubPaths.TryGetValue(chapter, out var sub))
            return null;
        var path = Path.Combine(BasePath, sub);
        return File.Exists(path) ? path : null;
    }

    public bool HasExeForAnyChapter() =>
        BasePath is not null &&
        ChapterSubPaths.Values.Any(sub => File.Exists(Path.Combine(BasePath, sub)));

    public void SetEnabled(bool enabled) { IsEnabled = enabled; Save(); }

    public void SetBasePath(string? path) { BasePath = path; Save(); }

    /// <summary>
    /// Tries to locate the Epic Games install of Poppy Playtime automatically.
    /// Checks Epic manifest JSON files first, then searches every fixed drive
    /// for a folder named PoppyPlaytimeChapterOne up to 4 levels deep.
    /// </summary>
    public string? TryAutoDetect()
    {
        // 1. Parse Epic Launcher manifest files
        var manifestDir = @"C:\ProgramData\Epic\EpicGamesLauncher\Data\Manifests";
        if (Directory.Exists(manifestDir))
        {
            foreach (var file in Directory.GetFiles(manifestDir, "*.item"))
            {
                try
                {
                    var doc  = JsonDocument.Parse(File.ReadAllText(file));
                    var root = doc.RootElement;

                    if (!root.TryGetProperty("InstallLocation", out var loc)) continue;
                    var installPath = loc.GetString();
                    if (installPath is null) continue;

                    // InstallLocation might be PoppyPlaytimeChapterOne itself or its parent
                    if (IsValidEpicBase(installPath)) return installPath;

                    var sub = Path.Combine(installPath, "PoppyPlaytimeChapterOne");
                    if (IsValidEpicBase(sub)) return sub;
                }
                catch { }
            }
        }

        // 2. Search every fixed drive for a folder named PoppyPlaytimeChapterOne
        foreach (var drive in DriveInfo.GetDrives().Where(d => d.IsReady && d.DriveType == DriveType.Fixed))
        {
            var found = FindPoppyFolder(drive.RootDirectory.FullName, maxDepth: 4);
            if (found != null) return found;
        }

        return null;
    }

    private string? FindPoppyFolder(string dir, int maxDepth)
    {
        if (maxDepth <= 0) return null;
        try
        {
            var target = Path.Combine(dir, "PoppyPlaytimeChapterOne");
            if (Directory.Exists(target) && IsValidEpicBase(target))
                return target;

            foreach (var sub in Directory.GetDirectories(dir))
            {
                var name = Path.GetFileName(sub);
                // Skip system folders that will never contain game installs
                if (name.Equals("Windows",           StringComparison.OrdinalIgnoreCase) ||
                    name.Equals("System32",          StringComparison.OrdinalIgnoreCase) ||
                    name.Equals("SysWOW64",          StringComparison.OrdinalIgnoreCase) ||
                    name.Equals("ProgramData",       StringComparison.OrdinalIgnoreCase) ||
                    name.StartsWith("$",             StringComparison.OrdinalIgnoreCase))
                    continue;

                var result = FindPoppyFolder(sub, maxDepth - 1);
                if (result != null) return result;
            }
        }
        catch { }
        return null;
    }

    private bool IsValidEpicBase(string path) =>
        ChapterSubPaths.Values.Any(sub => File.Exists(Path.Combine(path, sub)));

    private void Load()
    {
        try
        {
            if (!File.Exists(ConfigFile)) return;
            var data = JsonSerializer.Deserialize<EpicConfig>(File.ReadAllText(ConfigFile));
            if (data is null) return;
            BasePath  = data.BasePath;
            IsEnabled = data.IsEnabled;
        }
        catch { }
    }

    private void Save()
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(ConfigFile)!);
            File.WriteAllText(ConfigFile,
                JsonSerializer.Serialize(
                    new EpicConfig { BasePath = BasePath, IsEnabled = IsEnabled },
                    new JsonSerializerOptions { WriteIndented = true }));
        }
        catch { }
    }

    private sealed class EpicConfig
    {
        public string? BasePath  { get; set; }
        public bool    IsEnabled { get; set; }
    }
}
