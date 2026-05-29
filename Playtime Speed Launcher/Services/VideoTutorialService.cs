using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Net.Http;
using System.Text.RegularExpressions;

namespace SpeedrunLauncher.Services;

public sealed class TutorialVideo
{
    public required string Id          { get; init; }
    public required string Title       { get; init; }
    public          string Description { get; init; } = "";
    public required string Url         { get; init; }
    public          string Category    { get; init; } = "General";
    public          string Chapter     { get; set;  } = "";   // assigned by InChapter()
    public          string LocalPath   { get; set;  } = "";
    public          string SavedPath   { get; set;  } = "";
    public bool IsSaved      => !string.IsNullOrEmpty(SavedPath)  && File.Exists(SavedPath);
    public bool IsDownloaded => IsSaved || (!string.IsNullOrEmpty(LocalPath) && File.Exists(LocalPath));
    public string PlayablePath => IsSaved ? SavedPath : LocalPath;
}

public static class VideoTutorialService
{
    // Set true  → list shows only chapter headers; category/run-category filters are hidden.
    // Set false → full grouping with category headers and all filter pills (default).
    public static bool FlatList = true;

    // ─────────────────────────────────────────────────────────────────────────
    // TUTORIAL VIDEO LIST
    //
    // To add a new chapter : copy one of the ..InChapter("Chapter X", ...) blocks.
    // To add a video       : add a new() { ... } line inside the right chapter block.
    // ─────────────────────────────────────────────────────────────────────────
    public static readonly List<TutorialVideo> Videos =
    [
        // ─── CHAPTER 1 ────────────────────────────────────────────────────────
        ..InChapter("Chapter 1",

            // Major Skips
            new() { Id = "blue_hand_skip",   Title = "Blue Hand Skip (BHS)", Category = "Major Skips", Description = "Any%", Url = "https://www.youtube.com/watch?v=jqwK8JXwF8s" },
            new() { Id = "red_hand_skip",    Title = "Red Hand Skip (RHS)",  Category = "Major Skips", Description = "Any%", Url = "https://www.youtube.com/watch?v=KAbUV4uBTzY" },
            new() { Id = "stair_clip",       Title = "Stair Clip",           Category = "Major Skips", Description = "Any%", Url = "https://www.youtube.com/watch?v=rSWFhaQTHl8" },
            new() { Id = "cat_clip",         Title = "Cat Clip",             Category = "Major Skips", Description = "Any%", Url = "https://www.youtube.com/watch?v=v0039B49RTA" },
            new() { Id = "huggy_zip",        Title = "Huggy Zip",            Category = "Major Skips", Description = "Any%", Url = "https://www.youtube.com/watch?v=R61xu2V8DEY" },
            new() { Id = "catwalk_skip",           Title = "Catwalk Skip",                Category = "Major Skips", Description = "Any%",              Url = "https://www.youtube.com/watch?v=jrIoy7zHE3Q"    },

            // No Major Glitches
            new() { Id = "blue_hand_loading_skip", Title = "Blue Hand Loading Skip (BHLS)", Category = "Major Skips", Description = "No Major Glitches", Url = "https://youtube.com/watch?v=6VKOjaLtjhk"        },
            new() { Id = "unopened_door",          Title = "Unopened Door",                 Category = "Major Skips", Description = "No Major Glitches", Url = "https://www.youtube.com/watch?v=QgDjy8MAOZ8"    },
            new() { Id = "hyperspeed_hallway",     Title = "Hyperspeed Hallway",            Category = "Major Skips", Description = "No Major Glitches", Url = "https://www.youtube.com/watch?v=2bSGJwXH_w8"    },
            new() { Id = "mythic_cubes",           Title = "Mythic Cubes",                  Category = "Major Skips", Description = "No Major Glitches", Url = "https://www.youtube.com/watch?v=l-0or95GWdE"    },
            new() { Id = "rukki_skip_ch1",         Title = "Rukki Skip",                    Category = "Major Skips", Description = "No Major Glitches", Url = "https://www.youtube.com/watch?v=Md7_Y-Kesb8"    },
            new() { Id = "entarino_slide",         Title = "Entarino Slide",                Category = "Major Skips", Description = "No Major Glitches", Url = "https://www.youtube.com/watch?v=Z0jEhJsyJQ8"    }
        ),

        // ─── CHAPTER 2 ────────────────────────────────────────────────────────
        ..InChapter("Chapter 2",

            // Major Skips
            new() { Id = "hyperspeed_swing",                   Title = "Hyperspeed Swing",                       Category = "Major Skips",  Description = "Out of Bounds, Inbounds, No Major Glitches",                                                       Url = "https://youtube.com/watch?v=RI_O4bd5Nsc"          },
            new() { Id = "nam_skip",                           Title = "Nam Skip",                               Category = "Major Skips",  Description = "Out of Bounds",                                                                                    Url = "https://youtube.com/watch?v=a6CpFrU5WQo"          },
            new() { Id = "100_nam",                            Title = "100% Nam",                               Category = "Major Skips",  Description = "Out of Bounds",                                                                                    Url = "https://youtube.com/watch?v=7jYyGEOUBAc"          },
            new() { Id = "green_hand_early",                   Title = "Green Hand Early",                       Category = "Major Skips",  Description = "Out of Bounds, Inbounds",                                                                          Url = "https://youtube.com/watch?v=Upxk67wHM1g&t=128"    },
            new() { Id = "mommy_hand_grab_skip",               Title = "Mommy Hand Grab Skip",                   Category = "Major Skips",  Description = "Out of Bounds, Inbounds",                                                                          Url = "https://youtube.com/watch?v=Upxk67wHM1g&t=158"    },
            new() { Id = "green_hand_room_skip",               Title = "Green Hand Room Skip",                   Category = "Major Skips",  Description = "Out of Bounds, Inbounds, No Major Glitches",                                                       Url = "https://youtube.com/watch?v=L2Zf_6cQh7k&t=243"    },
            new() { Id = "standing_buttons_musical_memory",    Title = "Standing on Buttons in Musical Memory",  Category = "Major Skips",  Description = "Out of Bounds, Inbounds, No Major Glitches",                                                       Url = "https://youtube.com/watch?v=qBB-ICLxX4w"          },
            new() { Id = "musical_memory_skip",                Title = "Musical Memory Skip",                    Category = "Major Skips",  Description = "Out of Bounds",                                                                                    Url = "https://youtube.com/watch?v=hNH0PRbAbs0"          },
            new() { Id = "musical_memory_skip_cutout",         Title = "Musical Memory Skip w/ Cutout",          Category = "Major Skips",  Description = "Out of Bounds",                                                                                    Url = "https://youtube.com/watch?v=2AFgCdZmLGY"          },
            new() { Id = "door_prop_skip",                     Title = "Door Prop Skip",                         Category = "Major Skips",  Description = "Out of Bounds",                                                                                    Url = "https://youtube.com/watch?v=2AFgCdZmLGY&t=56"     },
            new() { Id = "musical_memory_load_manip",          Title = "Musical Memory Load Manipulation",       Category = "Major Skips",  Description = "All Categories",                                                                                   Url = "https://youtube.com/watch?v=BZ4Bdkj1k2U"          },
            new() { Id = "storage_skip",                       Title = "Storage Skip",                           Category = "Major Skips",  Description = "Out of Bounds, Inbounds, No Major Glitches",                                                       Url = "https://youtube.com/watch?v=f4PKNx-yUuM&t=596"    },
            new() { Id = "wack_a_wuggy_skip",                  Title = "Wack-A-Wuggy Skip",                      Category = "Major Skips",  Description = "Out of Bounds",                                                                                    Url = "https://youtube.com/watch?v=QiUpe37XkJE&t=344"    },
            new() { Id = "barry_skip",                         Title = "Barry Skip",                             Category = "Major Skips",  Description = "Out of Bounds",                                                                                    Url = "https://youtube.com/watch?v=gGNH_aNCqXI"          },
            new() { Id = "100_barry_skip",                     Title = "100% Barry Skip",                        Category = "Major Skips",  Description = "Out of Bounds",                                                                                    Url = "https://youtube.com/watch?v=yaClb5aj5Fo"      },
            new() { Id = "statues_skip",                       Title = "Statues Skip",                           Category = "Major Skips",  Description = "Out of Bounds",                                                                                    Url = "https://youtube.com/watch?v=2QY__BkFEKs"          },
            new() { Id = "statues_minigame_skip_objects",      Title = "Statues Minigame Skip w/ Objects",       Category = "Major Skips",  Description = "Out of Bounds, Inbounds, No Major Glitches",                                                       Url = "https://youtube.com/watch?v=obKKjiOkGxo&t=1060"   },
            new() { Id = "statues_minigame_skip_crouching",    Title = "Statues Minigame Skip w/ Crouching",     Category = "Major Skips",  Description = "Out of Bounds, Inbounds",                                                                          Url = "https://youtube.com/watch?v=ZN8YzGuwAjI"          },
            new() { Id = "hyperspeed_swing_over_tubes",        Title = "Hyperspeed Swing Over Tubes",            Category = "Major Skips",  Description = "Out of Bounds, Inbounds, No Major Glitches",                                                       Url = "https://youtube.com/watch?v=k-eF3a7cwfI"          },
            new() { Id = "storm_skip",                         Title = "Storm Skip",                             Category = "Major Skips",  Description = "Out of Bounds, Inbounds, No Major Glitches",                                                       Url = "https://youtube.com/watch?v=-xpfAY8EOLE"          },
            new() { Id = "ggd_skip",                           Title = "GGD Skip",                               Category = "Major Skips",  Description = "Out of Bounds",                                                                                    Url = "https://youtube.com/watch?v=Qg8z2TaWu-I"      },
            new() { Id = "oob_mommy_chase_skip",               Title = "OoB Mommy Chase Skip",                   Category = "Major Skips",  Description = "Out of Bounds",                                                                                    Url = "https://youtube.com/watch?v=kso--tLYwf8"          },
            new() { Id = "mommy_chase_skip_barrels",           Title = "Mommy Chase Skip w/ Barrels",            Category = "Major Skips",  Description = "Out of Bounds, Inbounds, No Major Glitches",                                                       Url = "https://youtube.com/watch?v=qxS2knT431Q&t=1335"   },
            new() { Id = "mommy_death_cutscene_skip",          Title = "Mommy Death Cutscene Skip",              Category = "Major Skips",  Description = "Out of Bounds",                                                                                    Url = "https://youtube.com/watch?v=1kF6yKN5HrY"          },
            new() { Id = "robin_office_skip",                  Title = "Robin/Office Skip",                      Category = "Major Skips",  Description = "Out of Bounds, Inbounds, No Major Glitches — Video by RubyRain",                                   Url = "https://youtube.com/watch?v=IDJreYBRPEQ"          },
            new() { Id = "100_office_skip",                    Title = "100% Office Skip",                       Category = "Major Skips",  Description = "Out of Bounds, Inbounds, No Major Glitches",                                                       Url = "https://youtube.com/watch?v=QiUpe37XkJE&t=960"    },

            // Small Tricks
            new() { Id = "rukki_skip",                         Title = "Rukki Skip",                             Category = "Small Tricks", Description = "All Categories — clicking, releasing, or holding to get the hand stuck",                           Url = "https://youtube.com/watch?v=mDXk5kS9OGY"          },
            new() { Id = "lever_sniping",                      Title = "Lever Sniping",                          Category = "Small Tricks", Description = "All Categories",                                                                                   Url = "https://youtube.com/watch?v=7yfLDnN8-Mw"          },
            new() { Id = "hyperventing",                       Title = "Hyperventing",                           Category = "Small Tricks", Description = "All Categories",                                                                                   Url = "https://youtube.com/watch?v=6JzYBfLK3SU"      },
            new() { Id = "small_ghs",                          Title = "Small GHS",                              Category = "Small Tricks", Description = "All Categories — Video by MutantAye",                                                              Url = "https://youtube.com/watch?v=BK2MMxyq5Mg"          },
            new() { Id = "wack_a_wuggy_double_hits",           Title = "Wack-A-Wuggy Double/Triple Hits",        Category = "Small Tricks", Description = "Out of Bounds, Inbounds, No Major Glitches — max 10 double hits in No Major Skips",               Url = "https://youtube.com/watch?v=qxS2knT431Q&t=698"    },
            new() { Id = "hyperswing_over_tubes",              Title = "Hyperswing Over Tubes",                  Category = "Small Tricks", Description = "Out of Bounds, Inbounds, No Major Glitches — landing in front of tubes allowed in all categories", Url = "https://streamable.com/wj3lyy"                    },

            // Legacy
            new() { Id = "bean_skip",                          Title = "Bean Skip",                              Category = "Legacy",       Description = "Out of Bounds",                                                                                    Url = "https://youtube.com/watch?v=wT_6ahpuPhU&t=23"     },
            new() { Id = "uis_box_skip",                       Title = "UIS/Box Skip",                           Category = "Legacy",       Description = "Out of Bounds",                                                                                    Url = "https://youtube.com/watch?v=xAjc3OM4SxM&t=82"     },
            new() { Id = "train_skip",                         Title = "Train Skip",                             Category = "Legacy",       Description = "All categories — ending cutscene must be shown; time stops when lever animation plays",             Url = "https://youtube.com/watch?v=J55JJCzKKA8"          },
            new() { Id = "fortnitegod123_skip",                Title = "Fortnitegod123 Skip",                    Category = "Legacy",       Description = "Out of Bounds",                                                                                    Url = "https://youtube.com/watch?v=trcFjoHfz5g"          },
            new() { Id = "water_treatment_skip",               Title = "Water Treatment Skip",                   Category = "Legacy",       Description = "Out of Bounds",                                                                                    Url = "https://youtube.com/watch?v=2lUwAA8QJa8"          },
            new() { Id = "wack_a_wuggy_skip_old",              Title = "Wack-A-Wuggy Skip (Old)",                Category = "Legacy",       Description = "Out of Bounds",                                                                                    Url = "https://youtube.com/watch?v=KGRqhbB38f0"          }
        ),

        // ─── CHAPTER 3 ────────────────────────────────────────────────────────
        // ..InChapter("Chapter 3",
        //     new() { Id = "...", Title = "...", Category = "...", Description = "...", Url = "..." },
        // ),

        // ─── CHAPTER 4 ────────────────────────────────────────────────────────
        // ..InChapter("Chapter 4",
        //     new() { Id = "...", Title = "...", Category = "...", Description = "...", Url = "..." },
        // ),
    ];
    // ─────────────────────────────────────────────────────────────────────────

    // Stamps all entries with the given chapter, so you don't repeat it on every line.
    private static TutorialVideo[] InChapter(string chapter, params TutorialVideo[] videos)
    {
        foreach (var v in videos) v.Chapter = chapter;
        return videos;
    }

    public static readonly string TempFolder =
        Path.Combine(Path.GetTempPath(), "SpeedrunLauncher", "TutorialVideos");

    public static readonly string SavedFolder =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "Tutorials");

    private static readonly string YtDlpPath =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "yt-dlp.exe");

    private static readonly HttpClient Http = new() { Timeout = TimeSpan.FromMinutes(30) };

    /// <summary>
    /// Clears the tutorial temp folder on startup. Videos are downloaded on demand.
    /// </summary>
    public static void Initialize()
    {
        try
        {
            if (Directory.Exists(TempFolder))
                Directory.Delete(TempFolder, recursive: true);
            Directory.CreateDirectory(TempFolder);
        }
        catch { }

        try
        {
            Directory.CreateDirectory(SavedFolder);
            foreach (var video in Videos)
            {
                var saved = Directory.GetFiles(SavedFolder, video.Id + ".*").FirstOrDefault();
                if (saved != null) video.SavedPath = saved;
            }
            SaveEnabled = Videos.Any(v => v.IsSaved);
        }
        catch { }
    }

    /// <summary>
    /// Downloads a single video on demand and reports progress (0–100).
    /// Ensures yt-dlp is available first (downloads it if missing).
    /// </summary>
    public static async Task<bool> DownloadAsync(TutorialVideo video, IProgress<double>? progress = null)
    {
        try
        {
            if (video.IsSaved) { progress?.Report(100); return true; }
            await EnsureYtDlpAsync();
            await DownloadVideoAsync(video, progress);
            if (video.IsDownloaded && SaveEnabled)
                await SaveSingleAsync(video);
            return video.IsDownloaded;
        }
        catch
        {
            return false;
        }
    }

    public static bool SaveEnabled { get; private set; }

    public static async Task ToggleSaveAsync()
    {
        SaveEnabled = !SaveEnabled;
        if (SaveEnabled)
        {
            Directory.CreateDirectory(SavedFolder);
            foreach (var video in Videos.Where(v => v.IsDownloaded && !v.IsSaved))
                await SaveSingleAsync(video);
        }
        else
        {
            foreach (var video in Videos.Where(v => v.IsSaved))
                await UnsaveSingleAsync(video);
        }
    }

    private static async Task SaveSingleAsync(TutorialVideo video)
    {
        var src = video.PlayablePath;
        var dst = Path.Combine(SavedFolder, video.Id + Path.GetExtension(src));
        if (src != dst)
            await Task.Run(() => File.Copy(src, dst, overwrite: true));
        video.SavedPath = dst;
    }

    private static Task UnsaveSingleAsync(TutorialVideo video)
    {
        var path = video.SavedPath;
        video.SavedPath = "";
        return Task.Run(() => { try { File.Delete(path); } catch { } });
    }

    private static async Task EnsureYtDlpAsync()
    {
        if (File.Exists(YtDlpPath)) return;
        Directory.CreateDirectory(Path.GetDirectoryName(YtDlpPath)!);
        using var response = await Http.GetAsync(
            "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe",
            HttpCompletionOption.ResponseHeadersRead);
        response.EnsureSuccessStatusCode();
        await using var stream = await response.Content.ReadAsStreamAsync();
        await using var file   = File.Create(YtDlpPath);
        await stream.CopyToAsync(file);
    }

    private static async Task DownloadVideoAsync(TutorialVideo video, IProgress<double>? progress)
    {
        var outputTemplate = Path.Combine(TempFolder, video.Id + ".%(ext)s");

        var psi = new ProcessStartInfo
        {
            FileName               = YtDlpPath,
            Arguments              = $"-f \"best[height<=480][ext=mp4]/best[height<=480]/best[ext=mp4]/best\" " +
                                     $"--no-playlist --no-warnings " +
                                     $"-o \"{outputTemplate}\" " +
                                     $"\"{video.Url}\"",
            UseShellExecute        = false,
            CreateNoWindow         = true,
            RedirectStandardOutput = true,  // read asynchronously — avoids buffer deadlock
        };

        using var process = Process.Start(psi)!;

        // Parse "[download]  45.3% of ..." lines from yt-dlp stdout
        process.OutputDataReceived += (_, e) =>
        {
            if (e.Data is null) return;
            var m = Regex.Match(e.Data, @"(\d+\.?\d*)%");
            if (m.Success && double.TryParse(m.Groups[1].Value,
                    NumberStyles.Float, CultureInfo.InvariantCulture, out var pct))
                progress?.Report(Math.Clamp(pct, 0, 100));
        };
        process.BeginOutputReadLine();

        await process.WaitForExitAsync();
        process.WaitForExit(); // flush any remaining buffered output

        var downloaded = Directory.GetFiles(TempFolder, video.Id + ".*")
            .FirstOrDefault(f => !f.EndsWith(".part", StringComparison.OrdinalIgnoreCase));
        if (downloaded != null)
            video.LocalPath = downloaded;
    }
}
