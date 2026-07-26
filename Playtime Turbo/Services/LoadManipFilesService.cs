using System.IO.Compression;
using IOPath = System.IO.Path;

namespace SpeedrunLauncher.Services;

/// <summary>
/// Installs/removes the "Load Manip" pak mod files (Chapter 1 and Chapter 4)
/// into a game install's Content\Paks folder. Zip contents are read at
/// runtime so the set of files/folders to extract or delete always matches
/// what actually ships in the zip.
/// </summary>
public static class LoadManipFilesService
{
    public static string? GetZipPath(int chapterNumber) => chapterNumber switch
    {
        1 => IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Tools", "Load Manip Chapter 1", "LoadManipCH1.zip"),
        4 => IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Tools", "Load Manip Chapter 4", "LoadManipCH4.zip"),
        _ => null,
    };

    /// <summary>Zip with the UE4SS build Load Manip Chapter 1 needs, extracted into
    /// the .../Binaries/Win64 folder alongside the game exe.</summary>
    public static string? GetUe4ssZipPath(int chapterNumber) => chapterNumber switch
    {
        1 => IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Tools", "Load Manip Chapter 1", "LoadManipCH1UE4SS.zip"),
        _ => null,
    };

    /// <summary>Marker zip containing the empty `launcher.playtime` file, extracted into the
    /// same Win64 folder as <see cref="GetUe4ssZipPath"/> so <see cref="IsUe4ssFromLoadManip"/>
    /// can reliably tell a Load Manip install apart from a standalone UE4SS install.</summary>
    public static string? GetPlaytimeMarkerZipPath(int chapterNumber) => chapterNumber switch
    {
        1 => IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Tools", "Load Manip Chapter 1", "LoadManipCH1Playtime.zip"),
        _ => null,
    };

    /// <summary>Filename of the Freeze/Normal keybind config file, both inside
    /// <see cref="GetConfigZipPath"/> and once extracted at the project root.</summary>
    public const string ConfigFileName = "LoadManip_Config.ini";

    /// <summary>Zip with the keybind config file, extracted into the project root
    /// (the folder containing Binaries/Content) only when one isn't already present there.</summary>
    public static string? GetConfigZipPath(int chapterNumber) => chapterNumber switch
    {
        1 => IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Tools", "Load Manip Chapter 1", "ConfigLoadManipCH1.zip"),
        _ => null,
    };

    /// <summary>Resolves the project root (folder containing Binaries/Content) from a
    /// .../Binaries/Win64 directory.</summary>
    public static string? GetProjectRoot(string win64Dir)
    {
        var binariesDir = Directory.GetParent(win64Dir)?.FullName;
        return binariesDir != null ? Directory.GetParent(binariesDir)?.FullName : null;
    }

    /// <summary>Resolves the Content\Paks folder from a .../Binaries/Win64 directory.</summary>
    public static string? GetPaksDir(string win64Dir)
    {
        var projectRoot = GetProjectRoot(win64Dir);
        return projectRoot != null ? IOPath.Combine(projectRoot, "Content", "Paks") : null;
    }

    public static bool IsInstalled(string paksDir, string zipPath)
    {
        if (!Directory.Exists(paksDir) || !File.Exists(zipPath)) return false;
        try
        {
            using var zip = ZipFile.OpenRead(zipPath);
            foreach (var entry in zip.Entries)
            {
                if (string.IsNullOrEmpty(entry.Name)) continue; // directory entry
                var rel = entry.FullName.Replace('/', IOPath.DirectorySeparatorChar);
                if (File.Exists(IOPath.Combine(paksDir, rel))) return true;
            }
        }
        catch { return false; }
        return false;
    }

    /// <summary>Extracts the pak into paksDir and — only if configZipPath is supplied and no
    /// config.ini already exists at the project root — the config.ini into the project root.</summary>
    public static void Install(string paksDir, string zipPath, string? configZipPath = null)
    {
        Directory.CreateDirectory(paksDir);
        ZipFile.ExtractToDirectory(zipPath, paksDir, overwriteFiles: true);

        if (configZipPath != null && File.Exists(configZipPath))
        {
            var contentDir  = Directory.GetParent(paksDir)?.FullName;
            var projectRoot = contentDir != null ? Directory.GetParent(contentDir)?.FullName : null;
            if (projectRoot != null && !File.Exists(IOPath.Combine(projectRoot, ConfigFileName)))
                ZipFile.ExtractToDirectory(configZipPath, projectRoot, overwriteFiles: false);
        }
    }

    /// <summary>Rewrites a single `Key = Value` line in an already-extracted config.ini,
    /// preserving every other line (comments, section headers, other keys) as-is.</summary>
    public static void UpdateConfigKey(string configPath, string key, string value)
    {
        var lines = File.ReadAllLines(configPath);
        for (int i = 0; i < lines.Length; i++)
        {
            var t = lines[i].TrimStart();
            if (t.StartsWith('#') || t.StartsWith('[') || !t.Contains('=')) continue;

            var k = t[..t.IndexOf('=')].Trim();
            if (!k.Equals(key, StringComparison.OrdinalIgnoreCase)) continue;

            lines[i] = $"{key} = {value}";
            File.WriteAllLines(configPath, lines);
            return;
        }
    }

    /// <summary>Extracts the Chapter 1 UE4SS build into .../Binaries/Win64, along with the
    /// launcher.playtime marker file (when <paramref name="markerZipPath"/> is supplied) used
    /// to identify this as a Load Manip install.</summary>
    public static void InstallUe4ss(string win64Dir, string ue4ssZipPath, string? markerZipPath = null)
    {
        Directory.CreateDirectory(win64Dir);
        ZipFile.ExtractToDirectory(ue4ssZipPath, win64Dir, overwriteFiles: true);
        if (markerZipPath != null && File.Exists(markerZipPath))
            ZipFile.ExtractToDirectory(markerZipPath, win64Dir, overwriteFiles: true);
    }

    public static void Uninstall(string paksDir, string zipPath) => DeleteZipContents(paksDir, zipPath);

    /// <summary>Removes the Chapter 1 UE4SS build (installed via <see cref="InstallUe4ss"/>) from Win64,
    /// along with the launcher.playtime marker file when <paramref name="markerZipPath"/> is supplied.</summary>
    public static void UninstallUe4ss(string win64Dir, string ue4ssZipPath, string? markerZipPath = null)
    {
        DeleteZipContents(win64Dir, ue4ssZipPath);
        if (markerZipPath != null && File.Exists(markerZipPath))
            DeleteZipContents(win64Dir, markerZipPath);
    }

    /// <summary>True when the UE4SS build currently in Win64 was installed by Load Manip rather
    /// than a standalone install. Primarily identified by the launcher.playtime marker file left
    /// alongside it; falls back to a Lua mod folder unique to that build for installs made before
    /// the marker file existed.</summary>
    public static bool IsUe4ssFromLoadManip(string win64Dir) =>
        File.Exists(IOPath.Combine(win64Dir, "launcher.playtime")) ||
        Directory.Exists(IOPath.Combine(win64Dir, "ue4ss", "Mods", "AsyncLoadToggle"));

    private static void DeleteZipContents(string targetDir, string zipPath)
    {
        using var zip = ZipFile.OpenRead(zipPath);
        var dirsToDelete = new List<string>();

        foreach (var entry in zip.Entries)
        {
            var rel = entry.FullName.Replace('/', IOPath.DirectorySeparatorChar);
            if (string.IsNullOrEmpty(entry.Name))
            {
                dirsToDelete.Add(IOPath.Combine(targetDir, rel.TrimEnd(IOPath.DirectorySeparatorChar)));
            }
            else
            {
                var target = IOPath.Combine(targetDir, rel);
                if (File.Exists(target)) File.Delete(target);
            }
        }

        foreach (var dir in dirsToDelete.OrderBy(d => d.Length))
        {
            if (Directory.Exists(dir))
                Directory.Delete(dir, recursive: true);
        }
    }
}
