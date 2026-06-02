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
            new() { Id = "catwalk_skip",     Title = "Catwalk Skip",         Category = "Major Skips", Description = "Any%", Url = "https://www.youtube.com/watch?v=jrIoy7zHE3Q" },

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
        ..InChapter("Chapter 3",

            // Major Skips
            new() { Id = "ch3_first_cycle",                      Title = "First Cycle",                      Category = "Major Skips", Description = "All Categories — Q on last footstep before CatNap opens gate; jump before pulling swing",               Url = "https://youtu.be/XYPp_BDuoxc"                },
            new() { Id = "ch3_piston_skip",                      Title = "0 Cycle / Piston Skip Full Guide", Category = "Major Skips", Description = "All Categories",                                                                                        Url = "https://youtu.be/wjzT-FLMEwc"                },
            new() { Id = "ch3_phone_call_skip",                  Title = "Phone Call Skip",                  Category = "Major Skips", Description = "All Categories — consistent setup found",                                                               Url = "https://youtu.be/KQQGQw2L6ME"                },
            new() { Id = "ch3_first_area_skip",                  Title = "First Area Skip",                  Category = "Major Skips", Description = "Out of Bounds — must hit orange piston room trigger to spawn Train Station trigger",                    Url = "https://youtu.be/8hHfolxcKxk"                },
            new() { Id = "ch3_puzzle_skip",                      Title = "Puzzle Skip",                      Category = "Major Skips", Description = "Out of Bounds, Inbounds — if missed on first try, marked as OOB",                                       Url = "https://youtu.be/JBBCngxUA0Y"                },
            new() { Id = "ch3_tram_skip",                        Title = "Tram Skip",                        Category = "Major Skips", Description = "Out of Bounds — Q input when close to red flashing light",                                              Url = "https://youtu.be/m2C35NRPwa4"                },
            new() { Id = "ch3_tram_skip_all_stages",             Title = "Tram Skip All Stages%",            Category = "Major Skips", Description = "Out of Bounds — normal version softlocks in gas room",                                                  Url = "https://youtu.be/I_yT2SQkur0"                },
            new() { Id = "ch3_hsh_skip_all_stages",              Title = "HSH Skip All Stages%",             Category = "Major Skips", Description = "Out of Bounds",                                                                                         Url = "https://youtu.be/-OLSIOhu3u4"                },
            new() { Id = "ch3_hsh_skip_mushy",                   Title = "HSH Skip",                         Category = "Major Skips", Description = "Out of Bounds, Inbounds — get HSH key and do the room before from the first key",                       Url = "https://youtu.be/deKXnsqJTa4"                },
            new() { Id = "ch3_hsh_skip",                         Title = "HSH Skip",                         Category = "Major Skips", Description = "Out of Bounds, Inbounds — Video by LA",                                                                 Url = "https://youtu.be/Vsr8tfY0xhk"                },
            new() { Id = "ch3_second_floor_skip",                Title = "Second Floor Skip",                Category = "Major Skips", Description = "Out of Bounds, Inbounds",                                                                               Url = "https://youtu.be/YwBX4QDMpso"                },
            new() { Id = "ch3_elevator_skip_la",                 Title = "Elevator Skip",                    Category = "Major Skips", Description = "All Categories — Video by LA",                                                                          Url = "https://youtu.be/ciDdzeHyM40"                },
            new() { Id = "ch3_school_skip",                      Title = "School Skip",                      Category = "Major Skips", Description = "Out of Bounds",                                                                                         Url = "https://youtu.be/9B35hRlswzU"                },
            new() { Id = "ch3_third_electric_puzzle_skip",       Title = "3rd Electric Puzzle Skip",         Category = "Major Skips", Description = "Out of Bounds, Inbounds",                                                                               Url = "https://youtu.be/tn0duTft9CM"                },
            new() { Id = "ch3_school_full_route",                Title = "School Full Route",                Category = "Major Skips", Description = "All Categories — ignore Caves part",                                                                    Url = "https://youtu.be/9s13jLuaeYM"                },
            new() { Id = "ch3_caves_skip",                       Title = "Caves Skip",                       Category = "Major Skips", Description = "Out of Bounds, Inbounds",                                                                               Url = "https://youtu.be/5LYVMCytYgQ"                },
            new() { Id = "ch3_box_puzzle_skip",                  Title = "Box Puzzle Skip",                  Category = "Major Skips", Description = "Out of Bounds, Inbounds",                                                                               Url = "https://youtu.be/aN_EbKmzcHg"                },
            new() { Id = "ch3_playhouse_skip",                   Title = "PlayHouse Skip",                   Category = "Major Skips", Description = "Out of Bounds — requires View Distance set to Ultra",                                                   Url = "https://youtu.be/20KgR3B38DM"                },
            new() { Id = "ch3_office_route_oob",                 Title = "Office Route OOB",                 Category = "Major Skips", Description = "Out of Bounds",                                                                                         Url = "https://youtu.be/FEJ9d4F_Bo4"                },
            new() { Id = "ch3_office_caves_half_puzzle_skip",    Title = "Office Caves Half Puzzle Skip",    Category = "Major Skips", Description = "All Categories",                                                                                        Url = "https://youtu.be/cnNjH6JGJZk"                },
            new() { Id = "ch3_office_caves_half_puzzle_skip_v2", Title = "Office Caves Half Puzzle Skip v2", Category = "Major Skips", Description = "All Categories",                                                                                        Url = "https://youtu.be/s2BGvsFSml8"                },
            new() { Id = "ch3_last_puzzle_skip",                 Title = "Last Puzzle Skip",                 Category = "Major Skips", Description = "All Categories",                                                                                        Url = "https://youtu.be/lsuZO4UnmCk"                },
            new() { Id = "ch3_catnap_jumpscare_skip",            Title = "CatNap Jumpscare Skip",            Category = "Major Skips", Description = "Out of Bounds, Inbounds",                                                                               Url = "https://youtu.be/sjvielVNElA"                },
            new() { Id = "ch3_poppy_door_key_skip",              Title = "Poppy Door Key Skip",              Category = "Major Skips", Description = "All Categories — door opens early; plug doesn't spawn until later, door stays open",                    Url = "https://youtu.be/qeiBXf9-x0U"                },
            new() { Id = "ch3_barrelevator_skip",                Title = "Barrelevator Skip",                Category = "Major Skips", Description = "Out of Bounds",                                                                                         Url = "https://youtu.be/PuCSfIOpOz8"                },
            new() { Id = "ch3_elevator_skip",                    Title = "Elevator Skip",                    Category = "Major Skips", Description = "Out of Bounds — hit button then go back to elevator edge",                                              Url = "https://www.youtube.com/watch?v=IuDxsjIeVuY"  },
            new() { Id = "ch3_hour_of_joy_skip",                 Title = "The Hour of Joy Skip",             Category = "Major Skips", Description = "Out of Bounds, Inbounds",                                                                               Url = "https://www.youtube.com/watch?v=mewCAtO8Ag4"  },

            // Small Tricks
            new() { Id = "ch3_tram_skip_flying",                 Title = "Tram Skip Flying Version",         Category = "Small Tricks", Description = "Out of Bounds — change FPS to hit trigger in red flashing area",                                      Url = "https://youtu.be/TMLl8k6toRI"                },
            new() { Id = "ch3_elevator_clip",                    Title = "Elevator Clip",                    Category = "Small Tricks", Description = "Out of Bounds — bounce at end not necessary; press button and elevator rises",                          Url = "https://youtu.be/-xa06l9Od5A"                },
            new() { Id = "ch3_purple_hand_skip",                 Title = "Purple Hand Skip (PHS)",           Category = "Small Tricks", Description = "Out of Bounds — Any% OOB only; get Green Charge at 5s left then scroll up",                            Url = "https://youtu.be/j9GRn98frEk"                },

            // Legacy
            new() { Id = "ch3_tram_early",                       Title = "Tram Early",                       Category = "Legacy",       Description = "Out of Bounds — very inconsistent, ~10-15s if perfect, not in use",                                    Url = "https://youtu.be/Am-4qQbOmBE"                },
            new() { Id = "ch3_puzzle_skip_office_chair",         Title = "Puzzle Skip w/ Office Chair",      Category = "Legacy",       Description = "Out of Bounds, Inbounds — FPS-dependent, not in use",                                                   Url = "https://youtu.be/7w4dqkTMaRY"                }
        ),

        // ─── CHAPTER 4 ────────────────────────────────────────────────────────
        ..InChapter("Chapter 4",

            // Major Skips
            new() { Id = "ch4_full_elevator_skip",       Title = "Full Elevator Skip",                              Category = "Major Skips",  Description = "All Categories",                        Url = "https://youtu.be/qIETQkA7AuQ"       },
            new() { Id = "ch4_mug_skip",                 Title = "Mug Skip",                                        Category = "Major Skips",  Description = "All Categories",                        Url = "https://youtu.be/60xJGGq6zBY"       },
            new() { Id = "ch4_gouda_puzzle_skip",        Title = "Gouda Cheese Puzzle Skip",                        Category = "Major Skips",  Description = "All Categories",                        Url = "https://youtu.be/Hehh430Azw0"       },
            new() { Id = "ch4_prison_vent_clip",         Title = "Prison Vent Clip",                                Category = "Major Skips",  Description = "Out of Bounds, Unrestricted",           Url = "https://youtu.be/Yxs1H8eRV6o"       },
            new() { Id = "ch4_prison_critters_skip",     Title = "Prison Critters Skip",                            Category = "Major Skips",  Description = "Inbounds, Out of Bounds, Unrestricted", Url = "https://youtu.be/AugaC6dccVM"       },
            new() { Id = "ch4_jailbreak",                Title = "Jailbreak (Prison Skip)",                         Category = "Major Skips",  Description = "Out of Bounds, Unrestricted",           Url = "https://youtu.be/EM8ifBMwabc"       },
            new() { Id = "ch4_rick_raft_skip",           Title = "I Mix The Rick With The Raft Skip",               Category = "Major Skips",  Description = "Out of Bounds, Unrestricted",           Url = "https://youtu.be/IQ3Y7lDr8cc"       },
            new() { Id = "ch4_little_yarnaby_skip",      Title = "Little Yarnaby Skip",                             Category = "Major Skips",  Description = "Inbounds, Out of Bounds, Unrestricted", Url = "https://youtu.be/iRLPyT7DAJY"       },
            new() { Id = "ch4_yarnaby1_prison_skip",     Title = "Skip First Yarnaby Encounter to Prison Utility Zone", Category = "Major Skips", Description = "Out of Bounds, Unrestricted",      Url = "https://youtu.be/OFyK8BmDTQI"       },
            new() { Id = "ch4_nasa_skip",                Title = "NASA Skip",                                       Category = "Major Skips",  Description = "Unrestricted",                          Url = "https://youtu.be/066G2F4aNzQ"       },
            new() { Id = "ch4_pianosaurus_skip",         Title = "Pianosaurus Skip",                                Category = "Major Skips",  Description = "Inbounds, Out of Bounds, Unrestricted", Url = "https://youtu.be/aHbj3oxmP0c"       },
            new() { Id = "ch4_thick_of_it_skip",         Title = "Thick Of It Skip",                                Category = "Major Skips",  Description = "Out of Bounds, Unrestricted",           Url = "https://youtu.be/J9CnYkX20ss"       },
            new() { Id = "ch4_yarnaby2_skip",            Title = "Second Yarnaby Encounter Skip",                   Category = "Major Skips",  Description = "Inbounds, Out of Bounds, Unrestricted", Url = "https://youtu.be/0Y1ellsmlpw"       },
            new() { Id = "ch4_full_morgue_skip",         Title = "Full Morgue Skip",                                Category = "Major Skips",  Description = "Unrestricted",                          Url = "https://youtu.be/NdzquCagLuE"       },
            new() { Id = "ch4_baba_chops_skip_easy",     Title = "Baba Chops Bossfight Skip (easy)",                Category = "Major Skips",  Description = "Inbounds, Out of Bounds, Unrestricted", Url = "https://youtu.be/WeNR2kRJKJA"       },
            new() { Id = "ch4_baba_chops_elevator_skip", Title = "Baba Chops + Elevator Skip (hard)",               Category = "Major Skips",  Description = "Inbounds, Out of Bounds, Unrestricted", Url = "https://youtu.be/hJtrdSEgD20"       },
            new() { Id = "ch4_doctor_fight_skip",        Title = "Doctor Fight Skip",                               Category = "Major Skips",  Description = "Inbounds, Out of Bounds, Unrestricted", Url = "https://youtu.be/KLvPEmIoVto"       },
            new() { Id = "ch4_doctor_fight_skip_100",    Title = "Doctor Fight Skip (100% Version)",                Category = "Major Skips",  Description = "Inbounds, Out of Bounds, Unrestricted", Url = "https://youtu.be/cRrvjuGMKUY"       },
            new() { Id = "ch4_foundation_skip",          Title = "Foundation Skip",                                 Category = "Major Skips",  Description = "Inbounds, Out of Bounds, Unrestricted", Url = "https://youtu.be/a2rjZrDu7qM"       },
            new() { Id = "ch4_foundation_doey_cutscene", Title = "Foundation Doey Cutscene Skip",                   Category = "Major Skips",  Description = "All Categories",                        Url = "https://youtu.be/Oojyq5TYQcI"       },
            new() { Id = "ch4_aidful_skip_morgue",       Title = "Aidful Skip (Morgue Skip Version)",               Category = "Major Skips",  Description = "Unrestricted",                          Url = "https://youtu.be/NdzquCagLuE?t=466" },
            new() { Id = "ch4_aidful_skip",              Title = "Aidful Skip (Normal Version)",                    Category = "Major Skips",  Description = "Out of Bounds, Unrestricted",           Url = "https://youtu.be/7R7oSOLPwdQ"       },

            // Small Tricks
            new() { Id = "ch4_proacventing",             Title = "Proacventing (Fast Vents)",                       Category = "Small Tricks", Description = "Inbounds, Out of Bounds, Unrestricted", Url = "https://youtu.be/TdEvu_JCg4M"       },
            new() { Id = "ch4_shimmying",                Title = "Shimmying",                                       Category = "Small Tricks", Description = "All Categories",                        Url = "https://youtu.be/wKvSSlMnBWk"       },
            new() { Id = "ch4_quick_ac_puzzle",          Title = "Quick Air Conditioner Puzzle",                    Category = "Small Tricks", Description = "All Categories",                        Url = "https://youtu.be/weJXK5_jcYI"       },
            new() { Id = "ch4_frozen_hand_bypass",       Title = "Frozen Hand Bypass",                              Category = "Small Tricks", Description = "All Categories",                        Url = "https://youtu.be/F7xWoQJ4ah8"       },
            new() { Id = "ch4_le_parkour",               Title = "Le Parkour",                                      Category = "Small Tricks", Description = "All Categories",                        Url = "https://youtu.be/URySFMRsFDQ"       }
        ),

        // ─── CHAPTER 5 ────────────────────────────────────────────────────────
        ..InChapter("Chapter 5",

            // Major Skips
            new() { Id = "ch5_lower_taper_fade_skip",        Title = "Lower Taper Fade Skip",                  Category = "Major Skips",  Description = "Unrestricted",                                                                                                   Url = "https://youtu.be/0zp2M5Q3mF4"                    },
            new() { Id = "ch5_fire_f11_skip",                Title = "Fire F11 Skip",                          Category = "Major Skips",  Description = "Unrestricted",                                                                                                   Url = "https://youtu.be/8pzFFrPFGVY"                    },
            new() { Id = "ch5_elevator_skip",                Title = "Elevator Skip",                          Category = "Major Skips",  Description = "Unrestricted, Out of Bounds — get hit & time the Red hand while getting hit by Huggy (RNG); Headphone Warning",   Url = "https://www.youtube.com/watch?v=6bthrB9OhEg"      },
            new() { Id = "ch5_outimals_oob_skip",            Title = "Outimals OOB Skip",                      Category = "Major Skips",  Description = "Unrestricted, Out of Bounds — grants Master Key early and skips to Giblet's hideout; precise movement required",  Url = "https://youtu.be/bT2YVHkzYzg"                    },
            new() { Id = "ch5_early_grabpack",               Title = "Early Grabpack",                         Category = "Major Skips",  Description = "Unrestricted — Patch 1 only; reload checkpoint before getting eaten by Chum to prevent crashes",                  Url = "https://youtu.be/yBTanZRCbYA"                    },
            new() { Id = "ch5_zero_gravity_67_skip",         Title = "0 Gravity (67 Skip)",                    Category = "Major Skips",  Description = "Unrestricted, Out of Bounds, Inbounds — get hit by Critter in 2nd elevator shaft grapple area to hover mid-air",  Url = "https://youtu.be/gkBFN_3WmHk"                    },
            new() { Id = "ch5_zero_gravity_updated",         Title = "0 Gravity (67 Skip) Updated Route",      Category = "Major Skips",  Description = "Unrestricted",                                                                                                   Url = "https://www.youtube.com/watch?v=38M1-1PXDs4"      },
            new() { Id = "ch5_zero_gravity_oob",             Title = "0 Gravity (67 Skip) Updated Route OOB",  Category = "Major Skips",  Description = "Unrestricted, Out of Bounds",                                                                                    Url = "https://youtu.be/jEuUNBPD8mo"                    },
            new() { Id = "ch5_gilbert_skip",                 Title = "Gilbert Skip",                           Category = "Major Skips",  Description = "Unrestricted, Out of Bounds, Inbounds — difficult jumps; reload checkpoint when saving icon appears",              Url = "https://www.youtube.com/watch?v=DxtDrdUn4QA"      },
            new() { Id = "ch5_magnet_cuff_box_skip",         Title = "Magnet Cuff Room Box Skip",              Category = "Major Skips",  Description = "Unrestricted, Out of Bounds, Inbounds — use the box to jump up on the shelves",                                   Url = "https://www.youtube.com/watch?v=m7nfM0Oa-IA"      },
            new() { Id = "ch5_huggy_memories_inbounds",      Title = "Huggy's Memories Skip (Inbounds)",       Category = "Major Skips",  Description = "Unrestricted, Out of Bounds, Inbounds",                                                                          Url = "https://www.youtube.com/watch?v=yzuKwJAlzQg"      },
            new() { Id = "ch5_huggy_memories_oob",           Title = "Huggy's Memories Skip (OOB)",            Category = "Major Skips",  Description = "Unrestricted, Out of Bounds — deposit 1 Memory into Machine & spam both hands while loading into first Dream",     Url = "https://youtu.be/iPBlDBxYls4"                    },
            new() { Id = "ch5_huggy_memories_unrestricted",  Title = "Huggy's Memories Skip (Unrestricted)",   Category = "Major Skips",  Description = "Unrestricted — hardware-dependent; requires well-timed F11 presses or severely lowered PC performance",            Url = "https://youtu.be/0KYxlmu9bLM"                    },
            new() { Id = "ch5_fent_skip",                    Title = "Fent Skip",                              Category = "Major Skips",  Description = "Unrestricted, Out of Bounds, Inbounds — skips the Valve Puzzle for one memory card",                              Url = "https://youtu.be/dMttzMp0bZ4"                    },
            new() { Id = "ch5_sid_skip",                     Title = "SID Skip",                               Category = "Major Skips",  Description = "Unrestricted",                                                                                                   Url = "https://youtu.be/5qqmLbt5zDA"                    },
            new() { Id = "ch5_sid_skip_no_f11",              Title = "SID Skip (No F11)",                      Category = "Major Skips",  Description = "Unrestricted, Out of Bounds",                                                                                    Url = "https://www.youtube.com/watch?v=Rx7y9Dzqe7o"      },
            new() { Id = "ch5_megabonk_skip",                Title = "Megabonk Skip",                          Category = "Major Skips",  Description = "Unrestricted, Out of Bounds",                                                                                    Url = "https://youtu.be/lhJRbSbKlZw"                    },
            new() { Id = "ch5_dollhouse_key_oob",            Title = "Dollhouse Key OOB",                      Category = "Major Skips",  Description = "Unrestricted, Out of Bounds — collect and place all friends first",                                               Url = "https://youtu.be/lhJRbSbKlZw"                    },
            new() { Id = "ch5_dollhouse_key_oob_faster",     Title = "Dollhouse Key OOB but Faster",           Category = "Major Skips",  Description = "Unrestricted, Out of Bounds",                                                                                    Url = "https://www.youtube.com/watch?v=Z2g7l9GVz2Y"      },
            new() { Id = "ch5_lily_chase_skip",              Title = "Lily Chase Skip",                        Category = "Major Skips",  Description = "Unrestricted, Out of Bounds, Inbounds — reload checkpoint after cutscene, then shoot hands at far magnetized wall", Url = "https://youtu.be/M5_f3a5yWzw"                   },
            new() { Id = "ch5_peaks_of_yore_skip",           Title = "Peaks of Yore Skip",                     Category = "Major Skips",  Description = "Unrestricted, Out of Bounds, Inbounds — near end of Lily chase; grab magnet strip with both hands to get above",   Url = "https://youtu.be/OJl497Iq57w"                    },
            new() { Id = "ch5_reanimation_skip",             Title = "Reanimation Skip",                       Category = "Major Skips",  Description = "Unrestricted, Out of Bounds — grab box, bring it OOB, jump on top to get above the map",                          Url = "https://youtu.be/8yn9rA5LI0Q"                    },
            new() { Id = "ch5_clanker_skip",                 Title = "Clanker Skip",                           Category = "Major Skips",  Description = "Unrestricted, Out of Bounds — trigger 'no more games, no more lies' dialogue first; area won't load otherwise",    Url = "https://youtu.be/loEGTZXmedU"                    },
            new() { Id = "ch5_computer_skip",                Title = "Computer Skip",                          Category = "Major Skips",  Description = "Unrestricted, Out of Bounds, Inbounds — pull levers through lockers by aiming above them",                        Url = "https://youtu.be/zIVch4Q4eq4"                    },

            // Small Tricks
            new() { Id = "ch5_small_intro_timesave",         Title = "Small Intro Timesave",                   Category = "Small Tricks", Description = "All Categories — grabs 2nd metal piece early",                                                               Url = "https://youtu.be/8hR_j6f27Rg"                    },
            new() { Id = "ch5_first_scanner_gate_jump",      Title = "First Scanner Gate Jump",                Category = "Small Tricks", Description = "All Categories",                                                                                             Url = "https://youtu.be/gbzaSQoIM7Q"                    },
            new() { Id = "ch5_fast_1st_elevator_shaft",      Title = "Fast 1st Elevator Shaft",                Category = "Small Tricks", Description = "All Categories",                                                                                             Url = "https://youtu.be/uzQPYh3quYI"                    },
            new() { Id = "ch5_spiderman_grapples",           Title = "Spiderman Grapples",                     Category = "Small Tricks", Description = "All Categories — saves grappling time; mostly timing",                                                        Url = "https://youtu.be/srSg14GaCWg"                    },
            new() { Id = "ch5_huggy_bossfight_route",        Title = "Huggy Bossfight Route (Any%)",           Category = "Small Tricks", Description = "All Categories",                                                                                             Url = "https://youtu.be/4uPKfRPgljc"                    },
            new() { Id = "ch5_sweet_street_last_section",    Title = "Sweet Street Last Section Fast",         Category = "Small Tricks", Description = "All Categories",                                                                                             Url = "https://youtu.be/QdrPRI-MH3M"                    },
            new() { Id = "ch5_finding_friends_route",        Title = "Finding Friends Route",                  Category = "Small Tricks", Description = "All Categories",                                                                                             Url = "https://youtu.be/SXvcGmZBJkA"                    },
            new() { Id = "ch5_2_cycle_lily_rlgl",            Title = "2 Cycle Lily Red Light/Green Light",     Category = "Small Tricks", Description = "All Categories",                                                                                             Url = "https://youtu.be/AGNZiN8qejc"                    },
            new() { Id = "ch5_small_last_huggy_chase_skip",  Title = "Small Last Huggy Chase Skip",            Category = "Small Tricks", Description = "All Categories",                                                                                             Url = "https://youtu.be/77cx0gpCvLQ"                    },

            // Legacy
            new() { Id = "ch5_dollhouse_early_lights_out",   Title = "Dollhouse Early Lights Out",             Category = "Legacy",       Description = "NG+ only — complete Dollhouse once without restarting before new run; not allowed in any regular category",    Url = "https://www.youtube.com/watch?v=htVnk7Uuw-c"      }
        ),
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
