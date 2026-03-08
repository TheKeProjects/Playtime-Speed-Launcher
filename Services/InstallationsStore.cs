using System.Text.Json;
using SpeedrunLauncher.Models;

namespace SpeedrunLauncher.Services;

public class InstallationsStore
{
    private static readonly string FilePath =
        Path.Combine(AppContext.BaseDirectory, "installations.json");

    // chapter number (1-based) → list of custom installations
    private Dictionary<int, List<InstallationInfo>> _customs = [];
    // chapter number → selected exe path; null means "Automático"
    private Dictionary<int, string?> _selected = [];
    // chapter number → last folder the user chose to store versions
    private Dictionary<int, string?> _preferredPaths = [];
    // manifest IDs that have already been moved & registered
    private HashSet<string> _installedManifests = [];
    // Steam username for SteamCMD (password is never stored)
    private string? _steamUsername;

    private InstallationsStore() { }

    public static InstallationsStore Load()
    {
        var store = new InstallationsStore();
        if (!File.Exists(FilePath)) return store;
        try
        {
            var json = File.ReadAllText(FilePath);
            var data = JsonSerializer.Deserialize<StoreData>(json);
            if (data is null) return store;

            // JSON stores keys as strings; convert back to int
            foreach (var (k, v) in data.Customs)
                if (int.TryParse(k, out var n) && v is not null)
                    store._customs[n] = v;

            foreach (var (k, v) in data.Selected)
                if (int.TryParse(k, out var n))
                    store._selected[n] = v;

            foreach (var (k, v) in data.PreferredPaths)
                if (int.TryParse(k, out var n) && v is not null)
                    store._preferredPaths[n] = v;

            store._installedManifests = new HashSet<string>(data.InstalledManifests ?? []);
            store._steamUsername = data.SteamUsername;
        }
        catch { }
        return store;
    }

    public List<InstallationInfo> GetCustoms(int chapter) =>
        _customs.GetValueOrDefault(chapter, []);

    public string? GetSelectedPath(int chapter) =>
        _selected.GetValueOrDefault(chapter, null);

    public void SetSelected(int chapter, string? path)
    {
        if (path is null)
            _selected.Remove(chapter);
        else
            _selected[chapter] = path;
        Save();
    }

    public void AddCustom(int chapter, string name, string exePath)
    {
        if (!_customs.ContainsKey(chapter))
            _customs[chapter] = [];

        // Avoid duplicates
        if (_customs[chapter].Any(x => x.ExePath.Equals(exePath, StringComparison.OrdinalIgnoreCase)))
            return;

        _customs[chapter].Add(new InstallationInfo { Name = name, ExePath = exePath });
        Save();
    }

    public string? GetPreferredPath(int chapter) =>
        _preferredPaths.GetValueOrDefault(chapter, null);

    public void SetPreferredPath(int chapter, string? path)
    {
        _preferredPaths[chapter] = path;
        Save();
    }

    public bool IsManifestInstalled(string manifestId) =>
        _installedManifests.Contains(manifestId);

    public void MarkManifestInstalled(string manifestId)
    {
        _installedManifests.Add(manifestId);
        Save();
    }

    public void UnmarkManifestInstalled(string manifestId)
    {
        if (_installedManifests.Remove(manifestId))
            Save();
    }

    public string? GetSteamUsername() => _steamUsername;
    public void SetSteamUsername(string username) { _steamUsername = username; Save(); }

    public void UpdateCustom(int chapter, string exePath, string? newName, string? newIconPath)
    {
        var list = _customs.GetValueOrDefault(chapter);
        if (list is null) return;
        var item = list.FirstOrDefault(x => x.ExePath.Equals(exePath, StringComparison.OrdinalIgnoreCase));
        if (item is null) return;
        if (!string.IsNullOrWhiteSpace(newName)) item.Name = newName;
        item.IconPath = newIconPath;
        Save();
    }

    public void RemoveCustom(int chapter, string exePath)
    {
        if (!_customs.ContainsKey(chapter)) return;
        _customs[chapter].RemoveAll(x => x.ExePath.Equals(exePath, StringComparison.OrdinalIgnoreCase));

        // If the removed entry was selected, revert to auto
        if (_selected.GetValueOrDefault(chapter) is string sel &&
            sel.Equals(exePath, StringComparison.OrdinalIgnoreCase))
            _selected.Remove(chapter);

        Save();
    }

    private void Save()
    {
        try
        {
            // Serialize with string keys for JSON compatibility
            var data = new StoreData
            {
                Customs            = _customs.ToDictionary(p => p.Key.ToString(), p => (List<InstallationInfo>?)p.Value),
                Selected           = _selected.ToDictionary(p => p.Key.ToString(), p => p.Value),
                PreferredPaths     = _preferredPaths.ToDictionary(p => p.Key.ToString(), p => (string?)p.Value),
                InstalledManifests = [.. _installedManifests],
                SteamUsername      = _steamUsername,
            };
            File.WriteAllText(FilePath,
                JsonSerializer.Serialize(data, new JsonSerializerOptions { WriteIndented = true }));
        }
        catch { }
    }

    private sealed class StoreData
    {
        public Dictionary<string, List<InstallationInfo>?> Customs            { get; set; } = [];
        public Dictionary<string, string?>                 Selected           { get; set; } = [];
        public Dictionary<string, string?>                 PreferredPaths     { get; set; } = [];
        public List<string>                                InstalledManifests { get; set; } = [];
        public string?                                     SteamUsername      { get; set; }
    }
}
