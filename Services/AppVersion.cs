namespace SpeedrunLauncher.Services;

public static class AppVersion
{
    public const string CURRENT_VERSION = "1.0.0";

    // TODO: Update these when the GitHub repository is created
    public const string GITHUB_OWNER  = "TheKeProjects";
    public const string GITHUB_REPO   = "Playtime-Speed-Launcher";
    public const string GITHUB_BRANCH = "main";

    // TODO: Set to the actual GameBanana tool ID when published (0 = disabled)
    public const int GB_TOOL_ID = 0;

    public static string GetDisplayVersion()  => $"v{CURRENT_VERSION}";
    public static string GetGitHubRepoUrl()   => $"https://github.com/{GITHUB_OWNER}/{GITHUB_REPO}";
}
