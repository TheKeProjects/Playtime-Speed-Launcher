using System.IO;
using System.IO.Compression;
using System.Reflection;

namespace SpeedrunLauncher.Services.App;

public static class ResourceExtractor
{
    public static readonly string TempDir =
        Path.Combine(Path.GetTempPath(), "SpeedrunLauncher");

    public static string SavesDir => Path.Combine(TempDir, "Assets", "Saves");

    /// <summary>Extracted Chapter 5 New Game+ save files, kept separate from <see cref="SavesDir"/>'s
    /// regular "Chapter 5" 100% saves since both would otherwise land on the same file names.</summary>
    public static string Chapter5NgPlusDir => Path.Combine(SavesDir, "Chapter 5 NG+");

    public static void Extract()
    {
        if (Directory.Exists(TempDir))
            Directory.Delete(TempDir, recursive: true);

        var asm = Assembly.GetExecutingAssembly();

        foreach (var name in asm.GetManifestResourceNames())
        {
            string destFolder;
            string relPath;

            if (name.StartsWith("assets___"))
            {
                destFolder = Path.Combine(TempDir, "Assets");
                relPath    = name["assets___".Length..];
            }
            else if (name.StartsWith("translations___"))
            {
                destFolder = Path.Combine(TempDir, "Transalations");
                relPath    = name["translations___".Length..];
            }
            else continue;

            // The per-checkpoint save files are bundled as a single Saves.zip (instead of hundreds
            // of loose embedded files) to keep the build small — unpack it straight into
            // Assets/Saves/ so it reproduces the same "Chapter N/..." layout callers expect.
            if (string.Equals(Path.GetFileName(relPath), "Saves.zip", StringComparison.OrdinalIgnoreCase) &&
                string.Equals(Path.GetDirectoryName(relPath), "Saves", StringComparison.OrdinalIgnoreCase))
            {
                Directory.CreateDirectory(SavesDir);
                using var zipStream = asm.GetManifestResourceStream(name)!;
                using var archive   = new ZipArchive(zipStream, ZipArchiveMode.Read);
                archive.ExtractToDirectory(SavesDir, overwriteFiles: true);
                continue;
            }

            // Chapter 5 New Game+ saves ship as their own zip (flat .sav files, no "Chapter N/"
            // wrapper inside) — unpack into their own subfolder so they never collide with the
            // regular Chapter 5 100% saves extracted above.
            if (string.Equals(Path.GetFileName(relPath), "CH5 NG+.zip", StringComparison.OrdinalIgnoreCase) &&
                string.Equals(Path.GetDirectoryName(relPath), "Saves", StringComparison.OrdinalIgnoreCase))
            {
                Directory.CreateDirectory(Chapter5NgPlusDir);
                using var zipStream = asm.GetManifestResourceStream(name)!;
                using var archive   = new ZipArchive(zipStream, ZipArchiveMode.Read);
                archive.ExtractToDirectory(Chapter5NgPlusDir, overwriteFiles: true);
                continue;
            }

            var dest = Path.Combine(destFolder, relPath);
            Directory.CreateDirectory(Path.GetDirectoryName(dest)!);

            using var stream = asm.GetManifestResourceStream(name)!;
            using var file   = File.Create(dest);
            stream.CopyTo(file);
        }
    }

    /// <summary>Deletes the extracted checkpoint saves — called on shutdown so the (unzipped,
    /// uncompressed) copy doesn't sit around on disk between launches. <see cref="Extract"/> wipes
    /// and recreates them fresh on every startup anyway.</summary>
    public static void CleanupSaves()
    {
        try
        {
            if (Directory.Exists(SavesDir))
                Directory.Delete(SavesDir, recursive: true);
        }
        catch { }
    }
}
