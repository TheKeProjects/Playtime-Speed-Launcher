using System.IO.Compression;
using IOPath = System.IO.Path;

namespace SpeedrunLauncher.Services;

/// <summary>
/// Installs/removes the "FullBright" pak mod files (Chapter 1) into a game
/// install's Content\Paks, Binaries\Win64 and project-root folders. Requires
/// Load Manip to already be installed, since its pak/UE4SS files intentionally
/// overwrite Load Manip's own. Zip contents are read at runtime so the set of
/// files/folders to extract or delete always matches what actually ships in the zip.
/// </summary>
public static class FullBrightFilesService
{
    public static string? GetZipPath(int chapterNumber) => chapterNumber switch
    {
        1 => IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Tools", "Chapter 1 Night Vision", "FullBright", "FullBrightCH1.zip"),
        _ => null,
    };

    /// <summary>Zip with the UE4SS build FullBright Chapter 1 needs, extracted into
    /// the .../Binaries/Win64 folder alongside the game exe, overwriting Load Manip's build.</summary>
    public static string? GetUe4ssZipPath(int chapterNumber) => chapterNumber switch
    {
        1 => IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Tools", "Chapter 1 Night Vision", "FullBright", "FullBrightCH1UE4SS.zip"),
        _ => null,
    };

    /// <summary>Marker zip containing the empty `fullbright.playtime` file, extracted into the
    /// same Win64 folder as <see cref="GetUe4ssZipPath"/> so <see cref="IsInstalled"/> can tell
    /// a FullBright install apart from a plain Load Manip install.</summary>
    public static string? GetPlaytimeMarkerZipPath(int chapterNumber) => chapterNumber switch
    {
        1 => IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Tools", "Chapter 1 Night Vision", "FullBright", "FullBrightCH1Playtime.zip"),
        _ => null,
    };

    /// <summary>Filename of the keybind config file, both inside <see cref="GetConfigZipPath"/>
    /// and once extracted at the project root.</summary>
    public const string ConfigFileName = "FullBright_Config.ini";

    /// <summary>Zip with the keybind config file, extracted into the project root
    /// (the folder containing Binaries/Content) only when one isn't already present there.</summary>
    public static string? GetConfigZipPath(int chapterNumber) => chapterNumber switch
    {
        1 => IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Tools", "Chapter 1 Night Vision", "FullBright", "ConfigFullBrightCH1.zip"),
        _ => null,
    };

    /// <summary>Loose (unzipped) copy of the bundled config template, used only to display
    /// its default keybinds in Settings when FullBright isn't installed — not extracted anywhere.</summary>
    public static string GetLooseConfigPath() =>
        IOPath.Combine(ResourceExtractor.TempDir, "Assets", "Tools", "Chapter 1 Night Vision", "FullBright", ConfigFileName);

    /// <summary>True when FullBright's marker file is present in Win64. The generic zip-content
    /// comparison Load Manip uses isn't reliable here since FullBright's pak shares Load Manip's
    /// own pak filename, so the marker file is the only trustworthy signal.</summary>
    public static bool IsInstalled(string win64Dir) =>
        File.Exists(IOPath.Combine(win64Dir, "fullbright.playtime"));

    /// <summary>Resolves the project root (folder containing Binaries/Content) from a
    /// .../Binaries/Win64 directory — same arithmetic as LoadManipFilesService.GetPaksDir.</summary>
    public static string? GetProjectRoot(string win64Dir)
    {
        var binariesDir = Directory.GetParent(win64Dir)?.FullName;
        return binariesDir != null ? Directory.GetParent(binariesDir)?.FullName : null;
    }

    /// <summary>Extracts the pak into paksDir, the UE4SS build + marker into win64Dir (all
    /// overwriting existing files), and — only if configZipPath is supplied and no config.ini
    /// already exists at the project root — the config.ini into the project root.</summary>
    public static void Install(string paksDir, string win64Dir, string zipPath, string ue4ssZipPath, string markerZipPath, string? configZipPath = null)
    {
        Directory.CreateDirectory(paksDir);
        ZipFile.ExtractToDirectory(zipPath, paksDir, overwriteFiles: true);

        Directory.CreateDirectory(win64Dir);
        ZipFile.ExtractToDirectory(ue4ssZipPath, win64Dir, overwriteFiles: true);
        ZipFile.ExtractToDirectory(markerZipPath, win64Dir, overwriteFiles: true);

        if (configZipPath != null && File.Exists(configZipPath))
        {
            var projectRoot = GetProjectRoot(win64Dir);
            if (projectRoot != null && !File.Exists(IOPath.Combine(projectRoot, ConfigFileName)))
                ZipFile.ExtractToDirectory(configZipPath, projectRoot, overwriteFiles: false);
        }
    }

    /// <summary>Removes exactly the files FullBright's zips ship, from paksDir, win64Dir and
    /// the project root's config.ini.</summary>
    public static void Uninstall(string paksDir, string win64Dir, string zipPath, string ue4ssZipPath, string markerZipPath, string? configZipPath = null)
    {
        DeleteZipContents(paksDir, zipPath);
        DeleteZipContents(win64Dir, ue4ssZipPath);
        DeleteZipContents(win64Dir, markerZipPath);

        if (configZipPath != null && File.Exists(configZipPath))
        {
            var projectRoot = GetProjectRoot(win64Dir);
            if (projectRoot != null)
                DeleteZipContents(projectRoot, configZipPath);
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
