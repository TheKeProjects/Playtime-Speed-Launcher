#include "Services/GameFiles/SaveGames.h"
#include <algorithm>
#include <cwctype>
#include "App/Paths.h"
#include "Core/Files.h"

namespace SpeedrunLauncher::Services::SaveGames {

namespace {

constexpr const wchar_t* kNgPlusMarker = L".ngplus_active";

std::wstring SaveGamesDir(const wchar_t* project) { return Files::Combine(Files::LocalAppData(), project, L"Saved", L"SaveGames"); }

// "[12] Name" → 12, anything else → -1 (special saves sort first)
int FolderNumber(const std::wstring& name) {
    if (name.size() < 3 || name[0] != L'[') return -1;
    size_t i = 1;
    int value = 0;
    while (i < name.size() && iswdigit(name[i])) value = value * 10 + (name[i++] - L'0');
    return i > 1 && i < name.size() && name[i] == L']' ? value : -1;
}

void DeleteMatching(const std::wstring& dir, const std::wstring& pattern) {
    if (!Files::DirectoryExists(dir)) return;
    for (const auto& f : Files::ListFiles(dir, pattern)) Files::Delete(f);
}

void CopyAll(const std::wstring& from, const std::wstring& to) {
    if (!Files::DirectoryExists(from)) return;
    Files::CreateDirectories(to);
    for (const auto& f : Files::ListFiles(from)) Files::Copy(f, Files::Combine(to, Files::FileName(f)), true);
}

const wchar_t* Project(int chapter) { return chapter == 4 ? L"ch4_pro" : L"ch5_pro"; }
const wchar_t* SlotPattern(int chapter) { return chapter == 4 ? L"SaveGame_*" : L"PoppySave_*"; }

} // namespace

std::vector<Checkpoint> Checkpoints(int chapter) {
    const wchar_t* file = chapter == 1 ? L"Chap1Checkpoint.sav" : chapter == 2 ? L"Chap2Checkpoint.sav" : chapter == 3 ? L"Playtime.sav" : nullptr;
    const wchar_t* project = chapter == 1 ? L"Poppy_Playtime" : chapter == 2 ? L"Playtime_Prototype4" : L"Playtime_Chapter3";
    if (!file) return {};
    const std::wstring savesDir = Files::Combine(Paths::SavesDir(), L"Chapter " + std::to_wstring(chapter));
    auto folders = Files::ListDirectories(savesDir);
    std::stable_sort(folders.begin(), folders.end(), [](const std::wstring& a, const std::wstring& b) {
        return FolderNumber(Files::FileName(a)) < FolderNumber(Files::FileName(b));
    });
    std::vector<Checkpoint> list;
    for (const auto& folder : folders) {
        const std::wstring source = Files::Combine(folder, file);
        if (!Files::Exists(source)) continue;
        list.push_back({ Files::FileName(folder), source, Files::Combine(SaveGamesDir(project), file) });
    }
    return list;
}

bool LoadCheckpoint(const Checkpoint& c) {
    Files::CreateDirectories(Files::DirectoryName(c.target));
    return Files::Copy(c.source, c.target, true);
}

void DeleteSaves(int chapter) {
    if (chapter != 4 && chapter != 5) return;
    const std::wstring dir = SaveGamesDir(Project(chapter));
    DeleteMatching(dir, SlotPattern(chapter));
    if (chapter == 5) Files::Delete(Files::Combine(dir, kNgPlusMarker));
}

void LoadStartSaves(int chapter) {
    if (chapter != 4 && chapter != 5) return;
    const std::wstring dir = SaveGamesDir(Project(chapter));
    DeleteMatching(dir, SlotPattern(chapter));
    if (chapter == 5) Files::Delete(Files::Combine(dir, kNgPlusMarker));
    CopyAll(Files::Combine(Paths::SavesDir(), L"Chapter " + std::to_wstring(chapter)), dir);
}

bool HasNgPlus() { return Files::DirectoryExists(Paths::Chapter5NgPlusDir()); }

bool NgPlusLoaded() { return Files::Exists(Files::Combine(SaveGamesDir(L"ch5_pro"), kNgPlusMarker)); }

void ToggleNgPlus() {
    const std::wstring dir = SaveGamesDir(L"ch5_pro");
    const std::wstring marker = Files::Combine(dir, kNgPlusMarker);
    const bool wasLoaded = Files::Exists(marker);
    DeleteMatching(dir, L"PoppySave_*");
    if (wasLoaded) {
        Files::Delete(marker);   // this click unloads them (the saves are already gone)
    } else if (HasNgPlus()) {
        CopyAll(Paths::Chapter5NgPlusDir(), dir);
        Files::WriteText(marker, L"");
    }
}

} // namespace SpeedrunLauncher::Services::SaveGames
