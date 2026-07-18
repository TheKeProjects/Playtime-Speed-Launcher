namespace SpeedrunLauncher.Services;

public record ChangelogEntry(string Version, string Date, string[] Changes);

public static class ChangelogData
{
    // Add new versions at the TOP of this array (newest first).
    public static readonly ChangelogEntry[] Entries =
    [
        new("2.0.7", "18/07/2026",
        [
            "Added more icons (Thanks to @ᴢᴀᴇᴇ)",
            "Added an FPS Overlay",
            "Fixed some skips",
            "Added a key remapping system for Load Manip",
        ]),
        new("2.0.6", "04/07/2026",
        [
            "Fixed some visual bugs with the controllers overlay",
            "Updated app icon (Thanks to @ᴢᴀᴇᴇ)",
            "Added a system to send new skips to the developers",
            "Added new skips to Chapter 1",
            "Added a setting to select the icon app (Taskbar and Discord)",
        ]),
        new("2.0.5", "01/07/2026",
        [
            "Added controller overlay",
            "Added keyboard and mouse overlay",
            "Added Load Manip Tool for chapter 1 and 4 (Thanks to @AdrianPG77)",
        ]),
        new("2.0.4", "26/06/2026",
        [
            "Added UE4SS for all the chapters",
        ]),
        new("2.0.3", "24/06/2026",
        [
            "Reorganized settings into separate tabs (Steam, Controller, Discord, etc.)",
            "Added CPU core and priority management",
        ]),
        new("2.0.2", "21/06/2026",
        [
            "Updated more skips (Thanks to @Edwin and @Technight)",
        ]),
        new("1.3.1", "20/06/2026",
        [
            "Updated skips across all categories (Thanks to @Edwin for all chapters and @Technight for the new patch Chapter 4 skips)",
            "Improved and updated Discord Rich Presence",
            "Redesigned the application icon",
        ]),
        new("1.3.0", "14/06/2026",
        [
            "Added a key remapping system for F11",
            "Fixed audio system issues (again)",
        ]),
        new("1.2.11", "10/06/2026",
        [
            "Added a tutorial on how to automatically connect LiveSplit to Discord", 
            "Added auto splitters for Chapters 1 and 2",
        ]),
        new("1.2.10", "07/06/2026",
        [
            "Added full controller support across all chapters",
            "Discord is now required for submitting bug reports and feedback",
        ]),
        new("1.2.9", "05/06/2026",
        [
            "Fixed issues with the audio system",
            "Improved and fixed various elements of the Strats Menu overlay",
            "Fixed several bugs affecting the video player controls",
        ]),
        new("1.2.8", "04/06/2026",
        [
            "Improved the beginner tutorial",
            "Updated the application icon",
        ]),
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
