#include "Services/GameFiles/HandModsService.h"
#include <algorithm>
#include "App/Config.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Http.h"
#include "Core/Text.h"
#include "Core/Zip.h"

namespace SpeedrunLauncher::Services::HandMods {

namespace {

constexpr const wchar_t* kOwner = Config::HandMods::Owner;
constexpr const wchar_t* kRepo = Config::HandMods::Repo;
constexpr const wchar_t* kBranch = Config::HandMods::Branch;
constexpr const wchar_t* kHandDeclaration = L"hand.txt";
constexpr const wchar_t* kExtrasMarker = L"_extras";
const std::vector<std::pair<std::wstring, std::wstring>> kHeaders = { { L"User-Agent", Config::UserAgent } };

std::wstring RawUrl(int chapter, const std::wstring& file) {
    return std::wstring(L"https://raw.githubusercontent.com/") + kOwner + L"/" + kRepo + L"/" + kBranch + L"/Chapter%20" +
           std::to_wstring(chapter) + L"/" + Text::UrlEncode(file);
}

// GitHub's LFS-resolving host (raw.githubusercontent.com only serves the pointer file for LFS zips)
std::wstring LfsUrl(int chapter, const std::wstring& file) {
    return std::wstring(L"https://media.githubusercontent.com/media/") + kOwner + L"/" + kRepo + L"/" + kBranch + L"/Chapter%20" +
           std::to_wstring(chapter) + L"/" + Text::UrlEncode(file);
}

long long HeadSize(const std::wstring& url) {
    Http::Request head;
    head.method = L"HEAD";
    head.url = url;
    head.headers = kHeaders;
    const Http::Response r = Http::Send(head);
    if (!r.Ok()) return 0;
    return Text::ToInt(r.Header(L"content-length")).value_or(0);
}

bool IsImage(const std::wstring& name) {
    const std::wstring ext = Text::Lower(Files::Extension(name));
    return ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".bmp" || ext == L".gif";
}

std::wstring ColorSuffix(const std::wstring& stem) {
    const size_t underscore = stem.find_last_of(L'_');
    return underscore != std::wstring::npos && underscore + 1 < stem.size() ? stem.substr(underscore + 1) : stem;
}

std::vector<std::wstring> ParseColors(const std::wstring& raw) {
    std::vector<std::wstring> colors;
    for (const auto& part : Text::Split(raw, L',')) {
        std::wstring color = Text::Trim(part);
        while (!color.empty() && (color.front() == L'"' || color.front() == L'\'')) color.erase(0, 1);
        while (!color.empty() && (color.back() == L'"' || color.back() == L'\'')) color.pop_back();
        color = Text::Trim(color);
        if (!color.empty()) colors.push_back(color);
    }
    return colors;
}

std::wstring MarkerPath(const std::wstring& paksDir, const std::wstring& baseName) {
    return Files::Combine(paksDir, baseName + L".hand.txt");
}

void RemoveStrayDeclaration(const std::wstring& paksDir) { Files::Delete(Files::Combine(paksDir, kHandDeclaration)); }

} // namespace

std::vector<Mod> GetMods(int chapter) {
    std::vector<Mod> mods;
    const Http::Response manifest = Http::Get(RawUrl(chapter, L"mods.txt"), kHeaders);
    if (!manifest.Ok()) return mods;
    for (const auto& raw : Text::SplitLines(Text::Widen(manifest.body))) {
        const std::wstring file = Text::Trim(raw);
        if (file.empty() || file[0] == L'#' || !Text::EndsWith(file, L".zip", true)) continue;
        Mod mod;
        mod.baseName = Files::FileNameWithoutExtension(file);
        mod.name = mod.baseName;
        mod.downloadUrl = RawUrl(chapter, file);
        mod.size = HeadSize(mod.downloadUrl);
        // A tiny "zip" is a Git LFS pointer: switch to the media host
        if (mod.size > 0 && mod.size < 1024) {
            mod.downloadUrl = LfsUrl(chapter, file);
            mod.size = HeadSize(mod.downloadUrl);
        }
        mods.push_back(std::move(mod));
    }
    return mods;
}

std::wstring CacheDir(int chapter) { return Paths::DataFile(L"HandMods\\Chapter" + std::to_wstring(chapter)); }

std::optional<std::wstring> Download(const Mod& mod, int chapter, const std::function<void(int)>& progress) {
    const std::wstring dir = CacheDir(chapter);
    Files::CreateDirectories(dir);
    const std::wstring zip = Files::Combine(dir, mod.baseName + L".zip");
    if (Files::Exists(zip) && mod.size > 0 && static_cast<long long>(Files::Size(zip).value_or(0)) == mod.size) {
        if (progress) progress(100);
        return zip;
    }
    const bool ok = Http::Download(mod.downloadUrl, zip, [&](uint64_t received, uint64_t total) {
        const uint64_t all = total ? total : static_cast<uint64_t>(std::max(0ll, mod.size));
        if (progress && all > 0) progress(static_cast<int>(received * 100 / all));
        return true;
    }, nullptr, kHeaders);
    if (!ok) return std::nullopt;
    return zip;
}

bool IsInstalled(const std::wstring& paksDir, const std::wstring& baseName) {
    return Files::Exists(Files::Combine(paksDir, baseName + L"_P.pak"));
}

std::vector<HandImage> ReadHandImages(const std::wstring& zipPath) {
    std::vector<HandImage> images;
    auto zip = Zip::Archive::Open(zipPath);
    if (!zip) return images;
    for (const auto& entry : zip->Entries()) {
        if (entry.name.empty() || !IsImage(entry.name)) continue;
        auto bytes = zip->Read(entry);
        if (!bytes) continue;
        images.push_back({ ColorSuffix(Files::FileNameWithoutExtension(entry.name)), std::move(*bytes) });
    }
    return images;
}

std::optional<std::vector<std::wstring>> ReadDeclaredHands(const std::wstring& zipPath) {
    auto zip = Zip::Archive::Open(zipPath);
    if (!zip) return std::nullopt;
    for (const auto& entry : zip->Entries()) {
        if (!Text::Equals(entry.name, kHandDeclaration, true)) continue;
        auto bytes = zip->Read(entry);
        if (!bytes) return std::nullopt;
        auto colors = ParseColors(Text::Widen(*bytes));
        if (colors.empty()) return std::nullopt;
        return colors;
    }
    return std::nullopt;
}

std::vector<std::wstring> InstalledHands(const std::wstring& paksDir, const std::wstring& baseName) {
    auto text = Files::ReadText(MarkerPath(paksDir, baseName));
    if (!text) return { UnknownHand };
    auto colors = ParseColors(*text);
    if (colors.empty()) return { UnknownHand };
    return colors;
}

bool HandsConflict(const std::vector<std::wstring>& a, const std::vector<std::wstring>& b) {
    const auto unknown = [](const std::wstring& h) { return Text::Equals(h, UnknownHand, true); };
    if (std::any_of(a.begin(), a.end(), unknown) || std::any_of(b.begin(), b.end(), unknown)) return true;
    for (const auto& x : a)
        for (const auto& y : b)
            if (Text::Equals(x, y, true)) return true;
    return false;
}

bool Install(const std::wstring& paksDir, const std::wstring& zipPath, const std::wstring& baseName,
             const std::optional<std::vector<std::wstring>>& declaredHands) {
    Files::CreateDirectories(paksDir);
    auto zip = Zip::Archive::Open(zipPath);
    if (!zip) return false;
    for (const auto& entry : zip->Entries()) {
        if (entry.name.empty()) continue;
        if (Text::Equals(entry.name, kHandDeclaration, true)) continue;   // recorded as a per-mod marker instead
        if (IsImage(entry.name)) continue;                                // preview, not a pak asset
        if (Text::Contains(entry.fullName, kExtrasMarker, true)) continue; // per-color variants
        if (!zip->ExtractEntry(entry, Files::Combine(paksDir, Text::Replace(entry.fullName, L"/", L"\\")))) return false;
    }
    std::vector<std::wstring> hands;
    if (declaredHands) hands = *declaredHands;
    else if (auto read = ReadDeclaredHands(zipPath)) hands = *read;
    else hands = { UnknownHand };
    Files::WriteText(MarkerPath(paksDir, baseName), Text::Join(hands, L", "));
    RemoveStrayDeclaration(paksDir);
    return true;
}

std::vector<Extra> ReadExtras(const std::wstring& zipPath) {
    std::vector<Extra> extras;
    auto zip = Zip::Archive::Open(zipPath);
    if (!zip) return extras;
    for (const auto& entry : zip->Entries()) {
        if (entry.name.empty() || !Text::EndsWith(entry.name, L".zip", true)) continue;
        const size_t slash = entry.fullName.find(L'/');
        if (slash == std::wstring::npos) continue;                              // not in a subfolder
        if (entry.fullName.find(L'/', slash + 1) != std::wstring::npos) continue; // nested too deep
        if (!Text::Contains(entry.fullName.substr(0, slash), kExtrasMarker, true)) continue;
        const std::wstring base = Files::FileNameWithoutExtension(entry.name);
        extras.push_back({ entry.fullName, base, ColorSuffix(base), static_cast<long long>(entry.length) });
    }
    return extras;
}

std::optional<std::wstring> ExtractExtra(const std::wstring& parentZip, const Extra& extra, int chapter) {
    const std::wstring dir = CacheDir(chapter);
    Files::CreateDirectories(dir);
    const std::wstring zipPath = Files::Combine(dir, extra.baseName + L".zip");
    if (Files::Exists(zipPath) && static_cast<long long>(Files::Size(zipPath).value_or(0)) == extra.size) return zipPath;
    auto zip = Zip::Archive::Open(parentZip);
    if (!zip) return std::nullopt;
    const Zip::Entry* entry = zip->Find(extra.entryName);
    if (!entry || !zip->ExtractEntry(*entry, zipPath)) return std::nullopt;
    return zipPath;
}

std::vector<std::pair<std::wstring, std::vector<std::wstring>>> AllInstalledHandMarkers(const std::wstring& paksDir) {
    std::vector<std::pair<std::wstring, std::vector<std::wstring>>> result;
    for (const auto& path : Files::ListFiles(paksDir, L"*.hand.txt")) {
        std::wstring name = Files::FileName(path);
        name.resize(name.size() - std::wstring(L".hand.txt").size());
        auto colors = ParseColors(Files::ReadText(path).value_or(L""));
        result.emplace_back(name, colors.empty() ? std::vector<std::wstring>{ UnknownHand } : colors);
    }
    return result;
}

void UninstallByBaseName(const std::wstring& paksDir, const std::wstring& baseName) {
    for (const wchar_t* ext : { L"pak", L"ucas", L"utoc" }) Files::Delete(Files::Combine(paksDir, baseName + L"_P." + ext));
    Files::Delete(MarkerPath(paksDir, baseName));
    RemoveStrayDeclaration(paksDir);
}

} // namespace SpeedrunLauncher::Services::HandMods
