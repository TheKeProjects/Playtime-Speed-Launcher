#include "Services/Platforms/EpicGamesService.h"
#include <windows.h>
#include <map>
#include "Core/Files.h"
#include "Core/Json.h"
#include "Core/Text.h"
#include "Settings/ConfigFile.h"

namespace SpeedrunLauncher::Services {

namespace {

constexpr const wchar_t* kFile = L"epic.json";

// Relative paths inside PoppyPlaytimeChapterOne to each chapter's exe
const std::map<int, std::wstring>& SubPaths() {
    static const std::map<int, std::wstring> paths = {
        { 1, L"Chapter1\\Poppy_Playtime.exe" },
        { 2, L"Chapter2\\Playtime_Prototype4.exe" },
        { 3, L"Chapter3\\Playtime_Chapter3.exe" },
        { 4, L"Chapter4\\Playtime_Chapter4.exe" },
        { 5, L"Chapter5\\ch5_pro.exe" },
    };
    return paths;
}

std::optional<std::wstring> FindPoppyFolder(const std::wstring& dir, int maxDepth) {
    if (maxDepth <= 0) return std::nullopt;
    const std::wstring target = Files::Combine(dir, L"PoppyPlaytimeChapterOne");
    if (Files::DirectoryExists(target) && EpicGamesService::IsValidBase(target)) return target;
    for (const auto& sub : Files::ListDirectories(dir)) {
        const std::wstring name = Files::FileName(sub);
        // Skip system folders that never contain game installs
        if (Text::Equals(name, L"Windows") || Text::Equals(name, L"System32") || Text::Equals(name, L"SysWOW64") ||
            Text::Equals(name, L"ProgramData") || Text::StartsWith(name, L"$"))
            continue;
        if (auto found = FindPoppyFolder(sub, maxDepth - 1)) return found;
    }
    return std::nullopt;
}

} // namespace

bool EpicGamesService::IsValidBase(const std::wstring& path) {
    for (const auto& [chapter, sub] : SubPaths())
        if (Files::Exists(Files::Combine(path, sub))) return true;
    return false;
}

std::optional<std::wstring> EpicGamesService::ExePath(int chapter) const {
    auto it = SubPaths().find(chapter);
    if (!basePath || it == SubPaths().end()) return std::nullopt;
    const std::wstring path = Files::Combine(*basePath, it->second);
    if (!Files::Exists(path)) return std::nullopt;
    return path;
}

bool EpicGamesService::HasExeForAnyChapter() const { return basePath && IsValidBase(*basePath); }

void EpicGamesService::SetEnabled(bool on) {
    enabled = on;
    Save();
}

void EpicGamesService::SetBasePath(const std::optional<std::wstring>& path) {
    basePath = path;
    Save();
}

std::optional<std::wstring> EpicGamesService::TryAutoDetect() const {
    const std::wstring manifests = L"C:\\ProgramData\\Epic\\EpicGamesLauncher\\Data\\Manifests";
    for (const auto& file : Files::ListFiles(manifests, L"*.item")) {
        auto bytes = Files::ReadBytes(file);
        if (!bytes) continue;
        const Json::Value doc = Json::Parse(std::string_view(*bytes));
        const std::wstring install = doc["InstallLocation"].WStr();
        if (install.empty()) continue;
        if (IsValidBase(install)) return install;
        const std::wstring sub = Files::Combine(install, L"PoppyPlaytimeChapterOne");
        if (IsValidBase(sub)) return sub;
    }
    wchar_t drives[512];
    const DWORD n = GetLogicalDriveStringsW(static_cast<DWORD>(std::size(drives)), drives);
    for (const wchar_t* d = drives; d < drives + n && *d; d += wcslen(d) + 1) {
        if (GetDriveTypeW(d) != DRIVE_FIXED) continue;
        if (auto found = FindPoppyFolder(d, 4)) return found;
    }
    return std::nullopt;
}

void EpicGamesService::Load() {
    const Json::Value data = Config::ReadJson(kFile);
    if (!data.IsObject()) return;
    basePath = data["BasePath"].IsString() ? std::optional<std::wstring>(data["BasePath"].WStr()) : std::nullopt;
    enabled = data["IsEnabled"].Bool(false);
}

void EpicGamesService::Save() const {
    Json::Value data = Json::Value::MakeObject();
    data.Set("BasePath", basePath ? Json::Value(*basePath) : Json::Value());
    data.Set("IsEnabled", enabled);
    Config::WriteJson(kFile, data, true);
}

} // namespace SpeedrunLauncher::Services
