#include "Services/Platforms/EpicGamesService.h"
#include <windows.h>
#include <map>
#include <vector>
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

// Reads one folder: returns the PoppyPlaytimeChapterOne inside it when it's a valid install, and
// adds the subfolders worth searching to `next` (when given)
std::optional<std::wstring> ScanFolder(const std::wstring& dir, std::vector<std::wstring>* next) {
    WIN32_FIND_DATAW data{};
    // Basic info + large fetch: no 8.3 names and fewer round trips on big folders
    HANDLE find = FindFirstFileExW(Files::Combine(dir, L"*").c_str(), FindExInfoBasic, &data, FindExSearchLimitToDirectories, nullptr,
                                   FIND_FIRST_EX_LARGE_FETCH);
    if (find == INVALID_HANDLE_VALUE) return std::nullopt;
    std::optional<std::wstring> found;
    do {
        const DWORD attributes = data.dwFileAttributes;
        if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        const wchar_t* name = data.cFileName;
        if (wcscmp(name, L".") == 0 || wcscmp(name, L"..") == 0) continue;
        if (Text::Equals(name, L"PoppyPlaytimeChapterOne")) {
            const std::wstring target = Files::Combine(dir, name);
            if (EpicGamesService::IsValidBase(target)) {
                found = target;
                break;
            }
        }
        if (!next) continue;
        // Skip junctions (old "Documents and Settings" style links that loop or are denied), hidden
        // system folders and system folders that never contain game installs
        if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
            ((attributes & FILE_ATTRIBUTE_HIDDEN) && (attributes & FILE_ATTRIBUTE_SYSTEM)) || name[0] == L'$' ||
            Text::Equals(name, L"Windows") || Text::Equals(name, L"System32") || Text::Equals(name, L"SysWOW64") ||
            Text::Equals(name, L"ProgramData"))
            continue;
        next->push_back(Files::Combine(dir, name));
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return found;
}

// Breadth-first over every drive at once (`depth` folder levels): shallow installs such as
// D:\Epic Games\PoppyPlaytimeChapterOne turn up before deep trees like C:\Users are crawled
std::optional<std::wstring> FindPoppyFolder(std::vector<std::wstring> level, int depth, const Async::Cancel& cancel,
                                            const EpicGamesService::DetectProgress& progress) {
    for (int d = 0; d < depth && !level.empty(); ++d) {
        std::vector<std::wstring> next;
        const bool deeper = d + 1 < depth;
        for (const auto& dir : level) {
            if (cancel.Requested()) return std::nullopt;
            if (progress) progress(EpicGamesService::DetectStage::Drives, dir);
            if (auto found = ScanFolder(dir, deeper ? &next : nullptr)) return found;
        }
        level = std::move(next);
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

std::optional<std::wstring> EpicGamesService::TryAutoDetect(const Async::Cancel& cancel, const DetectProgress& progress) const {
    const std::wstring manifests = L"C:\\ProgramData\\Epic\\EpicGamesLauncher\\Data\\Manifests";
    if (progress) progress(DetectStage::Manifests, manifests);
    for (const auto& file : Files::ListFiles(manifests, L"*.item")) {
        if (cancel.Requested()) return std::nullopt;
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
    std::vector<std::wstring> fixed;
    for (const wchar_t* d = drives; d < drives + n && *d; d += wcslen(d) + 1)
        if (GetDriveTypeW(d) == DRIVE_FIXED) fixed.emplace_back(d);
    return FindPoppyFolder(std::move(fixed), 4, cancel, progress);
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
