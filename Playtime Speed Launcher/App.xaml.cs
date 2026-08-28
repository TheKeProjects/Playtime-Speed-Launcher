using System.Windows;
using SpeedrunLauncher.Services;
using SpeedrunLauncher.Services.App;
using SpeedrunLauncher.Services.Platforms;

namespace SpeedrunLauncher;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        if (e.Args.Length > 0 && e.Args[0] == Services.Fps.FpsHelperEntryPoint.Arg)
        {
            Environment.Exit(Services.Fps.FpsHelperEntryPoint.Run(e.Args));
            return;
        }

        if (e.Args.Length > 0 && e.Args[0] == SteamShortcutHelperEntryPoint.Arg)
        {
            Environment.Exit(SteamShortcutHelperEntryPoint.Run(e.Args));
            return;
        }

        base.OnStartup(e);
        ResourceExtractor.Extract();
        new MainWindow().Show();
    }

    protected override void OnExit(ExitEventArgs e)
    {
        ResourceExtractor.CleanupSaves();
        base.OnExit(e);
    }
}
