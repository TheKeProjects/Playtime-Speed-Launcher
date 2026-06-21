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
            if (File.Exists(legacyPath))
            {
                File.Delete(legacyPath);
                CameFromLegacy = true;
            }
        }
        catch { }
    }
}
