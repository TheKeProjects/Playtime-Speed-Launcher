#include "Services/Hotkeys/CoresService.h"
#include <windows.h>
#include "Core/Files.h"
#include "Core/Process.h"

namespace SpeedrunLauncher::Services {

namespace {

uintptr_t AllCores() {
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors >= sizeof(uintptr_t) * 8 ? ~uintptr_t(0) : (uintptr_t(1) << info.dwNumberOfProcessors) - 1;
}

// Only PROCESS_SET_INFORMATION, like .NET's Process.ProcessorAffinity setter: the games'
// anti-cheat / DRM refuses handles that also ask for query rights, which made this fail silently.
void SetAffinity(uint32_t pid, uintptr_t mask) {
    HANDLE h = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    if (!h) return;
    SetProcessAffinityMask(h, mask);
    CloseHandle(h);
}

// Whether the pid is still in the process table (no handle needed, so anti-cheat can't hide it)
bool StillRunning(uint32_t pid) {
    for (const auto& p : Process::List())
        if (p.pid == pid) return true;
    return false;
}

uintptr_t GetAffinity(uint32_t pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return 0;
    DWORD_PTR process = 0, system = 0;
    const BOOL ok = GetProcessAffinityMask(h, &process, &system);
    CloseHandle(h);
    return ok ? process : 0;
}

unsigned long GetPriority(uint32_t pid, unsigned long fallback) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return fallback;
    const DWORD value = GetPriorityClass(h);
    CloseHandle(h);
    return value ? value : fallback;
}

// A narrow PROCESS_SET_INFORMATION handle gets past anti-cheat filters that deny the combined
// query+set request; the first call right after an affinity change can be dropped, so retry.
void SetPriority(uint32_t pid, unsigned long priority) {
    for (int attempt = 0; attempt < 5; ++attempt) {
        HANDLE h = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
        if (!h) return;
        const BOOL ok = SetPriorityClass(h, priority);
        CloseHandle(h);
        if (ok && GetPriority(pid, priority) == priority) return;
        Sleep(20);
    }
}

} // namespace

std::optional<CoresService::Found> CoresService::FindGameProcess(const std::vector<Models::Chapter>& chapters, const ExePathFn& exePath,
                                                                 const ShippingNameFn& shippingName) const {
    for (const auto& chapter : chapters) {
        auto path = exePath(chapter);
        if (!path || path->empty()) continue;
        // Prefer the shipping renderer over a thin launcher stub that stays resident
        if (shippingName) {
            if (auto shipping = shippingName(*path)) {
                auto pids = Process::FindByName(*shipping);
                if (!pids.empty()) return Found{ pids[0], *shipping, chapter.number };
            }
        }
        const std::wstring name = Files::FileNameWithoutExtension(*path);
        auto pids = Process::FindByName(name);
        if (!pids.empty()) return Found{ pids[0], name, chapter.number };
    }
    return std::nullopt;
}

std::optional<std::wstring> CoresService::ApplyMode(CoresMode newMode, const std::vector<Models::Chapter>& chapters,
                                                    const ExePathFn& exePath, const ShippingNameFn& shippingName,
                                                    unsigned long freezePriority) {
    // Always restore, then re-apply: each press heals a previous change that silently failed
    if (mode != CoresMode::Normal && targetPid != 0) RestoreCurrentMode();

    if (newMode == CoresMode::Normal) {
        auto result = detectedProcessName;
        mode = CoresMode::Normal;
        targetPid = 0;
        return result;
    }

    auto found = FindGameProcess(chapters, exePath, shippingName);
    if (!found) {
        detectedChapter = 0;
        detectedProcessName.reset();
        mode = CoresMode::Normal;
        targetPid = 0;
        return std::nullopt;
    }
    detectedChapter = found->chapter;
    detectedProcessName = found->processName;
    targetPid = found->pid;
    savedAffinity = GetAffinity(found->pid);
    if (!savedAffinity) savedAffinity = AllCores();
    savedPriority = GetPriority(found->pid, NORMAL_PRIORITY_CLASS);

    SetAffinity(found->pid, 1);
    if (newMode == CoresMode::Freeze) SetPriority(found->pid, freezePriority);
    mode = newMode;
    return found->processName;
}

void CoresService::RestoreIfActive() {
    if (mode != CoresMode::Normal && targetPid != 0) {
        RestoreCurrentMode();
        mode = CoresMode::Normal;
        targetPid = 0;
    }
}

void CoresService::RestoreCurrentMode() {
    if (!StillRunning(targetPid)) return;
    SetAffinity(targetPid, savedAffinity ? savedAffinity : AllCores());
    if (mode == CoresMode::Freeze) SetPriority(targetPid, savedPriority);
}

} // namespace SpeedrunLauncher::Services
