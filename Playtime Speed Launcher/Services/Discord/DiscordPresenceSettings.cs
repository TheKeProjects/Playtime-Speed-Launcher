using System.Text.Json;

namespace SpeedrunLauncher.Services.Discord;

public class DiscordPresenceSettings
{
    public bool ShowActivity  { get; set; } = true;
    public bool ShowVersion   { get; set; } = true;
    public bool ShowLiveSplit { get; set; } = false;

    private static readonly string FilePath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "SpeedrunLauncher", "discord_settings.json");

    public static DiscordPresenceSettings Load()
    {
        try
        {
            if (File.Exists(FilePath))
                return JsonSerializer.Deserialize<DiscordPresenceSettings>(File.ReadAllText(FilePath)) ?? new();
        }
        catch { }
        return new();
    }

    public void Save()
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);
            File.WriteAllText(FilePath, JsonSerializer.Serialize(this));
        }
        catch { }
    }
}
