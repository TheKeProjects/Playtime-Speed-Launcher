using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.IO.Pipes;

namespace SpeedrunLauncher.Services.Fps;

/// <summary>
/// Public entry point the launcher uses to track a specific game's real FPS. Launches a
/// second, hidden, elevated instance of this same executable (helper mode, see
/// <see cref="FpsHelperEntryPoint"/>) that traces DXGI/D3D9 Present events for one process ID
/// only, and streams the computed FPS back over a private named pipe. The main launcher
/// itself is never elevated — only this short-lived helper is, and only while a tracked game
/// is running.
/// </summary>
public sealed class GameFpsService : IDisposable
{
    /// <summary>Raised on a background thread whenever a new FPS reading arrives.</summary>
    public event Action<double>? FpsUpdated;

    /// <summary>Raised if the user declines the UAC elevation prompt.</summary>
    public event Action? ElevationDeclined;

    private Process?                _helperProcess;
    private NamedPipeServerStream?  _pipe;
    private CancellationTokenSource? _cts;

    public bool IsRunning => _helperProcess is { HasExited: false };
    public int  ActivePid { get; private set; }

    /// <summary>Starts (or restarts, if a different game) FPS tracking for the given process.</summary>
    public void Start(int gamePid)
    {
        if (IsRunning && ActivePid == gamePid) return;
        Stop();

        var exePath = Environment.ProcessPath;
        if (exePath == null) return;

        ActivePid = gamePid;
        _cts      = new CancellationTokenSource();

        var pipeName = "PlaytimeFps_" + Guid.NewGuid().ToString("N");
        _pipe = new NamedPipeServerStream(pipeName, PipeDirection.In, 1,
            PipeTransmissionMode.Byte, PipeOptions.Asynchronous);

        var psi = new ProcessStartInfo
        {
            FileName        = exePath,
            Arguments       = $"{FpsHelperEntryPoint.Arg} {gamePid} {pipeName}",
            UseShellExecute = true,
            Verb            = "runas",
            WindowStyle     = ProcessWindowStyle.Hidden,
        };

        try
        {
            _helperProcess = Process.Start(psi);
        }
        catch (Win32Exception ex) when (ex.NativeErrorCode == 1223) // ERROR_CANCELLED (UAC declined)
        {
            ElevationDeclined?.Invoke();
            Stop();
            return;
        }
        catch (Exception)
        {
            Stop();
            return;
        }

        if (_helperProcess == null)
        {
            Stop();
            return;
        }

        _ = RunPipeLoopAsync(_pipe, _cts.Token);
    }

    private async Task RunPipeLoopAsync(NamedPipeServerStream pipe, CancellationToken token)
    {
        try
        {
            using var connectCts = CancellationTokenSource.CreateLinkedTokenSource(token);
            connectCts.CancelAfter(TimeSpan.FromSeconds(15));
            await pipe.WaitForConnectionAsync(connectCts.Token);

            using var reader = new StreamReader(pipe);
            while (!token.IsCancellationRequested)
            {
                var line = await reader.ReadLineAsync(token);
                if (line == null) break;

                if (double.TryParse(line, NumberStyles.Float, CultureInfo.InvariantCulture, out var fps))
                    FpsUpdated?.Invoke(fps);
            }
        }
        catch (OperationCanceledException) { }
        catch (IOException) { }
    }

    /// <summary>Stops tracking and terminates the helper process, if any.</summary>
    public void Stop()
    {
        _cts?.Cancel();
        _cts?.Dispose();
        _cts = null;

        // Close the pipe first so the helper's next write hits a broken pipe and its own
        // loop exits on its own — that lets its `using` block dispose DxgiPresentWatcher and
        // stop the ETW trace session cleanly. Killing the process outright skips that
        // disposal and orphans a system-wide ETW session that keeps tracing every DirectX
        // app's Present calls (with real overhead) until something else stops it or the
        // machine reboots — surfacing as FPS drops in whatever game you launch next.
        try { _pipe?.Dispose(); }
        catch { }
        _pipe = null;

        if (_helperProcess is { HasExited: false } helper)
        {
            try { if (!helper.WaitForExit(500)) helper.Kill(); }
            catch { }
        }
        _helperProcess?.Dispose();
        _helperProcess = null;

        ActivePid = 0;
    }

    public void Dispose() => Stop();
}
