using Microsoft.Diagnostics.Tracing;
using Microsoft.Diagnostics.Tracing.Session;

namespace SpeedrunLauncher.Services.Fps;

/// <summary>
/// Traces DXGI (D3D11/12) and Direct3D9 swapchain Present events system-wide via ETW — the
/// same technique RTSS/PresentMon use — but only reports the ones whose ProcessID matches
/// <see cref="_targetPid"/>. This is what makes the resulting FPS reading specific to one
/// launched game rather than the whole system or the desktop compositor.
///
/// Each frame is reported with the ETW-assigned timestamp of the Present call itself
/// (<c>TimeStampRelativeMSec</c>), not the wall-clock time we happened to receive it — delivery
/// can lag behind (see the flush timer below), but the recorded moment of the frame is exact,
/// which is what makes a precise rolling FPS average possible downstream.
///
/// Creating a real-time ETW trace session requires administrator privileges, so this class
/// only ever runs inside the elevated helper process spawned by GameFpsService — never in the
/// main (unelevated) launcher.
/// </summary>
public sealed class DxgiPresentWatcher : IDisposable
{
    private static readonly Guid DxgiProviderGuid = new("CA11C036-0102-4A2D-A6AD-F03CFED5D3C9");
    private static readonly Guid D3D9ProviderGuid = new("783ACA0A-790E-4d7f-8451-AA850511C6B9");

    private readonly int            _targetPid;
    private readonly Action<double> _onFrame;
    private readonly string         _sessionName;

    private TraceEventSession? _session;
    private Thread?            _thread;
    private Timer?             _flushTimer;

    public DxgiPresentWatcher(int targetPid, Action<double> onFrame)
    {
        _targetPid   = targetPid;
        _onFrame     = onFrame;
        _sessionName = $"PlaytimeFpsMon_{targetPid}_{Environment.TickCount64}";
    }

    public void Start()
    {
        _session = new TraceEventSession(_sessionName) { StopOnDispose = true };
        _session.EnableProvider(DxgiProviderGuid);
        _session.EnableProvider(D3D9ProviderGuid);
        _session.Source.Dynamic.All += OnEvent;

        _thread = new Thread(() =>
        {
            try { _session.Source.Process(); }
            catch { }
        })
        { IsBackground = true };
        _thread.Start();

        // ETW real-time sessions only hand events to Process() when their internal buffers get
        // flushed, which by default happens on a multi-second timer (or whenever a buffer fills).
        // At typical game frame rates that meant long stretches with zero events delivered,
        // followed by a burst covering several seconds' worth of presents all at once — read as
        // "0 FPS" for a while, then a wildly inflated spike. Forcing a flush every 100ms keeps
        // delivery smooth enough that each 500ms reporting window sees a representative count.
        _flushTimer = new Timer(_ => { try { _session?.Flush(); } catch { } }, null, 100, 100);
    }

    private void OnEvent(TraceEvent data)
    {
        if (data.ProcessID != _targetPid) return;

        if (data.ProviderGuid == DxgiProviderGuid)
        {
            // DXGI presents are a Start/Stop pair per call — count the Start once per frame.
            if (data.TaskName == "Present" && data.OpcodeName == "Start")
                _onFrame(data.TimeStampRelativeMSec);
        }
        else if (data.ProviderGuid == D3D9ProviderGuid)
        {
            // Direct3D9's Present event is a single, unpaired event per frame.
            if (data.TaskName == "Present")
                _onFrame(data.TimeStampRelativeMSec);
        }
    }

    public void Dispose()
    {
        _flushTimer?.Dispose();
        _session?.Dispose();
        _thread?.Join(TimeSpan.FromSeconds(2));
    }
}
