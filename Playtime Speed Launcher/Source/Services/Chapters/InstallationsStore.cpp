#include "Services/Chapters/InstallationsStore.h"
#include <algorithm>
#include "Core/Text.h"
#include "Settings/ConfigFile.h"

namespace SpeedrunLauncher::Services {

namespace {

constexpr const wchar_t* kFile = L"installations.json";
const std::vector<Models::Installation> kNone;

bool SamePath(const std::wstring& a, const std::wstring& b) { return Text::Equals(a, b, true); }

} // namespace

InstallationsStore InstallationsStore::Load() {
    InstallationsStore store;
    bool exists = false;
    const Json::Value data = Config::ReadJson(kFile, &exists);
    if (!exists || !data.IsObject()) return store;

    for (const auto& [key, list] : data["Customs"].Items()) {
        auto chapter = Text::ToInt(key);
        if (!chapter || !list.IsArray()) continue;
        auto& entries = store.customs[static_cast<int>(*chapter)];
        for (const auto& item : list.Elements()) {
            Models::Installation info;
            info.name = item["Name"].WStr();
            info.exePath = item["ExePath"].WStr();
            info.iconPath = item["IconPath"].OptWStr();
            entries.push_back(std::move(info));
        }
    }
    for (const auto& [key, value] : data["Selected"].Items()) {
        auto chapter = Text::ToInt(key);
        if (chapter && value.IsString()) store.selected[static_cast<int>(*chapter)] = value.WStr();
    }
    for (const auto& [key, value] : data["PreferredPaths"].Items()) {
        auto chapter = Text::ToInt(key);
        if (chapter && value.IsString()) store.preferredPaths[static_cast<int>(*chapter)] = value.WStr();
    }
    for (const auto& m : data["InstalledManifests"].Elements()) store.installedManifests.insert(m.WStr());
    store.steamUsername = data["SteamUsername"].OptWStr();
    return store;
}

void InstallationsStore::Save() const {
    Json::Value data = Json::Value::MakeObject();
    Json::Value customsJson = Json::Value::MakeObject();
    for (const auto& [chapter, list] : customs) {
        Json::Value arr = Json::Value::MakeArray();
        for (const auto& item : list) {
            Json::Value obj = Json::Value::MakeObject();
            obj.Set("Name", item.name);
            obj.Set("ExePath", item.exePath);
            obj.Set("IconPath", item.iconPath ? Json::Value(*item.iconPath) : Json::Value());
            arr.Push(obj);
        }
        customsJson.Set(std::to_string(chapter), arr);
    }
    data.Set("Customs", customsJson);

    Json::Value selectedJson = Json::Value::MakeObject();
    for (const auto& [chapter, path] : selected) selectedJson.Set(std::to_string(chapter), path);
    data.Set("Selected", selectedJson);

    Json::Value preferredJson = Json::Value::MakeObject();
    for (const auto& [chapter, path] : preferredPaths)
        preferredJson.Set(std::to_string(chapter), path ? Json::Value(*path) : Json::Value());
    data.Set("PreferredPaths", preferredJson);

    Json::Value manifests = Json::Value::MakeArray();
    for (const auto& m : installedManifests) manifests.Push(m);
    data.Set("InstalledManifests", manifests);
    data.Set("SteamUsername", steamUsername ? Json::Value(*steamUsername) : Json::Value());
    Config::WriteJson(kFile, data, true);
}

const std::vector<Models::Installation>& InstallationsStore::Customs(int chapter) const {
    auto it = customs.find(chapter);
    return it == customs.end() ? kNone : it->second;
}

std::optional<std::wstring> InstallationsStore::SelectedPath(int chapter) const {
    auto it = selected.find(chapter);
    if (it == selected.end()) return std::nullopt;
    return it->second;
}

void InstallationsStore::SetSelected(int chapter, const std::optional<std::wstring>& path) {
    if (!path) selected.erase(chapter);
    else selected[chapter] = *path;
    Save();
}

void InstallationsStore::AddCustom(int chapter, const std::wstring& name, const std::wstring& exePath) {
    auto& list = customs[chapter];
    if (std::any_of(list.begin(), list.end(), [&](const auto& x) { return SamePath(x.exePath, exePath); })) return;
    list.push_back({ name, exePath, std::nullopt });
    Save();
}

void InstallationsStore::UpdateCustom(int chapter, const std::wstring& exePath, const std::wstring& newName,
                                      const std::optional<std::wstring>& newIconPath) {
    auto it = customs.find(chapter);
    if (it == customs.end()) return;
    for (auto& item : it->second) {
        if (!SamePath(item.exePath, exePath)) continue;
        if (!Text::Trim(newName).empty()) item.name = newName;
        item.iconPath = newIconPath;
        Save();
        return;
    }
}

void InstallationsStore::MoveCustom(int chapter, const std::wstring& exePath, int direction) {
    auto it = customs.find(chapter);
    if (it == customs.end()) return;
    auto& list = it->second;
    const auto pos = std::find_if(list.begin(), list.end(), [&](const auto& x) { return SamePath(x.exePath, exePath); });
    if (pos == list.end()) return;
    const long long idx = pos - list.begin(), target = idx + direction;
    if (target < 0 || target >= static_cast<long long>(list.size())) return;
    std::swap(list[static_cast<size_t>(idx)], list[static_cast<size_t>(target)]);
    Save();
}

void InstallationsStore::RemoveCustom(int chapter, const std::wstring& exePath) {
    auto it = customs.find(chapter);
    if (it == customs.end()) return;
    auto& list = it->second;
    list.erase(std::remove_if(list.begin(), list.end(), [&](const auto& x) { return SamePath(x.exePath, exePath); }), list.end());
    // Removing the selected entry reverts to automatic
    auto sel = selected.find(chapter);
    if (sel != selected.end() && SamePath(sel->second, exePath)) selected.erase(sel);
    Save();
}

std::optional<std::wstring> InstallationsStore::PreferredPath(int chapter) const {
    auto it = preferredPaths.find(chapter);
    return it == preferredPaths.end() ? std::nullopt : it->second;
}

void InstallationsStore::SetPreferredPath(int chapter, const std::optional<std::wstring>& path) {
    preferredPaths[chapter] = path;
    Save();
}

void InstallationsStore::MarkManifestInstalled(const std::wstring& manifestId) {
    installedManifests.insert(manifestId);
    Save();
}

void InstallationsStore::UnmarkManifestInstalled(const std::wstring& manifestId) {
    if (installedManifests.erase(manifestId)) Save();
}

void InstallationsStore::SetSteamUsername(const std::wstring& username) {
    steamUsername = username;
    Save();
}

} // namespace SpeedrunLauncher::Services
