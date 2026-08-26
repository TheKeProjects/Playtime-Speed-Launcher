using System.Windows;
using SpeedrunLauncher.Services;

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

        if (e.Args.Length > 0 && e.Args[0] == Services.SteamShortcutHelperEntryPoint.Arg)
        {
            Environment.Exit(Services.SteamShortcutHelperEntryPoint.Run(e.Args));
            return;
        }

        base.OnStartup(e);
        ResourceExtractor.Extract();
        new MainWindow().Show();
    }
}
