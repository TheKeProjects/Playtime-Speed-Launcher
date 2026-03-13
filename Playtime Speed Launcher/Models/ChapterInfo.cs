namespace SpeedrunLauncher.Models;

public class ChapterInfo
{
    public int Number { get; set; }
    public string Title { get; set; } = "";
    public string SubTitle { get; set; } = "";
    public string Description { get; set; } = "";
    public string SteamFolderName { get; set; } = "";
    /// <summary>
    /// If set, the chapter lives at steamapps\common\Poppy Playtime\{SubFolderName}
    /// instead of being its own top-level Steam game folder.
    /// </summary>
    public string? SubFolderName { get; set; }
    public int SteamAppId { get; set; }
    public string? GameExePath { get; set; }
    public string? DetectedVersion { get; set; }
    public bool IsAvailable { get; set; } = true;
    public bool IsInstalled => !string.IsNullOrEmpty(GameExePath) && File.Exists(GameExePath);
    public List<ChapterPreset> Presets { get; set; } = [];

    public static List<ChapterInfo> GetAll() =>
    [
        new ChapterInfo
        {
            Number = 1,
            Title = "UN FUERTE APRETÓN",
            SubTitle = "Chapter 1: A Tight Squeeze",
            Description = "Como exempleado de Playtime Co., al fin vuelves a la fábrica muchos años después de que todos adentro desaparecieron.",
            SteamFolderName = "Poppy Playtime",
            SubFolderName = @"WindowsNoEditor\Poppy_Playtime",
            SteamAppId = 1721470,
            Presets =
            [
                new() { Name = "Any% <1.1",     AppId = 1721470, DepotId = 1721471, ManifestId = "8897518061680100138", DownloadSize = "5,79 GB" },
                new() { Name = "NMG <1.1",      AppId = 1721470, DepotId = 1721471, ManifestId = "5760656490992572463", DownloadSize = "5,79 GB" },
                new() { Name = "NMG/Any% 1.2",  AppId = 1721470, DepotId = 1721471, ManifestId = "1908539100791719856", DownloadSize = "5,79 GB" },
            ],
        },
        new ChapterInfo
        {
            Number = 2,
            Title = "ATRAPADO EN LA RED",
            SubTitle = "Chapter 2: Fly in a Web",
            Description = "Profundiza en los oscuros secretos de Playtime Co. mientras escapas de las garras de Mommy Long Legs en la Game Station.",
            SteamFolderName = "Poppy Playtime",
            SubFolderName = @"WindowsNoEditor\Playtime_Prototype4",
            SteamAppId = 1882830,
            Presets =
            [
                new() { Name = "Parche 1.0", AppId = 1817490, DepotId = 1817491, ManifestId = "3472064775658982753", DownloadSize = "8,40 GB" },
                new() { Name = "Parche 1.1", AppId = 1817490, DepotId = 1817491, ManifestId = "2957411760343078601", DownloadSize = "8,40 GB" },
                new() { Name = "Parche 1.2", AppId = 1817490, DepotId = 1817491, ManifestId = "6474053569987505700", DownloadSize = "8,40 GB" },
            ],
        },
        new ChapterInfo
        {
            Number = 3,
            Title = "SUEÑO PROFUNDO",
            SubTitle = "Chapter 3: Deep Sleep",
            Description = "Desciende al Game Station y enfrenta los horrores que acechan en los niveles más profundos de la fábrica abandonada.",
            SteamFolderName = "Poppy Playtime",
            SubFolderName = "PoppyPlaytime_Chapter3",
            SteamAppId = 2395280,
            Presets =
            [
                new() { Name = "Parche 1.0", AppId = 2555190, DepotId = 2555198, ManifestId = "6195016546179599385", DownloadSize = "36,5 GB" },
            ],
        },
        new ChapterInfo
        {
            Number = 4,
            Title = "EL VIAJE OSCURO",
            SubTitle = "Chapter 4: The Dark Ride",
            Description = "Aventúrate por los oscuros pasillos de la fábrica en busca de la verdad sobre los experimentos de Playtime Co.",
            SteamFolderName = "Poppy Playtime",
            SubFolderName = "Playtime_Chapter4",
            SteamAppId = 2877340,
            Presets =
            [
                new() { Name = "Parche 1.0", AppId = 3008670, DepotId = 3008670, ManifestId = "692859848352034537", DownloadSize = "7,25 GB" },
            ],
        },
        new ChapterInfo
        {
            Number = 5,
            Title = "COSAS ROTAS",
            SubTitle = "Chapter 5: Coming Soon",
            Description = "El siguiente capítulo de la saga de Poppy Playtime está en desarrollo.",
            SteamFolderName = "Poppy Playtime",
            SubFolderName = "Chapter5",
            SteamAppId = 4100940,
            Presets =
            [
                new() { Name = "Parche 1", AppId = 4100940, DepotId = 4100940, ManifestId = "4806573455248764564", DownloadSize = "11,1 GB" },
                new() { Name = "Parche 2", AppId = 4100940, DepotId = 4100940, ManifestId = "2433159361022984191", DownloadSize = "11,1 GB" },
            ],
        },
    ];
}
