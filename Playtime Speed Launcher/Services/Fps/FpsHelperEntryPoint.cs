using System.Diagnostics;
using System.Globalization;
using System.IO.Pipes;
using Microsoft.Diagnostics.Tracing.Session;

namespace SpeedrunLauncher.Services.Fps;

/// <summary>
/// Entry point for the elevated helper mode: the launcher relaunches itself with argv
/// [<see cref="Arg"/>, pid, pipeName], and App.xaml.cs routes straight here instead of
/// showing the UI. This process traces Present events for <c>pid</c> via
/// <see cref="DxgiPresentWatcher"/> and writes one FPS value per line to the named pipe every
/// half second, until the target game exits or the parent (unelevated) launcher disconnects.
/// </summary>
public static class FpsHelperEntryPoint
{
    public const string Arg = "--fps-helper";

    // How often a new reading is sent to the launcher — fluid without flooding the pipe.
    private static readonly TimeSpan ReportInterval = TimeSpan.FromMilliseconds(100);
    // How far back CurrentFps() averages — long enough for a precise count (~90-120 frames
    // at typical rates), short enough to reflect a real change within about a second.
    private const double WindowMs = 800;

    public static int Run(string[] args)
    {
        if (args.Length < 3 || !int.TryParse(args[1], out var targetPid)) return 1;

        if (!TraceEventSession.IsElevated().GetValueOrDefault()) return 2;

        var pipeName = args[2];

        using var pipe = new NamedPipeClientStream(".", pipeName, PipeDirection.Out);
        try { pipe.Connect(3000); }
        catch (Exception) { return 3; }

        using var writer = new StreamWriter(pipe) { AutoFlush = true };

        var window = new FrameTimeWindow(WindowMs);
        using var watcher = new DxgiPresentWatcher(targetPid, window.AddFrame);
        watcher.Start();

        try
        {
            while (true)
            {
                Thread.Sleep(ReportInterval);

                try { Process.GetProcessById(targetPid); }
                catch (ArgumentException) { break; }

                var fps = window.CurrentFps();
                writer.WriteLine(fps.ToString("F1", CultureInfo.InvariantCulture));
            }
        }
        catch (IOException) { }
        catch (ObjectDisposedException) { }

        return 0;
    }
}
