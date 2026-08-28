using System.IO.Compression;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using SpeedrunLauncher.Services.App;
using IOPath = System.IO.Path;

namespace SpeedrunLauncher.Services.GameFiles;

/// <summary>Lets players submit a new community hand-skin mod straight from the launcher — sent as
/// a Discord embed (with the mod's files zipped and attached) to a review webhook, mirroring
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

    // Discord's webhook attachment cap depends on the target guild's boost level (8 MB with no
    // boosts, up to 100 MB at tier 3) — this webhook's server isn't boosted, and regular (non-bot)
    // webhooks don't get the newer 25 MB baseline either, so 8 MB is the only per-message size
    // that's guaranteed to go through. Leave a little headroom under that for the JSON payload part.
    public const long MaxAttachmentBytes = 8L * 1024 * 1024;
    private const long ChunkBytes = MaxAttachmentBytes - 128 * 1024;

    // Hard ceiling on how many messages one submission can spam the review channel with.
    // 24 parts × ~7.9 MB ≈ 190 MB zipped, which comfortably covers real hand-mod pak sets.
    public const int MaxParts = 24;

    private static readonly HttpClient Http = new() { Timeout = TimeSpan.FromSeconds(60) };

    public static bool IsConfigured => !string.IsNullOrWhiteSpace(WebhookUrl);

    public static async Task<(bool Success, string? Error)> SubmitAsync(
        string modName, int chapterNumber, IReadOnlyCollection<string> colors,
        IReadOnlyList<string> filePaths, string? submitterName, string? submitterId = null)
    {
        if (!IsConfigured) return (false, "Submissions aren't configured yet. Try again later.");
        if (filePaths.Count == 0) return (false, "Attach at least one mod file first.");

        byte[] zipBytes;
        var zipName = SanitizeFileName(modName) + ".zip";
        try
        {
            zipBytes = await Task.Run(() => BuildZip(filePaths));
        }
        catch (Exception ex)
        {
            return (false, $"Couldn't package your files: {ex.Message}");
        }

        var partCount = Math.Max(1, (int)Math.Ceiling(zipBytes.Length / (double)ChunkBytes));
        if (partCount > MaxParts)
        {
            var zippedMb = zipBytes.Length / 1024.0 / 1024.0;
            var maxMb    = MaxParts * ChunkBytes / 1024.0 / 1024.0;
            return (false, $"Your mod is too large even zipped ({zippedMb:F1} MB). " +
                            $"Keep it under about {maxMb:F0} MB total, then try again.");
        }

        var fields = new List<object>
        {
            new { name = "Mod Name", value = modName,                   inline = true },
            new { name = "Chapter",  value = $"Chapter {chapterNumber}", inline = true },
        };
        if (colors.Count > 0)
            fields.Add(new { name = "Hand Color(s)", value = string.Join(", ", colors), inline = true });
        if (!string.IsNullOrEmpty(submitterName))
            fields.Add(new { name = "Submitted By", value = submitterName, inline = true });

        try
        {
            for (int part = 0; part < partCount; part++)
            {
                var offset = part * ChunkBytes;
                var length = (int)Math.Min(ChunkBytes, zipBytes.Length - offset);
                var chunk  = new byte[length];
                Array.Copy(zipBytes, offset, chunk, 0, length);

                object payload;
                if (part == 0)
                {
                    // Pings the submitter above the embed — same pattern SendDiscordBugReportAsync
                    // uses in MainWindow.xaml.cs — so reviewers can follow up with them directly.
                    payload = new
                    {
                        content = BuildFirstMessageContent(submitterId, zipName, partCount),
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
                }
                else
                {
                    payload = new { content = $"**{modName}** — part {part + 1}/{partCount}" };
                }

                var partFileName = partCount > 1 ? $"{zipName}.{part + 1:000}" : zipName;
                var response = await PostPartAsync(payload, chunk, partFileName);

                if (!response.IsSuccessStatusCode)
                {
                    if ((int)response.StatusCode == 413)
                        return (false, partCount > 1
                            ? $"Part {part + 1} of {partCount} was still too large for Discord. Try trimming your mod files."
                            : "Those files are too large for Discord to accept. Zip them tighter or remove some files, then try again.");

                    return (false, $"Failed sending part {part + 1}/{partCount} ({(int)response.StatusCode}). Try again.");
                }

                // Stay comfortably under Discord's per-webhook rate limit (5 requests / 2s).
                if (part < partCount - 1)
                    await Task.Delay(1200);
            }

            return (true, null);
        }
        catch (Exception ex)
        {
            return (false, $"Error: {ex.Message}");
        }
    }

    private static string BuildFirstMessageContent(string? submitterId, string zipName, int partCount)
    {
        var mention = string.IsNullOrEmpty(submitterId) ? "" : $"<@{submitterId}>";
        if (partCount <= 1) return string.IsNullOrEmpty(mention) ? null! : mention;

        var instructions =
            $"This mod's zip didn't fit in one message, so it's split into {partCount} parts below. " +
            $"Download all {partCount} attachments into the same folder, then merge them back into `{zipName}` before extracting:\n" +
            $"• Windows (cmd): `copy /b {zipName}.001+{zipName}.002+... {zipName}`\n" +
            $"• Mac/Linux: `cat {zipName}.* > {zipName}`";

        return string.IsNullOrEmpty(mention) ? instructions : $"{mention} {instructions}";
    }

    private static async Task<HttpResponseMessage> PostPartAsync(object payload, byte[] fileBytes, string fileName)
    {
        const int maxAttempts = 5;
        HttpResponseMessage response = null!;

        for (int attempt = 0; attempt < maxAttempts; attempt++)
        {
            using var form = new MultipartFormDataContent();
            form.Add(new StringContent(JsonSerializer.Serialize(payload), Encoding.UTF8, "application/json"), "payload_json");

            var fileContent = new ByteArrayContent(fileBytes);
            fileContent.Headers.ContentType = new MediaTypeHeaderValue("application/octet-stream");
            form.Add(fileContent, "files[0]", fileName);

            response = await Http.PostAsync(WebhookUrl, form);
            if ((int)response.StatusCode != 429 || attempt == maxAttempts - 1) return response;

            var retryAfter = response.Headers.RetryAfter?.Delta?.TotalSeconds ?? 1.5;
            response.Dispose();
            await Task.Delay(TimeSpan.FromSeconds(Math.Max(retryAfter, 0.5) + 0.25));
        }

        return response;
    }

    private static byte[] BuildZip(IReadOnlyList<string> filePaths)
    {
        using var ms = new MemoryStream();
        using (var archive = new ZipArchive(ms, ZipArchiveMode.Create, leaveOpen: true))
        {
            foreach (var path in filePaths)
            {
                var entry = archive.CreateEntry(IOPath.GetFileName(path), CompressionLevel.Optimal);
                using var entryStream = entry.Open();
                using var fileStream  = File.OpenRead(path);
                fileStream.CopyTo(entryStream);
            }
        }
        return ms.ToArray();
    }

    private static string SanitizeFileName(string name)
    {
        var invalid = IOPath.GetInvalidFileNameChars();
        var clean   = new string([.. name.Where(c => !invalid.Contains(c))]).Trim();
        return string.IsNullOrEmpty(clean) ? "mod" : clean;
    }
}
