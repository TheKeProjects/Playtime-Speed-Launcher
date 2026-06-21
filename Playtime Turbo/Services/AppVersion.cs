namespace SpeedrunLauncher.Services;

public static class AppVersion
{
    public const string CURRENT_VERSION = "2.0.1";

    // TODO: Update these when the GitHub repository is created
    public const string GITHUB_OWNER  = "TheKeProjects";
    public const string GITHUB_REPO   = "Playtime-Turbo";
    public const string GITHUB_REPO_LEGACY = "Playtime-Speed-Launcher";
    public const string GITHUB_BRANCH = "main";

    // TODO: Set to the actual GameBanana tool ID when published (0 = disabled)
    public static readonly int GB_TOOL_ID = 0;

    // Controlled by <LgbtqMode> in the .csproj
#if LGBTQ_MODE
    public const bool LGBTQ_MODE = true;
#else
    public const bool LGBTQ_MODE = false;
#endif

    public static string GetDisplayVersion()  => $"v{CURRENT_VERSION}";
    public static string GetGitHubRepoUrl()   => $"https://github.com/{GITHUB_OWNER}/{GITHUB_REPO}";

    public static string LauncherImageKey => LGBTQ_MODE ? "launcher_lgbtq" : "launcher";
}
