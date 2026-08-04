using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using IOPath = System.IO.Path;

namespace SpeedrunLauncher.Services;

/// <summary>Lets players submit a new community hand-skin mod straight from the launcher — sent as
/// a Discord embed (with the mod's files attached) to a review webhook, mirroring
/// <see cref="SkipReportService"/>'s pattern for skip submissions.</summary>
public static class HandModSubmissionService
{
    // TODO: replace with the real Discord webhook URL for hand mod submissions before shipping.
    private const string WebhookUrl =
        "WebhookUrl";

    /// <summary>Fixed palette of hand colors a submitted mod can be tagged as reskinning —
    /// matches the in-game hand colors, same idea as <c>HandModsService</c>'s hand.txt convention.</summary>
    public static readonly string[] Colors =
        ["Blue", "Red", "Green", "Purple", "Flare", "OmiHand", "Pressurized", "Conductive"];

    private static readonly HttpClient Http = new() { Timeout = TimeSpan.FromSeconds(60) };

    public static bool IsConfigured => !string.IsNullOrWhiteSpace(WebhookUrl);

    public static async Task<(bool Success, string? Error)> SubmitAsync(
        string modName, int chapterNumber, IReadOnlyCollection<string> colors,
        IReadOnlyList<string> filePaths, string? submitterName)
    {
        if (!IsConfigured) return (false, "Submissions aren't configured yet. Try again later.");
        if (filePaths.Count == 0) return (false, "Attach at least one mod file first.");

        try
        {
            var fields = new List<object>
            {
                new { name = "Mod Name", value = modName,                   inline = true },
                new { name = "Chapter",  value = $"Chapter {chapterNumber}", inline = true },
            };
            if (colors.Count > 0)
                fields.Add(new { name = "Hand Color(s)", value = string.Join(", ", colors), inline = true });
            if (!string.IsNullOrEmpty(submitterName))
                fields.Add(new { name = "Submitted By", value = submitterName, inline = true });

            var payload = new
            {
                embeds = new[]
                {
                    new
                    {
                        title     = "New Hand Mod Submission",
                        color     = 52394, // #00CCAA
                        fields    = fields.ToArray(),
                        footer    = new { text = $"Sent via Playtime Turbo · {AppVersion.GetDisplayVersion()}" },
                        timestamp = DateTime.UtcNow.ToString("o"),
                    }
                }
            };

            using var form = new MultipartFormDataContent();
            form.Add(new StringContent(JsonSerializer.Serialize(payload), Encoding.UTF8, "application/json"), "payload_json");

            for (int i = 0; i < filePaths.Count; i++)
            {
                var bytes       = await File.ReadAllBytesAsync(filePaths[i]);
                var fileContent = new ByteArrayContent(bytes);
                fileContent.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
                form.Add(fileContent, $"files[{i}]", IOPath.GetFileName(filePaths[i]));
            }

            var response = await Http.PostAsync(WebhookUrl, form);
            return response.IsSuccessStatusCode
                ? (true, null)
                : (false, $"Failed ({(int)response.StatusCode}). Try again.");
        }
        catch (Exception ex)
        {
            return (false, $"Error: {ex.Message}");
        }
    }
}
