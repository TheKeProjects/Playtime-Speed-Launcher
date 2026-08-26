using System.Text.Json;

namespace SpeedrunLauncher.Services.OnlineUsers;

/// <summary>
/// Per-install identifier used by <see cref="OnlineUsersService"/> so each install
/// upserts the same presence row instead of creating a new one every heartbeat.
/// </summary>
public class HeartbeatSettings
{
    public Guid InstallId { get; set; } = Guid.NewGuid();
    public bool ShowCounter { get; set; } = true;

    private static readonly string FilePath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "SpeedrunLauncher", "heartbeat_settings.json");

    private static HeartbeatSettings? _current;
    public static HeartbeatSettings Current => _current ??= Load();

    public static HeartbeatSettings Load()
    {
        try
        {
            if (File.Exists(FilePath))
                return _current = JsonSerializer.Deserialize<HeartbeatSettings>(File.ReadAllText(FilePath)) ?? new();
        }
        catch { }

        var created = new HeartbeatSettings();
        created.Save();
        return _current = created;
    }

    public void Save()
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);
            File.WriteAllText(FilePath, JsonSerializer.Serialize(this));
        }
        catch { }
        _current = this;
    }
}
