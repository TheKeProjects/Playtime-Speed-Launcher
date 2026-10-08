using System.Windows.Media;
using SpeedrunLauncher.Services.App;

namespace SpeedrunLauncher.Seasonal;

/// <summary>Colors of the launcher's main screen for one season.</summary>
public sealed record SeasonalPalette(
    Color Background,      // window background (under the scenery)
    Color Bar,             // top status bar / bottom bar
    Color BarBorder,
    Color Accent,          // selected chapter card border, card buttons, version label
    Color PlayBackground,  // PLAY button
    Color PlayForeground,
    Color Title,           // selected chapter title
    Color Muted);          // chapter status / description text

/// <summary>
/// Tracks which season's visuals are shown. The season follows the icon theme setting
/// (<see cref="IconThemeSettings"/>); themes without their own visuals use the classic look.
/// The launcher is dark-only, so the seasonal art is always drawn in its dark variant.
/// </summary>
public sealed class SeasonalThemeService
{
    private static SeasonalThemeService? _instance;
    public static SeasonalThemeService Instance => _instance ??= new SeasonalThemeService();

    /// <summary>The season currently shown ("halloween", "summer", "christmas"), or "classic" when none.</summary>
    public string ActiveSeason { get; private set; } = ResolveSeason();

    /// <summary>Always true: the launcher has no light mode.</summary>
    public bool IsDarkTheme => true;

    /// <summary>Raised whenever <see cref="ActiveSeason"/> changes.</summary>
    public event EventHandler<string>? SeasonChanged;

    private SeasonalThemeService() { }

    /// <summary>Re-reads the icon theme setting, e.g. after the user picks another theme.</summary>
    public void RefreshSeason()
    {
        var season = ResolveSeason();
        if (season == ActiveSeason) return;
        ActiveSeason = season;
        SeasonChanged?.Invoke(this, season);
    }

    private static string ResolveSeason() => IconThemeSettings.Current.EffectiveTheme switch
    {
        "halloween" => "halloween",
        "summer"    => "summer",
        "christmas" => "christmas",
        _           => "classic",
    };

    public SeasonalPalette Palette => ActiveSeason switch
    {
        // Near-black purple with pumpkin orange accents.
        "halloween" => new(
            Color.FromRgb(14, 8, 22), Color.FromArgb(225, 16, 8, 26), Color.FromArgb(255, 70, 34, 100),
            Color.FromRgb(255, 140, 50),
            Color.FromRgb(255, 122, 26), Color.FromRgb(18, 10, 28),
            Color.FromRgb(195, 155, 255),
            Color.FromRgb(170, 140, 205)),
        // Tropical sunset: ocean blues, sunny yellow and coral.
        "summer" => new(
            Color.FromRgb(6, 24, 31), Color.FromArgb(225, 10, 22, 46), Color.FromArgb(255, 120, 60, 110),
            Color.FromRgb(255, 196, 70),
            Color.FromRgb(255, 182, 39), Color.FromRgb(30, 16, 40),
            Color.FromRgb(255, 220, 130),
            Color.FromRgb(255, 205, 190)),
        // Christmas night: dark pine, red button, gold accents.
        "christmas" => new(
            Color.FromRgb(10, 22, 15), Color.FromArgb(225, 6, 14, 26), Color.FromArgb(255, 40, 80, 70),
            Color.FromRgb(255, 201, 40),
            Color.FromRgb(214, 40, 40), Color.FromRgb(255, 255, 255),
            Color.FromRgb(255, 213, 79),
            Color.FromRgb(170, 200, 225)),
        // Classic launcher look.
        _ => new(
            Color.FromRgb(7, 12, 18), Color.FromRgb(8, 17, 26), Color.FromRgb(13, 32, 48),
            Color.FromRgb(0, 204, 170),
            Color.FromRgb(0, 204, 170), Color.FromRgb(6, 12, 18),
            Color.FromRgb(255, 170, 0),
            Color.FromRgb(42, 90, 122)),
    };
}
