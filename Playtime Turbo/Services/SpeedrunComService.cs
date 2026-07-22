using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net.Http;
using System.Text.Json;
using System.Threading.Tasks;

namespace SpeedrunLauncher.Services;

public class SpeedrunVariableValue
{
    public string Id    { get; set; } = string.Empty;
    public string Label { get; set; } = string.Empty;
}

/// <summary>A secondary subcategory variable (Route, Restriction, Game Version, ...) exposed as a selector.</summary>
public class SpeedrunVariable
{
    public string                     Id              { get; set; } = string.Empty;
    public string                     Name            { get; set; } = string.Empty;
    public List<SpeedrunVariableValue> Values         { get; set; } = [];
    public string                     SelectedValueId { get; set; } = string.Empty;
}

public class SpeedrunRunType
{
    public string GameId         { get; set; } = string.Empty;
    public string CategoryId     { get; set; } = string.Empty;
    public string Label          { get; set; } = string.Empty;
    public string PrimaryVarId   { get; set; } = string.Empty;
    public string PrimaryValueId { get; set; } = string.Empty;
    public List<SpeedrunVariable> SecondaryVariables { get; set; } = [];
}

public class SpeedrunLeaderboardEntry
{
    public int    Place       { get; set; }
    public string PlayerName  { get; set; } = string.Empty;
    public double TimeSeconds { get; set; }
    public string RunWeblink  { get; set; } = string.Empty;
}

public class SpeedrunComService : IDisposable
{
    private const string ApiBase = "https://www.speedrun.com/api/v1";

    // Chapter number -> speedrun.com game id (matched against this launcher's chapters).
    public static readonly Dictionary<int, string> ChapterGameIds = new()
    {
        { 1, "w6j7vpx6" }, // Poppy Playtime: Chapter 1 (A Tight Squeeze)
        { 2, "4d7nqx36" }, // Poppy Playtime: Chapter 2 (Fly in a Web)
        { 3, "w6jge376" }, // Poppy Playtime: Chapter 3 (Deep Sleep)
        { 4, "v1pl8ep6" }, // Poppy Playtime: Chapter 4 (Safe Haven)
        { 5, "w6j9lr5d" }, // Poppy Playtime: Chapter 5 (Broken Things)
    };

    private readonly HttpClient _httpClient;
    private bool _disposed;

    public SpeedrunComService()
    {
        _httpClient = new HttpClient();
        _httpClient.DefaultRequestHeaders.Add("User-Agent", "PlaytimeSpeedLauncher");
        _httpClient.Timeout = TimeSpan.FromSeconds(20);
    }

    /// <summary>
    /// Resolves the "types of runs" (subcategories, e.g. Any%/100%) for a chapter's PC leaderboard.
    /// Every other subcategory variable found on the category (Route, Restriction, Game Version, ...)
    /// is returned as a selector on the run type, defaulted to speedrun.com's own default value,
    /// instead of being collapsed into a single fixed combination.
    /// </summary>
    public async Task<List<SpeedrunRunType>> GetRunTypesAsync(int chapterNumber)
    {
        var results = new List<SpeedrunRunType>();

        if (!ChapterGameIds.TryGetValue(chapterNumber, out var gameId))
            return results;

        try
        {
            var json = await _httpClient.GetStringAsync($"{ApiBase}/games/{gameId}/categories?embed=variables");
            using var doc = JsonDocument.Parse(json);

            JsonElement? chosenCategory = null;
            foreach (var cat in doc.RootElement.GetProperty("data").EnumerateArray())
            {
                if (cat.GetProperty("type").GetString() != "per-game") continue;

                var name = cat.GetProperty("name").GetString() ?? "";
                if (string.Equals(name, "PC", StringComparison.OrdinalIgnoreCase))
                {
                    chosenCategory = cat;
                    break;
                }
                chosenCategory ??= cat;
            }

            if (chosenCategory is not { } category)
                return results;

            var categoryId   = category.GetProperty("id").GetString() ?? "";
            var categoryName = category.GetProperty("name").GetString() ?? "";

            JsonElement? primaryVar = null;
            var secondary = new List<SpeedrunVariable>();

            if (category.TryGetProperty("variables", out var variablesWrapper) &&
                variablesWrapper.TryGetProperty("data", out var variables))
            {
                foreach (var v in variables.EnumerateArray())
                {
                    if (!v.TryGetProperty("is-subcategory", out var isSub) || !isSub.GetBoolean())
                        continue;

                    var varName = v.GetProperty("name").GetString() ?? "";

                    // The Any%/100%/All Stages split is always named "Category" or "Subcategory"
                    // on speedrun.com. Everything else (Route, Restriction, Game Version, ...) is
                    // a secondary axis and becomes a selector instead of forking into its own card.
                    if (primaryVar is null &&
                        (string.Equals(varName, "Category", StringComparison.OrdinalIgnoreCase) ||
                         string.Equals(varName, "Subcategory", StringComparison.OrdinalIgnoreCase)))
                    {
                        primaryVar = v;
                        continue;
                    }

                    var valuesObj = v.GetProperty("values");
                    var values = new List<SpeedrunVariableValue>();
                    foreach (var kv in valuesObj.GetProperty("values").EnumerateObject())
                    {
                        var label = kv.Value.TryGetProperty("label", out var labelEl) ? labelEl.GetString() ?? kv.Name : kv.Name;
                        values.Add(new SpeedrunVariableValue { Id = kv.Name, Label = label });
                    }
                    if (values.Count == 0) continue;

                    var defaultId = valuesObj.TryGetProperty("default", out var defEl) && defEl.ValueKind == JsonValueKind.String
                        ? defEl.GetString()
                        : null;

                    secondary.Add(new SpeedrunVariable
                    {
                        Id              = v.GetProperty("id").GetString() ?? "",
                        Name            = varName,
                        Values          = values,
                        SelectedValueId = defaultId ?? values[0].Id,
                    });
                }
            }

            if (primaryVar is not { } pv)
            {
                // No Any%/100% split on this category (e.g. it's already scoped to a single route,
                // like a very new chapter) -- one run type card carrying whatever selectors exist.
                results.Add(new SpeedrunRunType
                {
                    GameId             = gameId,
                    CategoryId         = categoryId,
                    Label              = categoryName,
                    SecondaryVariables = secondary,
                });
                return results;
            }

            var pvId = pv.GetProperty("id").GetString() ?? "";
            foreach (var kv in pv.GetProperty("values").GetProperty("values").EnumerateObject())
            {
                var label = kv.Value.TryGetProperty("label", out var labelEl) ? labelEl.GetString() ?? kv.Name : kv.Name;

                // Each run type owns an independent copy of the selectors so switching the
                // Route on "Any%" doesn't affect "100%"'s selection.
                var clonedSecondary = secondary
                    .Select(s => new SpeedrunVariable { Id = s.Id, Name = s.Name, Values = s.Values, SelectedValueId = s.SelectedValueId })
                    .ToList();

                results.Add(new SpeedrunRunType
                {
                    GameId             = gameId,
                    CategoryId         = categoryId,
                    Label              = label,
                    PrimaryVarId       = pvId,
                    PrimaryValueId     = kv.Name,
                    SecondaryVariables = clonedSecondary,
                });
            }
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[SpeedrunComService] Failed to fetch run types for chapter {chapterNumber}: {ex.Message}");
        }

        return results;
    }

    /// <summary>Builds the var-XXXX=YYYY query string for a run type's current selector state.</summary>
    public static string BuildVarQuery(SpeedrunRunType runType)
    {
        var parts = new List<string>();
        if (!string.IsNullOrEmpty(runType.PrimaryVarId) && !string.IsNullOrEmpty(runType.PrimaryValueId))
            parts.Add($"var-{runType.PrimaryVarId}={runType.PrimaryValueId}");

        foreach (var v in runType.SecondaryVariables)
            if (!string.IsNullOrEmpty(v.SelectedValueId))
                parts.Add($"var-{v.Id}={v.SelectedValueId}");

        return string.Join("&", parts);
    }

    /// <summary>Top 10 runners for a run type, using its current selector state.</summary>
    public async Task<List<SpeedrunLeaderboardEntry>> GetTopRunnersAsync(SpeedrunRunType runType)
    {
        var results = new List<SpeedrunLeaderboardEntry>();

        try
        {
            var varQuery = BuildVarQuery(runType);
            var url = $"{ApiBase}/leaderboards/{runType.GameId}/category/{runType.CategoryId}?top=10&embed=players" +
                       (string.IsNullOrEmpty(varQuery) ? "" : $"&{varQuery}");

            var json = await _httpClient.GetStringAsync(url);
            using var doc = JsonDocument.Parse(json);
            var data = doc.RootElement.GetProperty("data");

            var playerNames = new Dictionary<string, string>();
            if (data.TryGetProperty("players", out var playersEmbed) &&
                playersEmbed.TryGetProperty("data", out var playersData))
            {
                foreach (var p in playersData.EnumerateArray())
                {
                    if (!p.TryGetProperty("id", out var idEl)) continue;
                    var id = idEl.GetString() ?? "";
                    var name = p.TryGetProperty("names", out var namesEl) &&
                               namesEl.TryGetProperty("international", out var intlEl)
                        ? intlEl.GetString() ?? id
                        : id;
                    playerNames[id] = name;
                }
            }

            foreach (var entry in data.GetProperty("runs").EnumerateArray())
            {
                var place = entry.GetProperty("place").GetInt32();
                if (place <= 0) continue;

                var run         = entry.GetProperty("run");
                var weblink     = run.GetProperty("weblink").GetString() ?? "";
                var timeSeconds = run.GetProperty("times").GetProperty("primary_t").GetDouble();

                var names = new List<string>();
                foreach (var pl in run.GetProperty("players").EnumerateArray())
                {
                    if (pl.TryGetProperty("rel", out var relEl) && relEl.GetString() == "guest" &&
                        pl.TryGetProperty("name", out var guestNameEl))
                    {
                        names.Add(guestNameEl.GetString() ?? "Guest");
                    }
                    else if (pl.TryGetProperty("id", out var pidEl))
                    {
                        var pid = pidEl.GetString() ?? "";
                        names.Add(playerNames.TryGetValue(pid, out var n) ? n : pid);
                    }
                }

                results.Add(new SpeedrunLeaderboardEntry
                {
                    Place       = place,
                    PlayerName  = names.Count > 0 ? string.Join(" & ", names) : "Unknown",
                    TimeSeconds = timeSeconds,
                    RunWeblink  = weblink,
                });

                if (results.Count >= 10) break;
            }
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[SpeedrunComService] Failed to fetch leaderboard for {runType.CategoryId}: {ex.Message}");
        }

        return results;
    }

    public static string FormatTime(double seconds)
    {
        var ts = TimeSpan.FromSeconds(seconds);
        return ts.TotalHours >= 1
            ? $"{(int)ts.TotalHours}:{ts.Minutes:D2}:{ts.Seconds:D2}"
            : $"{ts.Minutes}:{ts.Seconds:D2}.{ts.Milliseconds:D3}";
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;
        _httpClient.Dispose();
    }
}
