using System.Text.Json;

namespace SpeedrunLauncher.Services.Chapters;

public class ChapterPlaytimeStore
{
    private static readonly string DataDir =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "SpeedrunLauncher");

    private static readonly string FilePath =
        Path.Combine(DataDir, "playtime.json");

    // chapter number (1-based) → total seconds played
    private Dictionary<int, double> _seconds = [];

    private ChapterPlaytimeStore() { }

    public static ChapterPlaytimeStore Load()
    {
        var store = new ChapterPlaytimeStore();
        if (!File.Exists(FilePath)) return store;
        try
        {
            var json = File.ReadAllText(FilePath);
            var data = JsonSerializer.Deserialize<Dictionary<string, double>>(json);
            if (data is null) return store;

            foreach (var (k, v) in data)
                if (int.TryParse(k, out var n))
                    store._seconds[n] = v;
        }
        catch { }
        return store;
    }

    public TimeSpan GetPlaytime(int chapter) =>
        TimeSpan.FromSeconds(_seconds.GetValueOrDefault(chapter, 0));

    public void AddSeconds(int chapter, double seconds)
    {
        _seconds[chapter] = _seconds.GetValueOrDefault(chapter, 0) + seconds;
        Save();
    }

    public static string Format(TimeSpan ts)
    {
        if (ts.TotalHours >= 1) return $"{(int)ts.TotalHours}h {ts.Minutes}m";
        if (ts.TotalMinutes >= 1) return $"{(int)ts.TotalMinutes}m";
        return "0m";
    }

    private void Save()
    {
        try
        {
            Directory.CreateDirectory(DataDir);
            var data = _seconds.ToDictionary(p => p.Key.ToString(), p => p.Value);
            File.WriteAllText(FilePath,
                JsonSerializer.Serialize(data, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch { }
    }
}
