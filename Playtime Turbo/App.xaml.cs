using System.IO;
using System.Windows;
using SpeedrunLauncher.Services;

namespace SpeedrunLauncher;

public partial class App : Application
{
    private const string LegacyExeName = "PlaytimeSpeedLauncher.exe";

    public static bool CameFromLegacy { get; private set; }

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        CleanupLegacyExe();
        ResourceExtractor.Extract();
        new MainWindow().Show();
    }

    private static void CleanupLegacyExe()
    {
        try
        {
            var legacyPath = Path.Combine(AppContext.BaseDirectory, LegacyExeName);
            if (!File.Exists(legacyPath)) return;

            var currentExeName = Path.GetFileName(Environment.ProcessPath ?? "");
            if (currentExeName.Equals(LegacyExeName, StringComparison.OrdinalIgnoreCase))
            {
                CameFromLegacy = true;
                return;
            }

            var currentExePath = Environment.ProcessPath;
            if (currentExePath != null)
            {
                try { File.Copy(currentExePath, legacyPath, overwrite: true); }
                catch { try { File.Delete(legacyPath); } catch { } }
            }
            else
            {
                try { File.Delete(legacyPath); } catch { }
            }

            CameFromLegacy = true;
        }
        catch { }
    }
}
