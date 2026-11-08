#include "Services/Collectibles/SaveGameService.h"
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string_view>
#include <vector>
#include "Core/Files.h"
#include "Core/Zip.h"

namespace SpeedrunLauncher::Services::Collectibles::SaveGames {

namespace {

struct ChapterDef {
    int chapter;
    const wchar_t* folder;    // under %LOCALAPPDATA%
    const wchar_t* pattern;   // checkpoint saves (not the settings ones)
    const wchar_t* exe;       // shipping exe, for "running"
};

constexpr ChapterDef kDefs[] = {
    // Chapter 1 saves only the checkpoint: no pattern, its tapes are only read live
    { 1, L"Poppy_Playtime\\Saved\\SaveGames", L"", L"Poppy_Playtime-Win64-Shipping.exe" },
    { 2, L"Playtime_Prototype4\\Saved\\SaveGames", L"Chap2Checkpoint.sav", L"Playtime_Prototype4-Win64-Shipping.exe" },
    { 3, L"Playtime_Chapter3\\Saved\\SaveGames", L"Playtime.sav", L"Playtime_Chapter3-Win64-Shipping.exe" },
    { 4, L"ch4_pro\\Saved\\SaveGames", L"SaveGame_*.sav", L"ch4_pro-Win64-Shipping.exe" },
    { 5, L"ch5_pro\\Saved\\SaveGames", L"PoppySave_*.sav", L"ch5_pro-Win64-Shipping.exe" },
};

struct FileEntry {
    std::wstring path;
    int64_t lastWrite = 0;
    uint64_t size = 0;
};

const ChapterDef* Def(int chapter) {
    for (const auto& d : kDefs)
        if (d.chapter == chapter) return &d;
    return nullptr;
}

std::wstring Folder(const ChapterDef& d) { return Files::Combine(Files::LocalAppData(), d.folder); }

// Newest by name (the date is in it), then by write time
std::vector<FileEntry> Saves(const ChapterDef& d) {
    std::vector<FileEntry> files;
    if (!*d.pattern) return files;   // a chapter that saves no collectibles
    for (std::wstring& path : Files::ListFiles(Folder(d), d.pattern)) {
        FileEntry e;
        e.lastWrite = Files::LastWriteTime(path).value_or(0);
        e.size = Files::Size(path).value_or(0);
        e.path = std::move(path);
        files.push_back(std::move(e));
    }
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
        const std::wstring na = Files::FileName(a.path), nb = Files::FileName(b.path);
        return na != nb ? na > nb : a.lastWrite > b.lastWrite;
    });
    return files;
}

// When the exe was started (FILETIME ticks), 0 if it isn't running. 1 if running but the start time
// can't be read.
int64_t StartedAt(const wchar_t* exe) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W e{ sizeof(e) };
    int64_t started = 0;
    for (BOOL ok = Process32FirstW(snap, &e); ok; ok = Process32NextW(snap, &e)) {
        if (_wcsicmp(e.szExeFile, exe) != 0) continue;
        started = 1;
        if (HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.th32ProcessID)) {
            FILETIME create{}, exit{}, kernel{}, user{};
            if (GetProcessTimes(process, &create, &exit, &kernel, &user))
                started = (static_cast<int64_t>(create.dwHighDateTime) << 32) | create.dwLowDateTime;
            CloseHandle(process);
        }
        break;
    }
    CloseHandle(snap);
    return started;
}

bool Running(const wchar_t* exe) { return StartedAt(exe) != 0; }

int64_t ReadI64(std::string_view s, size_t at) {
    int64_t v = 0;
    if (at + 8 <= s.size()) memcpy(&v, s.data() + at, 8);
    return v;
}

// A zlib stream: 2-byte header, then raw DEFLATE (the Adler-32 checksum is not verified)
bool InflateZlib(std::string_view in, std::string& out) {
    if (in.size() < 2 || (static_cast<uint8_t>(in[0]) & 0x0F) != 8) return false;
    return Zip::Inflate(reinterpret_cast<const uint8_t*>(in.data()) + 2, in.size() - 2, out);
}

// Unreal's compressed save: int64 uncompressed size, FString "Zlib", then one or more chunks of
//   int64 tag (0x222222229E2A83C1), int64 block size, uint8 format, int64 total compressed, int64 total
//   uncompressed, (int64 compressed, int64 uncompressed) per block, block data (zlib streams)
std::string Decompress(const std::string& file) {
    if (file.size() >= 4 && file.compare(0, 4, "GVAS") == 0) return file;
    std::string out;
    constexpr char kTag[4] = { '\xC1', '\x83', '\x2A', '\x9E' };
    const std::string_view s(file);
    const int64_t expected = ReadI64(s, 0);
    size_t at = s.find(std::string_view(kTag, 4));
    while (at != std::string_view::npos && at + 41 <= s.size()) {
        const int64_t blockSize = ReadI64(s, at + 8);
        if (blockSize <= 0 || blockSize > (1 << 26)) break;
        // UE5 writes a one-byte format after the block size, UE4 doesn't: take the layout whose first
        // block starts with a zlib header
        size_t p = 0;
        int64_t total = 0, blocks = 0;
        for (const size_t skip : { size_t{ 1 }, size_t{ 0 } }) {
            const size_t q = at + 16 + skip;
            const int64_t t = ReadI64(s, q + 8);
            if (t <= 0 || t > (int64_t{ 1 } << 32)) continue;
            const int64_t n = (t + blockSize - 1) / blockSize;
            const size_t dataStart = q + 16 + 16 * static_cast<size_t>(n);
            if (dataStart < s.size() && static_cast<uint8_t>(s[dataStart]) == 0x78) {
                p = q + 16;
                total = t;
                blocks = n;
                break;
            }
        }
        if (!total) break;
        std::vector<int64_t> sizes;
        for (int64_t b = 0; b < blocks; ++b, p += 16) sizes.push_back(ReadI64(s, p));
        for (const int64_t c : sizes) {
            if (c <= 0 || p + static_cast<size_t>(c) > s.size()) return {};
            if (!InflateZlib(s.substr(p, static_cast<size_t>(c)), out)) return {};
            p += static_cast<size_t>(c);
        }
        if (expected > 0 && static_cast<int64_t>(out.size()) >= expected) break;
        at = s.find(std::string_view(kTag, 4), p);
    }
    // Chapter 4's payload is its own archive, not GVAS: only the size can be checked
    return expected <= 0 || static_cast<int64_t>(out.size()) == expected ? out : std::string();
}

// Chapter 2's save keeps the trophies as TrophyData structs (an ObjectProperty with the trophy class's
// path, then the Collected BoolProperty: name, type, int64 size 0, the value, the GUID flag) and the
// tapes in hand as paths in 3DInventoryItems. Appended to the save as the same names the live reader
// gives: "\0trophy:<Name>\0" per collected trophy, "\0vhs:<Name>\0" per tape.
void AddChapter2Names(std::string& data) {
    std::string names;
    const auto nameBefore = [&](size_t end) {   // ".../Daisy_Trophy_3DItem.Daisy" <- end
        size_t start = end;
        while (start > 0 && data[start - 1] != '.' && data[start - 1] != '/' && data[start - 1] != '\0') --start;
        return data.substr(start, end - start);
    };
    constexpr std::string_view kTrophy = "_Trophy_3DItem_C", kTape = "VHSTape_Inventory_3DItem_C", kBool = "BoolProperty";
    for (size_t at = data.find(kTrophy); at != std::string::npos; at = data.find(kTrophy, at + kTrophy.size())) {
        const size_t value = data.find(kBool, at);
        const size_t next = data.find(kTrophy, at + kTrophy.size());
        if (value == std::string::npos || (next != std::string::npos && value > next)) continue;
        const size_t flag = value + kBool.size() + 1 + 8;   // past the terminator and the int64 size
        if (flag < data.size() && data[flag] == 1) names += std::string(1, '\0') + "trophy:" + nameBefore(at) + std::string(1, '\0');
    }
    for (size_t at = data.find(kTape); at != std::string::npos; at = data.find(kTape, at + kTape.size()))
        names += std::string(1, '\0') + "vhs:" + nameBefore(at) + std::string(1, '\0');
    data += names;
}

std::mutex lock;
std::shared_ptr<const Save> cached[5];   // Chapters 1 to 5

} // namespace

std::string Save::Stamp() const {
    char buffer[64];
    sprintf_s(buffer, "\"%d-%llx-%llx\"", chapter, static_cast<unsigned long long>(lastWrite), static_cast<unsigned long long>(size));
    return buffer;
}

ChapterInfo Info(int chapter) {
    ChapterInfo info;
    info.chapter = chapter;
    const ChapterDef* d = Def(chapter);
    if (!d) return info;
    info.folder = Folder(*d);
    info.folderExists = Files::DirectoryExists(info.folder);
    const int64_t started = StartedAt(d->exe);
    info.running = started != 0;
    const auto saves = Saves(*d);
    info.saves = saves.size();
    if (!saves.empty()) {
        info.newest = Files::FileName(saves.front().path);
        info.newestWrite = saves.front().lastWrite;
    }
    info.waitingFirstSave = started > 1 && (saves.empty() || saves.front().lastWrite < started);
    return info;
}

std::string AlongLive(int chapter) {
    if (chapter != 2) return {};
    const auto save = Current(chapter);
    if (!save || FromBeforeLaunch(*save)) return {};
    // Only the "\0trophy:<Name>\0" names AddChapter2Names put at the end: the tapes come from memory
    std::string out;
    constexpr std::string_view kTrophy{ "\0trophy:", 8 };
    for (size_t at = save->data.find(kTrophy); at != std::string::npos; at = save->data.find(kTrophy, at + 1)) {
        const size_t end = save->data.find('\0', at + 1);
        if (end == std::string::npos) break;
        out.append(save->data, at, end + 1 - at);
    }
    return out;
}

bool FromBeforeLaunch(const Save& save) {
    const ChapterDef* d = Def(save.chapter);
    if (!d) return false;
    const int64_t started = StartedAt(d->exe);
    return started > 1 && save.lastWrite < started;
}

int AutoChapter() {
    int best = 5;
    int64_t bestWrite = -1;
    for (const auto& d : kDefs) {
        if (Running(d.exe)) return d.chapter;
        for (const auto& e : Saves(d))
            if (e.lastWrite > bestWrite) {
                bestWrite = e.lastWrite;
                best = d.chapter;
            }
    }
    return best;
}

std::shared_ptr<const Save> Current(int chapter) {
    const ChapterDef* d = Def(chapter);
    if (!d) return nullptr;
    const auto saves = Saves(*d);
    std::lock_guard guard(lock);
    auto& slot = cached[chapter - 1];
    // The newest file may be half-written: fall back to the previous ones
    for (size_t i = 0; i < saves.size() && i < 3; ++i) {
        const auto& e = saves[i];
        if (slot && slot->path == e.path && slot->lastWrite == e.lastWrite && slot->size == e.size) return slot;
        auto bytes = Files::ReadBytes(e.path);
        if (!bytes) continue;
        std::string data = Decompress(*bytes);
        if (chapter == 2 && !data.empty()) AddChapter2Names(data);
        if (data.empty()) continue;
        auto save = std::make_shared<Save>();
        save->chapter = chapter;
        save->path = e.path;
        save->lastWrite = e.lastWrite;
        save->size = e.size;
        save->data = std::move(data);
        slot = save;
        return slot;
    }
    slot.reset();
    return nullptr;
}

} // namespace SpeedrunLauncher::Services::Collectibles::SaveGames
