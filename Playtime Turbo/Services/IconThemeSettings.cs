using System.Text.Json;

namespace SpeedrunLauncher.Services;

/// <summary>
/// User-selectable launcher icon / Discord Rich Presence theme.
/// "default" defers to whichever mode is baked into the .csproj at build time
/// (see AppVersion.BuildDefaultTheme); any other value overrides it at runtime.
/// </summary>
public class IconThemeSettings
{
    public string Theme { get; set; } = "default"; // "default" | "classic" | "lgbtq" | "summer" | "halloween"

    private static readonly string FilePath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "SpeedrunLauncher", "icon_theme_settings.json");

    private static IconThemeSettings? _current;
    public static IconThemeSettings Current => _current ??= Load();

    public static IconThemeSettings Load()
    {
        try
        {
            if (File.Exists(FilePath))
                return _current = JsonSerializer.Deserialize<IconThemeSettings>(File.ReadAllText(FilePath)) ?? new();
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

    public string EffectiveTheme => Theme == "default" ? AppVersion.BuildDefaultTheme : Theme;

    public string DiscordImageKey => EffectiveTheme switch
    {
        "lgbtq"     => "launcher_lgbtq",
        "summer"    => "launcher_summer",
        "halloween" => "launcher_halloween",
        _           => "launcher", // "classic" and unrecognized values use the plain icon
    };

    /// <summary>Embedded resource logical name for the window icon, or null to keep the executable's built-in icon (only true for "default").</summary>
    public string? EmbeddedIconResourceName => EffectiveTheme switch
    {
        "classic"   => "default_icon",
        "lgbtq"     => "lgbtq_icon",
        "summer"    => "summer_icon",
        "halloween" => "halloween_icon",
        _           => null,
    };
}
