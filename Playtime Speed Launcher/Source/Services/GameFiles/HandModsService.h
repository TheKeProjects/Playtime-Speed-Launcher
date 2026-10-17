#pragma once
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// Community hand-skin pak mods from github.com/An-Average-Developer/Hand-Mods-PP
// ("Chapter N/mods.txt" lists the zips; each zip holds {Name}_P.pak/.ucas/.utoc plus an optional
// hand.txt declaring which hand colors it reskins, preview images and an optional _Extras folder).
// Network / zip calls block: run them on worker threads.
namespace SpeedrunLauncher::Services::HandMods {

inline constexpr const wchar_t* UnknownHand = L"Unknown";

struct Mod {
    std::wstring name;
    std::wstring baseName;
    std::wstring downloadUrl;
    long long size = 0;
};

struct HandImage {
    std::wstring color;
    std::string data;     // encoded image bytes
};

struct Extra {
    std::wstring entryName;   // path inside the parent zip
    std::wstring baseName;
    std::wstring color;
    long long size = 0;
};

std::vector<Mod> GetMods(int chapter);
std::wstring CacheDir(int chapter);
// Cached download (skipped when an identically sized copy exists). Returns the zip path or nullopt.
std::optional<std::wstring> Download(const Mod& mod, int chapter, const std::function<void(int)>& progress);

bool IsInstalled(const std::wstring& paksDir, const std::wstring& baseName);
std::vector<HandImage> ReadHandImages(const std::wstring& zipPath);
std::optional<std::vector<std::wstring>> ReadDeclaredHands(const std::wstring& zipPath);
std::vector<std::wstring> InstalledHands(const std::wstring& paksDir, const std::wstring& baseName);
bool HandsConflict(const std::vector<std::wstring>& a, const std::vector<std::wstring>& b);
bool Install(const std::wstring& paksDir, const std::wstring& zipPath, const std::wstring& baseName,
             const std::optional<std::vector<std::wstring>>& declaredHands = std::nullopt);
std::vector<Extra> ReadExtras(const std::wstring& zipPath);
std::optional<std::wstring> ExtractExtra(const std::wstring& parentZip, const Extra& extra, int chapter);
std::vector<std::pair<std::wstring, std::vector<std::wstring>>> AllInstalledHandMarkers(const std::wstring& paksDir);
void UninstallByBaseName(const std::wstring& paksDir, const std::wstring& baseName);

} // namespace SpeedrunLauncher::Services::HandMods
