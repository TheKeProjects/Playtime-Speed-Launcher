#include "Services/App/ResourceExtractor.h"
#include <windows.h>
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Text.h"
#include "Core/Zip.h"
#include "Generated/AssetTable.h"

namespace SpeedrunLauncher::Services::ResourceExtractor {

namespace {

bool ResourceBytes(int id, const void*& data, DWORD& size) {
    HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!res) return false;
    HGLOBAL handle = LoadResource(nullptr, res);
    if (!handle) return false;
    data = LockResource(handle);
    size = SizeofResource(nullptr, res);
    return data != nullptr;
}

// A stamp of the exe build + embedded content: when it matches the previous run, files whose size
// still matches are left alone instead of being rewritten (~120 MB) on every start.
std::wstring CurrentStamp() {
    const std::wstring exe = Files::ExecutablePath();
    return std::wstring(kEmbeddedAssetsStamp) + L"|" + std::to_wstring(Files::Size(exe).value_or(0)) + L"|" +
           std::to_wstring(Files::LastWriteTime(exe).value_or(0));
}

} // namespace

void Extract() {
    const std::wstring temp = Paths::TempDir();
    const std::wstring stampFile = Files::Combine(temp, L".extracted");
    const std::wstring stamp = CurrentStamp();
    const bool sameBuild = Files::ReadText(stampFile).value_or(L"") == stamp;
    if (!sameBuild) Files::DeleteDirectory(temp);
    Files::CreateDirectories(temp);

    for (const auto& asset : kEmbeddedAssets) {
        const void* data = nullptr;
        DWORD size = 0;
        if (!ResourceBytes(asset.id, data, size)) continue;
        const std::wstring root = asset.root;
        const std::wstring relative = asset.path;

        // Checkpoint saves ship as zips: unpack them into Assets\Saves (regular) and
        // Assets\Saves\Chapter 5 NG+ (flat .sav files) so they never collide.
        if (root == L"Assets" && Text::Equals(Files::DirectoryName(relative), L"Saves", true)) {
            const std::wstring name = Files::FileName(relative);
            std::wstring target;
            if (Text::Equals(name, L"Saves.zip", true)) target = Paths::SavesDir();
            else if (Text::Equals(name, L"CH5 NG+.zip", true)) target = Paths::Chapter5NgPlusDir();
            if (!target.empty()) {
                auto archive = Zip::Archive::FromMemory(std::string(static_cast<const char*>(data), size));
                if (archive) archive->ExtractAll(target, true);
                continue;
            }
        }

        const std::wstring dest = Files::Combine(temp, root, relative);
        if (sameBuild && Files::Size(dest).value_or(~0ull) == size) continue;
        Files::WriteBytes(dest, data, size);
    }
    Files::WriteText(stampFile, stamp);
}

void CleanupSaves() { Files::DeleteDirectory(Paths::SavesDir()); }

} // namespace SpeedrunLauncher::Services::ResourceExtractor
