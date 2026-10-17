#include "Services/GameFiles/ModFiles.h"
#include <windows.h>
#include <algorithm>
#include <vector>
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Text.h"
#include "Core/Zip.h"

namespace SpeedrunLauncher::Services::ModFiles {

std::optional<std::wstring> ProjectRoot(const std::wstring& win64Dir) {
    const std::wstring binaries = Files::DirectoryName(Text::Trim(win64Dir));
    if (binaries.empty()) return std::nullopt;
    const std::wstring root = Files::DirectoryName(binaries);
    if (root.empty()) return std::nullopt;
    return root;
}

std::optional<std::wstring> PaksDir(const std::wstring& win64Dir) {
    auto root = ProjectRoot(win64Dir);
    if (!root) return std::nullopt;
    return Files::Combine(*root, L"Content", L"Paks");
}

void DeleteZipContents(const std::wstring& targetDir, const std::wstring& zipPath) {
    auto zip = Zip::Archive::Open(zipPath);
    if (!zip) return;
    std::vector<std::wstring> dirs;
    for (const auto& entry : zip->Entries()) {
        std::wstring rel = Text::Replace(entry.fullName, L"/", L"\\");
        if (entry.name.empty()) {
            while (!rel.empty() && rel.back() == L'\\') rel.pop_back();
            dirs.push_back(Files::Combine(targetDir, rel));
        } else {
            Files::Delete(Files::Combine(targetDir, rel));
        }
    }
    std::sort(dirs.begin(), dirs.end(), [](const auto& a, const auto& b) { return a.size() < b.size(); });
    for (const auto& dir : dirs)
        if (Files::DirectoryExists(dir)) Files::DeleteDirectory(dir);   // clears read-only flags too
}

bool AnyZipFileExists(const std::wstring& targetDir, const std::wstring& zipPath) {
    if (!Files::DirectoryExists(targetDir)) return false;
    auto zip = Zip::Archive::Open(zipPath);
    if (!zip) return false;
    for (const auto& entry : zip->Entries()) {
        if (entry.name.empty()) continue;
        if (Files::Exists(Files::Combine(targetDir, Text::Replace(entry.fullName, L"/", L"\\")))) return true;
    }
    return false;
}

void UpdateConfigKey(const std::wstring& configPath, const std::wstring& key, const std::wstring& value) {
    auto lines = Files::ReadLines(configPath);
    for (auto& line : lines) {
        const std::wstring t = Text::TrimStart(line);
        if (Text::StartsWith(t, L"#") || Text::StartsWith(t, L"[") || t.find(L'=') == std::wstring::npos) continue;
        if (!Text::Equals(Text::Trim(t.substr(0, t.find(L'='))), key, true)) continue;
        line = key + L" = " + value;
        Files::WriteText(configPath, Text::Join(lines, L"\r\n") + L"\r\n");
        return;
    }
}

std::optional<std::wstring> ReadConfigKey(const std::wstring& configPath, const std::wstring& key) {
    for (const auto& line : Files::ReadLines(configPath)) {
        const std::wstring t = Text::Trim(line);
        if (t.empty() || t[0] == L'#' || t[0] == L'[' || t.find(L'=') == std::wstring::npos) continue;
        const size_t eq = t.find(L'=');
        if (Text::Equals(Text::Trim(t.substr(0, eq)), key, true)) return Text::Trim(t.substr(eq + 1));
    }
    return std::nullopt;
}

std::wstring ToolPath(const std::wstring& relative) { return Paths::Asset(L"Tools\\" + relative); }

} // namespace SpeedrunLauncher::Services::ModFiles
