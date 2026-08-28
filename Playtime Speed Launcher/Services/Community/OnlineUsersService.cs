using System.Text;
using System.Text.Json;

namespace SpeedrunLauncher.Services.Community;

/// <summary>
/// Reports this install as "online" via a periodic Supabase heartbeat and reports back
/// how many installs have checked in within the last 2 minutes (see the `presence` table
/// and `get_online_count()` RPC created in the Supabase project's SQL editor).
/// </summary>
public static class OnlineUsersService
{
    // TODO: replace with your real Supabase project URL and anon public key before shipping.
    private const string SupabaseUrl     = "SupabaseUrl";
    private const string SupabaseAnonKey = "SupabaseAnonKey";

    private static readonly HttpClient _http = new();

    private static CancellationTokenSource? _pollCts;

    public static event Action<int>? OnlineCountUpdated;

    public static bool IsConfigured =>
        !string.IsNullOrWhiteSpace(SupabaseUrl) && !SupabaseUrl.Contains("YOUR-PROJECT") &&
        !string.IsNullOrWhiteSpace(SupabaseAnonKey) && !SupabaseAnonKey.Contains("YOUR-ANON");

    public static void Start()
    {
        if (!IsConfigured || _pollCts != null) return;
        _pollCts = new CancellationTokenSource();
        _ = PollAsync(_pollCts.Token);
    }

    public static void Stop()
    {
        _pollCts?.Cancel();
        _pollCts = null;
    }

    private static async Task PollAsync(CancellationToken ct)
    {
        while (!ct.IsCancellationRequested)
        {
            try
            {
                await SendHeartbeatAsync();
                var count = await GetOnlineCountAsync();
                if (count.HasValue) OnlineCountUpdated?.Invoke(count.Value);
            }
            catch { }

            try { await Task.Delay(60_000, ct); }
            catch (OperationCanceledException) { break; }
        }
    }

    private static async Task SendHeartbeatAsync()
    {
        try
        {
            var body = JsonSerializer.Serialize(new
            {
                p_id = HeartbeatSettings.Current.InstallId
            });

            using var request = new HttpRequestMessage(HttpMethod.Post, $"{SupabaseUrl}/rest/v1/rpc/heartbeat")
            {
                Content = new StringContent(body, Encoding.UTF8, "application/json")
            };
            request.Headers.Add("apikey", SupabaseAnonKey);
            request.Headers.Add("Authorization", $"Bearer {SupabaseAnonKey}");

            await _http.SendAsync(request);
        }
        catch { }
    }

    private static async Task<int?> GetOnlineCountAsync()
    {
        try
        {
            using var request = new HttpRequestMessage(HttpMethod.Post, $"{SupabaseUrl}/rest/v1/rpc/get_online_count")
            {
                Content = new StringContent("{}", Encoding.UTF8, "application/json")
            };
            request.Headers.Add("apikey", SupabaseAnonKey);
            request.Headers.Add("Authorization", $"Bearer {SupabaseAnonKey}");

            var response = await _http.SendAsync(request);
            if (!response.IsSuccessStatusCode) return null;

            var json = await response.Content.ReadAsStringAsync();
            return JsonSerializer.Deserialize<int>(json);
        }
        catch
        {
            return null;
        }
    }
}
