namespace SpeedrunLauncher.Services;

public record ChangelogEntry(string Version, string Date, string[] Changes);

public static class ChangelogData
{
    // Add new versions at the TOP of this array (newest first).
    public static readonly ChangelogEntry[] Entries =
    [
        new("1.2.7", "03/06/2026",
        [
            "Updated the Skips Menu; videos no longer need to be downloaded",
            "Fixed category assignments for several strats",
            "Added the ability to search for strats by author",
        ]),
        new("1.2.6", "02/06/2026",
        [
            "Added Glitches & Skips Tutorials for Chapters 3, 4, and 5",
            "Updated the Tutorials Menu overlay",
            "Discord is now required for submitting error reports and feedback",
            "Removed the 'Delete Saves' button from Chapters 1, 2, and 3",
            "Added a beginner tutorial to help users learn how to use the launcher",
        ]),
        new("1.2.5", "31/05/2026",
        [
            "Added a setting to customize the Glitches and Skips Tutorials Menu keybind",
            "Added an in-game popup displaying the keybind used to open the Tutorials Menu",
            "Added a Version History menu to view the complete changelog from all releases",
        ]),
        new("1.2.4", "29/05/2026",
        [
            "Added in-game glitch and skip tutorials (currently available for Chapters 1 and 2, default key: F9)",
            "Added Discord Rich Presence support while watching tutorials",
            "Added Steam profile user and avatar display to the main menu",
            "Added an option to save the user and password for automatic depot downloads (still require Steam Guard)",
            "Fixed the LOAD button in the in-game checkpoint loader for Chapters 1, 2, and 3",
        ]),
        new("1.2.3", "27/05/2026",
        [
            "Added Epic Games support",
            "Added LiveSplit timer integration to Discord Rich Presence",
            "Added settings to manage Discord Rich Presence",
            "Added an in-game checkpoint loader for Chapters 4 and 5 (load and delete all saves)",
        ]),
        new("1.2.2", "25/05/2026",
        [
            "Improved Discord Rich Presence integration",
            "Added checkpoint selection status support to Discord Rich Presence",
        ]),
        new("1.2.1", "24/05/2026",
        [
            "Added Discord Rich Presence support",
            "Fixed the installation of Chapter 1 version: NMG_Any% 1.2",
        ]),
        new("1.2.0", "23/05/2026",
        [
            "Added an integrated error reporting system to the launcher (bottom-left corner)",
            "Fixed the camera when opening the checkpoint loader",
            "Added a popup in Poppy Playtime Chapters 1–3 showing the keybind combination to open the checkpoint loader",
            "Added a setting to customize the checkpoint loader keybind",
        ]),
        new("1.1.1", "19/05/2026",
        [
            "Fixed checkpoint 27 from Chapter 2",
        ]),
        new("1.1.0", "24/04/2026",
        [
            "Added checkpoint system for Chapters 1, 2, and 3",
            "Added an in-game checkpoint loader (Ctrl + Alt + Enter)",
            "Added all checkpoints to Chapters 4 and 5",
            "Updated the image for Chapter 5",
        ]),
        new("1.0.5", "16/04/2026",
        [
            "Added an option to open the launcher from steam",
        ]),
        new("1.0.4", "14/04/2026",
        [
            "Fixed the name for the chapter 4",
        ]),
        new("1.0.3", "14/04/2026",
        [
            "Added patch 1.3 for Poppy Playtime 1",
            "Now the tool is approved by Speedrun.com",
        ]),
        new("1.0.2", "23/03/2026",
        [
            "Added a manual mode without Steam Guard",
            "Added a Installer of Live Split",
        ]),
        new("1.0.1", "13/03/2026",
        [
            "Added better instructions when sending the steam guard verification",
        ]),
        new("1.0.0", "08/03/2026",
        [
            "Initial release",
        ]),
    ];
}
