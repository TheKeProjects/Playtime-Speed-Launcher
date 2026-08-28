namespace SpeedrunLauncher.Services.App;

public static class AppVersion
{
    public const string CURRENT_VERSION = "2.3.0";

    // TODO: Update these when the GitHub repository is created
    public const string GITHUB_OWNER  = "TheKeProjects";
    public const string GITHUB_REPO   = "Playtime-Speed-Launcher";
    public const string GITHUB_BRANCH = "main";

    // TODO: Set to the actual GameBanana tool ID when published (0 = disabled)
    public static readonly int GB_TOOL_ID = 0;

    // Controlled by <LgbtqMode> in the .csproj
#if LGBTQ_MODE
    public const bool LGBTQ_MODE = true;
#else
    public const bool LGBTQ_MODE = false;
#endif

    // Controlled by <SummerMode> in the .csproj
#if SUMMER_MODE
    public const bool SUMMER_MODE = true;
#else
    public const bool SUMMER_MODE = false;
#endif

    // Controlled by <HalloweenMode> in the .csproj
#if HALLOWEEN_MODE
    public const bool HALLOWEEN_MODE = true;
#else
    public const bool HALLOWEEN_MODE = false;
#endif

    // Controlled by <ChristmasMode> in the .csproj
#if CHRISTMAS_MODE
    public const bool CHRISTMAS_MODE = true;
#else
    public const bool CHRISTMAS_MODE = false;
#endif

    public static string GetDisplayVersion()  => $"v{CURRENT_VERSION}";
    public static string GetGitHubRepoUrl()   => $"https://github.com/{GITHUB_OWNER}/{GITHUB_REPO}";

    /// <summary>The icon theme baked into the .csproj at build time (used when the user's icon theme setting is "default").</summary>
    public static string BuildDefaultTheme => SUMMER_MODE ? "summer" : HALLOWEEN_MODE ? "halloween" : CHRISTMAS_MODE ? "christmas" : LGBTQ_MODE ? "lgbtq" : "default";

    public static string LauncherImageKey => IconThemeSettings.Current.DiscordImageKey;
}
