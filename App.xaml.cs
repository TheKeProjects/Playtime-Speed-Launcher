using System.Windows;
using SpeedrunLauncher.Services;

namespace SpeedrunLauncher;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        ResourceExtractor.Extract();
        new MainWindow().Show();
    }
}
