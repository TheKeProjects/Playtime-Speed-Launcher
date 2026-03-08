using Microsoft.Windows.ApplicationModel.DynamicDependency;

namespace SpeedrunLauncher;

public static class Program
{
    [STAThread]
    static void Main(string[] args)
    {
        Environment.SetEnvironmentVariable("MICROSOFT_WINDOWSAPPRUNTIME_BASE_DIRECTORY", AppContext.BaseDirectory);

        // Initialize the Windows App Runtime bootstrap before any WinUI 3 usage.
        // 0x00010006 = major 1, minor 6 (Windows App SDK 1.6)
        Bootstrap.Initialize(0x00010006);

        WinRT.ComWrappersSupport.InitializeComWrappers();
        Microsoft.UI.Xaml.Application.Start(_ => new App());
    }
}
