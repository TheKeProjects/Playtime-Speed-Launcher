using System.Runtime.InteropServices;
using Microsoft.Windows.ApplicationModel.DynamicDependency;

namespace SpeedrunLauncher;

public static class Program
{
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    static extern int MessageBoxW(IntPtr hWnd, string text, string caption, uint type);

    [STAThread]
    static void Main(string[] args)
    {
        // Required for single-file publishing: tells the runtime where to find WinAppSDK DLLs.
        Environment.SetEnvironmentVariable("MICROSOFT_WINDOWSAPPRUNTIME_BASE_DIRECTORY", AppContext.BaseDirectory);

        // Initialize the Windows App Runtime bootstrap before any WinUI 3 usage.
        // 0x00010006 = major 1, minor 6 (Windows App SDK 1.6)
        try
        {
            Bootstrap.Initialize(0x00010006);
        }
        catch (Exception ex)
        {
            MessageBoxW(
                IntPtr.Zero,
                "Windows App Runtime 1.6 is required to run this app.\n\n" +
                "Please download and install it from:\nhttps://aka.ms/windowsappsdk/1.6/latest/windowsappruntimeinstall-x64.exe\n\n" +
                $"Details: {ex.Message}",
                "Missing Dependency",
                0x10); // MB_OK | MB_ICONERROR
            Environment.Exit(1);
        }

        WinRT.ComWrappersSupport.InitializeComWrappers();
        Microsoft.UI.Xaml.Application.Start(p =>
        {
            // Install DispatcherQueueSynchronizationContext so that 'await' in async
            // event handlers resumes on the UI thread (required for unpackaged WinUI 3).
            var context = new Microsoft.UI.Dispatching.DispatcherQueueSynchronizationContext(
                Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread());
            SynchronizationContext.SetSynchronizationContext(context);
            _ = new App();
        });
    }
}
