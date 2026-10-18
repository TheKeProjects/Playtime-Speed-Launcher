#include "Services/Platforms/SteamDetector.h"
#include <windows.h>
#include <algorithm>
#include <map>
#include <regex>
#include "Core/Files.h"
#include "Core/Text.h"

#pragma comment(lib, "version.lib")

namespace SpeedrunLauncher::Services::SteamDetector {

namespace {

const std::map<int, std::vector<std::wstring>>& ChapterFolders() {
    static const std::map<int, std::vector<std::wstring>> folders = {
        { 1, { L"Poppy Playtime" } },
        { 2, { L"Poppy Playtime Chapter 2", L"Poppy Playtime - Chapter 2" } },
        { 3, { L"Poppy Playtime Chapter 3", L"Poppy Playtime - Chapter 3" } },
        { 4, { L"Poppy Playtime Chapter 4", L"Poppy Playtime - Chapter 4" } },
        { 5, { L"Poppy Playtime Chapter 5", L"Poppy Playtime - Chapter 5" } },
    };
    return folders;
}

struct LowerLess {
    bool operator()(const std::wstring& a, const std::wstring& b) const { return Text::Lower(a) < Text::Lower(b); }
};

void CollectExes(const std::wstring& dir, std::vector<std::wstring>& out, const std::set<std::wstring, LowerLess>* exclude) {
    for (auto& f : Files::ListFiles(dir, L"*.exe")) out.push_back(f);
    for (const auto& sub : Files::ListDirectories(dir)) {
        if (exclude && exclude->count(Files::FileName(sub))) continue;
        CollectExes(sub, out, exclude);
    }
}

std::optional<std::wstring> FindMainExe(const std::wstring& gamePath, const std::set<std::wstring, LowerLess>* exclude = nullptr) {
    std::vector<std::wstring> exes;
    CollectExes(gamePath, exes, exclude);
    std::optional<std::wstring> best;
    uint64_t bestSize = 0;
    for (const auto& f : exes) {
        const std::wstring n = Text::Lower(Files::FileNameWithoutExtension(f));
        static const wchar_t* skip[] = { L"crash", L"setup", L"prereq", L"unins", L"install", L"redist", L"launcher", L"helper" };
        if (std::any_of(std::begin(skip), std::end(skip), [&](const wchar_t* s) { return n.find(s) != std::wstring::npos; }))
            continue;
        const uint64_t size = Files::Size(f).value_or(0);
        if (!best || size > bestSize) {
            best = f;
            bestSize = size;
        }
    }
    return best;
}

std::wstring GetVersion(const std::wstring& gamePath, const std::wstring& basePath, int appId) {
    if (appId > 0) {
        const std::wstring steamapps = Files::DirectoryName(basePath);
        if (!steamapps.empty()) {
            const std::wstring acf = Files::Combine(steamapps, L"appmanifest_" + std::to_wstring(appId) + L".acf");
            if (auto content = Files::ReadText(acf)) {
                std::wsmatch m;
                static const std::wregex buildId(LR"re("buildid"\s+"(\d+)")re");
                if (std::regex_search(*content, m, buildId)) return L"Build " + m[1].str();
            }
        }
    }
    if (auto exe = FindMainExe(gamePath)) {
        const std::wstring ver = FileProductVersion(*exe);
        if (!Text::Trim(ver).empty() && ver != L"0.0.0.0") return ver;
    }
    return L"Instalado";
}

std::vector<std::wstring> CollectSearchBases() {
    std::vector<std::wstring> result;
    const auto add = [&](const std::wstring& p) {
        if (std::none_of(result.begin(), result.end(), [&](const std::wstring& x) { return Text::Equals(x, p, true); }))
            result.push_back(p);
    };
    // 1. The launcher itself sits inside a steamapps\common folder
    for (std::wstring dir = ExecutableDirectory(); !dir.empty();) {
        if (Text::Equals(Files::FileName(dir), L"common", true)) {
            add(dir);
            break;
        }
        const std::wstring parent = Files::DirectoryName(dir);
        if (parent == dir) break;
        dir = parent;
    }
    // 2. Common Steam library locations on every fixed drive
    static const wchar_t* relative[] = {
        L"SteamLibrary\\steamapps\\common",
        L"Steam\\steamapps\\common",
        L"Program Files (x86)\\Steam\\steamapps\\common",
        L"Program Files\\Steam\\steamapps\\common",
        L"Games\\SteamLibrary\\steamapps\\common",
    };
    for (const auto& drive : FixedDrives())
        for (const wchar_t* rel : relative) add(Files::Combine(drive, rel));
    // 3. libraryfolders.vdf
    std::vector<std::wstring> vdfs = { L"C:\\Program Files (x86)\\Steam\\config\\libraryfolders.vdf",
                                       L"C:\\Program Files\\Steam\\config\\libraryfolders.vdf" };
    for (const auto& drive : FixedDrives()) vdfs.push_back(Files::Combine(drive, L"Steam\\config\\libraryfolders.vdf"));
    static const std::wregex pathRe(LR"re("path"\s+"([^"]+)")re");
    for (const auto& vdf : vdfs) {
        auto content = Files::ReadText(vdf);
        if (!content) continue;
        for (std::wsregex_iterator it(content->begin(), content->end(), pathRe), end; it != end; ++it) {
            const std::wstring lib = Text::Replace((*it)[1].str(), L"\\\\", L"\\");
            add(Files::Combine(lib, L"steamapps", L"common"));
        }
    }
    return result;
}

} // namespace

std::wstring ExecutableDirectory() { return Files::DirectoryName(Files::ExecutablePath()); }

std::vector<std::wstring> FixedDrives() {
    std::vector<std::wstring> out;
    wchar_t drives[512];
    const DWORD n = GetLogicalDriveStringsW(static_cast<DWORD>(std::size(drives)), drives);
    for (const wchar_t* d = drives; d < drives + n && *d; d += wcslen(d) + 1)
        if (GetDriveTypeW(d) == DRIVE_FIXED) out.emplace_back(d);
    return out;
}

std::wstring FileProductVersion(const std::wstring& exe) {
    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(exe.c_str(), &handle);
    if (!size) return {};
    std::vector<BYTE> data(size);
    if (!GetFileVersionInfoW(exe.c_str(), 0, size, data.data())) return {};
    struct Translation {
        WORD language, codePage;
    }* translations = nullptr;
    UINT len = 0;
    if (VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<void**>(&translations), &len) && len >= 4) {
        for (const wchar_t* field : { L"ProductVersion", L"FileVersion" }) {
            wchar_t query[128];
            swprintf_s(query, L"\\StringFileInfo\\%04x%04x\\%s", translations[0].language, translations[0].codePage, field);
            wchar_t* value = nullptr;
            UINT valueLen = 0;
            if (VerQueryValueW(data.data(), query, reinterpret_cast<void**>(&value), &valueLen) && valueLen > 1) return value;
        }
    }
    return {};
}

void DetectAll(std::vector<Models::Chapter>& chapters) {
    const auto bases = CollectSearchBases();
    // First path component of every SubFolderName, so a top-level chapter folder scan skips them
    std::set<std::wstring, LowerLess> chapterSubFolders;
    for (const auto& c : chapters)
        if (c.subFolderName) chapterSubFolders.insert(Text::Split(*c.subFolderName, L'\\')[0]);

    for (auto& chapter : chapters) {
        if (!chapter.isAvailable) continue;
        bool found = false;
        for (const auto& base : bases) {
            if (found) break;
            if (!Files::DirectoryExists(base)) continue;
            // Strategy A: the chapter lives inside the main game folder
            if (chapter.subFolderName) {
                for (const auto& parent : ChapterFolders().at(1)) {
                    const std::wstring sub = Files::Combine(base, parent, *chapter.subFolderName);
                    if (!Files::DirectoryExists(sub)) continue;
                    auto exe = FindMainExe(sub);
                    if (!exe) continue;
                    chapter.gameExePath = exe;
                    chapter.detectedVersion = GetVersion(sub, base, chapter.steamAppId);
                    found = true;
                    break;
                }
                if (found) break;
            }
            // Strategy B: the chapter is its own top-level Steam folder
            auto folders = ChapterFolders().count(chapter.number) ? ChapterFolders().at(chapter.number)
                                                                   : std::vector<std::wstring>{ chapter.steamFolderName };
            for (const auto& folder : folders) {
                const std::wstring gamePath = Files::Combine(base, folder);
                if (!Files::DirectoryExists(gamePath)) continue;
                auto exe = FindMainExe(gamePath, &chapterSubFolders);
                if (!exe) continue;
                chapter.gameExePath = exe;
                chapter.detectedVersion = GetVersion(gamePath, base, chapter.steamAppId);
                found = true;
                break;
            }
        }
    }
}

void DetectMultiplayers(std::vector<Models::MultiplayerGame>& games) {
    const auto bases = CollectSearchBases();
    for (auto& game : games) {
        if (!game.isAvailable || game.steamExe.empty()) continue;
        for (const auto& base : bases) {
            const std::wstring exe = Files::Combine(base, game.steamExe);
            if (!Files::Exists(exe)) continue;
            game.gameExePath = exe;
            break;
        }
    }
}

std::optional<std::wstring> FindDepotDownloadPath(int appId, int depotId, const std::optional<std::wstring>& steamCmdPath) {
    const std::wstring sub = Files::Combine(L"steamapps", L"content", L"app_" + std::to_wstring(appId), L"depot_" + std::to_wstring(depotId));
    std::vector<std::wstring> roots;
    if (steamCmdPath) roots.push_back(Files::DirectoryName(*steamCmdPath));
    roots.push_back(Files::Combine(ExecutableDirectory(), L"steamcmd"));
    for (const auto& drive : FixedDrives())
        for (const wchar_t* steamBase : { L"Program Files (x86)\\Steam", L"Program Files\\Steam", L"Steam" })
            roots.push_back(Files::Combine(drive, steamBase));
    for (const auto& root : roots) {
        const std::wstring path = Files::Combine(root, sub);
        if (!Files::DirectoryExists(path)) continue;
        if (!Files::ListFiles(path, L"*", true).empty()) return path;
    }
    return std::nullopt;
}

std::optional<std::wstring> FindGameExe(const std::wstring& gamePath) { return FindMainExe(gamePath); }

} // namespace SpeedrunLauncher::Services::SteamDetector
