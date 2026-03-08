# Playtime Speed Launcher

**A dedicated launcher for speedrunning *Poppy Playtime* — select any chapter, manage multiple game versions, download specific patches via SteamCMD, and launch instantly. Fully free, no strings attached.**

[![Version](https://img.shields.io/badge/Version-1.0.0-green)](https://github.com/TheKeProjects/Playtime-Speed-Launcher/releases/latest)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11-lightgrey)](https://github.com/TheKeProjects/Playtime-Speed-Launcher/releases/latest)
[![Free](https://img.shields.io/badge/Price-Free-brightgreen)](https://github.com/TheKeProjects/Playtime-Speed-Launcher/releases/latest)

---

## Features

### Chapter Selection
Browse all five Poppy Playtime chapters in a card-based UI. The currently selected chapter's title, install status, and detected build version are shown at a glance.

| Chapter | Title |
|---------|-------|
| 1 | A Tight Squeeze |
| 2 | Fly in a Web |
| 3 | Deep Sleep |
| 4 | The Dark Ride |
| 5 | Broken Things |

---

### Automatic Game Detection
On startup the launcher scans all fixed drives for Steam library paths (`steamapps\common`) and parses `libraryfolders.vdf` to locate every installed Poppy Playtime chapter automatically. No manual path configuration required.

---

### Version Management
Each chapter can have multiple game installations registered — both auto-detected and manually added. The **Versions** overlay lets you:

- Browse all registered installations for any chapter
- Switch between chapters using arrow navigation
- Set the active installation with one click
- Add a custom installation by browsing to any `.exe`
- Remove entries you no longer need
- Toggle speedrun **presets** to show/hide preset download rows

Your selections are saved in `installations.json` next to the launcher so they persist across sessions.

---

### Speedrun Presets & SteamCMD Downloads
Each chapter ships with curated presets that map to specific Steam depot manifests used by the speedrunning community:

| Chapter | Preset | Size |
|---------|--------|------|
| 1 | Any% <1.1 · NMG <1.1 · NMG/Any% 1.2 | ~5.79 GB |
| 2 | Patch 1.0 · 1.1 · 1.2 | ~8.40 GB |
| 3 | Patch 1.0 | ~36.5 GB |
| 4 | Patch 1.0 | ~7.25 GB |
| 5 | Patch 1 · Patch 2 | ~11.1 GB |

Clicking a preset downloads the exact game version using SteamCMD's `download_depot` command. The launcher:

1. Locates or downloads SteamCMD automatically (saved to `%LocalAppData%\SpeedrunLauncher\steamcmd`)
2. Copies your existing Steam login credentials so no password is needed
3. Runs the download in the background with a live log
4. Moves the downloaded files to the folder of your choice and registers the installation

---

### Auto-Updates
The launcher checks GitHub for new versions on startup. When an update is found it shows the changelog and file size, then downloads and installs the update in one click. GameBanana update checks are also supported when a tool ID is configured.

---

### Settings
- **Sound effects volume** — adjustable slider (0 – 100 %)
- **Language** — Spanish or English

---

## How to Use

### Launch a Chapter
1. Open the launcher — it will auto-detect your installed chapters.
2. Click the card for the chapter you want to play.
3. Press **Play** to launch the game.

### Switch to a Specific Version
1. Click the version badge next to the chapter title to open the **Versions** overlay.
2. Select the installation you want to use.
3. Close the overlay and press **Play**.

### Download a Speedrun Preset
1. Open the **Versions** overlay for the target chapter.
2. Click **Show presets** to reveal the preset rows.
3. Click the desired preset — if SteamCMD is not found it will be downloaded first.
4. Follow the on-screen prompts (Steam Guard code if requested).
5. Once finished, choose a destination folder. The new installation is added and selected automatically.

---

## Requirements

- Windows 10 / 11
- A Steam account that owns the Poppy Playtime chapters you want to download
- Internet connection (for SteamCMD downloads and update checks)

---

## Download

| Source | Link |
|--------|------|
| **GitHub (Recommended)** | [Latest Release](https://github.com/TheKeProjects/Playtime-Speed-Launcher/releases/latest) |

---

## AI Assistance Notice

Artificial intelligence was used during development to help with code implementation and error correction.

---

Made by **An Average Developer**

