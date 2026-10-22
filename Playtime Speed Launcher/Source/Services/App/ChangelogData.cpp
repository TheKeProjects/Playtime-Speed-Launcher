// Launcher changelog. Add new versions at the TOP (newest first).
#include "Services/App/ChangelogData.h"
#include "Core/Text.h"

namespace SpeedrunLauncher::Services::Changelog {

const std::vector<Entry>& Entries() {
    static const std::vector<Entry> entries = {
        { L"4.1.5", L"22/10/2026", {
            L"## Additions",
            L"Multiplayer games now have the FPS overlay, Discord status, playtime on their card, key to click and the in-game easter eggs, like the chapters",
            L"Added a new easter egg",
            L"## Bug Fixes",
            L"Fixed online not working in the multiplayer games (Escape From Playtime) when launched from the launcher: they now start through Steam",
        } },
        { L"4.1.4", L"21/10/2026", {
            L"## Additions",
            L"Escape From Playtime is out: the launcher finds your Steam install and launches it",
            L"New Updates button next to Settings, with the Updates and Changelog pages side by side (it glows red when there's a new version)",
            L"Add to Steam is now a button next to the quit button, with a window that shows each step live",
            L"Installing or removing Load Manip and FullBright shows an animated loading screen and the result in the same popup",
            L"## Redesigns",
            L"New look for the bug and skip reports, with a checklist that ticks itself as you fill them in",
            L"New look for the mod popups, saves and checkpoints, LiveSplit, manual update, tutorials and notifications",
            L"Settings: sliding ON / OFF switches and animated tabs and options",
            L"## Seasonal Themes",
            L"Every menu and window now has the season's decorations and particles",
            L"## Bug Fixes",
            L"Fixed runners' pictures not showing in the leaderboards for many people",
        } },
        { L"4.1.3", L"20/10/2026", {
            L"## Additions",
            L"Chapter 4: new FULLBRIGHT button on the chapter card, installed like Chapters 1 and 5",
            L"Chapter 4: Load Manip now installs like Chapters 1 and 5, with its own UE4SS build and config file",
            L"Chapter 4: if the old Load Manip (LoadManipV2_P) is found in Content\\Paks, the launcher asks to delete it before installing Load Manip or FullBright",
            L"Added two new easter eggs, one of them a picture",
            L"## Redesigns",
            L"Settings \x2192 Load Manip: Chapter 4 now edits the Freeze / Slow / Normal loads keys and the FullBright keys straight in the mods' config files, like Chapters 1 and 5",
            L"## Other",
            L"Removed the old Chapter 4 key remap (the new Load Manip reads its keys from its config file)",
        } },
        { L"4.1.2", L"19/10/2026", {
            L"## Bug Fixes",
            L"Fixed the FPS counter and the controller overlay not showing up in OBS's Window Capture list",
        } },
        { L"4.1.1", L"18/10/2026", {
            L"## Additions",
            L"New loading screen: a picture from a random chapter and a progress bar while the launcher gets everything ready, so the main screen opens with all its pictures already loaded",
            L"New Chapters / Multiplayers tabs above the cards: Project Playtime can be launched from the launcher, and Escape From Playtime shows up locked until it comes out",
            L"Leaderboards: the podium shows each runner's speedrun.com profile picture",
            L"Leaderboards: pictures in a run's comment now show up under it (click one to open it full size)",
            L"## Redesigns",
            L"Epic Games: switching to Epic shows a searching screen with the Epic logo in a progress ring, the drive and folder being checked, and a success animation when the game is found",
            L"Epic Games: new \"not found\" window that shows which folder to select, with a button to search again",
            L"## Seasonal Themes",
            L"The SteamCMD and version install windows now follow the seasonal themes, with the season's decorations and particles",
            L"## Performance",
            L"The loading screen decodes several pictures at once, so the launcher opens sooner on slower PCs",
            L"The Epic Games search checks the top folders of every drive first and skips system links and folders, so most installs are found much sooner",
            L"Updates restart the launcher about 2 seconds sooner",
            L"The chapter and multiplayer cards no longer check files on disk every frame",
            L"Pictures from the internet (flags, video thumbnails, profile pictures) download and load in the background without stuttering, profile pictures are kept on your PC so they show up instantly the next time, and a picture that speedrun.com refuses is tried again",
            L"## Bug Fixes",
            L"Fixed automatic updates failing on some PCs: the updater now waits for the launcher to fully close and retries if the files are still in use",
            L"Fixed the launcher freezing while it searched your drives for the Epic Games install",
            L"Fixed the leaderboards always showing in Spanish, and the beginner tutorials, skip report, skips and glitches, Hand Mods menus and the loading screen always showing in English: they now follow the launcher's language",
        } },
        { L"4.1.0", L"18/10/2026", {
            L"## Additions",
            L"Leaderboards: a run's YouTube video now plays inside the launcher, like the skips and glitches tutorials (other sites open in the browser)",
            L"Leaderboards: click a runner to see the run's date, country, comment and links to the run and their profile",
            L"New hand mod preview: one big picture per hand, browse with the arrows or the arrow keys, and install it right there",
            L"The checkpoint menu can be used with the keyboard (arrows to pick, Enter to load), a double click loads a checkpoint, and it confirms when something was loaded or deleted",
            L"Downloading SteamCMD shows each step and a progress bar with the downloaded size, and can be cancelled",
            L"The beginner tutorials remember which videos you've watched and have a Next button to go to the following one",
            L"## Redesigns",
            L"Leaderboards: chapter and category tabs, a podium for the top 3 and the full top 10 with each runner's flag, name colour, gap to the record and how long ago the run was",
            L"Hand Mods: chapter and version cards, a step bar, screens sliding in and out, loading placeholders, a retry button and a new install screen with a progress ring",
            L"Hand Mods: the Individual Hands menu is now a grid of picture cards, and the Hand Conflict warning shows what goes in and which mods will be removed first",
            L"Checkpoint menu (hotkey): new look, with animations when it opens and closes and checkpoints sliding in one by one",
            L"Beginner tutorials: a 4-step path, cards that explain each video and a step list beside the video while watching",
            L"SteamCMD install window: explains what SteamCMD is and both ways to get it",
            L"## Seasonal Themes",
            L"The checkpoint, skips and glitches, skip report, leaderboards, beginner tutorials and Hand Mods menus now follow the seasonal themes (Halloween, Summer and Christmas)",
            L"Those menus get the season's decorations on their top edge and their own particles: snow, embers with a bat or bubbles and glints",
            L"## Performance",
            L"Menus that play videos no longer redraw at your monitor's full refresh rate while a video is loaded",
            L"The new menus only keep redrawing while something is actually moving, and their decorative animations settle down after a few seconds",
            L"The Hand Mods menu no longer checks the mod files on disk every frame, and hand pictures load in the background",
            L"## Bug Fixes",
            L"Fixed a tutorial video staying on screen after going back to the list",
            L"Fixed the SteamCMD download status always showing in Spanish",
        } },
        { L"4.0.0", L"16/10/2026", {
            L"## Additions",
            L"The launcher has been rebuilt from scratch in C++: it opens faster and uses far fewer resources",
            L"New Store to browse and install old versions of every chapter",
            L"Installing a version now guides you step by step, including the Steam login and the phone confirmation",
            L"Redesigned the versions menu: switch chapters at the top and drag versions to reorder them",
            L"Removing a version can now also delete its folder (it goes to the Recycle Bin)",
            L"New Key to Click option in Settings > Controls: turn any key into a mouse click, with a shortcut to turn it on and off",
            L"Load Manip keys can't be changed while that chapter is open, and a button lets you close the game",
            L"Redesigned the version history, tutorials, bug report and skip report menus",
            L"Every menu has new animations: a glow when it opens and its content sliding in one by one",
            L"Seasonal scenery is more alive, and its animations pause while a game is running",
            L"## Bug Fixes",
            L"Fixed the launcher stuttering every few seconds while idle",
            L"Fixed menus lagging and using a lot of CPU",
            L"Fixed some popups showing a black area",
            L"Fixed saved Steam accounts not showing up when installing a version",
            L"Fixed keyboard lag in every app while the Cores option was enabled",
            L"Fixed the Cores hotkeys sometimes stopping working or not affecting the game",
            L"Fixed the FPS counter not asking for administrator permission",
            L"Fixed the Cores option and the FPS counter not detecting Chapter 2 and Chapter 3",
            L"Fixed the version history taking a long time to open",
            L"## Other",
            L"Much lower CPU usage, especially while a game is running",
        } },
        { L"3.0.1", L"11/10/2026", {
            L"## Additions",
            L"Added two new easter eggs",
            L"Easter eggs can now also be activated in-game",
            L"## Bug Fixes",
            L"Fixed easter eggs not working while custom keys or the Cores hotkeys were enabled",
        } },
        { L"3.0.0", L"08/10/2026", {
            L"## Additions",
            L"Added seasonal themes (Halloween, Summer and Christmas)",
            L"Added a Chapter 2 checkpoint save with a fixed train code (4312)",
            L"## Other",
            L"Renamed the window mode options to Fullscreen and Windowed and removed the note below them",
        } },
        { L"2.3.3", L"12/09/2026", {
            L"## Bug Fixes",
            L"Updated Fullbright and LoadManip for Chapter 1 and 5",
        } },
        { L"2.3.2", L"11/09/2026", {
            L"## Additions",
            L"Added an AZERTY option for the keyboard overlay",
            L"## Other",
            L"Moved per-chapter hours tracking to the cloud",
        } },
        { L"2.3.1", L"29/08/2026", {
            L"## Bug Fixes",
            L"Fixed a bug with the cores option (Reported by @nickk893)",
        } },
        { L"2.3.0", L"29/08/2026", {
            L"## Additions",
            L"Added a New Game+ save option for Chapter 5",
            L"## Bug Fixes",
            L"Fixed the Cores option not detecting the game",
            L"Fixed new Hand Mod submissions failing when the files were too large for Discord to accept, files are now zipped automatically",
            L"## Other",
            L"Removed the Auto Splitter option, since splits were only ever available for Chapters 1 and 2",
            L"Reorganized the launcher's internal file structure by feature area",
            L"Changed how checkpoint saves are stored to shrink the launcher's install size, and clean them up automatically when the launcher closes",
        } },
        { L"2.2.4", L"20/08/2026", {
            L"## Bug Fixes",
            L"Fixed Load Manip for Chapter 5",
        } },
        { L"2.2.3", L"19/08/2026", {
            L"## Additions",
            L"Added more EasterEggs",
        } },
        { L"2.2.2", L"17/08/2026", {
            L"## Bug Fixes",
            L"Fixed more performance issues",
        } },
        { L"2.2.1", L"15/08/2026", {
            L"## Additions",
            L"Added new skips for chapter 5",
            L"## Bug Fixes",
            L"Fixed Skips and Glitches tutorial videos failing to load or getting stuck when switching between videos (Reported by @lumpydesk_yt)",
        } },
        { L"2.2.0", L"14/08/2026", {
            L"## Additions",
            L"Added a Manual Update option that downloads the latest version and replaces the launcher automatically if the update fails",
            L"Added a Priority for 0 cores setting in the Cores tab, letting you choose between High (default) and Low priority",
        } },
        { L"2.1.3", L"13/08/2026", {
            L"## Bug Fixes",
            L"Fixed Cores priority not re-applying after switching modes several times, previously requiring a game restart to fix",
            L"Fixed Cores priority not changing for Chapter 2 even though the core count changed correctly",
            L"## Other",
            L"Renamed the Cores tab's 'Freeze/Slower/Normal loads' options to '0 cores', '1 core', and 'All cores'",
            L"Changed the default Cores hotkeys to B (0 cores), N (1 core), and M (all cores)",
            L"Removed the 'not allowed in official runs' warning from the Cores tab",
            L"Moved the Cores tab to appear right after Load Manip in settings",
        } },
        { L"2.1.2", L"09/08/2026", {
            L"## Bug Fixes",
            L"Fixed Chapter 5 hand mods failing to install after being moved to Git LFS",
        } },
        { L"2.1.1", L"08/08/2026", {
            L"## Additions",
            L"Added a live online users counter",
            L"## Bug Fixes",
            L"Fixed the chapter cards getting cut off on screens with different resolutions",
        } },
        { L"2.1.0", L"05/08/2026", {
            L"## Additions",
            L"Added the option to install the hand mods individually (Still working on the mods)",
            L"Discord is now required for submitting new Hand Mods",
            L"## Bug Fixes",
            L"Fixed the FPS counter not working in Chapter 4",
            L"Fixed hand mod conflict detection not accounting for individually installed hands",
        } },
        { L"2.0.11", L"04/08/2026", {
            L"## Additions",
            L"Added Load Manip mod for Chapter 5 (Thanks to @AdrianPG77)",
            L"Added Fullbright mod for Chapter 5 (Thanks to @AdrianPG77)",
            L"Added an option to add the launcher to Steam as an independent game",
            L"Added a Hand Mods system to browse and install community hand-skin mods per chapter",
            L"Added the option to submit new Hand Mods for review",
            L"Added a Chapter 6 card (Coming Soon)",
            L"## Bug Fixes",
            L"Fixed Load Manip mod not working with the Thumb Mouse Buttons",
            L"Fixed Fullbright mod not working with the Thumb Mouse Buttons",
            L"Fixed more bugs with the FPS counter",
            L"Fixed the Steam profile not showing up inside the launcher",
            L"Fixed the chapter description text shifting the chapter cards up or down depending on its length",
            L"## Other",
            L"Moved Load Manip settings into their own tab, with a chapter selector for Chapter 1, 4, and 5",
        } },
        { L"2.0.10", L"25/07/2026", {
            L"Added a timer that tracks in-game hours per chapter",
            L"Fixed several bugs with the FPS counter",
            L"Improved detection of Load Manip for Chapter 1",
            L"Added the ability to reorder installed versions",
            L"Added Fullbright mod for chapter 1 (Thanks to @AdrianPG77)",
        } },
        { L"2.0.9", L"22/07/2026", {
            L"Fixed position of the FPS overlay",
            L"Added the option to minimize the launcher",
            L"Added the leaderboards for every chapter",
        } },
        { L"2.0.8", L"18/07/2026", {
            L"Fixed position of the FPS overlay",
            L"Fixed a bug that causes the game stop if you click the keyboard overlay",
        } },
        { L"2.0.7", L"18/07/2026", {
            L"Added more icons (Thanks to @ᴢᴀᴇᴇ)",
            L"Added an FPS Overlay",
            L"Fixed some skips",
            L"Added a key remapping system for Load Manip",
        } },
        { L"2.0.6", L"04/07/2026", {
            L"Fixed some visual bugs with the controllers overlay",
            L"Updated app icon (Thanks to @ᴢᴀᴇᴇ)",
            L"Added a system to send new skips to the developers",
            L"Added new skips to Chapter 1",
            L"Added a setting to select the icon app (Taskbar and Discord)",
        } },
        { L"2.0.5", L"01/07/2026", {
            L"Added controller overlay",
            L"Added keyboard and mouse overlay",
            L"Added Load Manip Tool for chapter 1 and 4 (Thanks to @AdrianPG77)",
        } },
        { L"2.0.4", L"26/06/2026", {
            L"Added UE4SS for all the chapters",
        } },
        { L"2.0.3", L"24/06/2026", {
            L"Reorganized settings into separate tabs (Steam, Controller, Discord, etc.)",
            L"Added CPU core and priority management",
        } },
        { L"2.0.2", L"21/06/2026", {
            L"Updated more skips (Thanks to @Edwin and @Technight)",
        } },
        { L"1.3.1", L"20/06/2026", {
            L"Updated skips across all categories (Thanks to @Edwin for all chapters and @Technight for the new patch Chapter 4 skips)",
            L"Improved and updated Discord Rich Presence",
            L"Redesigned the application icon",
        } },
        { L"1.3.0", L"14/06/2026", {
            L"Added a key remapping system for F11",
            L"Fixed audio system issues (again)",
        } },
        { L"1.2.11", L"10/06/2026", {
            L"Added a tutorial on how to automatically connect LiveSplit to Discord",
            L"Added auto splitters for Chapters 1 and 2",
        } },
        { L"1.2.10", L"07/06/2026", {
            L"Added full controller support across all chapters",
            L"Discord is now required for submitting bug reports and feedback",
        } },
        { L"1.2.9", L"05/06/2026", {
            L"Fixed issues with the audio system",
            L"Improved and fixed various elements of the Strats Menu overlay",
            L"Fixed several bugs affecting the video player controls",
        } },
        { L"1.2.8", L"04/06/2026", {
            L"Improved the beginner tutorial",
            L"Updated the application icon",
        } },
        { L"1.2.7", L"03/06/2026", {
            L"Updated the Skips Menu; videos no longer need to be downloaded",
            L"Fixed category assignments for several strats",
            L"Added the ability to search for strats by author",
        } },
        { L"1.2.6", L"02/06/2026", {
            L"Added Glitches & Skips Tutorials for Chapters 3, 4, and 5",
            L"Updated the Tutorials Menu overlay",
            L"Discord is now required for submitting error reports and feedback",
            L"Removed the 'Delete Saves' button from Chapters 1, 2, and 3",
            L"Added a beginner tutorial to help users learn how to use the launcher",
        } },
        { L"1.2.5", L"31/05/2026", {
            L"Added a setting to customize the Glitches and Skips Tutorials Menu keybind",
            L"Added an in-game popup displaying the keybind used to open the Tutorials Menu",
            L"Added a Version History menu to view the complete changelog from all releases",
        } },
        { L"1.2.4", L"29/05/2026", {
            L"Added in-game glitch and skip tutorials (currently available for Chapters 1 and 2, default key: F9)",
            L"Added Discord Rich Presence support while watching tutorials",
            L"Added Steam profile user and avatar display to the main menu",
            L"Added an option to save the user and password for automatic depot downloads (still require Steam Guard)",
            L"Fixed the LOAD button in the in-game checkpoint loader for Chapters 1, 2, and 3",
        } },
        { L"1.2.3", L"27/05/2026", {
            L"Added Epic Games support",
            L"Added LiveSplit timer integration to Discord Rich Presence",
            L"Added settings to manage Discord Rich Presence",
            L"Added an in-game checkpoint loader for Chapters 4 and 5 (load and delete all saves)",
        } },
        { L"1.2.2", L"25/05/2026", {
            L"Improved Discord Rich Presence integration",
            L"Added checkpoint selection status support to Discord Rich Presence",
        } },
        { L"1.2.1", L"24/05/2026", {
            L"Added Discord Rich Presence support",
            L"Fixed the installation of Chapter 1 version: NMG_Any% 1.2",
        } },
        { L"1.2.0", L"23/05/2026", {
            L"Added an integrated error reporting system to the launcher (bottom-left corner)",
            L"Fixed the camera when opening the checkpoint loader",
            L"Added a popup in Poppy Playtime Chapters 1–3 showing the keybind combination to open the checkpoint loader",
            L"Added a setting to customize the checkpoint loader keybind",
        } },
        { L"1.1.1", L"19/05/2026", {
            L"Fixed checkpoint 27 from Chapter 2",
        } },
        { L"1.1.0", L"24/04/2026", {
            L"Added checkpoint system for Chapters 1, 2, and 3",
            L"Added an in-game checkpoint loader (Ctrl + Alt + Enter)",
            L"Added all checkpoints to Chapters 4 and 5",
            L"Updated the image for Chapter 5",
        } },
        { L"1.0.5", L"16/04/2026", {
            L"Added an option to open the launcher from steam",
        } },
        { L"1.0.4", L"14/04/2026", {
            L"Fixed the name for the chapter 4",
        } },
        { L"1.0.3", L"14/04/2026", {
            L"Added patch 1.3 for Poppy Playtime 1",
            L"Now the tool is approved by Speedrun.com",
        } },
        { L"1.0.2", L"23/03/2026", {
            L"Added a manual mode without Steam Guard",
            L"Added a Installer of Live Split",
        } },
        { L"1.0.1", L"13/03/2026", {
            L"Added better instructions when sending the steam guard verification",
        } },
        { L"1.0.0", L"08/03/2026", {
            L"Initial release",
        } },
    };
    return entries;
}

namespace {

// "4.1.1", "v4.1.1" or "4.1.1 | 18/10/2026": digits and dots (at least one dot) before any " | "
bool ParseVersionLine(const std::wstring& line, std::wstring& version, std::wstring& date) {
    const size_t bar = line.find(L'|');
    std::wstring head = Text::Trim(line.substr(0, bar));
    if (!head.empty() && (head[0] == L'v' || head[0] == L'V')) head.erase(0, 1);
    if (head.empty() || head.find(L'.') == std::wstring::npos) return false;
    for (wchar_t c : head)
        if (!((c >= L'0' && c <= L'9') || c == L'.')) return false;
    version = head;
    date = bar == std::wstring::npos ? L"" : Text::Trim(line.substr(bar + 1));
    return true;
}

} // namespace

std::vector<Entry> Parse(const std::wstring& text, const std::wstring& fallbackVersion) {
    std::vector<Entry> entries;
    for (const auto& raw : Text::SplitLines(text)) {
        const std::wstring line = Text::Trim(raw);
        if (line.empty()) continue;
        std::wstring version, date;
        if (ParseVersionLine(line, version, date)) {
            entries.push_back({ version, date, {} });
            continue;
        }
        if (entries.empty()) entries.push_back({ fallbackVersion, L"", {} });
        std::wstring change = line;
        if (Text::StartsWith(change, L"- ") || Text::StartsWith(change, L"* ")) change = Text::Trim(change.substr(2));
        entries.back().changes.push_back(change);
    }
    return entries;
}

} // namespace SpeedrunLauncher::Services::Changelog
