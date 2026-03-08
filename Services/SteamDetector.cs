using System.Diagnostics;
using System.Text.RegularExpressions;
using SpeedrunLauncher.Models;

namespace SpeedrunLauncher.Services;

public static class SteamDetector
{
    private static readonly Dictionary<int, string[]> ChapterFolders = new()
    {
        [1] = ["Poppy Playtime"],
        [2] = ["Poppy Playtime Chapter 2", "Poppy Playtime - Chapter 2"],
        [3] = ["Poppy Playtime Chapter 3", "Poppy Playtime - Chapter 3"],
        [4] = ["Poppy Playtime Chapter 4", "Poppy Playtime - Chapter 4", "Poppy Playtime The Dark Ride"],
        [5] = ["Poppy Playtime Chapter 5", "Poppy Playtime - Chapter 5"],
    };

    public static void DetectAll(List<ChapterInfo> chapters)
    {
        var bases = CollectSearchBases();

        // Top-level directory names to exclude when scanning for a top-level chapter's exe.
        // We take only the first path component of SubFolderName so that multi-level
        // paths like "WindowsNoEditor\Playtime_Prototype4" correctly exclude "WindowsNoEditor".
        var chapterSubFolders = new HashSet<string>(
            chapters
                .Where(c => c.SubFolderName != null)
                .Select(c => c.SubFolderName!.Split(
                    [Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar], 2)[0]),
            StringComparer.OrdinalIgnoreCase);

        foreach (var chapter in chapters)
        {
            if (!chapter.IsAvailable) continue;

            foreach (var basePath in bases)
            {
                if (!Directory.Exists(basePath)) continue;

                // Strategy A: chapter lives as a subfolder inside the main game folder
                // e.g. steamapps\common\Poppy Playtime\PoppyPlaytime_Chapter3
                if (chapter.SubFolderName != null)
                {
                    var parentCandidates = ChapterFolders.GetValueOrDefault(1, ["Poppy Playtime"]);
                    foreach (var parentName in parentCandidates)
                    {
                        var subPath = Path.Combine(basePath, parentName, chapter.SubFolderName);
                        if (!Directory.Exists(subPath)) continue;

                        var exe = FindMainExe(subPath);
                        if (exe == null) continue;

                        chapter.GameExePath = exe;
                        chapter.DetectedVersion = GetVersion(subPath, basePath, chapter.SteamAppId);
                        goto nextChapter;
                    }
                }

                // Strategy B: chapter is its own top-level Steam game folder.
                // Exclude chapter subfolders so Ch1 doesn't pick up Ch3/4/5 exes.
                var candidates = ChapterFolders.GetValueOrDefault(chapter.Number, [chapter.SteamFolderName]);
                foreach (var folderName in candidates)
                {
                    var gamePath = Path.Combine(basePath, folderName);
                    if (!Directory.Exists(gamePath)) continue;

                    var exe = FindMainExe(gamePath, chapterSubFolders);
                    if (exe == null) continue;

                    chapter.GameExePath = exe;
                    chapter.DetectedVersion = GetVersion(gamePath, basePath, chapter.SteamAppId);
                    goto nextChapter;
                }
            }

            nextChapter:;
        }
    }

    private static List<string> CollectSearchBases()
    {
        var result = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        // 1. If the launcher lives inside a steamapps\common folder, use that
        var launcherDir = new DirectoryInfo(AppContext.BaseDirectory);
        for (var dir = launcherDir; dir != null; dir = dir.Parent)
        {
            if (dir.Name.Equals("common", StringComparison.OrdinalIgnoreCase))
            {
                result.Add(dir.FullName);
                break;
            }
        }

        // 2. Scan every fixed drive with common Steam library paths
        var relativePaths = new[]
        {
            @"SteamLibrary\steamapps\common",
            @"Steam\steamapps\common",
            @"Program Files (x86)\Steam\steamapps\common",
            @"Program Files\Steam\steamapps\common",
            @"Games\SteamLibrary\steamapps\common",
        };

        foreach (var drive in DriveInfo.GetDrives().Where(d => d.IsReady && d.DriveType == DriveType.Fixed))
        {
            foreach (var rel in relativePaths)
                result.Add(Path.Combine(drive.RootDirectory.FullName, rel));
        }

        // 3. Parse Steam libraryfolders.vdf for additional library paths
        foreach (var vdfPath in SteamVdfPaths())
        {
            if (!File.Exists(vdfPath)) continue;
            try
            {
                var content = File.ReadAllText(vdfPath);
                foreach (Match m in Regex.Matches(content, @"""path""\s+""([^""]+)"""))
                {
                    var libPath = m.Groups[1].Value.Replace("\\\\", "\\");
                    result.Add(Path.Combine(libPath, "steamapps", "common"));
                }
            }
            catch { }
        }

        return [.. result];
    }

    private static IEnumerable<string> SteamVdfPaths()
    {
        yield return @"C:\Program Files (x86)\Steam\config\libraryfolders.vdf";
        yield return @"C:\Program Files\Steam\config\libraryfolders.vdf";

        // Also check each drive root for a Steam install
        foreach (var drive in DriveInfo.GetDrives().Where(d => d.IsReady && d.DriveType == DriveType.Fixed))
        {
            yield return Path.Combine(drive.RootDirectory.FullName, @"Steam\config\libraryfolders.vdf");
        }
    }

    /// <summary>
    /// Finds the depot download folder produced by a <c>download_depot</c> command.
    /// SteamCMD (standalone) writes to <c>&lt;steamcmd_dir&gt;\steamapps\content\…</c>;
    /// a full Steam install uses its own steamapps folder.
    /// </summary>
    public static string? FindDepotDownloadPath(int appId, int depotId,
        string? steamCmdPath = null)
    {
        var subPath = Path.Combine("steamapps", "content", $"app_{appId}", $"depot_{depotId}");

        var roots = new List<string>();

        // 1. Folder where steamcmd.exe lives (highest priority — this is where standalone
        //    SteamCMD always writes its downloads)
        if (steamCmdPath != null)
        {
            var dir = Path.GetDirectoryName(steamCmdPath);
            if (dir != null) roots.Add(dir);
        }

        // 2. Launcher's own bundled steamcmd folder
        roots.Add(Path.Combine(AppContext.BaseDirectory, "steamcmd"));

        // 3. Standard Steam installation paths on every fixed drive
        var steamBases = new[] { @"Program Files (x86)\Steam", @"Program Files\Steam", "Steam" };
        foreach (var drive in DriveInfo.GetDrives().Where(d => d.IsReady && d.DriveType == DriveType.Fixed))
            foreach (var steamBase in steamBases)
                roots.Add(Path.Combine(drive.RootDirectory.FullName, steamBase));

        foreach (var root in roots)
        {
            var path = Path.Combine(root, subPath);
            if (!Directory.Exists(path)) continue;
            try
            {
                if (Directory.EnumerateFiles(path, "*", SearchOption.AllDirectories).Any())
                    return path;
            }
            catch { }
        }
        return null;
    }

    /// <summary>Public wrapper to find the main game exe in a folder (used after moving depot downloads).</summary>
    public static string? FindGameExe(string gamePath) => FindMainExe(gamePath);

    private static string? FindMainExe(string gamePath, HashSet<string>? excludeDirs = null)
    {
        try
        {
            var exes = new List<string>();
            CollectExes(gamePath, exes, excludeDirs);
            return exes
                .Where(f =>
                {
                    var n = Path.GetFileNameWithoutExtension(f).ToLowerInvariant();
                    return !n.Contains("crash") && !n.Contains("setup") &&
                           !n.Contains("prereq") && !n.Contains("unins") &&
                           !n.Contains("install") && !n.Contains("redist");
                })
                .OrderByDescending(f => new FileInfo(f).Length)
                .FirstOrDefault();
        }
        catch { return null; }
    }

    private static void CollectExes(string dir, List<string> result, HashSet<string>? excludeDirs)
    {
        try
        {
            result.AddRange(Directory.GetFiles(dir, "*.exe"));
            foreach (var sub in Directory.GetDirectories(dir))
            {
                if (excludeDirs != null && excludeDirs.Contains(Path.GetFileName(sub)))
                    continue;
                CollectExes(sub, result, excludeDirs);
            }
        }
        catch { }
    }

    private static string GetVersion(string gamePath, string basePath, int appId)
    {
        // Try Steam ACF buildid (most reliable)
        if (appId > 0)
        {
            var steamapps = Directory.GetParent(basePath)?.FullName;
            if (steamapps != null)
            {
                var acf = Path.Combine(steamapps, $"appmanifest_{appId}.acf");
                if (File.Exists(acf))
                {
                    try
                    {
                        var content = File.ReadAllText(acf);
                        var m = Regex.Match(content, @"""buildid""\s+""(\d+)""");
                        if (m.Success) return $"Build {m.Groups[1].Value}";
                    }
                    catch { }
                }
            }
        }

        // Fallback: FileVersionInfo from the main exe
        var exe = FindMainExe(gamePath);
        if (exe != null)
        {
            try
            {
                var info = FileVersionInfo.GetVersionInfo(exe);
                var ver = info.ProductVersion ?? info.FileVersion;
                if (!string.IsNullOrWhiteSpace(ver) && ver != "0.0.0.0")
                    return ver;
            }
            catch { }
        }

        return "Instalado";
    }
}
