using System.Text.RegularExpressions;
using YoutubeExplode;
using YoutubeExplode.Videos.Streams;

namespace SpeedrunLauncher.Services;

public sealed class TutorialVideo
{
    public required string   Id            { get; init; }
    public required string   Title         { get; init; }
    public          string   Description   { get; init; } = "";
    public required string   Url           { get; init; }
    public          string   Category      { get; init; } = "General";
    public          string   Chapter       { get; set;  } = "";
    public          string[] RunCategories { get; init; } = [];
    public          string   Version       { get; init; } = "";
    public          string[] Routes        { get; init; } = [];
    public          string   Restrictions  { get; init; } = "";
    public          string   Author        { get; init; } = "";
    public          bool     OpenInBrowser { get; init; } = false;
}

public static class VideoTutorialService
{
    public static bool FlatList = true;

    public static readonly List<TutorialVideo> Videos =
    [
        // ─── CHAPTER 1 ────────────────────────────────────────────────────────
        ..InChapter("Chapter 1",
            new() { Id = "jqwK8JXwF8s",     Title = "Blue Hand Skip (BHS)",       Version = "<1.1, 1.2", Author = "MythicCheese",               RunCategories = ["Any%", "100%"], Routes = ["Unrestricted"],               Restrictions = "", Url = "https://youtu.be/jqwK8JXwF8s",                OpenInBrowser = true },
            new() { Id = "xDBjyu_9oVs",     Title = "VHS Intro Skip",             Version = "1.3",       Author = "Mago",                       RunCategories = ["Any%", "100%"], Routes = ["Restricted", "Unrestricted"], Restrictions = "", Url = "https://youtu.be/xDBjyu_9oVs",                OpenInBrowser = true },
            new() { Id = "QgDjy8MAOZ8",     Title = "Unopened Door",              Version = "<1.1",      Author = "MythicCheese",               RunCategories = ["Any%", "100%"], Routes = ["Restricted", "Unrestricted"], Restrictions = "", Url = "https://youtu.be/QgDjy8MAOZ8",                OpenInBrowser = true },
            new() { Id = "M9fj9qSwM7Y",     Title = "Hyperspeed Hallway (HSH)",   Version = "<1.1, 1.2", Author = "Pezzy",                      RunCategories = ["Any%", "100%"], Routes = ["Unrestricted"],               Restrictions = "", Url = "https://youtu.be/M9fj9qSwM7Y",                OpenInBrowser = true },
            new() { Id = "tlxYrFR3L4o",     Title = "Red Hand Skip 1.3",          Version = "1.3",       Author = "Mago",                       RunCategories = ["Any%", "100%"], Routes = ["Restricted", "Unrestricted"], Restrictions = "", Url = "https://youtu.be/tlxYrFR3L4o",                OpenInBrowser = true },
            new() { Id = "l-0or95GWdE_v1",  Title = "Mythic Cubes | <1.1",        Version = "<1.1",      Author = "MythicCheese",               RunCategories = ["Any%", "100%"], Routes = ["Restricted", "Unrestricted"], Restrictions = "", Url = "https://youtu.be/l-0or95GWdE",                OpenInBrowser = true },
            new() { Id = "l-0or95GWdE_v2",  Title = "Mythic Cubes | 1.2",         Version = "1.2",       Author = "MythicCheese",               RunCategories = ["Any%", "100%"], Routes = ["Restricted", "Unrestricted"], Restrictions = "", Url = "https://youtu.be/l-0or95GWdE",                OpenInBrowser = true },
            new() { Id = "KAbUV4uBTzY",     Title = "Red Hand Skip (RHS) | <1.1", Version = "<1.1",      Author = "MythicCheese",               RunCategories = ["Any%", "100%"], Routes = ["Unrestricted"],               Restrictions = "", Url = "https://youtu.be/KAbUV4uBTzY",                OpenInBrowser = true },
            new() { Id = "EKQ3hXL0jSk",     Title = "Red Hand Skip (RHS) | 1.2",  Version = "1.2",       Author = "Ajai",                       RunCategories = ["Any%", "100%"], Routes = ["Unrestricted"],               Restrictions = "", Url = "https://youtu.be/EKQ3hXL0jSk",                OpenInBrowser = true },
            new() { Id = "sSrDxPO0-s8",     Title = "Rukki Skip",                 Version = "<1.1, 1.2", Author = "n0kitsune",                  RunCategories = ["Any%", "100%"], Routes = ["Restricted", "Unrestricted"], Restrictions = "", Url = "https://youtu.be/sSrDxPO0-s8",                OpenInBrowser = true },
            new() { Id = "0ZhO4evCif8",     Title = "Unpowered Pole",             Version = "1.3",       Author = "Mago",                       RunCategories = ["Any%", "100%"], Routes = ["Restricted", "Unrestricted"], Restrictions = "", Url = "https://youtu.be/0ZhO4evCif8",                OpenInBrowser = true },
            new() { Id = "Md7_Y-Kesb8",     Title = "No Slide Reload",            Version = "<1.1",      Author = "MythicCheese",               RunCategories = ["Any%", "100%"], Routes = ["Restricted", "Unrestricted"], Restrictions = "", Url = "https://youtu.be/Md7_Y-Kesb8",                OpenInBrowser = true },
            new() { Id = "rSWFhaQTHl8",     Title = "Stair Clip",                 Version = "<1.1",      Author = "MythicCheese",               RunCategories = ["Any%", "100%"], Routes = ["Restricted", "Unrestricted"], Restrictions = "", Url = "https://youtu.be/rSWFhaQTHl8",                OpenInBrowser = true },
            new() { Id = "v0039B49RTA",     Title = "Catclip | <1.1",             Version = "<1.1",      Author = "MythicCheese",               RunCategories = ["Any%", "100%"], Routes = ["Restricted", "Unrestricted"], Restrictions = "", Url = "https://youtu.be/v0039B49RTA",                OpenInBrowser = true },
            new() { Id = "w3gDZGgijUg",     Title = "Catclip | 1.2",              Version = "1.2",       Author = "Nia",                        RunCategories = ["Any%", "100%"], Routes = ["Restricted", "Unrestricted"], Restrictions = "", Url = "https://youtu.be/w3gDZGgijUg",                OpenInBrowser = true },
            new() { Id = "uXYnafCMPXA",     Title = "Catclip | 1.3",              Version = "1.3",       Author = "Mago",                       RunCategories = ["Any%", "100%"], Routes = ["Restricted", "Unrestricted"], Restrictions = "", Url = "https://youtu.be/uXYnafCMPXA",                OpenInBrowser = true },
            new() { Id = "Z0jEhJsyJQ8",     Title = "Entarino Slide",             Version = "<1.1, 1.2", Author = "MythicCheese",               RunCategories = ["Any%", "100%"], Routes = ["Restricted", "Unrestricted"], Restrictions = "", Url = "https://youtu.be/Z0jEhJsyJQ8",                OpenInBrowser = true },
            new() { Id = "zc-DRhZhn5E",     Title = "Anti-Stuck Jump",            Version = "1.3",       Author = "Mago",                       RunCategories = ["Any%", "100%"], Routes = ["Restricted", "Unrestricted"], Restrictions = "", Url = "https://youtu.be/zc-DRhZhn5E",                OpenInBrowser = true },
            new() { Id = "jrIoy7zHE3Q",     Title = "Catwalk Skip",               Version = "<1.1, 1.2", Author = "MythicCheese",               RunCategories = ["Any%", "100%"], Routes = ["Restricted", "Unrestricted"], Restrictions = "", Url = "https://youtu.be/jrIoy7zHE3Q",                OpenInBrowser = true },
            new() { Id = "Tarzan_skip",     Title = "Tarzan Skip",                Version = "1.3",       Author = "Mago, Mushy, AdrianPG77...", RunCategories = ["Any%"],         Routes = ["Unrestricted"],               Restrictions = "", Url = "https://www.youtube.com/watch?v=HMM3T_1SGsA", OpenInBrowser = true },
            new() { Id = "Sigmaventskip",   Title = "Sigma Vent Skip",            Version = "1.3",       Author = "Salulin",                    RunCategories = ["Any%", "100%"], Routes = ["Unrestricted"],               Restrictions = "", Url = "https://youtu.be/_QH-dVyW7_A",                OpenInBrowser = true },
            new() { Id = "HallwayF11Boost", Title = "Hallway F11 Boost",          Version = "1.3",       Author = "Edwin",                      RunCategories = ["Any%", "100%"], Routes = ["Unrestricted"],               Restrictions = "", Url = "https://youtu.be/6xmAMRI6zGk",                OpenInBrowser = true },
            new() { Id = "RedHandF11",      Title = "Red Hand F11 Boost",         Version = "1.3",       Author = "Edwin",                      RunCategories = ["Any%", "100%"], Routes = ["Unrestricted"],               Restrictions = "", Url = "https://youtu.be/rNX_cMrf5Bk",                OpenInBrowser = true },
            new() { Id = "MAF-Route",       Title = "Full 'Make A Friend' Route", Version = "1.3",       Author = "Edwin",                      RunCategories = ["Any%", "100%"], Routes = ["Unrestricted"],               Restrictions = "", Url = "https://youtu.be/j9-cYBqCkW0",                OpenInBrowser = true },
            new() { Id = "BestSkip",        Title = "Clip OOB + Full Skip",       Version = "<1.1, 1.2", Author = "Edwin",                      RunCategories = ["Any%"],         Routes = ["Unrestricted"],               Restrictions = "", Url = "https://youtu.be/aj7PPeVqRKI",                OpenInBrowser = true }
        ),

        // ─── CHAPTER 2 ────────────────────────────────────────────────────────
        ..InChapter("Chapter 2",
            new() { Id = "Sh0JLIJpudE", Title = "Hyperspeed Swing (HSS)",                   Version = "1.0, 1.1", Author = "n0kitsune",         RunCategories = ["Any%", "All Minigames", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Glitches"],                   Restrictions = "", Url = "https://youtu.be/Sh0JLIJpudE",                OpenInBrowser = true },
            new() { Id = "XdiDEVpVfxQ", Title = "Pillow Clip Tutorial",                     Version = "1.0, 1.1", Author = "proac",             RunCategories = ["Any%", "All Minigames", "100%"], Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/XdiDEVpVfxQ",                OpenInBrowser = true },
            new() { Id = "Rf0N2HS6GtI", Title = "Pillow Boost Tutorial",                    Version = "1.0, 1.1", Author = "proac",             RunCategories = ["Any%", "All Minigames", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Glitches", "No Major Skips"], Restrictions = "", Url = "https://youtu.be/Rf0N2HS6GtI",                OpenInBrowser = true },
            new() { Id = "n7I348UI2U0", Title = "Poppy Woppy Skip",                         Version = "1.1",      Author = "Nia",               RunCategories = ["Any%"],                          Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/n7I348UI2U0",                OpenInBrowser = true },
            new() { Id = "SEfdsbruRI4", Title = "Green Hand Early (GHE)",                   Version = "1.0",      Author = "Nia",               RunCategories = ["Any%", "All Minigames", "100%"], Routes = ["Out of Bounds", "Inbounds"],                                        Restrictions = "", Url = "https://youtu.be/SEfdsbruRI4",                OpenInBrowser = true },
            new() { Id = "a6CpFrU5WQo", Title = "NAM Skip",                                 Version = "1.0",      Author = "Nam (JMK)",         RunCategories = ["Any%", "All Minigames"],         Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/a6CpFrU5WQo",                OpenInBrowser = true },
            new() { Id = "7jYyGEOUBAc", Title = "100% NAM Skip",                            Version = "1.0",      Author = "LilQuince",         RunCategories = ["100%"],                          Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/7jYyGEOUBAc",                OpenInBrowser = true },
            new() { Id = "o37M1CTMeOw", Title = "NAM Skip 1.1",                             Version = "1.1",      Author = "Nia",               RunCategories = ["Any%", "All Minigames", "100%"], Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/o37M1CTMeOw",                OpenInBrowser = true },
            new() { Id = "mDXk5kS9OGY", Title = "Rukki Skip",                               Version = "1.0, 1.1", Author = "n0kitsune",         RunCategories = ["Any%", "All Minigames", "100%"], Routes = ["Inbounds", "No Major Glitches", "No Major Skips"],                  Restrictions = "", Url = "https://youtu.be/mDXk5kS9OGY",                OpenInBrowser = true },
            new() { Id = "0S0KuGMnrg4", Title = "Mommy Hand Grab Skip",                     Version = "1.0, 1.1", Author = "n0kitsune",         RunCategories = ["Any%", "All Minigames", "100%"], Routes = ["Inbounds"],                                                         Restrictions = "", Url = "https://youtu.be/0S0KuGMnrg4",                OpenInBrowser = true },
            new() { Id = "BK2MMxyq5Mg", Title = "Small Green Hand Skip",                    Version = "1.0, 1.1", Author = "MutantEye",         RunCategories = ["Any%", "All Minigames", "100%"], Routes = ["No Major Skips"],                                                   Restrictions = "", Url = "https://youtu.be/BK2MMxyq5Mg",                OpenInBrowser = true },
            new() { Id = "OIv0kd9y-bo", Title = "Green Hand Room Skip",                     Version = "1.0, 1.1", Author = "Nia",               RunCategories = ["Any%", "All Minigames", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Glitches"],                   Restrictions = "", Url = "https://youtu.be/OIv0kd9y-bo",                OpenInBrowser = true },
            new() { Id = "qBB-ICLxX4w", Title = "Standing on Buttons in Musical Memory",    Version = "1.0, 1.1", Author = "Hawkz",             RunCategories = ["Any%", "All Minigames", "100%"], Routes = ["Inbounds", "No Major Glitches"],                                    Restrictions = "", Url = "https://youtu.be/qBB-ICLxX4w",                OpenInBrowser = true },
            new() { Id = "hNH0PRbAbs0", Title = "Musical Memory Skip (MMS) | Hard Version", Version = "1.0, 1.1", Author = "Mello",             RunCategories = ["All Minigames", "100%"],         Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/hNH0PRbAbs0",                OpenInBrowser = true },
            new() { Id = "OBT-0fPhT2U", Title = "Musical Memory Skip (MMS) | Easy Version", Version = "1.0, 1.1", Author = "n0kitsune",         RunCategories = ["All Minigames", "100%"],         Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/OBT-0fPhT2U",                OpenInBrowser = true },
            new() { Id = "BZ4Bdkj1k2U", Title = "Musical Memory Load Manipulation",         Version = "1.0, 1.1", Author = "n0kitsune",         RunCategories = ["Any%", "All Minigames", "100%"], Routes = ["No Major Skips"],                                                   Restrictions = "", Url = "https://youtu.be/BZ4Bdkj1k2U",                OpenInBrowser = true },
            new() { Id = "5KIrOpgXKPE", Title = "Cutout Jump",                              Version = "1.2",      Author = "Technight",         RunCategories = ["Any%"],                          Routes = ["Inbounds", "No Major Skips"],                                       Restrictions = "", Url = "https://youtu.be/5KIrOpgXKPE",                OpenInBrowser = true },
            new() { Id = "8A_TMcBmEk8", Title = "Whack-a-Wuggy Skip Out of Bounds",         Version = "1.0",      Author = "Nia",               RunCategories = ["All Minigames", "100%"],         Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/8A_TMcBmEk8",                OpenInBrowser = true },
            new() { Id = "_94xLVzYOXs", Title = "Whack-a-Wuggy Skip New Patch",             Version = "1.0, 1.1", Author = "Nerd Squared",      RunCategories = ["All Minigames", "100%"],         Routes = ["Out of Bounds", "Inbounds"],                                        Restrictions = "", Url = "https://youtu.be/_94xLVzYOXs",                OpenInBrowser = true },
            new() { Id = "gGNH_aNCqXI", Title = "Barry Skip",                               Version = "1.0",      Author = "Sangohanvde",       RunCategories = ["All Minigames"],                 Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/gGNH_aNCqXI",                OpenInBrowser = true },
            new() { Id = "WG877Aaafrc", Title = "Barry Skip | Hard Version",                Version = "1.1",      Author = "Nia",               RunCategories = ["All Minigames"],                 Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/WG877Aaafrc",                OpenInBrowser = true },
            new() { Id = "YFk9bjBmJ8E", Title = "Barry Skip | Easy Version",                Version = "1.1",      Author = "n0kitsune",         RunCategories = ["All Minigames"],                 Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/YFk9bjBmJ8E",                OpenInBrowser = true },
            new() { Id = "AsNVxIypWkc", Title = "Barry Skip Extended",                      Version = "1.1",      Author = "Nia",               RunCategories = ["Any%", "All Minigames"],         Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/AsNVxIypWkc",                OpenInBrowser = true },
            new() { Id = "yaClb5aj5Fo", Title = "100% Barry Skip",                          Version = "1.0, 1.1", Author = "Nerd Squared",      RunCategories = ["100%"],                          Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/yaClb5aj5Fo",                OpenInBrowser = true },
            new() { Id = "eEp6xB-ebOE", Title = "Statues Skip w/ Objects",                  Version = "1.0, 1.1", Author = "Nia",               RunCategories = ["All Minigames", "100%"],         Routes = ["Out of Bounds", "Inbounds", "No Major Glitches"],                   Restrictions = "", Url = "https://youtu.be/eEp6xB-ebOE",                OpenInBrowser = true },
            new() { Id = "uOxXseQ9iUo", Title = "Statues Skip w/ Crouching",                Version = "1.0, 1.1", Author = "Nia",               RunCategories = ["All Minigames", "100%"],         Routes = ["Out of Bounds", "Inbounds"],                                        Restrictions = "", Url = "https://youtu.be/uOxXseQ9iUo",                OpenInBrowser = true },
            new() { Id = "k-eF3a7cwfI", Title = "Hyperspeed Swing Over Tubes",              Version = "1.0",      Author = "n0kitsune",         RunCategories = ["All Minigames", "100%"],         Routes = ["Out of Bounds", "Inbounds", "No Major Glitches"],                   Restrictions = "", Url = "https://youtu.be/k-eF3a7cwfI",                OpenInBrowser = true },
            new() { Id = "u_JQEKfLT2o", Title = "Caves Skip",                               Version = "1.1",      Author = "Nia",               RunCategories = ["Any%", "All Minigames"],         Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/u_JQEKfLT2o",                OpenInBrowser = true },
            new() { Id = "-xpfAY8EOLE", Title = "Ruby Skip",                                Version = "1.0, 1.1", Author = "n0kitsune",         RunCategories = ["All Minigames", "100%"],         Routes = ["Inbounds", "No Major Glitches"],                                    Restrictions = "", Url = "https://youtu.be/-xpfAY8EOLE",                OpenInBrowser = true },
            new() { Id = "2lUwAA8QJa8", Title = "Water Treatment Skip",                     Version = "1.0, 1.1", Author = "Laupig",            RunCategories = ["All Minigames", "100%"],         Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/2lUwAA8QJa8",                OpenInBrowser = true },
            new() { Id = "-4RkZYFO95g", Title = "Water Treatment Skip 100%",                Version = "1.0, 1.1", Author = "Sangohanvde",       RunCategories = ["All Minigames", "100%"],         Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/-4RkZYFO95g",                OpenInBrowser = true },
            new() { Id = "0OnrGhW8Jvw", Title = "Water Treatment Skip Swing Variant",       Version = "1.1",      Author = "Nia",               RunCategories = ["Any%", "All Minigames", "100%"], Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/0OnrGhW8Jvw",                OpenInBrowser = true },
            new() { Id = "Qg8z2TaWu-I", Title = "GGD Skip",                                 Version = "1.0",      Author = "Sangohanvde",       RunCategories = ["All Minigames", "100%"],         Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/Qg8z2TaWu-I",                OpenInBrowser = true },
            new() { Id = "gE2DgVK5oxI", Title = "GGD Skip",                                 Version = "1.1",      Author = "Nia",               RunCategories = ["Any%", "All Minigames", "100%"], Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/gE2DgVK5oxI",                OpenInBrowser = true },
            new() { Id = "mjKg1DPQ5mg", Title = "proac Skip",                               Version = "1.1",      Author = "Nia",               RunCategories = ["Any%", "All Minigames"],         Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/mjKg1DPQ5mg",                OpenInBrowser = true },
            new() { Id = "kso--tLYwf8", Title = "Mommy Chase Skip",                         Version = "1.0, 1.1", Author = "Sangohanvde",       RunCategories = ["All Minigames"],                 Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/kso--tLYwf8",                OpenInBrowser = true },
            new() { Id = "bG2Hd3GGgOI", Title = "Mommy Chase Skip w/ Barrels",              Version = "1.0, 1.1", Author = "Nia",               RunCategories = ["All Minigames"],                 Routes = ["Inbounds", "No Major Glitches"],                                    Restrictions = "", Url = "https://youtu.be/bG2Hd3GGgOI",                OpenInBrowser = true },
            new() { Id = "1kF6yKN5HrY", Title = "Mommy Death Skip",                         Version = "1.0",      Author = "Sangohanvde",       RunCategories = ["100%"],                          Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://youtu.be/1kF6yKN5HrY",                OpenInBrowser = true },
            new() { Id = "IDJreYBRPEQ", Title = "Robin/Office Skip",                        Version = "1.0, 1.1", Author = "Ruby Rain",         RunCategories = ["All Minigames", "100%"],         Routes = ["Out of Bounds", "Inbounds", "No Major Glitches"],                   Restrictions = "", Url = "https://youtu.be/IDJreYBRPEQ",                OpenInBrowser = true },
            new() { Id = "JwJ-NZRR6x0", Title = "100% Office Skip",                         Version = "1.0, 1.1", Author = "n0kitsune",         RunCategories = ["100%"],                          Routes = ["Out of Bounds", "Inbounds", "No Major Glitches"],                   Restrictions = "", Url = "https://youtu.be/JwJ-NZRR6x0",                OpenInBrowser = true },
            new() { Id = "cmhEAkPqPCU", Title = "Full All Minigames New Route",             Version = "1.0",      Author = "AdrianPG77, Sango", RunCategories = ["All Minigames"],                 Routes = ["Out of Bounds"],                                                    Restrictions = "", Url = "https://www.youtube.com/watch?v=cmhEAkPqPCU", OpenInBrowser = true }
        ),

        // ─── CHAPTER 3 ────────────────────────────────────────────────────────
        ..InChapter("Chapter 3",
            new() { Id = "KQQGQw2L6ME", Title = "Phone Call Skip",                 Version = "Old Patch", Author = "Sangohanvde",      RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "", Url = "https://youtu.be/KQQGQw2L6ME",                OpenInBrowser = true },
            new() { Id = "8hHfolxcKxk", Title = "First Area Skip",                 Version = "Old Patch", Author = "ontrigger",        RunCategories = ["Any%"],         Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://youtu.be/8hHfolxcKxk",                OpenInBrowser = true },
            new() { Id = "JBBCngxUA0Y", Title = "First Puzzle Skip",               Version = "Old Patch", Author = "Hawkz",            RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "", Url = "https://youtu.be/JBBCngxUA0Y",                OpenInBrowser = true },
            new() { Id = "TMLl8k6toRI", Title = "Tram Skip | Prop Launch",         Version = "Old Patch", Author = "ClownTech",        RunCategories = ["Any%"],         Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://youtu.be/TMLl8k6toRI",                OpenInBrowser = true },
            new() { Id = "m2C35NRPwa4", Title = "Tram Skip",                       Version = "Old Patch", Author = "Nia",              RunCategories = ["Any%"],         Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://youtu.be/m2C35NRPwa4",                OpenInBrowser = true },
            new() { Id = "sq3qA0Hs7-Y", Title = "Tram Skip Easier Way",            Version = "Old Patch", Author = "Nerd Squared",     RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://youtu.be/sq3qA0Hs7-Y",                OpenInBrowser = true },
            new() { Id = "-OLSIOhu3u4", Title = "HSH Skip IL",                     Version = "Old Patch", Author = "ontrigger",        RunCategories = ["Any%"],         Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://youtu.be/-OLSIOhu3u4",                OpenInBrowser = true },
            new() { Id = "YwBX4QDMpso", Title = "Second Floor Skip",               Version = "Old Patch", Author = "LA",               RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "", Url = "https://youtu.be/YwBX4QDMpso",                OpenInBrowser = true },
            new() { Id = "Vsr8tfY0xhk", Title = "Full Early Dome",                 Version = "Old Patch", Author = "LA",               RunCategories = ["Any%"],         Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "", Url = "https://youtu.be/ByXyM7zCJDs",                OpenInBrowser = true },
            new() { Id = "ciDdzeHyM40", Title = "Dome Elevator Skip",              Version = "Old Patch", Author = "LA",               RunCategories = ["Any%", "100%"], Routes = ["No Major Skips"],                              Restrictions = "", Url = "https://youtu.be/ciDdzeHyM40",                OpenInBrowser = true },
            new() { Id = "tn0duTft9CM", Title = "Second Vent Skip (School)",       Version = "Old Patch", Author = "Sangohanvde",      RunCategories = ["Any%", "100%"], Routes = ["Inbounds"],                                    Restrictions = "", Url = "https://youtu.be/tn0duTft9CM",                OpenInBrowser = true },
            new() { Id = "9s13jLuaeYM", Title = "Full School Route",               Version = "Old Patch", Author = "LA",               RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "", Url = "https://youtu.be/9s13jLuaeYM",                OpenInBrowser = true },
            new() { Id = "school_oob",  Title = "Full School Route OOB",           Version = "Old Patch", Author = "Edwin",            RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://youtu.be/hU_Yf-rHTa0",                OpenInBrowser = true },
            new() { Id = "5LYVMCytYgQ", Title = "Caves Skip",                      Version = "Old Patch", Author = "ontrigger",        RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "", Url = "https://youtu.be/5LYVMCytYgQ",                OpenInBrowser = true },
            new() { Id = "aN_EbKmzcHg", Title = "Box Puzzle Skip",                 Version = "Old Patch", Author = "mushymeow",        RunCategories = ["Any%", "100%"], Routes = ["Inbounds"],                                    Restrictions = "", Url = "https://youtu.be/aN_EbKmzcHg",                OpenInBrowser = true },
            new() { Id = "20KgR3B38DM", Title = "Playhouse Skip",                  Version = "Old Patch", Author = "Nam (JMK)",        RunCategories = ["100%"],         Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://youtu.be/20KgR3B38DM",                OpenInBrowser = true },
            new() { Id = "cnNjH6JGJZk", Title = "Office Caves Half Puzzle Skip",   Version = "Old Patch", Author = "LA",               RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "", Url = "https://youtu.be/cnNjH6JGJZk",                OpenInBrowser = true },
            new() { Id = "lsuZO4UnmCk", Title = "Last Puzzle Skip",                Version = "Old Patch", Author = "LA",               RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "", Url = "https://youtu.be/lsuZO4UnmCk",                OpenInBrowser = true },
            new() { Id = "sjvielVNElA", Title = "Catnap Jumpscare Skip",           Version = "Old Patch", Author = "Cindorian",        RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "", Url = "https://youtu.be/sjvielVNElA",                OpenInBrowser = true },
            new() { Id = "U7IK1U4JEvo", Title = "Poppy Door Key Skip",             Version = "Old Patch", Author = "Nia",              RunCategories = ["Any%", "100%"], Routes = ["Inbounds", "No Major Skips"],                  Restrictions = "", Url = "https://youtu.be/U7IK1U4JEvo",                OpenInBrowser = true },
            new() { Id = "PuCSfIOpOz8", Title = "Barrelevator Skip",               Version = "Old Patch", Author = "Nia",              RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://youtu.be/PuCSfIOpOz8",                OpenInBrowser = true },
            new() { Id = "-xa06l9Od5A", Title = "Elevator Clip",                   Version = "Old Patch", Author = "Laupig",           RunCategories = ["Any%"],         Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://youtu.be/-xa06l9Od5A",                OpenInBrowser = true },
            new() { Id = "j9GRn98frEk", Title = "Purple Hand Skip",                Version = "Old Patch", Author = "Danger1451",       RunCategories = ["Any%"],         Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://youtu.be/j9GRn98frEk",                OpenInBrowser = true },
            new() { Id = "IuDxsjIeVuY", Title = "Elevator Skip",                   Version = "Old Patch", Author = "RayRay",           RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://youtu.be/IuDxsjIeVuY",                OpenInBrowser = true },
            new() { Id = "mewCAtO8Ag4", Title = "The Hour of Joy Skip",            Version = "Old Patch", Author = "RayRay",           RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "", Url = "https://youtu.be/mewCAtO8Ag4",                OpenInBrowser = true },
            new() { Id = "mewCAtO8Ag5", Title = "The Hour of Joy Skip w/Canister", Version = "Old Patch", Author = "RayRay",           RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "", Url = "https://youtu.be/A9nayw_9mXo",                OpenInBrowser = true },
            new() { Id = "mewCAtO8Ag6", Title = "Catnap Skip",                     Version = "Old Patch", Author = "Mushy, Technight", RunCategories = ["Any%"],         Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "", Url = "https://www.youtube.com/watch?v=FIZG7mrM31U", OpenInBrowser = true },
            new() { Id = "mewCAtO8Ag7", Title = "Hampter Skip",                    Version = "Old Patch", Author = "Hampter",          RunCategories = ["Any%", "100%"], Routes = ["Inbounds"],                                    Restrictions = "", Url = "https://youtu.be/x2I8bnHUOS0",                OpenInBrowser = true },
            new() { Id = "mewCAtO8Ag8", Title = "Cable Skip",                      Version = "Old Patch", Author = "Technight",        RunCategories = ["100%"],         Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://www.youtube.com/watch?v=nD4H7m-xYgc", OpenInBrowser = true }
        ),

        // ─── CHAPTER 4 ────────────────────────────────────────────────────────
        ..InChapter("Chapter 4",
            new() { Id = "qIETQkA7AuQ", Title = "Full Elevator Skip",                 Version = "Old Patch", Author = "Weet",         RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/qIETQkA7AuQ", OpenInBrowser = true },
            new() { Id = "60xJGGq6zBY", Title = "Mug Skip",                           Version = "Old Patch", Author = "BarneyGoose",  RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/60xJGGq6zBY", OpenInBrowser = true },
            new() { Id = "Hehh430Azw0", Title = "Gouda Cheese Puzzle Skip",           Version = "Old Patch", Author = "Sangohanvde",  RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/Hehh430Azw0", OpenInBrowser = true },
            new() { Id = "VV5TYzxxYVo", Title = "Crouch Trick",                       Version = "Old Patch", Author = "LA",           RunCategories = ["Any%", "100%"], Routes = ["No Major Skips"],                              Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/VV5TYzxxYVo", OpenInBrowser = true },
            new() { Id = "TdEvu_JCg4M", Title = "Proacventing",                       Version = "Old Patch", Author = "proac",        RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/TdEvu_JCg4M", OpenInBrowser = true },
            new() { Id = "nsDaUU8oM2o", Title = "Limon Skip",                         Version = "Old Patch", Author = "LA",           RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/nsDaUU8oM2o", OpenInBrowser = true },
            new() { Id = "AugaC6dccVM", Title = "Prison Critters Skip",               Version = "Old Patch", Author = "n0kitsune",    RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/AugaC6dccVM", OpenInBrowser = true },
            new() { Id = "EM8ifBMwabc", Title = "Jailbreak (Prison Skip)",            Version = "Old Patch", Author = "Laupig",       RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds"],                               Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/EM8ifBMwabc", OpenInBrowser = true },
            new() { Id = "s-hbIPkz9cQ", Title = "Prison Rail Jump",                   Version = "Old Patch", Author = "LA",           RunCategories = ["Any%", "100%"], Routes = ["Inbounds", "No Major Skips"],                  Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/s-hbIPkz9cQ", OpenInBrowser = true },
            new() { Id = "IQ3Y7lDr8cc", Title = "IMTRWTRATRWTR Skip (Mix Skip)",      Version = "Old Patch", Author = "Weet",         RunCategories = ["Any%"],         Routes = ["Out of Bounds"],                               Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/IQ3Y7lDr8cc", OpenInBrowser = true },
            new() { Id = "klGDuGQRg1E", Title = "No Boxes Skip",                      Version = "Old Patch", Author = "LA",           RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/klGDuGQRg1E", OpenInBrowser = true },
            new() { Id = "iRLPyT7DAJY", Title = "Little Yarnaby Skip",                Version = "Old Patch", Author = "ZacGames25",   RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/iRLPyT7DAJY", OpenInBrowser = true },
            new() { Id = "ze17WlDp_AI", Title = "Yarnaby Cutscene Skip",              Version = "Old Patch", Author = "LA",           RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/ze17WlDp_AI", OpenInBrowser = true },
            new() { Id = "OFyK8BmDTQI", Title = "Small Yarnaby Skip",                 Version = "Old Patch", Author = "ontrigger",    RunCategories = ["Any%"],         Routes = ["Out of Bounds"],                               Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/OFyK8BmDTQI", OpenInBrowser = true },
            new() { Id = "f1bDGT2Jpm4", Title = "NASA Skip Tutorial",                 Version = "Old Patch", Author = "Nia",          RunCategories = ["Any%"],         Routes = ["Out of Bounds"],                               Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/f1bDGT2Jpm4", OpenInBrowser = true },
            new() { Id = "aHbj3oxmP0c", Title = "Pianosaurus Skip",                   Version = "Old Patch", Author = "BarneyGoose",  RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/aHbj3oxmP0c", OpenInBrowser = true },
            new() { Id = "HcYNMeEG70I", Title = "Cave Jump",                          Version = "Old Patch", Author = "LA",           RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/HcYNMeEG70I", OpenInBrowser = true },
            new() { Id = "wKvSSlMnBWk", Title = "Shimmying",                          Version = "Old Patch", Author = "ontrigger",    RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/wKvSSlMnBWk", OpenInBrowser = true },
            new() { Id = "J9CnYkX20ss", Title = "Thick Of It Skip (Safe Haven Skip)", Version = "Old Patch", Author = "proac",        RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds"],                               Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/J9CnYkX20ss", OpenInBrowser = true },
            new() { Id = "sX63cpBnDb4", Title = "No Man's Land Puzzle Skip",          Version = "Old Patch", Author = "LA",           RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/sX63cpBnDb4", OpenInBrowser = true },
            new() { Id = "0Y1ellsmlpw", Title = "Big Yarnaby Skip",                   Version = "Old Patch", Author = "BarneyGoose",  RunCategories = ["Any%"],         Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/0Y1ellsmlpw", OpenInBrowser = true },
            new() { Id = "gMY7OBxKwf4", Title = "Big Yarnaby Skip (100%)",            Version = "Old Patch", Author = "Nia",          RunCategories = ["100%"],         Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/gMY7OBxKwf4", OpenInBrowser = true },
            new() { Id = "weJXK5_jcYI", Title = "Quick AC Puzzle",                    Version = "Old Patch", Author = "n0kitsune",    RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/weJXK5_jcYI", OpenInBrowser = true },
            new() { Id = "F7xWoQJ4ah8", Title = "Frozen Hand Bypass",                 Version = "Old Patch", Author = "n0kitsune",    RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/F7xWoQJ4ah8", OpenInBrowser = true },
            new() { Id = "NdzquCagLuE", Title = "Full Morgue Skip",                   Version = "Old Patch", Author = "Danger1451",   RunCategories = ["Any%"],         Routes = ["Out of Bounds"],                               Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/NdzquCagLuE", OpenInBrowser = true },
            new() { Id = "DG6M5MKyK5g", Title = "Grapple Skip",                       Version = "Old Patch", Author = "LA",           RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/DG6M5MKyK5g", OpenInBrowser = true },
            new() { Id = "WeNR2kRJKJA", Title = "Baba Chops Bossfight Skip (easy)",   Version = "Old Patch", Author = "n0kitsune",    RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/WeNR2kRJKJA", OpenInBrowser = true },
            new() { Id = "hJtrdSEgD20", Title = "Baba Chops + Elevator Skip (hard)",  Version = "Old Patch", Author = "Nerd Squared", RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/hJtrdSEgD20", OpenInBrowser = true },
            new() { Id = "KLvPEmIoVto", Title = "Doctor Fight Skip",                  Version = "Old Patch", Author = "Nia",          RunCategories = ["Any%"],         Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/KLvPEmIoVto", OpenInBrowser = true },
            new() { Id = "cRrvjuGMKUY", Title = "Doctor Fight Skip (100%)",           Version = "Old Patch", Author = "Nia",          RunCategories = ["100%"],         Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/cRrvjuGMKUY", OpenInBrowser = true },
            new() { Id = "URySFMRsFDQ", Title = "Le Parkour",                         Version = "Old Patch", Author = "RNG_Retr0",    RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/URySFMRsFDQ", OpenInBrowser = true },
            new() { Id = "a2rjZrDu7qM", Title = "Foundation Skip",                    Version = "Old Patch", Author = "n0kitsune",    RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/a2rjZrDu7qM", OpenInBrowser = true },
            new() { Id = "7R7oSOLPwdQ", Title = "Aidful Skip",                        Version = "Old Patch", Author = "Danger1451",   RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds"],                               Restrictions = "Unrestricted, Restricted", Url = "https://youtu.be/7R7oSOLPwdQ", OpenInBrowser = true },
            new() { Id = "DhHted8z6Nk", Title = "Safe Haven Skip",                    Version = "New Patch", Author = "Technight",    RunCategories = ["Any%"],         Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "Unrestricted",             Url = "https://youtu.be/DhHted8z6Nk", OpenInBrowser = true },
            new() { Id = "Mt-cee3IIcU", Title = "Afterpath Skip Restricted Version",  Version = "New Patch", Author = "Technight",    RunCategories = ["Any%"],         Routes = ["Out of Bounds"],                               Restrictions = "Unrestricted",             Url = "https://youtu.be/Mt-cee3IIcU", OpenInBrowser = true },
            new() { Id = "MeSsdkAaG4I", Title = "Omnihandless OOB Unres",             Version = "New Patch", Author = "Technight",    RunCategories = ["Any%"],         Routes = ["Out of Bounds"],                               Restrictions = "Unrestricted",             Url = "https://youtu.be/MeSsdkAaG4I", OpenInBrowser = true },
            new() { Id = "BuLU45f5pYU", Title = "Yarnaby Skip",                       Version = "New Patch", Author = "Technight",    RunCategories = ["Any%"],         Routes = ["Out of Bounds"],                               Restrictions = "Unrestricted",             Url = "https://youtu.be/BuLU45f5pYU", OpenInBrowser = true },
            new() { Id = "NOEAA33Bvc0", Title = "Containment Zone Full Skip",         Version = "New Patch", Author = "Technight",    RunCategories = ["Any%"],         Routes = ["Out of Bounds"],                               Restrictions = "Unrestricted",             Url = "https://youtu.be/NOEAA33Bvc0", OpenInBrowser = true },
            new() { Id = "oWju6aVM18E", Title = "Second Prision Skip",                Version = "New Patch", Author = "Technight",    RunCategories = ["Any%"],         Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "Unrestricted",             Url = "https://youtu.be/oWju6aVM18E", OpenInBrowser = true },
            new() { Id = "IXkJ-K1xsss", Title = "Smiles Criatures Skip",              Version = "New Patch", Author = "Technight",    RunCategories = ["Any%"],         Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "Unrestricted",             Url = "https://youtu.be/IXkJ-K1xsss", OpenInBrowser = true }
        ),

        // ─── CHAPTER 5 ────────────────────────────────────────────────────────
        ..InChapter("Chapter 5",

            // Major Skips
            new() { Id = "ch5_lower_taper_fade_skip",       Title = "Lower Taper Fade Skip",                 Version = "Patch 1, Patch 2", Author = "Arkham",                RunCategories = ["Any%", "100%"], Routes = [],                                              Restrictions = "", Url = "https://youtu.be/0zp2M5Q3mF4",                OpenInBrowser = true },
            new() { Id = "ch5_fire_f11_skip",               Title = "Fire F11 Skip",                         Version = "Patch 1, Patch 2", Author = "Arkham",                RunCategories = ["Any%", "100%"], Routes = [],                                              Restrictions = "", Url = "https://youtu.be/8pzFFrPFGVY",                OpenInBrowser = true },
            new() { Id = "ch5_elevator_skip",               Title = "Elevator Skip",                         Version = "Patch 1, Patch 2", Author = "Weet",                  RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://www.youtube.com/watch?v=6bthrB9OhEg", OpenInBrowser = true },
            new() { Id = "ch5_outimals_oob_skip",           Title = "Outimals OOB Skip",                     Version = "Patch 1, Patch 2", Author = "Ontrigger",             RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://youtu.be/bT2YVHkzYzg",                OpenInBrowser = true },
            new() { Id = "ch5_early_grabpack",              Title = "Early Grabpack",                        Version = "Patch 1, Patch 2", Author = "Technight",             RunCategories = ["Any%", "100%"], Routes = [],                                              Restrictions = "", Url = "https://youtu.be/yBTanZRCbYA",                OpenInBrowser = true },
            new() { Id = "ch5_zero_gravity_67_skip",        Title = "0 Gravity (67 Skip)",                   Version = "Patch 1",          Author = "Sango",                 RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://youtu.be/gkBFN_3WmHk",                OpenInBrowser = true },
            new() { Id = "ch5_zero_gravity_updated",        Title = "0 Gravity (67 Skip) Updated Route",     Version = "Patch 1, Patch 2", Author = "Keirahela",             RunCategories = ["Any%", "100%"], Routes = [],                                              Restrictions = "", Url = "https://www.youtube.com/watch?v=38M1-1PXDs4", OpenInBrowser = true },
            new() { Id = "ch5_zero_gravity_oob",            Title = "0 Gravity (67 Skip) Updated Route OOB", Version = "Patch 1, Patch 2", Author = "Technight",             RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://youtu.be/jEuUNBPD8mo",                OpenInBrowser = true },
            new() { Id = "ch5_gilbert_skip",                Title = "Gilbert Skip",                          Version = "Patch 1, Patch 2", Author = "Davidbaron",            RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "", Url = "https://www.youtube.com/watch?v=DxtDrdUn4QA", OpenInBrowser = true },
            new() { Id = "ch5_magnet_cuff_box_skip",        Title = "Magnet Cuff Room Box Skip",             Version = "Patch 1, Patch 2", Author = "LA, Rayray & Bwoomz",   RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "", Url = "https://www.youtube.com/watch?v=m7nfM0Oa-IA", OpenInBrowser = true },
            new() { Id = "ch5_huggy_memories_inbounds",     Title = "Huggy's Memories Skip (Inbounds)",      Version = "Patch 1, Patch 2", Author = "realturhun, Technight", RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "", Url = "https://www.youtube.com/watch?v=yzuKwJAlzQg", OpenInBrowser = true },
            new() { Id = "ch5_huggy_memories_oob",          Title = "Huggy's Memories Skip (OOB)",           Version = "Patch 1, Patch 2", Author = "MushyMeow",             RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://youtu.be/iPBlDBxYls4",                OpenInBrowser = true },
            new() { Id = "ch5_huggy_memories_unrestricted", Title = "Huggy's Memories Skip (Unrestricted)",  Version = "Patch 1, Patch 2", Author = "Mushymeow",             RunCategories = ["Any%", "100%"], Routes = [],                                              Restrictions = "", Url = "https://youtu.be/0KYxlmu9bLM",                OpenInBrowser = true },
            new() { Id = "ch5_fent_skip",                   Title = "Fent Skip",                             Version = "Patch 1, Patch 2", Author = "Broomz",                RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "", Url = "https://youtu.be/dMttzMp0bZ4",                OpenInBrowser = true },
            new() { Id = "ch5_sid_skip",                    Title = "SID Skip",                              Version = "Patch 1, Patch 2", Author = "MushyMeow",             RunCategories = ["Any%", "100%"], Routes = [],                                              Restrictions = "", Url = "https://youtu.be/5qqmLbt5zDA",                OpenInBrowser = true },
            new() { Id = "ch5_sid_skip_no_f11",             Title = "SID Skip (No F11)",                     Version = "Patch 1, Patch 2", Author = "Technight, Nia",        RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://www.youtube.com/watch?v=Rx7y9Dzqe7o", OpenInBrowser = true },
            new() { Id = "ch5_megabonk_skip",               Title = "Megabonk Skip",                         Version = "Patch 1, Patch 2", Author = "Nia",                   RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://youtu.be/lhJRbSbKlZw",                OpenInBrowser = true },
            new() { Id = "ch5_dollhouse_key_oob",           Title = "Dollhouse Key OOB",                     Version = "Patch 1, Patch 2", Author = "Technight",             RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://youtu.be/lhJRbSbKlZw",                OpenInBrowser = true },
            new() { Id = "ch5_dollhouse_key_oob_faster",    Title = "Dollhouse Key OOB but Faster",          Version = "Patch 1, Patch 2", Author = "Proac",                 RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://www.youtube.com/watch?v=Z2g7l9GVz2Y", OpenInBrowser = true },
            new() { Id = "ch5_lily_chase_skip",             Title = "Lily Chase Skip",                       Version = "Patch 1, Patch 2", Author = "Clowntech2",            RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "", Url = "https://youtu.be/M5_f3a5yWzw",                OpenInBrowser = true },
            new() { Id = "ch5_peaks_of_yore_skip",          Title = "Peaks of Yore Skip",                    Version = "Patch 1, Patch 2", Author = "Clowntech2",            RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds"],                   Restrictions = "", Url = "https://youtu.be/OJl497Iq57w",                OpenInBrowser = true },
            new() { Id = "ch5_reanimation_skip",            Title = "Reanimation Skip",                      Version = "Patch 1, Patch 2", Author = "Hawkz",                 RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://youtu.be/8yn9rA5LI0Q",                OpenInBrowser = true },
            new() { Id = "ch5_clanker_skip",                Title = "Clanker Skip",                          Version = "Patch 1, Patch 2", Author = "AdrianPG77",            RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds"],                               Restrictions = "", Url = "https://youtu.be/loEGTZXmedU",                OpenInBrowser = true },
            new() { Id = "ch5_computer_skip",               Title = "Computer Skip",                         Version = "Patch 1, Patch 2", Author = "Ontrigger",             RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "", Url = "https://youtu.be/zIVch4Q4eq4",                OpenInBrowser = true },

            // Small Tricks
            new() { Id = "ch5_small_intro_timesave",        Title = "Small Intro Timesave",                  Version = "Patch 1, Patch 2", Author = "LA",                    RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "", Url = "https://youtu.be/8hR_j6f27Rg",                OpenInBrowser = true },
            new() { Id = "ch5_first_scanner_gate_jump",     Title = "First Scanner Gate Jump",               Version = "Patch 1, Patch 2", Author = "AdrianPG77",            RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "", Url = "https://youtu.be/gbzaSQoIM7Q",                OpenInBrowser = true },
            new() { Id = "ch5_fast_1st_elevator_shaft",     Title = "Fast 1st Elevator Shaft",               Version = "Patch 1, Patch 2", Author = "None",                  RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "", Url = "https://youtu.be/uzQPYh3quYI",                OpenInBrowser = true },
            new() { Id = "ch5_spiderman_grapples",          Title = "Spiderman Grapples",                    Version = "Patch 1, Patch 2", Author = "None",                  RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "", Url = "https://youtu.be/srSg14GaCWg",                OpenInBrowser = true },
            new() { Id = "ch5_huggy_bossfight_route",       Title = "Huggy Bossfight Route (Any%)",          Version = "Patch 1, Patch 2", Author = "Arkham",                RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "", Url = "https://youtu.be/4uPKfRPgljc",                OpenInBrowser = true },
            new() { Id = "ch5_sweet_street_last_section",   Title = "Sweet Street Last Section Fast",        Version = "Patch 1, Patch 2", Author = "LA",                    RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "", Url = "https://youtu.be/QdrPRI-MH3M",                OpenInBrowser = true },
            new() { Id = "ch5_finding_friends_route",       Title = "Finding Friends Route",                 Version = "Patch 1, Patch 2", Author = "Sky",                   RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "", Url = "https://youtu.be/SXvcGmZBJkA",                OpenInBrowser = true },
            new() { Id = "ch5_2_cycle_lily_rlgl",           Title = "2 Cycle Lily Red Light/Green Light",    Version = "Patch 1, Patch 2", Author = "Clown",                 RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "", Url = "https://youtu.be/AGNZiN8qejc",                OpenInBrowser = true },
            new() { Id = "ch5_small_last_huggy_chase_skip", Title = "Small Last Huggy Chase Skip",           Version = "Patch 1, Patch 2", Author = "AdrianPG77",            RunCategories = ["Any%", "100%"], Routes = ["Out of Bounds", "Inbounds", "No Major Skips"], Restrictions = "", Url = "https://youtu.be/77cx0gpCvLQ",                OpenInBrowser = true },

            // Legacy
            new() { Id = "ch5_dollhouse_early_lights_out",  Title = "Dollhouse Early Lights Out",            Version = "",                 Author = "Icewolf",               RunCategories = ["NG+"],          Description = "NOT ALLOWED to perform at the moment of this update.", Restrictions = "", Url = "https://www.youtube.com/watch?v=htVnk7Uuw-c", OpenInBrowser = true }
        ),
    ];

    public static void Initialize() { }

    private static readonly YoutubeClient _yt = new();
    private static readonly Dictionary<string, string> _cache = new();
    private static readonly Dictionary<string, (string Video, string Audio)> _adaptiveCache = new();

    // Returns (url, errorMessage) — url is null on failure
    public static async Task<(string? Url, string? Error)> GetStreamUrlAsync(TutorialVideo video)
    {
        try
        {
            var m = Regex.Match(video.Url, @"youtu\.be/([A-Za-z0-9_-]+)");
            if (!m.Success) m = Regex.Match(video.Url, @"[?&]v=([A-Za-z0-9_-]+)");
            if (!m.Success) return (null, "Could not extract video ID from URL");

            var videoId = m.Groups[1].Value;

            if (_cache.TryGetValue(videoId, out var cached))
                return (cached, null);

            var manifest = await _yt.Videos.Streams.GetManifestAsync(videoId);

            var stream = manifest.GetMuxedStreams()
                             .OrderByDescending(s => s.VideoQuality.MaxHeight)
                             .FirstOrDefault();

            if (stream != null)
            {
                _cache[videoId] = stream.Url;
                return (stream.Url, null);
            }

            return (null, manifest.GetMuxedStreams().Any()
                ? "No stream found"
                : "No playable stream (DASH-only video)");
        }
        catch (Exception ex)
        {
            return (null, ex.Message);
        }
    }

    // Returns separate high-quality video + audio stream URLs.
    // Prefers mp4 adaptive streams (up to 1080p+); falls back to muxed.
    // audioUrl is null when falling back to a muxed stream.
    public static async Task<(string? VideoUrl, string? AudioUrl, string? Error)> GetAdaptiveStreamsAsync(TutorialVideo video)
    {
        try
        {
            var m = Regex.Match(video.Url, @"youtu\.be/([A-Za-z0-9_-]+)");
            if (!m.Success) m = Regex.Match(video.Url, @"[?&]v=([A-Za-z0-9_-]+)");
            if (!m.Success) return (null, null, "Could not extract video ID from URL");

            var videoId = m.Groups[1].Value;

            if (_adaptiveCache.TryGetValue(videoId, out var ac))
                return (ac.Video, ac.Audio, null);

            var manifest = await _yt.Videos.Streams.GetManifestAsync(videoId);

            var bestVideo = manifest.GetVideoOnlyStreams()
                .Where(s => s.Container.Name == "mp4")
                .OrderByDescending(s => s.VideoQuality.MaxHeight)
                .FirstOrDefault();

            var bestAudio = manifest.GetAudioOnlyStreams()
                .Where(s => s.Container.Name == "mp4")
                .OrderByDescending(s => s.Bitrate)
                .FirstOrDefault();

            if (bestVideo != null && bestAudio != null)
            {
                _adaptiveCache[videoId] = (bestVideo.Url, bestAudio.Url);
                return (bestVideo.Url, bestAudio.Url, null);
            }

            // Fallback: best muxed stream
            var muxed = manifest.GetMuxedStreams()
                .OrderByDescending(s => s.VideoQuality.MaxHeight)
                .FirstOrDefault();

            if (muxed != null)
            {
                _adaptiveCache[videoId] = (muxed.Url, string.Empty);
                return (muxed.Url, null, null);
            }

            return (null, null, "No playable stream found");
        }
        catch (Exception ex)
        {
            return (null, null, ex.Message);
        }
    }

    // Pre-fetches stream URLs for a batch of videos in the background
    public static void PrefetchAsync(IEnumerable<TutorialVideo> videos)
    {
        foreach (var video in videos)
            _ = Task.Run(() => GetStreamUrlAsync(video));
    }

    private static TutorialVideo[] InChapter(string chapter, params TutorialVideo[] videos)
    {
        foreach (var v in videos) v.Chapter = chapter;
        return videos;
    }
}
