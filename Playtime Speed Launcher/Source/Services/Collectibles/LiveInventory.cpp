#include "Services/Collectibles/LiveInventory.h"
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "Core/Text.h"
#include "Services/Collectibles/CollectiblesData.h"
#include "Services/Collectibles/SaveGameService.h"

namespace SpeedrunLauncher::Services::Collectibles::Live {

namespace {

// UObjectBase (UE5): vtable, ObjectFlags, InternalIndex, ClassPrivate, NamePrivate, OuterPrivate
constexpr uint64_t kObjFlags = 0x08, kObjIndex = 0x0C, kObjClass = 0x10, kObjName = 0x18, kObjOuter = 0x20, kObjHeader = 0x28;
constexpr uint64_t kStructPropertiesSize = 0x58;   // UStruct::PropertiesSize
constexpr uint32_t kItemSize = 24;                  // FUObjectItem
constexpr int kChunk = 65536;                       // FChunkedFixedUObjectArray elements per chunk
constexpr uint32_t RF_ClassDefaultObject = 0x10, RF_ArchetypeObject = 0x20, RF_BeginDestroyed = 0x8000, RF_FinishDestroyed = 0x10000;
constexpr int32_t kGarbage = 1 << 21;               // EInternalObjectFlags::Garbage (UE 5.4+)
constexpr int32_t kGarbage51 = 1 << 29;             // the same flag in UE 5.1 (Chapter 3)
constexpr uint64_t kStructSuper = 0x40;             // UStruct::SuperStruct
constexpr DWORD kRelocateEveryMs = 2000;            // retry finding the pools while the game starts
constexpr DWORD kCacheMs = 250;                     // the overlay asks twice a second
constexpr ULONGLONG kLookupEveryMs = 2000;         // look for a chapter's exe among the processes at most this often
constexpr ULONGLONG kRescanMs = 3000;               // Chapters 1 and 2: look for new actors (a level streamed in)

constexpr std::string_view kInstanceClass = "PoppyInventoryItemInstance";
constexpr std::string_view kManagerClass = "PoppyInventoryManagerComponent";

// The chapter's shipping exe(s). Chapter 1's Any% V1.1 still had the prototype's project name (the same
// exe name as Chapter 2): the reader tells them apart by what is in memory.
std::vector<const wchar_t*> Exes(int chapter) {
    switch (chapter) {
        case 1: return { L"Poppy_Playtime-Win64-Shipping.exe", L"Playtime_Prototype4-Win64-Shipping.exe" };
        case 2: return { L"Playtime_Prototype4-Win64-Shipping.exe" };
        case 3: return { L"Playtime_Chapter3-Win64-Shipping.exe" };
        case 4: return { L"ch4_pro-Win64-Shipping.exe" };
        case 5: return { L"ch5_pro-Win64-Shipping.exe" };
        default: return {};
    }
}

// Chapter 1's tapes by color: the inventory holds the tape's icon texture ("Green_VHS_Tape"), each VHS
// player has the tape's material ("vhs_tape_MAT_green", "MI_VHSTape_Orange", the black one's is purple
// before 1.3). Empty when it's none of them.
std::string TapeColor(std::string name) {
    for (char& c : name) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    struct Match { const char* word; const char* color; };
    static constexpr Match kMatches[] = { { "blue", "blue" }, { "green", "green" }, { "yellow", "yellow" }, { "orange", "yellow" },
                                          { "pink", "pink" }, { "black", "black" }, { "purple", "black" } };
    for (const Match& m : kMatches)
        if (name.find(m.word) != std::string::npos) return m.color;
    return {};
}

bool Pointer(uint64_t p) { return p > 0x10000 && p < 0x7FFFFFFFFFFF; }

class Game {
public:
    ~Game() { Close(); }

    // Still attached to a running game (no process listing needed)
    bool Attached(const wchar_t* exe) const {
        return process && attachedExe == exe && WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
    }

    // Keeps the process open between polls; reattaches when it restarts
    bool Attach(const wchar_t* exe) {
        if (Attached(exe)) {
            if ((!names || !objects) && GetTickCount() - lastLocate >= kRelocateEveryMs) {
                lastLocate = GetTickCount();
                LocatePools();
            }
            return names && objects;
        }
        const DWORD id = FindPid(exe);
        if (!id) {
            Close();
            return false;
        }
        if (id != pid) {
            Close();
            process = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, id);
            if (!process) return false;
            pid = id;
            attachedExe = exe;
            if (!FindModule(exe)) return false;
        }
        if ((!names || !objects) && GetTickCount() - lastLocate >= kRelocateEveryMs) {
            lastLocate = GetTickCount();
            LocatePools();
        }
        return names && objects;
    }

    void Forget() {
        docsGot.clear();
        tapesGot.clear();
        tapesPlayed.clear();
        mapSeen.clear();
        seenIn.clear();
        played.clear();
        ch5Seen.clear();
        keptInventory = nullptr;
    }

    std::shared_ptr<Inventory> ReadInventory(int chapter) {
        const auto inv = ReadInventoryNow(chapter);
        if (!inv) return nullptr;
        auto out = std::make_shared<Inventory>(*inv);
        out->pid = pid;
        out->newGames = newGames;
        return out;
    }

private:
    std::shared_ptr<Inventory> ReadInventoryNow(int chapter) {
        // Chapters 1 and 2 keep their objects between reads (FindActors loads the object array when needed)
        // While a save loads (no player yet) the last read is kept: otherwise the overlay drops to 0 for a
        // moment and announces the tapes again when they come back
        if (chapter == 1 || chapter == 2) {
            auto inv = chapter == 1 ? ReadTapes(chapter) : ReadChapter2(chapter);
            if (inv) keptInventory = inv;
            else if (!otherChapter) return keptInventory;
            return inv;
        }
        if (!LoadObjects()) return nullptr;
        UpdateClasses();
        if (chapter == 3) {   // same as Chapters 1 and 2 while a save loads
            auto inv = ReadMapInventory(chapter);
            if (inv) keptInventory = inv;
            return inv ? inv : keptInventory;
        }

        // The live inventory objects, grouped by owner (the player character): the manager component
        // (Chapter 5 lists the item definitions there) and the item instances (Chapter 4)
        std::unordered_map<uint64_t, std::vector<Item>> byOuter;
        for (size_t i = 0; i < objectPtrs.size(); ++i) {
            const uint64_t o = objectPtrs[i], cls = scanClasses[i];
            if (!cls || (objectFlags[i] & kGarbage)) continue;
            const std::string& className = ClassName(cls);
            const bool manager = className == kManagerClass;
            if (!manager && className != kInstanceClass) continue;
            uint8_t header[kObjHeader];
            if (!Read(o, header, sizeof(header))) continue;
            if (Get<uint32_t>(header, kObjFlags) & (RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed)) continue;
            byOuter[Get<uint64_t>(header, kObjOuter)].push_back({ o, cls, manager });
        }
        // Owners still alive. Other characters can have an (empty) inventory component, and a reloaded
        // checkpoint can leave the old player's items until the next garbage collection: keep the owner
        // with the most items, the newest one on a tie.
        std::shared_ptr<Inventory> best;
        int32_t bestIndex = -1;
        uint64_t bestOuter = 0;
        for (const auto& [outer, items] : byOuter) {
            const int32_t index = Index(outer);
            if (index < 0 || (objectFlags[index] & kGarbage)) continue;
            uint32_t flags = 0;
            Read(outer + kObjFlags, &flags, sizeof(flags));
            if (flags & (RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed)) continue;
            auto inv = ReadOwner(chapter, items);
            if (!best || inv->items > best->items || (inv->items == best->items && index > bestIndex)) {
                best = std::move(inv);
                bestIndex = index;
                bestOuter = outer;
            }
        }
        if (chapter == 4) {   // same as Chapters 1 to 3 while a save loads
            if (best) {
                KeepChapter4Collectibles(*best, bestOuter);
                keptInventory = best;
            }
            return best ? best : keptInventory;
        }
        if (chapter == 5) {   // same while a save loads (the glitch below happens on a loading screen)
            if (best) {
                KeepChapter5Collectibles(*best);
                keptInventory = best;
            }
            return best ? best : keptInventory;
        }
        return best;   // null: no player loaded
    }

    struct Item { uint64_t object; uint64_t cls; bool manager; };

    // Chapter 1 (UE 4.26 to 5): a picked up tape goes to the player's "Stored Inventory" (InventoryItem
    // structs, the tape's icon texture among them) and leaves it when it's put in its VHS player, whose
    // InsertTape timeline float goes from 0 to 1. Neither is saved, and the VHS players unload with their
    // level: what was seen is kept until the game closes (or Forget).
    // Text: "\0vhs:<color>\0" for each tape got, "\0played:vhs:<color>\0" for each one put in its player.
    std::shared_ptr<Inventory> ReadTapes(int chapter) {
        if (otherChapter) return nullptr;
        const Actors& found = FindActors();
        if (!found.components.empty()) {
            otherChapter = true;   // Chapter 2 (its exe has the same name as Chapter 1's Any% V1.1): leave it alone
            return nullptr;
        }
        const uint64_t player = found.player;
        const std::vector<uint64_t>& vhsPlayers = found.vhsPlayers;
        if (!player) return nullptr;   // main menu, loading
        const uint64_t playerClass = Read<uint64_t>(player + kObjClass);
        const int32_t stored = PropertyOffset(playerClass, "Stored Inventory");
        if (stored < 0) return nullptr;   // not Chapter 1 (Chapter 2 runs under the same exe name)

        // The inventory: a pointer to an object named after a tape color in each element
        const uint64_t data = Read<uint64_t>(player + stored);
        const int32_t num = Read<int32_t>(player + stored + 8);
        constexpr int32_t kElement = 40;   // InventoryItem: Texture, DisplayName, ToolTip
        if (Pointer(data) && num > 0 && num < 64) {
            std::vector<uint8_t> raw(static_cast<size_t>(num) * kElement);
            if (Read(data, raw.data(), raw.size()))
                for (int32_t k = 0; k < num; ++k)
                    for (int32_t at = 0; at < kElement; at += 8) {
                        const uint64_t p = Get<uint64_t>(raw.data(), static_cast<size_t>(k) * kElement + at);
                        if (!Alive(p)) continue;
                        const std::string& texture = Name(Read<uint32_t>(p + kObjName));
                        if (texture.find("VHS_Tape") == std::string::npos) continue;
                        if (std::string color = TapeColor(texture); !color.empty()) tapesGot.insert(color);
                    }
        }
        // The VHS players with their tape in
        for (const uint64_t vhs : vhsPlayers) {
            const uint64_t cls = Read<uint64_t>(vhs + kObjClass);
            const int32_t inserted = PropertyOffset(cls, "InsertTape_VHSFLoat"), material = PropertyOffset(cls, "VHSMaterial");
            if (inserted < 0 || material < 0 || Read<float>(vhs + inserted) < 0.5f) continue;
            const uint64_t m = Read<uint64_t>(vhs + material);
            if (!Pointer(m)) continue;
            if (std::string color = TapeColor(Name(Read<uint32_t>(m + kObjName))); !color.empty()) {
                tapesGot.insert(color);
                tapesPlayed.insert(color);
            }
        }

        auto inv = std::make_shared<Inventory>();
        inv->chapter = chapter;
        for (const std::string& color : tapesGot) inv->text += std::string(1, '\0') + "vhs:" + color + std::string(1, '\0');
        for (const std::string& color : tapesPlayed) inv->text += std::string(1, '\0') + "played:vhs:" + color + std::string(1, '\0');
        inv->items = tapesGot.size();
        return inv;
    }

    // Chapter 2 (UE 4.27): a tape is an item definition class ("LudwigVHSTape_Inventory_3DItem_C") in
    // the player's PlayerInventoryComponent (3DInventoryItems); like Chapter 1, it leaves it when put in
    // its VHS player (Correct3DInventoryItem, InsertTape float 0 -> 1). The trophies are in the checkpoint
    // save object (TrophyDataWrapper: trophy class, Collected), which the game writes to disk at once.
    // Text: "\0vhs:<Name>\0" / "\0played:vhs:<Name>\0" per tape (Name without VHSTape_Inventory_3DItem_C),
    // "\0trophy:<Name>\0" per trophy collected (Name without _Trophy_3DItem_C). The notes are item
    // definitions too ("Document814_Inventory_3DItem_C", "DocumentMatteo_..."), and stay in the
    // inventory: "\0doc:<Name>\0" (Name between Document and _Inventory_3DItem_C).
    std::shared_ptr<Inventory> ReadChapter2(int chapter) {
        const Actors& found = FindActors();
        const uint64_t player = found.player;
        const std::vector<uint64_t>& components = found.components;
        const std::vector<uint64_t>& vhsPlayers = found.vhsPlayers;
        const std::vector<uint64_t>& saves = found.saves;
        if (!player || components.empty()) return nullptr;   // main menu, loading, or not Chapter 2
        constexpr std::string_view kTapeSuffix = "VHSTape_Inventory_3DItem_C", kTrophySuffix = "_Trophy_3DItem_C", kDocSuffix = "_Inventory_3DItem_C";
        const auto stripped = [](const std::string& name, std::string_view suffix) {
            return name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0
                       ? name.substr(0, name.size() - suffix.size())
                       : std::string();
        };

        // The player's inventory (the component owned by the player, not the class's template)
        for (const uint64_t comp : components) {
            if (Read<uint64_t>(comp + kObjOuter) != player) continue;
            const int32_t items = PropertyOffset(Read<uint64_t>(comp + kObjClass), "3DInventoryItems");
            if (items < 0) continue;
            const uint64_t data = Read<uint64_t>(comp + items);
            const int32_t num = Read<int32_t>(comp + items + 8);
            if (!Pointer(data) || num <= 0 || num > 64) continue;
            std::vector<uint64_t> classes(static_cast<size_t>(num));
            if (!Read(data, classes.data(), classes.size() * 8)) continue;
            for (const uint64_t cls : classes) {
                if (!Pointer(cls)) continue;
                const std::string& name = Name(Read<uint32_t>(cls + kObjName));
                if (std::string tape = stripped(name, kTapeSuffix); !tape.empty()) tapesGot.insert(tape);
                if (std::string doc = stripped(name, kDocSuffix); doc.size() > 8 && doc.rfind("Document", 0) == 0) docsGot.insert(doc.substr(8));
            }
        }
        // The VHS players with their tape in
        for (const uint64_t vhs : vhsPlayers) {
            const uint64_t cls = Read<uint64_t>(vhs + kObjClass);
            const int32_t inserted = PropertyOffset(cls, "InsertTape_VHSFLoat"), correct = PropertyOffset(cls, "Correct3DInventoryItem");
            if (inserted < 0 || correct < 0 || Read<float>(vhs + inserted) < 0.5f) continue;
            const uint64_t item = Read<uint64_t>(vhs + correct);
            if (!Pointer(item)) continue;
            if (std::string tape = stripped(Name(Read<uint32_t>(item + kObjName)), kTapeSuffix); !tape.empty()) {
                tapesGot.insert(tape);
                tapesPlayed.insert(tape);
            }
        }
        // Trophies: TrophyData is { trophy class, bool Collected } (16 bytes)
        std::set<std::string> trophies;
        for (const uint64_t save : saves) {
            const int32_t wrapper = PropertyOffset(Read<uint64_t>(save + kObjClass), "TrophyDataWrapper");
            if (wrapper < 0) continue;
            const uint64_t data = Read<uint64_t>(save + wrapper);
            const int32_t num = Read<int32_t>(save + wrapper + 8);
            if (!Pointer(data) || num <= 0 || num > 64) continue;
            std::vector<uint8_t> raw(static_cast<size_t>(num) * 16);
            if (!Read(data, raw.data(), raw.size())) continue;
            for (int32_t k = 0; k < num; ++k) {
                const uint64_t cls = Get<uint64_t>(raw.data(), static_cast<size_t>(k) * 16);
                if (!Pointer(cls) || !raw[static_cast<size_t>(k) * 16 + 8]) continue;
                if (std::string trophy = stripped(Name(Read<uint32_t>(cls + kObjName)), kTrophySuffix); !trophy.empty()) trophies.insert(trophy);
            }
        }

        auto inv = std::make_shared<Inventory>();
        inv->chapter = chapter;
        const auto add = [&](const char* kind, const std::string& name) { inv->text += std::string(1, '\0') + kind + name + std::string(1, '\0'); };
        for (const std::string& t : tapesGot) add("vhs:", t);
        for (const std::string& t : tapesPlayed) add("played:vhs:", t);
        for (const std::string& t : trophies) add("trophy:", t);
        for (const std::string& d : docsGot) add("doc:", d);
        inv->items = tapesGot.size() + trophies.size() + docsGot.size();
        return inv;
    }


    // Chapters 1 and 2: the objects the tapes and trophies are read from. Going through the whole object
    // array takes a while (one read per object), so they're kept and only checked to be alive on each
    // read; the whole array is gone through again every few seconds (new levels streamed in, their VHS
    // players) or as soon as one of them is gone (a new game, a reload).
    struct Actors {
        uint64_t player = 0;                       // PlayerBP in the loaded level
        std::vector<uint64_t> components;          // Chapter 2's PlayerInventoryComponents
        std::vector<uint64_t> vhsPlayers;
        std::vector<uint64_t> saves;               // Chapter 2's checkpoint save objects (trophies)
    };

    // Still the live object at its index of the object array: reads only its own entry (FUObjectItem:
    // object, flags), not the whole array
    bool Alive(uint64_t o) {
        if (!Pointer(o)) return false;
        const int32_t index = Read<int32_t>(o + kObjIndex);
        if (index < 0) return false;
        const uint64_t chunk = Read<uint64_t>(Read<uint64_t>(objects) + static_cast<uint64_t>(index / kChunk) * 8);
        uint8_t item[16];
        if (!Pointer(chunk) || !Read(chunk + static_cast<uint64_t>(index % kChunk) * kItemSize, item, sizeof(item))) return false;
        if (Get<uint64_t>(item, 0) != o || (Get<int32_t>(item, 8) & (kGarbage | kGarbage51))) return false;
        return !(Read<uint32_t>(o + kObjFlags) & (RF_BeginDestroyed | RF_FinishDestroyed));
    }

    // The class of each object in the object array (0: none, or a template / an object being destroyed
    // when it was first seen). Incremental: only the slots that hold a different object than at the
    // last call are read (their header), so a few hundred reads instead of one per object.
    void UpdateClasses() {
        scanPtrs.resize(objectPtrs.size(), 0);
        scanClasses.resize(objectPtrs.size(), 0);
        for (size_t i = 0; i < objectPtrs.size(); ++i) {
            const uint64_t o = objectPtrs[i];
            if (o == scanPtrs[i]) continue;
            scanPtrs[i] = o;
            scanClasses[i] = 0;
            uint8_t header[kObjHeader];
            if (o && Read(o, header, sizeof(header)) &&
                !(Get<uint32_t>(header, kObjFlags) & (RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed)))
                scanClasses[i] = Get<uint64_t>(header, kObjClass);
        }
    }

    const Actors& FindActors() {
        const ULONGLONG now = GetTickCount64();
        bool fresh = actorsAt && now - actorsAt < kRescanMs && actors.player && Alive(actors.player);
        for (const uint64_t o : actors.vhsPlayers) fresh = fresh && Alive(o);
        for (const uint64_t o : actors.components) fresh = fresh && Alive(o);
        if (fresh) return actors;
        actors = {};
        actorsAt = now;
        if (!LoadObjects()) {
            actorsAt = 0;
            return actors;
        }
        UpdateClasses();
        for (size_t i = 0; i < objectPtrs.size(); ++i) {
            const uint64_t o = objectPtrs[i], cls = scanClasses[i];
            if (!cls || (objectFlags[i] & (kGarbage | kGarbage51))) continue;
            const std::string& className = ClassName(cls);
            if (className == "PlayerBP_C") {
                // The one in the loaded level (not a template elsewhere)
                if (Name(Read<uint32_t>(Read<uint64_t>(o + kObjOuter) + kObjName)) == "PersistentLevel" && Alive(o)) actors.player = o;
            } else if (className == "PlayerInventoryComponent_C") {
                actors.components.push_back(o);
            } else if (className == "VHSPlayerBP_C") {
                actors.vhsPlayers.push_back(o);
            } else if (className == "Chap2_Checkpoints_C" || className == "Chap1_Checkpoints_C") {
                actors.saves.push_back(o);
            }
        }
        if (!actors.player) actorsAt = 0;   // not in a level yet: look again on the next read
        return actors;
    }

    // Offset of a reflected property of the class (or a parent), found by the start of its name; -1 if
    // none. FField: Next at 0x20, NamePrivate at 0x28; FProperty::Offset_Internal at 0x4C; the class's
    // ChildProperties at 0x50 (UE 4.25 to 5).
    int32_t PropertyOffset(uint64_t cls, std::string_view prefix) {
        const auto key = std::make_pair(cls, std::string(prefix));
        if (auto it = propertyOffsets.find(key); it != propertyOffsets.end()) return it->second;
        int32_t found = -1;
        for (int depth = 0; depth < 12 && Pointer(cls) && found < 0; ++depth, cls = Read<uint64_t>(cls + kStructSuper)) {
            uint64_t field = Read<uint64_t>(cls + 0x50);
            for (int n = 0; n < 512 && Pointer(field); ++n, field = Read<uint64_t>(field + 0x20))
                if (Name(Read<uint32_t>(field + 0x28)).rfind(prefix, 0) == 0) {
                    found = Read<int32_t>(field + 0x4C);
                    break;
                }
        }
        if (found > 0x10000) found = -1;
        propertyOffsets.emplace(key, found);
        return found;
    }

    // Chapter 3: the live inventory components (class, or a parent class, named *InventoryComponent)
    // and the TMap<FName, int32> inside each. The player's is the one with the most items, the newest
    // on a tie (a reloaded checkpoint can leave the old one until the next garbage collection).
    std::shared_ptr<Inventory> ReadMapInventory(int chapter) {
        std::shared_ptr<Inventory> best;
        int32_t bestIndex = -1;
        uint64_t bestObject = 0;
        for (size_t i = 0; i < objectPtrs.size(); ++i) {
            const uint64_t o = objectPtrs[i], cls = scanClasses[i];
            if (!cls || (objectFlags[i] & kGarbage51) || !InventoryComponentClass(cls)) continue;
            if (Read<uint32_t>(o + kObjFlags) & (RF_ClassDefaultObject | RF_ArchetypeObject | RF_BeginDestroyed | RF_FinishDestroyed)) continue;
            int32_t size = 0;
            Read(cls + kStructPropertiesSize, &size, sizeof(size));
            size = std::clamp(size, static_cast<int32_t>(kObjHeader + 8), 0x1000);
            std::vector<uint8_t> bytes(static_cast<size_t>(size));
            if (!Read(o, bytes.data(), bytes.size())) continue;
            auto inv = std::make_shared<Inventory>();
            inv->chapter = chapter;
            for (size_t off = kObjHeader; off + 56 <= bytes.size(); off += 8) ReadNameMap(*inv, bytes.data() + off);
            const int32_t index = static_cast<int32_t>(i);
            if (!best || inv->items > best->items || (inv->items == best->items && index > bestIndex)) {
                best = std::move(inv);
                bestIndex = index;
                bestObject = o;
            }
        }
        if (!best) return nullptr;   // no player loaded
        KeepChapter3Collectibles(*best, bestObject);
        return best;
    }

    // Chapter 3: a tape leaves the inventory map when it's put in its VHS player, and a reloaded
    // checkpoint (or a death) spawns a new inventory without the tapes picked up or played since that
    // checkpoint. The tapes and notes seen are kept until the game closes (or Forget). Tapes are the
    // names in quotes, notes "Note <n>". A new game can't be told from a loaded save here: the
    // launcher's memory (CollectiblesMemory) is only reset by hand for Chapters 1 to 3.
    void KeepChapter3Collectibles(Inventory& inv, uint64_t component) {
        for (size_t at = 0; (at = inv.text.find('\0', at)) != std::string::npos;) {
            const size_t end = inv.text.find('\0', at + 1);
            if (end == std::string::npos) break;
            const std::string name = inv.text.substr(at + 1, end - at - 1);
            if (!name.empty() && (name.front() == '"' || name.rfind("Note ", 0) == 0)) mapSeen.insert(name);
            at = end + 1;
        }
        std::set<std::string> tapes;
        for (const std::string& name : mapSeen)
            if (name.front() == '"') tapes.insert(name);
        KeepSeen(inv, component, mapSeen, tapes);
    }

    // Chapter 4: like Chapter 3, a tape's item instance is gone once it's put in its VHS player, and a
    // reloaded checkpoint (or going back) spawns a new player with the items of that checkpoint. The
    // collectibles seen (any key of Data's Chapter 4 list) are kept for the whole session. They start
    // over on a new run: a new save, written since the game was opened, at the very start of the chapter
    // (see Chapter4AtStart).
    void KeepChapter4Collectibles(Inventory& inv, uint64_t owner) {
        if (NewRunSaved(4, Chapter4AtStart)) {
            mapSeen.clear();
            seenIn.clear();
            played.clear();
        }
        const auto& data = Data::Get(4);
        std::set<std::string> tapes;
        for (const auto& cat : data.cats)
            for (const auto& item : cat.items)
                for (const char* key : item.keys) {
                    if (std::string_view(cat.id) == "vhs") tapes.insert(key);
                    if (inv.text.find(std::string(1, '\0') + key + std::string(1, '\0')) != std::string::npos) mapSeen.insert(key);
                }
        KeepSeen(inv, owner, mapSeen, tapes);
    }

    // Chapter 5: loading the save again on a loading screen (a speedrun glitch) leaves the player with
    // no collectibles at all. Like Chapters 3 and 4, the ones seen ("/<key>.", any key of Data's
    // Chapter 5 list) are kept for the whole session. They start over on a new run: a new save, written
    // since the game was opened, at the very start of the chapter (see Chapter5AtStart). The glitch
    // happens mid-chapter, so it never looks like one.
    void KeepChapter5Collectibles(Inventory& inv) {
        if (NewRunSaved(5, Chapter5AtStart)) ch5Seen.clear();   // what's in the inventory right now is added back below
        for (const auto& cat : Data::Get(5).cats)
            for (const auto& item : cat.items)
                for (const char* key : item.keys) {
                    const std::string needle = "/" + std::string(key) + ".";
                    if (inv.text.find(needle) != std::string::npos) ch5Seen.insert(key);
                    else if (ch5Seen.count(key)) {
                        inv.text += needle;
                        ++inv.items;
                    }
                }
    }

    // Chapter 5's save (PoppySave_<date>.sav) has CurrentLevelTag, a gameplay tag naming the level and its
    // section: "LevelStreaming.01_LabsChase.0" is the start of the chapter.
    static bool Chapter5AtStart(const std::string& save) {
        constexpr std::string_view kProperty = "CurrentLevelTag", kStart{ "LevelStreaming.01_LabsChase.0\0", 30 };
        const size_t at = save.find(kProperty);
        if (at == std::string::npos) return false;
        const size_t start = save.find(kStart, at);
        return start != std::string::npos && start - at < 400;
    }

    // Chapters 4 and 5: the newest save is a new one (written since the game was opened, not checked
    // yet) and atStart says it's at the start of the chapter: a new run (counted in newGames).
    bool NewRunSaved(int chapter, bool (*atStart)(const std::string&)) {
        const auto save = SaveGames::Current(chapter);
        if (!save || SaveGames::FromBeforeLaunch(*save) || save->Stamp() == runSaveStamp) return false;
        runSaveStamp = save->Stamp();
        if (!atStart(save->data)) return false;
        ++newGames;
        return true;
    }

    // Chapter 4's save (decompressed) has the "Level.Checkpoint.<zone>.<n>" tags of the zones around the
    // player: at the start of the chapter, only "Level.Checkpoint.LiftControlRoom.1".
    static bool Chapter4AtStart(const std::string& save) {
        constexpr std::string_view kTag = "Level.Checkpoint.", kStart = "Level.Checkpoint.LiftControlRoom.1";
        bool start = false;
        for (size_t at = save.find(kTag); at != std::string::npos; at = save.find(kTag, at + kTag.size())) {
            size_t end = at + kTag.size();
            while (end < save.size() && (isalnum(static_cast<unsigned char>(save[end])) || save[end] == '_' || save[end] == '.')) ++end;
            if (std::string_view(save).substr(at, end - at) != kStart) return false;
            start = true;
        }
        return start;
    }

    // Chapters 3 and 4: the collectibles seen this session go back into the text (and count), and a
    // tape that leaves the same inventory it was seen in has been put in its VHS player: from then on
    // it's added as "\0played:<name>\0". One missing from a new inventory (a reloaded checkpoint) isn't:
    // it may have been picked up after that checkpoint.
    void KeepSeen(Inventory& inv, uint64_t holder, const std::set<std::string>& seen, const std::set<std::string>& tapes) {
        const auto key = [](const std::string& name) { return std::string(1, '\0') + name + std::string(1, '\0'); };
        for (const std::string& name : seen) {
            const bool present = inv.text.find(key(name)) != std::string::npos;
            if (tapes.count(name)) {
                if (present) seenIn[name] = holder;
                else if (auto it = seenIn.find(name); it != seenIn.end() && it->second == holder) played.insert(name);
            }
            if (!present) {
                inv.text += key(name);
                ++inv.items;
            }
        }
        for (const std::string& name : played) inv.text += key("played:" + name);
    }

    bool InventoryComponentClass(uint64_t cls) {
        if (auto it = inventoryClasses.find(cls); it != inventoryClasses.end()) return it->second;
        constexpr std::string_view kSuffix = "InventoryComponent";
        bool found = false;
        for (uint64_t c = cls; !found && Pointer(c); c = Read<uint64_t>(c + kStructSuper)) {
            const std::string& name = ClassName(c);
            found = name.size() >= kSuffix.size() && name.compare(name.size() - kSuffix.size(), kSuffix.size(), kSuffix) == 0;
            if (inventoryClasses.size() > 100000) break;
        }
        inventoryClasses.emplace(cls, found);
        return found;
    }

    // A TMap<FName, int32> at this spot of the object, if it is one: TSparseArray (TArray data, num,
    // max; TBitArray allocation flags: 4 inline words, secondary pointer, NumBits, MaxBits;
    // FirstFreeIndex, NumFreeIndices). Elements of 20 bytes: FName, int32, hash next id, hash index.
    void ReadNameMap(Inventory& inv, const uint8_t* at) {
        constexpr size_t kStride = 20;
        const uint64_t data = Get<uint64_t>(at, 0);
        const int32_t num = Get<int32_t>(at, 8), max = Get<int32_t>(at, 12);
        const uint64_t secondary = Get<uint64_t>(at, 32);
        const int32_t numBits = Get<int32_t>(at, 40), maxBits = Get<int32_t>(at, 44), numFree = Get<int32_t>(at, 52);
        if (!Pointer(data) || num <= 0 || num > max || max > 1024 || numBits != num || maxBits < numBits || numFree < 0 || numFree >= num) return;
        std::vector<uint32_t> bits(static_cast<size_t>((maxBits + 31) / 32));
        if (maxBits <= 128) memcpy(bits.data(), at + 16, bits.size() * 4);
        else if (!Pointer(secondary) || !Read(secondary, bits.data(), bits.size() * 4)) return;
        std::vector<uint8_t> raw(kStride * static_cast<size_t>(num));
        if (!Read(data, raw.data(), raw.size())) return;
        std::string text;
        size_t items = 0;
        for (int32_t k = 0; k < num; ++k) {
            if (!(bits[k / 32] & (1u << (k % 32)))) continue;   // a removed item's free slot
            const uint32_t index = Get<uint32_t>(raw.data(), k * kStride), number = Get<uint32_t>(raw.data(), k * kStride + 4);
            const std::string& name = Name(index);
            if (number || !Printable(name)) return;   // not a map of names
            text += std::string(1, '\0') + name + std::string(1, '\0');
            ++items;
        }
        if (items + static_cast<size_t>(numFree) != static_cast<size_t>(num)) return;
        inv.text += text;
        inv.items += items;
    }

    // Chapter 3's item names: "Wire Hand", "\"WELCOME TO PLAYTIME\"", "Note 1"...
    static bool Printable(const std::string& s) {
        if (s.empty() || s.size() > 120 || s == "None") return false;
        for (const char c : s)
            if (c < 0x20 || c > 0x7E) return false;
        return true;
    }

    std::shared_ptr<Inventory> ReadOwner(int chapter, const std::vector<Item>& items) {
        auto inv = std::make_shared<Inventory>();
        inv->chapter = chapter;
        for (const Item& item : items) {
            int32_t size = 0;
            Read(item.cls + kStructPropertiesSize, &size, sizeof(size));
            size = std::clamp(size, static_cast<int32_t>(kObjHeader + 8), 0x800);
            std::vector<uint8_t> bytes(static_cast<size_t>(size));
            if (!Read(item.object, bytes.data(), bytes.size())) continue;
            if (item.manager) {
                // Arrays whose elements hold an object (TArray: data, num, max): the item definitions
                // (ID_..._C) and the inventory entries (structs with the entry object inside)
                for (size_t off = kObjHeader; off + 16 <= bytes.size(); off += 8) {
                    const uint64_t data = Get<uint64_t>(bytes.data(), off);
                    const int32_t num = Get<int32_t>(bytes.data(), off + 8), max = Get<int32_t>(bytes.data(), off + 12);
                    if (!Pointer(data) || num <= 0 || num > max || max > 4096) continue;
                    for (const uint64_t e : ArrayObjects(data, num)) {
                        if (!AddObject(*inv, e)) continue;
                        ++inv->items;
                        // An entry object (Chapter 5's BP_VM_InventoryItem): the definition it points to. A
                        // new pickup is only here; the definitions array is filled in when the game saves.
                        uint64_t cls = 0;
                        int32_t entrySize = 0;
                        if (!Read(e + kObjClass, &cls, sizeof(cls)) || !Read(cls + kStructPropertiesSize, &entrySize, sizeof(entrySize))) continue;
                        entrySize = std::clamp(entrySize, static_cast<int32_t>(kObjHeader + 8), 0x200);
                        std::vector<uint8_t> entry(static_cast<size_t>(entrySize));
                        if (!Read(e, entry.data(), entry.size())) continue;
                        for (size_t eo = kObjHeader; eo + 8 <= entry.size(); eo += 8) AddObject(*inv, Get<uint64_t>(entry.data(), eo));
                    }
                }
                continue;
            }
            // FNames (Chapter 4: the DT_Items row)
            for (size_t off = kObjHeader; off + 8 <= bytes.size(); off += 4) {
                const uint32_t index = Get<uint32_t>(bytes.data(), off), number = Get<uint32_t>(bytes.data(), off + 4);
                if (!index || number) continue;
                const std::string& name = Name(index);
                if (Identifier(name)) inv->text += std::string(1, '\0') + name + std::string(1, '\0');
            }
            // Objects it points to (an item definition, if it has one)
            for (size_t off = kObjHeader; off + 8 <= bytes.size(); off += 8) AddObject(*inv, Get<uint64_t>(bytes.data(), off));
            ++inv->items;
        }
        return inv;
    }

private:
    // The object in each element of an array of unknown element type: the first stride / offset at
    // which every one of the `num` elements holds a live object. Empty if there is none.
    std::vector<uint64_t> ArrayObjects(uint64_t data, int32_t num) {
        constexpr size_t kStrides[] = { 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 128 };
        for (const size_t stride : kStrides) {
            std::vector<uint8_t> raw(stride * static_cast<size_t>(num));
            if (!Read(data, raw.data(), raw.size())) continue;
            for (size_t at = 0; at < stride; at += 8) {
                std::vector<uint64_t> out;
                for (int32_t k = 0; k < num; ++k) {
                    const uint64_t p = Get<uint64_t>(raw.data(), k * stride + at);
                    if (!Pointer(p) || Index(p) < 0) break;
                    out.push_back(p);
                }
                if (static_cast<int32_t>(out.size()) == num) return out;
            }
        }
        return {};
    }

    // "/<name>." for a live object (class names without _C, CDOs without Default__)
    bool AddObject(Inventory& inv, uint64_t p) {
        if (!Pointer(p) || Index(p) < 0) return false;
        uint32_t nameIndex = 0;
        if (!Read(p + kObjName, &nameIndex, sizeof(nameIndex))) return false;
        std::string name = Name(nameIndex);
        if (name.rfind("Default__", 0) == 0) name.erase(0, 9);
        if (name.size() > 2 && name.compare(name.size() - 2, 2, "_C") == 0) name.resize(name.size() - 2);
        if (!Identifier(name)) return false;
        inv.text += "/" + name + ".";
        return true;
    }

    template <typename T>
    static T Get(const void* base, size_t offset) {
        T v;
        memcpy(&v, static_cast<const uint8_t*>(base) + offset, sizeof(T));
        return v;
    }

    bool Read(uint64_t address, void* out, size_t size) const {
        SIZE_T got = 0;
        return ReadProcessMemory(process, reinterpret_cast<LPCVOID>(address), out, size, &got) && got == size;
    }
    template <typename T>
    T Read(uint64_t address) const {
        T v{};
        Read(address, &v, sizeof(T));
        return v;
    }

public:
    static DWORD FindPid(const wchar_t* exe) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;
        PROCESSENTRY32W e{ sizeof(e) };
        DWORD found = 0;
        for (BOOL ok = Process32FirstW(snap, &e); ok && !found; ok = Process32NextW(snap, &e))
            if (_wcsicmp(e.szExeFile, exe) == 0) found = e.th32ProcessID;
        CloseHandle(snap);
        return found;
    }

private:
    bool FindModule(const wchar_t* exe) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap == INVALID_HANDLE_VALUE) return false;
        MODULEENTRY32W m{ sizeof(m) };
        for (BOOL ok = Module32FirstW(snap, &m); ok; ok = Module32NextW(snap, &m))
            if (_wcsicmp(m.szModule, exe) == 0) {
                moduleBase = reinterpret_cast<uint64_t>(m.modBaseAddr);
                break;
            }
        CloseHandle(snap);
        return moduleBase != 0;
    }

    // FNamePool: FRWLock (8), uint32 CurrentBlock, uint32 CurrentByteCursor, uint8* Blocks[]; block 0
    // starts with the "None" entry. FChunkedFixedUObjectArray: FUObjectItem** Objects, PreAllocated,
    // int32 MaxElements, NumElements, MaxChunks, NumChunks.
    void LocatePools() {
        uint8_t header[0x1000];
        if (!Read(moduleBase, header, sizeof(header))) return;
        const uint32_t pe = Get<uint32_t>(header, 0x3C);
        if (pe + 0x200 > sizeof(header)) return;
        const uint16_t count = Get<uint16_t>(header, pe + 6), optional = Get<uint16_t>(header, pe + 20);
        std::vector<uint64_t> candidates;
        size_t at = pe + 24 + optional;
        for (uint16_t s = 0; s < count && at + 40 <= sizeof(header); ++s, at += 40) {
            if (memcmp(header + at, ".data\0", 6) != 0) continue;
            const uint32_t size = Get<uint32_t>(header, at + 8), va = Get<uint32_t>(header, at + 12);
            std::vector<uint8_t> data(size);
            // Large sections can have unreadable pages: read page by page, keeping zeros there
            for (uint32_t off = 0; off < size; off += 0x1000)
                Read(moduleBase + va + off, data.data() + off, std::min<uint32_t>(0x1000, size - off));
            for (size_t off = 0; off + 0x40 <= data.size(); off += 8) {
                if (!names) {
                    const uint32_t current = Get<uint32_t>(data.data(), off + 8), cursor = Get<uint32_t>(data.data(), off + 12);
                    const uint64_t block0 = Get<uint64_t>(data.data(), off + 16);
                    char none[4];
                    if (current > 0 && current < 8192 && cursor < 0x40000 && Pointer(block0) && Read(block0 + 2, none, 4) && memcmp(none, "None", 4) == 0)
                        names = moduleBase + va + off;
                }
                const uint64_t ptr = Get<uint64_t>(data.data(), off);
                const int32_t max = Get<int32_t>(data.data(), off + 16), num = Get<int32_t>(data.data(), off + 20);
                const int32_t maxChunks = Get<int32_t>(data.data(), off + 24), numChunks = Get<int32_t>(data.data(), off + 28);
                if (Pointer(ptr) && num > 1000 && num <= max && max < 0x4000000 && maxChunks == (max + kChunk - 1) / kChunk &&
                    numChunks == (num + kChunk - 1) / kChunk)
                    candidates.push_back(moduleBase + va + off);
            }
        }
        // Other arrays share that shape (UE 5.6 has a few): the real one starts with /Script/CoreUObject
        if (!names) return;
        for (const uint64_t c : candidates) {
            const uint64_t chunk0 = Read<uint64_t>(Read<uint64_t>(c));
            const uint64_t first = Pointer(chunk0) ? Read<uint64_t>(chunk0) : 0;
            if (Pointer(first) && Name(Read<uint32_t>(first + kObjName)) == "/Script/CoreUObject") {
                objects = c;
                break;
            }
        }
    }

    // Every object pointer and its internal flags, in index order
    bool LoadObjects() {
        objectPtrs.clear();
        objectFlags.clear();
        const int32_t num = Read<int32_t>(objects + 20);
        if (num <= 0 || num > 0x4000000) return false;
        const uint64_t chunks = Read<uint64_t>(objects);
        const int chunkCount = (num + kChunk - 1) / kChunk;
        std::vector<uint64_t> chunkPtrs(static_cast<size_t>(chunkCount));
        if (!Read(chunks, chunkPtrs.data(), chunkPtrs.size() * 8)) return false;
        objectPtrs.resize(static_cast<size_t>(num));
        objectFlags.resize(static_cast<size_t>(num));
        std::vector<uint8_t> buffer(kChunk * kItemSize);
        for (int c = 0; c < chunkCount; ++c) {
            const int n = std::min(kChunk, num - c * kChunk);
            if (!Pointer(chunkPtrs[c]) || !Read(chunkPtrs[c], buffer.data(), static_cast<size_t>(n) * kItemSize)) continue;
            for (int i = 0; i < n; ++i) {
                objectPtrs[static_cast<size_t>(c) * kChunk + i] = Get<uint64_t>(buffer.data(), static_cast<size_t>(i) * kItemSize);
                objectFlags[static_cast<size_t>(c) * kChunk + i] = Get<int32_t>(buffer.data(), static_cast<size_t>(i) * kItemSize + 8);
            }
        }
        return true;
    }

    // Index of a live object at that address, -1 if it isn't one
    int32_t Index(uint64_t object) const {
        int32_t index = -1;
        if (!Read(object + kObjIndex, &index, sizeof(index))) return -1;
        return index >= 0 && static_cast<size_t>(index) < objectPtrs.size() && objectPtrs[index] == object ? index : -1;
    }

    const std::string& ClassName(uint64_t cls) {
        auto it = classNames.find(cls);
        if (it != classNames.end()) return it->second;
        uint32_t index = 0;
        std::string name;
        if (Pointer(cls) && Read(cls + kObjName, &index, sizeof(index))) name = Name(index);
        return classNames.emplace(cls, std::move(name)).first->second;
    }

    // FNameEntryHeader (UE5): bWide:1, LowercaseProbeHash:5, Len:10
    const std::string& Name(uint32_t index) {
        auto it = nameCache.find(index);
        if (it != nameCache.end()) return it->second;
        std::string out;
        const uint32_t block = index >> 16, offset = (index & 0xFFFF) * 2;
        if (block <= Read<uint32_t>(names + 8)) {
            const uint64_t base = Read<uint64_t>(names + 16 + block * 8);
            uint16_t header = 0;
            if (Pointer(base) && Read(base + offset, &header, 2)) {
                const uint32_t len = header >> 6;
                if (len > 0 && len < 1024) {
                    if (header & 1) {
                        std::wstring wide(len, L'\0');
                        if (Read(base + offset + 2, wide.data(), len * 2)) out = Text::Narrow(wide);
                    } else {
                        out.resize(len);
                        if (!Read(base + offset + 2, out.data(), len)) out.clear();
                    }
                }
            }
        }
        return nameCache.emplace(index, std::move(out)).first->second;
    }

    // Row names and asset names: letters, digits, _ and : (Chapter 4 has "1322Report:Jack")
    static bool Identifier(const std::string& s) {
        if (s.size() < 3 || s.size() > 120 || s == "None") return false;
        for (const char c : s)
            if (!isalnum(static_cast<unsigned char>(c)) && c != '_' && c != ':') return false;
        return true;
    }

    void Close() {
        if (process) CloseHandle(process);
        process = nullptr;
        pid = 0;
        moduleBase = names = objects = 0;
        lastLocate = 0;
        nameCache.clear();
        classNames.clear();
        propertyOffsets.clear();
        actors = {};
        actorsAt = 0;
        otherChapter = false;
        scanPtrs.clear();
        scanClasses.clear();
        inventoryClasses.clear();
        attachedExe = nullptr;
        tapesGot.clear();
        tapesPlayed.clear();
        docsGot.clear();
        keptInventory = nullptr;
        mapSeen.clear();
        seenIn.clear();
        played.clear();
        ch5Seen.clear();
        runSaveStamp.clear();
        newGames = 0;
    }

    HANDLE process = nullptr;
    DWORD pid = 0;
    uint64_t moduleBase = 0, names = 0, objects = 0;
    DWORD lastLocate = 0;
    std::unordered_map<uint32_t, std::string> nameCache;
    std::unordered_map<uint64_t, std::string> classNames;
    std::map<std::pair<uint64_t, std::string>, int32_t> propertyOffsets;
    Actors actors;
    ULONGLONG actorsAt = 0;
    bool otherChapter = false;   // Chapter 1's reader attached to Chapter 2's game (same exe name)
    std::vector<uint64_t> scanPtrs;      // UpdateClasses: the object array as of the last call...
    std::vector<uint64_t> scanClasses;   // ...and the class of each of those objects
    std::unordered_map<uint64_t, bool> inventoryClasses;   // Chapter 3: class -> is an *InventoryComponent
    const wchar_t* attachedExe = nullptr;
    std::set<std::string> tapesGot, tapesPlayed, docsGot;   // Chapters 1 and 2 (docs: Chapter 2's notes)
    std::shared_ptr<Inventory> keptInventory;   // the last read, kept while a save loads
    std::set<std::string> mapSeen;      // Chapters 3 and 4: the collectibles seen this session (see KeepSeen)...
    std::map<std::string, uint64_t> seenIn;   // ...the inventory (Chapter 3) / player (Chapter 4) each tape was last seen in...
    std::set<std::string> played;       // ...and the tapes put in their VHS player
    std::set<std::string> ch5Seen;      // Chapter 5: the collectibles seen this session
    std::string runSaveStamp;           // Chapters 4 and 5: the newest save last checked for a new run (see NewRunSaved)
    int newGames = 0;                   // new games seen start since attaching (Inventory::newGames)
    std::vector<uint64_t> objectPtrs;
    std::vector<int32_t> objectFlags;
};

struct Slot {
    Game game;
    std::shared_ptr<const Inventory> last;
    ULONGLONG at = 0;
    ULONGLONG nextLookup = 0;   // not running: when to look among the processes again
};

std::mutex lock;
Slot slots[5];   // Chapters 1 to 5

} // namespace

std::shared_ptr<const Inventory> Read(int chapter) {
    const auto exes = Exes(chapter);
    if (exes.empty()) return nullptr;
    std::lock_guard guard(lock);
    Slot& slot = slots[chapter - 1];
    const ULONGLONG now = GetTickCount64();
    if (slot.at && now - slot.at < kCacheMs) return slot.last;
    slot.at = now;
    // The one of its exes that is running: the one already attached to, else look among the processes
    // (at most every few seconds while none is running)
    const wchar_t* exe = nullptr;
    for (const wchar_t* e : exes)
        if (slot.game.Attached(e)) exe = e;
    if (!exe) {
        if (now < slot.nextLookup) return slot.last = nullptr;
        slot.nextLookup = now + kLookupEveryMs;
        for (const wchar_t* e : exes)
            if (Game::FindPid(e)) {
                exe = e;
                break;
            }
        if (!exe) {
            slot.game.Attach(exes.front());   // lets go of a game that was closed
            return slot.last = nullptr;
        }
    }
    slot.last = slot.game.Attach(exe) ? slot.game.ReadInventory(chapter) : nullptr;
    return slot.last;
}

void Forget(int chapter) {
    if (chapter < 1 || chapter > 5) return;
    std::lock_guard guard(lock);
    Slot& slot = slots[chapter - 1];
    slot.game.Forget();
    slot.last = nullptr;
    slot.at = 0;
}

} // namespace SpeedrunLauncher::Services::Collectibles::Live
