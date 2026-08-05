using System.Diagnostics;
using System.IO.Compression;
using IOPath = System.IO.Path;

namespace SpeedrunLauncher.Services;

/// <summary>
/// Lists and installs the community hand-skin pak mods hosted at
/// github.com/An-Average-Developer/Hand-Mods-PP (one folder per chapter, each
/// holding zero or more `*.zip` mods plus a `mods.txt` manifest listing them).
/// Mirrors <see cref="UpdateService"/>'s approach: everything is fetched over
/// plain raw.githubusercontent.com file requests (a manifest text file, then a
/// HEAD request per mod for its size) rather than the rate-limited GitHub API,
/// and failures are swallowed the same way — an unreachable/missing chapter
/// just yields no mods instead of surfacing an error. Each zip is always three
/// flat files — `{Name}_P.pak/.ucas/.utoc` — dropped straight into Content\Paks,
/// with `{Name}` matching the zip's own filename. A zip may optionally also
/// ship a `hand.txt` declaring which hand color it reskins (there are more
/// than two — e.g. "Blue", "Green", "Yellow"), used to warn about and resolve
/// conflicts between two installed mods that touch the same color.
/// </summary>
public static class HandModsService
{
    private const string Owner  = "An-Average-Developer";
    private const string Repo   = "Hand-Mods-PP";
    private const string Branch = "main";

    /// <summary>Per-chapter manifest listing that chapter's mod zips, one filename per line
    /// (blank lines and lines starting with `#` are ignored) — same idea as the launcher's own
    /// version.txt/changelog.txt, just per chapter instead of per repo.</summary>
    private const string ManifestFileName = "mods.txt";

    private static readonly HttpClient Http = CreateClient();

    private static HttpClient CreateClient()
    {
        var client = new HttpClient { Timeout = TimeSpan.FromSeconds(30) };
        client.DefaultRequestHeaders.Add("User-Agent", "PlaytimeSpeedLauncher");
        return client;
    }

    public sealed class HandMod
    {
        public required string Name        { get; init; }
        public required string BaseName    { get; init; }
        public required string DownloadUrl { get; init; }
        public long            Size        { get; init; }
    }

    private static string GetChapterRawUrl(int chapterNumber, string fileName) =>
        $"https://raw.githubusercontent.com/{Owner}/{Repo}/{Branch}/Chapter%20{chapterNumber}/{Uri.EscapeDataString(fileName)}";

    /// <summary>Fetches the chapter's `mods.txt` manifest and resolves each listed zip into a
    /// <see cref="HandMod"/> (with a best-effort HEAD-request size lookup, same trick
    /// <see cref="UpdateService"/> uses for its own download size). Never throws — a missing
    /// manifest, an offline connection, or any other failure just yields an empty list, same as
    /// how <see cref="UpdateService"/> silently no-ops on a failed update check.</summary>
    public static async Task<List<HandMod>> GetModsAsync(int chapterNumber)
    {
        var mods = new List<HandMod>();

        string manifest;
        try
        {
            using var response = await Http.GetAsync(GetChapterRawUrl(chapterNumber, ManifestFileName));
            if (!response.IsSuccessStatusCode) return mods; // no manifest for this chapter yet
            manifest = await response.Content.ReadAsStringAsync();
        }
        catch (Exception ex)
        {
            Debug.WriteLine($"[HandModsService] Failed to fetch mod list for chapter {chapterNumber}: {ex.Message}");
            return mods;
        }

        foreach (var rawLine in manifest.Replace("\r", "").Split('\n'))
        {
            var fileName = rawLine.Trim();
            if (fileName.Length == 0 || fileName.StartsWith('#')) continue;
            if (!fileName.EndsWith(".zip", StringComparison.OrdinalIgnoreCase)) continue;

            var baseName    = IOPath.GetFileNameWithoutExtension(fileName);
            var downloadUrl = GetChapterRawUrl(chapterNumber, fileName);

            long size = 0;
            try
            {
                using var head = new HttpRequestMessage(HttpMethod.Head, downloadUrl);
                using var res  = await Http.SendAsync(head);
                if (res.IsSuccessStatusCode && res.Content.Headers.ContentLength.HasValue)
                    size = res.Content.Headers.ContentLength.Value;
            }
            catch { }

            mods.Add(new HandMod
            {
                Name        = baseName,
                BaseName    = baseName,
                DownloadUrl = downloadUrl,
                Size        = size,
            });
        }

        return mods;
    }

    public static string GetCacheDir(int chapterNumber) => IOPath.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "SpeedrunLauncher", "HandMods", $"Chapter{chapterNumber}");

    /// <summary>Downloads a mod's zip into the local cache (skipping the request entirely if
    /// an identically-sized copy is already cached) and returns the local zip path.</summary>
    public static async Task<string> DownloadModAsync(HandMod mod, int chapterNumber, IProgress<int>? progress = null)
    {
        var cacheDir = GetCacheDir(chapterNumber);
        Directory.CreateDirectory(cacheDir);
        var zipPath = IOPath.Combine(cacheDir, $"{mod.BaseName}.zip");

        if (File.Exists(zipPath) && mod.Size > 0 && new FileInfo(zipPath).Length == mod.Size)
        {
            progress?.Report(100);
            return zipPath;
        }

        using var response = await Http.GetAsync(mod.DownloadUrl, HttpCompletionOption.ResponseHeadersRead);
        response.EnsureSuccessStatusCode();
        var totalBytes = response.Content.Headers.ContentLength ?? mod.Size;
        var buffer     = new byte[8192];
        var bytesRead  = 0L;

        var tempPath = zipPath + ".tmp";
        using (var contentStream = await response.Content.ReadAsStreamAsync())
        using (var fileStream = new FileStream(tempPath, FileMode.Create, FileAccess.Write, FileShare.None, 8192, true))
        {
            int read;
            while ((read = await contentStream.ReadAsync(buffer.AsMemory())) > 0)
            {
                await fileStream.WriteAsync(buffer.AsMemory(0, read));
                bytesRead += read;
                if (totalBytes > 0)
                    progress?.Report((int)(bytesRead * 100 / totalBytes));
            }
        }

        File.Copy(tempPath, zipPath, overwrite: true);
        File.Delete(tempPath);
        return zipPath;
    }

    public static bool IsInstalled(string paksDir, string baseName) =>
        File.Exists(IOPath.Combine(paksDir, $"{baseName}_P.pak"));

    /// <summary>Filename mod authors can ship inside their zip (alongside the pak/ucas/utoc) to
    /// declare which hand color(s) it reskins — a comma-separated, quoted list, e.g.
    /// <c>"Blue", "Purple", "Red"</c> — since one mod can touch more than one color.</summary>
    private const string HandDeclarationEntry = "hand.txt";

    /// <summary>Sentinel used when a mod never declared any colors (predates the hand.txt
    /// convention, or shipped without one) — treated as conflicting with every other hand
    /// mod since its actual colors are unknown.</summary>
    public const string UnknownHand = "Unknown";

    private static string GetHandMarkerPath(string paksDir, string baseName) =>
        IOPath.Combine(paksDir, $"{baseName}.hand.txt");

    /// <summary>Extensions recognized as hand-preview images shipped inside a mod's zip, named
    /// <c>{name}_{color}.ext</c> (e.g. "Ninja_Blue.png") — one per reskinned hand color.</summary>
    private static readonly string[] ImageExtensions = [".png", ".jpg", ".jpeg", ".bmp", ".gif"];

    private static bool IsImageEntry(string entryName) =>
        ImageExtensions.Contains(IOPath.GetExtension(entryName), StringComparer.OrdinalIgnoreCase);

    public sealed class HandImage
    {
        public required string Color { get; init; }
        public required byte[] Data  { get; init; }
    }

    /// <summary>Reads every hand-preview image a mod's zip ships — named <c>{name}_{color}.ext</c>
    /// — straight out of the zip without extracting to disk. Returns an empty list if the zip
    /// ships none.</summary>
    public static List<HandImage> ReadHandImages(string zipPath)
    {
        var images = new List<HandImage>();
        using var zip = ZipFile.OpenRead(zipPath);
        foreach (var entry in zip.Entries)
        {
            if (string.IsNullOrEmpty(entry.Name)) continue;
            if (!IsImageEntry(entry.Name)) continue;

            var stem = IOPath.GetFileNameWithoutExtension(entry.Name);
            var underscoreIndex = stem.LastIndexOf('_');
            var color = underscoreIndex >= 0 && underscoreIndex < stem.Length - 1
                ? stem[(underscoreIndex + 1)..]
                : stem;

            using var stream = entry.Open();
            using var ms = new MemoryStream();
            stream.CopyTo(ms);
            images.Add(new HandImage { Color = color, Data = ms.ToArray() });
        }
        return images;
    }

    /// <summary>Parses a `hand.txt`-style declaration — comma-separated, optionally quoted
    /// color names, e.g. <c>"Blue", "Purple", "Red"</c> — into individual trimmed color names.
    /// Quotes and blank entries are stripped; returns an empty list for blank input.</summary>
    private static List<string> ParseHandColors(string raw)
    {
        var colors = new List<string>();
        foreach (var part in raw.Split(','))
        {
            var color = part.Trim().Trim('"', '\'').Trim();
            if (color.Length > 0) colors.Add(color);
        }
        return colors;
    }

    /// <summary>Reads the mod's declared hand color(s) straight out of its zip (before install),
    /// or null if the zip doesn't ship a <see cref="HandDeclarationEntry"/> yet (or it's blank).</summary>
    public static List<string>? ReadDeclaredHands(string zipPath)
    {
        using var zip = ZipFile.OpenRead(zipPath);
        foreach (var entry in zip.Entries)
        {
            if (!entry.Name.Equals(HandDeclarationEntry, StringComparison.OrdinalIgnoreCase)) continue;
            using var reader = new StreamReader(entry.Open());
            var colors = ParseHandColors(reader.ReadToEnd());
            return colors.Count > 0 ? colors : null;
        }
        return null;
    }

    /// <summary>Which hand color(s) a currently-installed mod affects, read from the marker left
    /// by <see cref="Install"/>. Defaults to a single <see cref="UnknownHand"/> entry (the
    /// conservative assumption) for mods installed before the hand.txt convention existed, or
    /// whose zip never declared any.</summary>
    public static List<string> GetInstalledHands(string paksDir, string baseName)
    {
        var path = GetHandMarkerPath(paksDir, baseName);
        if (!File.Exists(path)) return [UnknownHand];
        var colors = ParseHandColors(File.ReadAllText(path));
        return colors.Count > 0 ? colors : [UnknownHand];
    }

    /// <summary>True when two mods' hand-color declarations would visibly clash if both were
    /// installed at once — an undeclared/<see cref="UnknownHand"/> side clashes with anything
    /// (we can't tell it apart from any other mod), otherwise any shared color between the two
    /// lists clashes.</summary>
    public static bool HandsConflict(List<string> handsA, List<string> handsB)
    {
        if (handsA.Any(h => h.Equals(UnknownHand, StringComparison.OrdinalIgnoreCase))) return true;
        if (handsB.Any(h => h.Equals(UnknownHand, StringComparison.OrdinalIgnoreCase))) return true;
        return handsA.Any(a => handsB.Any(b => a.Equals(b, StringComparison.OrdinalIgnoreCase)));
    }

    /// <summary>Extracts the pak/ucas/utoc into paksDir — deliberately skipping the zip's own
    /// hand.txt entry, since extracting it verbatim would leave a bare, mod-agnostic "hand.txt"
    /// sitting in Paks that every later install/uninstall would silently clobber or orphan —
    /// and records which hand color(s) this mod affects under a per-mod marker name instead
    /// (from the zip's hand.txt, defaulting to <see cref="UnknownHand"/>, unless
    /// <paramref name="declaredHands"/> already supplies them — used for an Extras variant,
    /// whose own zip carries no hand.txt), so later installs can detect conflicts without
    /// needing this zip on disk.</summary>
    public static void Install(string paksDir, string zipPath, string baseName, List<string>? declaredHands = null)
    {
        Directory.CreateDirectory(paksDir);

        using (var zip = ZipFile.OpenRead(zipPath))
        {
            foreach (var entry in zip.Entries)
            {
                if (string.IsNullOrEmpty(entry.Name)) continue; // directory entry
                if (entry.Name.Equals(HandDeclarationEntry, StringComparison.OrdinalIgnoreCase)) continue;
                if (IsImageEntry(entry.Name)) continue; // preview image, not a pak asset
                if (entry.FullName.Contains(ExtrasFolderMarker, StringComparison.OrdinalIgnoreCase)) continue; // nested per-color variants, not this mod's own assets

                var target = IOPath.Combine(paksDir, entry.FullName.Replace('/', IOPath.DirectorySeparatorChar));
                Directory.CreateDirectory(IOPath.GetDirectoryName(target)!);
                entry.ExtractToFile(target, overwrite: true);
            }
        }

        var hands = declaredHands ?? ReadDeclaredHands(zipPath) ?? [UnknownHand];
        File.WriteAllText(GetHandMarkerPath(paksDir, baseName), string.Join(", ", hands));

        RemoveStrayHandDeclaration(paksDir);
    }

    /// <summary>Substring identifying a mod's "Extras" folder (e.g. "Popiass11_Extras/") — a
    /// zip may ship one holding individually-installable per-color variants as nested zips,
    /// each named "{variantBaseName}_{Color}.zip", for mods whose main pak reskins several
    /// hands at once but also wants to offer installing just one.</summary>
    private const string ExtrasFolderMarker = "_extras";

    public sealed class HandModExtra
    {
        public required string EntryName { get; init; }
        public required string BaseName  { get; init; }
        public required string Color     { get; init; }
        public long            Size      { get; init; }
    }

    /// <summary>Lists the individually-installable per-color variants a mod's zip ships inside
    /// its "{baseName}_Extras" folder — one nested zip per option, named
    /// "{variantBaseName}_{Color}.zip" — or an empty list if the mod doesn't ship one.</summary>
    public static List<HandModExtra> ReadExtras(string zipPath)
    {
        var extras = new List<HandModExtra>();
        using var zip = ZipFile.OpenRead(zipPath);
        foreach (var entry in zip.Entries)
        {
            if (string.IsNullOrEmpty(entry.Name)) continue; // directory entry
            if (!entry.Name.EndsWith(".zip", StringComparison.OrdinalIgnoreCase)) continue;

            var slash = entry.FullName.IndexOf('/');
            if (slash < 0) continue; // not inside a subfolder
            if (entry.FullName.IndexOf('/', slash + 1) >= 0) continue; // nested deeper than one folder
            if (!entry.FullName[..slash].Contains(ExtrasFolderMarker, StringComparison.OrdinalIgnoreCase)) continue;

            var baseName = IOPath.GetFileNameWithoutExtension(entry.Name);
            var underscoreIndex = baseName.LastIndexOf('_');
            var color = underscoreIndex >= 0 && underscoreIndex < baseName.Length - 1
                ? baseName[(underscoreIndex + 1)..]
                : baseName;

            extras.Add(new HandModExtra
            {
                EntryName = entry.FullName,
                BaseName  = baseName,
                Color     = color,
                Size      = entry.Length,
            });
        }
        return extras;
    }

    /// <summary>Pulls one of <see cref="ReadExtras"/>'s entries out of the parent zip into its
    /// own cached zip (skipping the copy if an identically-sized one is already there), so it
    /// can be installed through the normal <see cref="Install"/>/<see cref="UninstallByBaseName"/>
    /// flow using the extra's own <see cref="HandModExtra.BaseName"/>.</summary>
    public static string ExtractExtra(string parentZipPath, HandModExtra extra, int chapterNumber)
    {
        var cacheDir = GetCacheDir(chapterNumber);
        Directory.CreateDirectory(cacheDir);
        var zipPath = IOPath.Combine(cacheDir, $"{extra.BaseName}.zip");

        if (File.Exists(zipPath) && new FileInfo(zipPath).Length == extra.Size)
            return zipPath;

        using var zip = ZipFile.OpenRead(parentZipPath);
        var entry = zip.GetEntry(extra.EntryName)
            ?? throw new InvalidOperationException($"{extra.EntryName} not found in {parentZipPath}.");
        entry.ExtractToFile(zipPath, overwrite: true);
        return zipPath;
    }

    /// <summary>Scans <paramref name="paksDir"/> for every "*.hand.txt" marker <see cref="Install"/>
    /// leaves behind, pairing each installed mod's base name with the hand color(s) it declared.
    /// Unlike <see cref="GetInstalledHands"/> (which needs a specific base name already in hand),
    /// this finds every installed mod regardless of whether it's part of any chapter's known mod
    /// list — needed to detect conflicts against an Extras variant, whose base name lives only
    /// inside its parent's zip.</summary>
    public static List<(string BaseName, List<string> Hands)> GetAllInstalledHandMarkers(string paksDir)
    {
        var result = new List<(string, List<string>)>();
        if (!Directory.Exists(paksDir)) return result;

        foreach (var path in Directory.EnumerateFiles(paksDir, "*.hand.txt"))
        {
            var baseName = IOPath.GetFileName(path)[..^".hand.txt".Length];
            var colors   = ParseHandColors(File.ReadAllText(path));
            result.Add((baseName, colors.Count > 0 ? colors : [UnknownHand]));
        }
        return result;
    }

    public static void UninstallByBaseName(string paksDir, string baseName)
    {
        foreach (var ext in new[] { "pak", "ucas", "utoc" })
        {
            var path = IOPath.Combine(paksDir, $"{baseName}_P.{ext}");
            if (File.Exists(path)) File.Delete(path);
        }

        var handMarker = GetHandMarkerPath(paksDir, baseName);
        if (File.Exists(handMarker)) File.Delete(handMarker);

        RemoveStrayHandDeclaration(paksDir);
    }

    /// <summary>Deletes a bare, mod-agnostic "hand.txt" if one is sitting in paksDir — leftover
    /// cruft from an earlier build that extracted zips wholesale instead of skipping that entry.
    /// Safe to call unconditionally; harmless once no install has ever hit the old bug.</summary>
    private static void RemoveStrayHandDeclaration(string paksDir)
    {
        var strayPath = IOPath.Combine(paksDir, HandDeclarationEntry);
        if (File.Exists(strayPath)) File.Delete(strayPath);
    }

    public static string FormatFileSize(long bytes)
    {
        if (bytes <= 0)          return "—";
        if (bytes < 1024)        return $"{bytes} B";
        if (bytes < 1024 * 1024) return $"{bytes / 1024.0:F1} KB";
        return $"{bytes / (1024.0 * 1024):F2} MB";
    }
}
