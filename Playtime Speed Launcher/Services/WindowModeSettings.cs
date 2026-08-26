using System.Text.Json;

namespace SpeedrunLauncher.Services;

/// <summary>
/// User-selectable main window mode. Both modes keep the window maximized and
/// borderless, covering the full screen; "windowed" additionally shows a
/// custom minimize button since the borderless window has no native chrome.
/// </summary>
public class WindowModeSettings
{
    public string Mode { get; set; } = "actual"; // "actual" | "windowed"

    private static readonly string FilePath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "SpeedrunLauncher", "window_mode_settings.json");

    private static WindowModeSettings? _current;
    public static WindowModeSettings Current => _current ??= Load();

    public static WindowModeSettings Load()
    {
        try
        {
            if (File.Exists(FilePath))
                return _current = JsonSerializer.Deserialize<WindowModeSettings>(File.ReadAllText(FilePath)) ?? new();
        }
        catch { }
        return _current = new();
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
