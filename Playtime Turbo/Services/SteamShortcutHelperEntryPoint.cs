using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using SpeedrunLauncher;
using Loc = SpeedrunLauncher.Services.LocalizationService;

namespace SpeedrunLauncher.Services;

/// <summary>
/// Entry point for the Add-to-Steam helper process: <c>MainWindow</c> relaunches itself with
/// argv [<see cref="Arg"/>, exePath, iconSourcePath] and shuts down immediately, so this
/// instance — never launched by Steam — is the one that closes Steam, edits shortcuts.vdf,
/// and restarts Steam. That split matters because the running exe can itself be the
/// Steam-tracked "game" process (the older setup where the game's own exe is replaced by this
/// launcher): Steam force-kills the process it's tracking when it shuts down, which used to
/// kill the operation before <see cref="SteamShortcutService.AddOrUpdateShortcut"/> ever ran.
/// </summary>
public static class SteamShortcutHelperEntryPoint
{
    public const string Arg = "--steam-shortcut-helper";

    public static int Run(string[] args)
    {
        if (args.Length < 3) return 1;
        var exePath         = args[1];
        var iconSourcePath  = args[2];

        ResourceExtractor.Extract();
        Loc.LoadSaved();

        var owner = new Window
        {
            Width = 1, Height = 1,
            WindowStyle = WindowStyle.None,
            AllowsTransparency = true,
            Background = Brushes.Transparent,
            ShowInTaskbar = false,
            WindowStartupLocation = WindowStartupLocation.CenterScreen,
        };
        owner.Show();

        var wasRunning = SteamShortcutService.IsSteamRunning();
        var warningKey = wasRunning ? "add_to_steam_warning_running" : "add_to_steam_warning_not_running";

        var confirmed = WpfDialog.Show(owner, "ADD TO STEAM",
            MakeText(Loc.Get(warningKey), Color.FromArgb(200, 232, 160, 0)),
            primaryText: Loc.Get("add_to_steam_btn"),
            closeText: Loc.Get("cancel")) == WpfDialogResult.Primary;

        if (!confirmed)
        {
            owner.Close();
            return 0;
        }

        string message;
        bool success;
        try
        {
            var steamPath = SteamShortcutService.FindSteamPath()
                ?? throw new InvalidOperationException(Loc.Get("add_to_steam_error_no_steam"));

            if (wasRunning) SteamShortcutService.CloseSteam(steamPath);
            SteamShortcutService.AddOrUpdateShortcut(exePath, iconSourcePath);
            if (wasRunning) SteamShortcutService.StartSteam(steamPath);

            message = Loc.Get("add_to_steam_success");
            success = true;
        }
        catch (Exception ex)
        {
            message = string.Format(Loc.Get("add_to_steam_error_generic"), ex.Message);
            success = false;
        }

        WpfDialog.Show(owner, "ADD TO STEAM",
            MakeText(message, success ? Color.FromArgb(200, 0, 204, 170) : Color.FromArgb(200, 160, 180, 200)),
            closeText: "OK");

        owner.Close();
        return 0;
    }

    private static TextBlock MakeText(string text, Color color) => new()
    {
        Text         = text,
        FontFamily   = new FontFamily("Cascadia Code, Consolas, Courier New"),
        FontSize     = 12,
        Foreground   = new SolidColorBrush(color),
        TextWrapping = TextWrapping.Wrap,
        MaxWidth     = 360,
    };
}
