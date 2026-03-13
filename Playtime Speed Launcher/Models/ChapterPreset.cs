namespace SpeedrunLauncher.Models;

public class ChapterPreset
{
    public string Name        { get; set; } = "";
    public int    AppId       { get; set; }
    public int    DepotId     { get; set; }
    public string ManifestId  { get; set; } = ""; // ulong as string to avoid overflow
    public string DownloadSize { get; set; } = "";

    public string Command => $"download_depot {AppId} {DepotId} {ManifestId}";
}
