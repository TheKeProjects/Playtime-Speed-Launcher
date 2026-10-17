#pragma once
#include <optional>
#include <string>
#include <vector>

// Saved SteamCMD logins; passwords are DPAPI-encrypted (steam_accounts.json).
namespace SpeedrunLauncher::Services::SteamCredentials {

struct Account {
    std::wstring username;
    std::wstring encryptedPassword;   // base64 of the DPAPI blob
};

std::vector<Account> LoadAll();
void Save(const std::wstring& username, const std::wstring& password);   // adds or updates
std::optional<std::wstring> Decrypt(const Account& account);

} // namespace SpeedrunLauncher::Services::SteamCredentials
