using System.Net.Sockets;
using System.Text;

namespace SpeedrunLauncher.Services.LiveSplit;

/// <summary>
/// Connects to LiveSplit's built-in Server component (Control → Start Server, default port 16834).
/// All reads/writes use a short timeout so a stalled LiveSplit never blocks the UI.
/// </summary>
public sealed class LiveSplitServerClient : IDisposable
{
    private const string Host        = "127.0.0.1";
    public  const int    DefaultPort = 16834;
    private const int    TimeoutMs   = 500;

    private TcpClient?    _tcp;
    private StreamWriter? _writer;
    private StreamReader? _reader;

    public bool IsConnected => _tcp?.Connected == true;

    public bool TryConnect(int port = DefaultPort)
    {
        Disconnect();
        try
        {
            _tcp = new TcpClient { ReceiveTimeout = TimeoutMs, SendTimeout = TimeoutMs };
            _tcp.Connect(Host, port);
            var stream = _tcp.GetStream();
            _writer = new StreamWriter(stream, Encoding.UTF8) { AutoFlush = true };
            _reader = new StreamReader(stream, Encoding.UTF8);
            return true;
        }
        catch { Disconnect(); return false; }
    }

    /// <summary>Returns the current timer phase: NotRunning | Running | Paused | Ended</summary>
    public string? GetTimerPhase() => Send("getcurrenttimerphase");

    /// <summary>Returns the real-time value as a string, e.g. "0:01:23.4500000"</summary>
    public string? GetCurrentTime() => Send("getcurrenttime");

    private string? Send(string command)
    {
        try
        {
            _writer?.Write(command + "\r\n");
            return _reader?.ReadLine();
        }
        catch { Disconnect(); return null; }
    }

    public void Disconnect()
    {
        try { _writer?.Dispose(); } catch { } _writer = null;
        try { _reader?.Dispose(); } catch { } _reader = null;
        try { _tcp?.Close();     } catch { } _tcp    = null;
    }

    public void Dispose() => Disconnect();
}
