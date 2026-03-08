using System.IO;
using System.Text;

namespace SpeedrunLauncher.Services;

public static class LocalizationService
{
    private static Dictionary<string, string> _strings = [];
    private static string _currentLang = "es";

    private static readonly string TranslationsDir =
        Path.Combine(AppContext.BaseDirectory, "Transalations");

    private static readonly string DataDir =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "SpeedrunLauncher");

    private static readonly string SettingsFile =
        Path.Combine(DataDir, "language.cfg");

    public static string CurrentLang => _currentLang;

    public static void Load(string lang)
    {
        _currentLang = lang;
        var path = Path.Combine(TranslationsDir, $"{lang}.txt");
        if (!File.Exists(path)) return;

        _strings = [];
        foreach (var line in File.ReadAllLines(path, Encoding.UTF8))
        {
            if (string.IsNullOrWhiteSpace(line) || line.TrimStart().StartsWith('#')) continue;
            var idx = line.IndexOf('=');
            if (idx < 0) continue;
            var key = line[..idx].Trim();
            var val = line[(idx + 1)..].TrimStart().Replace("\\n", "\n");
            _strings[key] = val;
        }

        try { Directory.CreateDirectory(DataDir); File.WriteAllText(SettingsFile, lang); } catch { }
    }

    public static string LoadSaved()
    {
        if (File.Exists(SettingsFile))
        {
            var saved = File.ReadAllText(SettingsFile).Trim();
            if (!string.IsNullOrEmpty(saved))
            {
                Load(saved);
                return saved;
            }
        }
        Load("es");
        return "es";
    }

    public static string Get(string key, params object[] args)
    {
        if (!_strings.TryGetValue(key, out var val)) return key;
        return args.Length > 0 ? string.Format(val, args) : val;
    }
}
