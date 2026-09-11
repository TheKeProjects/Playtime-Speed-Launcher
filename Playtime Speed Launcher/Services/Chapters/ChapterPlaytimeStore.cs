using System.Text;
using System.Text.Json;
using SpeedrunLauncher.Services.Community;

namespace SpeedrunLauncher.Services.Chapters;

/// <summary>
/// Tracks per-chapter playtime in Supabase, keyed by the same per-install ID used by
/// <see cref="OnlineUsersService"/>, instead of a local file — so an interrupted update
/// or a bad write can no longer wipe a player's hours (see `playtime` table and the
/// `add_playtime()` / `get_playtime()` RPCs created in the Supabase project's SQL editor).
/// </summary>
public class ChapterPlaytimeStore
{
    private const string SupabaseUrl     = "SupabaseUrl";
    private const string SupabaseAnonKey = "SupabaseAnonKey";

    // Path of the old local-file store this class used to read/write. Kept only so
    // existing installs can have their already-tracked hours uploaded once instead of
    // silently resetting to zero after updating to the Supabase-backed store.
    private static readonly string LegacyFilePath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "SpeedrunLauncher", "playtime.json");

    private static readonly HttpClient _http = new();

    private readonly Guid _installId;
    private readonly object _lock = new();

    // chapter number (1-based) → total seconds played
    private readonly Dictionary<int, double> _seconds = [];
    // increments not yet confirmed as saved in Supabase
    private readonly Dictionary<int, double> _pending = [];
    private bool _syncing;

    /// <summary>Raised once the initial totals have been fetched from Supabase.</summary>
    public event Action? Loaded;

    private ChapterPlaytimeStore(Guid installId) => _installId = installId;

    public static ChapterPlaytimeStore Load()
    {
        var store = new ChapterPlaytimeStore(HeartbeatSettings.Current.InstallId);
        store.MigrateLegacyLocalFile();
        _ = store.RefreshFromCloudAsync();
        return store;
    }

    // One-time upgrade path: uploads whatever this install already had in the old local
    // playtime.json (if any) as pending increments, then deletes the file so it isn't
    // re-uploaded on the next launch. Best-effort — a corrupt legacy file is simply left
    // in place and ignored, same as before this store was moved to Supabase.
    private void MigrateLegacyLocalFile()
    {
        try
        {
            if (!File.Exists(LegacyFilePath)) return;

            var json = File.ReadAllText(LegacyFilePath);
            var data = JsonSerializer.Deserialize<Dictionary<string, double>>(json);
            if (data != null)
                foreach (var (key, value) in data)
                    if (int.TryParse(key, out var chapter) && value > 0)
                        AddSeconds(chapter, value);

            File.Delete(LegacyFilePath);
        }
        catch { }
    }

    public TimeSpan GetPlaytime(int chapter)
    {
        lock (_lock)
            return TimeSpan.FromSeconds(_seconds.GetValueOrDefault(chapter, 0));
    }

    public void AddSeconds(int chapter, double seconds)
    {
        lock (_lock)
        {
            _seconds[chapter] = _seconds.GetValueOrDefault(chapter, 0) + seconds;
            _pending[chapter] = _pending.GetValueOrDefault(chapter, 0) + seconds;
        }
        _ = FlushPendingAsync();
    }

    public static string Format(TimeSpan ts)
    {
        if (ts.TotalHours >= 1) return $"{(int)ts.TotalHours}h {ts.Minutes}m";
        if (ts.TotalMinutes >= 1) return $"{(int)ts.TotalMinutes}m";
        return "0m";
    }

    private async Task RefreshFromCloudAsync()
    {
        try
        {
            using var request = new HttpRequestMessage(HttpMethod.Post, $"{SupabaseUrl}/rest/v1/rpc/get_playtime")
            {
                Content = new StringContent(
                    JsonSerializer.Serialize(new { p_id = _installId }), Encoding.UTF8, "application/json")
            };
            request.Headers.Add("apikey", SupabaseAnonKey);
            request.Headers.Add("Authorization", $"Bearer {SupabaseAnonKey}");

            var response = await _http.SendAsync(request);
            if (!response.IsSuccessStatusCode) return;

            var json = await response.Content.ReadAsStringAsync();
            var rows = JsonSerializer.Deserialize<List<PlaytimeRow>>(json);
            if (rows is null) return;

            lock (_lock)
            {
                foreach (var row in rows)
                    // A row with unsynced pending seconds already added locally wins over
                    // the server snapshot fetched before those pending seconds landed.
                    if (!_pending.ContainsKey(row.chapter))
                        _seconds[row.chapter] = row.seconds;
            }
        }
        catch { }

        Loaded?.Invoke();
    }

    private async Task FlushPendingAsync()
    {
        lock (_lock)
        {
            if (_syncing) return;
            _syncing = true;
        }

        try
        {
            while (true)
            {
                int chapter;
                double amount;
                lock (_lock)
                {
                    if (_pending.Count == 0) return;
                    var next = _pending.First();
                    chapter = next.Key;
                    amount  = next.Value;
                    _pending.Remove(chapter);
                }

                var ok = await PushSecondsAsync(_installId, chapter, amount);
                if (!ok)
                {
                    lock (_lock)
                        _pending[chapter] = _pending.GetValueOrDefault(chapter, 0) + amount;
                    return; // retried on the next AddSeconds() call
                }
            }
        }
        finally
        {
            lock (_lock) _syncing = false;
        }
    }

    private static async Task<bool> PushSecondsAsync(Guid installId, int chapter, double seconds)
    {
        try
        {
            using var request = new HttpRequestMessage(HttpMethod.Post, $"{SupabaseUrl}/rest/v1/rpc/add_playtime")
            {
                Content = new StringContent(
                    JsonSerializer.Serialize(new { p_id = installId, p_chapter = chapter, p_seconds = seconds }),
                    Encoding.UTF8, "application/json")
            };
            request.Headers.Add("apikey", SupabaseAnonKey);
            request.Headers.Add("Authorization", $"Bearer {SupabaseAnonKey}");

            var response = await _http.SendAsync(request);
            return response.IsSuccessStatusCode;
        }
        catch
        {
            return false;
        }
    }

    private class PlaytimeRow
    {
        public int    chapter { get; set; }
        public double seconds { get; set; }
    }
}
