using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

namespace SpeedrunLauncher.Services.Platforms;

public sealed record SavedAccount(string Username, string EncryptedPassword);

public static class SteamCredentialStore
{
    private static readonly string FilePath = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "SpeedrunLauncher", "steam_accounts.json");

    public static List<SavedAccount> LoadAll()
    {
        try
        {
            if (!File.Exists(FilePath)) return [];
            return JsonSerializer.Deserialize<List<SavedAccount>>(
                File.ReadAllText(FilePath)) ?? [];
        }
        catch { return []; }
    }

    // Adds the account if new, updates password if username already exists.
    public static void Save(string username, string password)
    {
        var accounts = LoadAll();
        var idx = accounts.FindIndex(a =>
            string.Equals(a.Username, username, StringComparison.OrdinalIgnoreCase));
        var entry = new SavedAccount(username, Encrypt(password));
        if (idx >= 0) accounts[idx] = entry;
        else          accounts.Add(entry);

        Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);
        File.WriteAllText(FilePath, JsonSerializer.Serialize(accounts,
            new JsonSerializerOptions { WriteIndented = true }));
    }

    public static string? Decrypt(SavedAccount account)
    {
        try
        {
            var bytes = ProtectedData.Unprotect(
                Convert.FromBase64String(account.EncryptedPassword),
                null, DataProtectionScope.CurrentUser);
            return Encoding.UTF8.GetString(bytes);
        }
        catch { return null; }
    }

    private static string Encrypt(string plain)
    {
        var bytes = ProtectedData.Protect(
            Encoding.UTF8.GetBytes(plain), null, DataProtectionScope.CurrentUser);
        return Convert.ToBase64String(bytes);
    }
}
