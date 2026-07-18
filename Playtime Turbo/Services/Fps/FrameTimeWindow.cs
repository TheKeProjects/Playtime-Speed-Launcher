namespace SpeedrunLauncher.Services.Fps;

/// <summary>
/// Rolling window of ETW-timestamped frame arrivals (milliseconds since session start).
/// FPS is derived from the actual span between the oldest and newest frame still in the
/// window rather than from a fixed, non-overlapping sampling bucket — that's what avoids the
/// bucket-edge quantization jitter a naive "count per tick" approach produces, while a short
/// window keeps it responsive to a real change in the game's frame rate.
/// </summary>
public sealed class FrameTimeWindow(double windowMs)
{
    private readonly object _lock = new();
    private readonly Queue<double> _timestampsMs = new();
    private double _newestMs;

    public void AddFrame(double timestampMs)
    {
        lock (_lock)
        {
            _timestampsMs.Enqueue(timestampMs);
            _newestMs = timestampMs;
            while (_timestampsMs.Count > 0 && _newestMs - _timestampsMs.Peek() > windowMs)
                _timestampsMs.Dequeue();
        }
    }

    /// <summary>Average FPS over the frames currently in the window, or 0 if there aren't enough yet.</summary>
    public double CurrentFps()
    {
        lock (_lock)
        {
            if (_timestampsMs.Count < 2) return 0;
            var spanSec = (_newestMs - _timestampsMs.Peek()) / 1000.0;
            return spanSec > 0 ? (_timestampsMs.Count - 1) / spanSec : 0;
        }
    }
}
