#include "Services/Platforms/SteamCredentialStore.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Settings/ConfigFile.h"

namespace SpeedrunLauncher::Services::SteamCredentials {

namespace {
constexpr const wchar_t* kFile = L"steam_accounts.json";
}

std::vector<Account> LoadAll() {
    std::vector<Account> out;
    // Keep the parsed document alive: Elements() refers into it (iterating it straight off the
    // temporary read freed memory and usually found no accounts)
    const Json::Value accounts = Config::ReadJson(kFile);
    for (const auto& item : accounts.Elements())
        out.push_back({ item["Username"].WStr(), item["EncryptedPassword"].WStr() });
    return out;
}

void Save(const std::wstring& username, const std::wstring& password) {
    auto accounts = LoadAll();
    auto blob = System::Protect(Text::Narrow(password));
    const std::wstring encrypted = blob ? Text::Widen(System::Base64Encode(*blob)) : L"";
    bool found = false;
    for (auto& a : accounts) {
        if (Text::Equals(a.username, username, true)) {
            a = { username, encrypted };
            found = true;
        }
    }
    if (!found) accounts.push_back({ username, encrypted });

    Json::Value list = Json::Value::MakeArray();
    for (const auto& a : accounts) {
        Json::Value obj = Json::Value::MakeObject();
        obj.Set("Username", a.username);
        obj.Set("EncryptedPassword", a.encryptedPassword);
        list.Push(obj);
    }
    Config::WriteJson(kFile, list, true);
}

std::optional<std::wstring> Decrypt(const Account& account) {
    auto blob = System::Base64Decode(Text::Narrow(account.encryptedPassword));
    if (!blob) return std::nullopt;
    auto plain = System::Unprotect(*blob);
    if (!plain) return std::nullopt;
    return Text::Widen(*plain);
}

} // namespace SpeedrunLauncher::Services::SteamCredentials
