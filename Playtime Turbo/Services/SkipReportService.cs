using System.Text;
using System.Text.Json;

namespace SpeedrunLauncher.Services;

public static class SkipReportService
{
    // TODO: replace with the real Discord webhook URL for skip submissions before shipping.
    private const string WebhookUrl = "https://canary.discord.com/api/webhooks/1522343295265669282/pp1KTDerhujSDg2k1xSuA67gQG7TX7fi6NolRPxj98FVDXIzRjruYn1NPwlIeltJywqm";

    private static readonly HttpClient _http = new();

    public static bool IsConfigured => !string.IsNullOrWhiteSpace(WebhookUrl);

    public static async Task<(bool Success, string? Error)> SendReportAsync(
        string name, string chapter, string author, string videoUrl, string description,
        string version, IReadOnlyCollection<string> runCategories,
        IReadOnlyCollection<string> routes, IReadOnlyCollection<string> restrictions, bool cores)
    {
        if (!IsConfigured) return (false, "Reporting isn't configured yet. Try again later.");

        try
        {
            var fields = new List<object>
            {
                new { name = "Skip Name", value = name, inline = true }
            };
            if (!string.IsNullOrEmpty(chapter))     fields.Add(new { name = "Chapter",        value = chapter,                       inline = true  });
            if (!string.IsNullOrEmpty(version))     fields.Add(new { name = "Version",        value = version,                       inline = true  });
            if (runCategories.Count > 0)            fields.Add(new { name = "Run Category",   value = string.Join(", ", runCategories), inline = true });
            if (routes.Count > 0)                   fields.Add(new { name = "Route",          value = string.Join(", ", routes),        inline = true });
            if (restrictions.Count > 0)             fields.Add(new { name = "Restrictions",   value = string.Join(", ", restrictions),  inline = true });
                                                     fields.Add(new { name = "Uses Core",      value = cores ? "Yes" : "No",          inline = true  });
            if (!string.IsNullOrEmpty(author))      fields.Add(new { name = "Author",         value = author,                        inline = true  });
            if (!string.IsNullOrEmpty(videoUrl))    fields.Add(new { name = "Video URL",      value = videoUrl,                      inline = false });
            if (!string.IsNullOrEmpty(description)) fields.Add(new { name = "Description",    value = description,                   inline = false });

            var payload = new
            {
                embeds = new[]
                {
                    new
                    {
                        title  = "New Skip Submission",
                        color  = 52394, // #00CCAA
                        fields = fields.ToArray(),
                        footer = new { text = $"Sent via Playtime Turbo · {AppVersion.GetDisplayVersion()}" }
                    }
                }
            };

            string json     = JsonSerializer.Serialize(payload);
            var    content  = new StringContent(json, Encoding.UTF8, "application/json");
            var    response = await _http.PostAsync(WebhookUrl, content);

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
