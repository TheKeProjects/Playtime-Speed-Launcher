using System.IO;
using System.Reflection;

namespace SpeedrunLauncher.Services;

public static class ResourceExtractor
{
    public static readonly string TempDir =
        Path.Combine(Path.GetTempPath(), "SpeedrunLauncher");

    public static void Extract()
    {
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

            var dest = Path.Combine(destFolder, relPath);
            Directory.CreateDirectory(Path.GetDirectoryName(dest)!);

            using var stream = asm.GetManifestResourceStream(name)!;
            using var file   = File.Create(dest);
            stream.CopyTo(file);
        }
    }
}
