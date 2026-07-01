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

    /// <summary>Resolves the Content\Paks folder from a .../Binaries/Win64 directory.</summary>
    public static string? GetPaksDir(string win64Dir)
    {
        var binariesDir = Directory.GetParent(win64Dir)?.FullName;
        var projectRoot = binariesDir != null ? Directory.GetParent(binariesDir)?.FullName : null;
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

    public static void Install(string paksDir, string zipPath)
    {
        Directory.CreateDirectory(paksDir);
        ZipFile.ExtractToDirectory(zipPath, paksDir, overwriteFiles: true);
    }

    /// <summary>Extracts the Chapter 1 UE4SS build into .../Binaries/Win64.</summary>
    public static void InstallUe4ss(string win64Dir, string ue4ssZipPath)
    {
        Directory.CreateDirectory(win64Dir);
        ZipFile.ExtractToDirectory(ue4ssZipPath, win64Dir, overwriteFiles: true);
    }

    public static void Uninstall(string paksDir, string zipPath) => DeleteZipContents(paksDir, zipPath);

    /// <summary>Removes the Chapter 1 UE4SS build (installed via <see cref="InstallUe4ss"/>) from Win64.</summary>
    public static void UninstallUe4ss(string win64Dir, string ue4ssZipPath) => DeleteZipContents(win64Dir, ue4ssZipPath);

    /// <summary>True when the UE4SS build currently in Win64 is the one bundled with Load Manip
    /// Chapter 1 (identified by a Lua mod folder unique to that build) rather than a standalone install.</summary>
    public static bool IsUe4ssFromLoadManip(string win64Dir) =>
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
