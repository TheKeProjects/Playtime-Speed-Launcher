#include "Services/Fps/GameFpsService.h"
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <cstdio>
#include <locale.h>
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Services/Fps/DxgiPresentWatcher.h"
#include "Services/Fps/FrameTimeWindow.h"

namespace SpeedrunLauncher::Services {

bool GameFpsService::IsRunning() const {
    return helperProcess && WaitForSingleObject(static_cast<HANDLE>(helperProcess), 0) == WAIT_TIMEOUT;
}

void GameFpsService::Prepare(const std::vector<std::wstring>& processNames) {
    declined = false;   // a new launch: the user gets asked again
    Stop();
    if (processNames.empty()) return;
    Launch(0, processNames);
    waitingForGame = IsRunning();
}

void GameFpsService::Start(uint32_t gamePid) {
    if (declined) return;
    if (IsRunning() && (activePid == gamePid || waitingForGame)) {
        // A helper started from PLAY finds the game by itself
        activePid = gamePid;
        waitingForGame = false;
        return;
    }
    Stop();
    Launch(gamePid, {});
}

void GameFpsService::Launch(uint32_t gamePid, const std::vector<std::wstring>& processNames) {
    activePid = gamePid;

    const std::wstring pipeName = L"PlaytimeFps_" + Text::Widen(System::RandomHex(16));
    const std::wstring fullName = L"\\\\.\\pipe\\" + pipeName;
    HANDLE server = CreateNamedPipeW(fullName.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 0,
                                     4096, 0, nullptr);
    if (server == INVALID_HANDLE_VALUE) return;
    pipe = server;

    const std::wstring exe = Files::ExecutablePath();
    std::wstring args = std::wstring(HelperArg) + L" " + std::to_wstring(gamePid) + L" " + pipeName;
    for (const auto& name : processNames) args += L" \"" + name + L"\"";   // pid 0: the helper waits for one of these
    SHELLEXECUTEINFOW info{ sizeof(info) };
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas";
    info.lpFile = exe.c_str();
    info.lpParameters = args.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info) || !info.hProcess) {   // UAC declined (ERROR_CANCELLED) or failed
        if (GetLastError() == ERROR_CANCELLED) declined = true;   // don't ask again until the game closes
        Stop();
        return;
    }
    helperProcess = info.hProcess;

    alive = std::make_shared<std::atomic_bool>(true);
    auto flag = alive;
    auto onFps = callback;
    reader = std::thread([server, flag, onFps] {
        // Wait up to 15 s for the helper to connect
        OVERLAPPED ov{};
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        bool connected = ConnectNamedPipe(server, &ov) != 0 || GetLastError() == ERROR_PIPE_CONNECTED;
        if (!connected && GetLastError() == ERROR_IO_PENDING) {
            for (int i = 0; i < 150 && *flag && !connected; ++i) {
                if (WaitForSingleObject(ov.hEvent, 100) == WAIT_OBJECT_0) connected = true;
            }
            if (!connected) CancelIo(server);
        }
        std::string line;
        while (connected && *flag) {
            char buffer[256];
            DWORD read = 0;
            ResetEvent(ov.hEvent);
            BOOL ok = ReadFile(server, buffer, sizeof(buffer), &read, &ov);
            if (!ok && GetLastError() == ERROR_IO_PENDING) {
                while (*flag && WaitForSingleObject(ov.hEvent, 200) == WAIT_TIMEOUT) {}
                if (!*flag) {
                    CancelIo(server);
                    break;
                }
                ok = GetOverlappedResult(server, &ov, &read, FALSE);
            }
            if (!ok || read == 0) break;
            line.append(buffer, read);
            size_t nl;
            while ((nl = line.find('\n')) != std::string::npos) {
                const std::string value = Text::Trim(std::string_view(line).substr(0, nl));
                line.erase(0, nl + 1);
                if (auto fps = Text::ToDouble(Text::Widen(value)); fps && onFps) {
                    const double v = *fps;
                    Async::Post([flag, onFps, v] {
                        if (*flag) onFps(v);
                    });
                }
            }
        }
        CloseHandle(ov.hEvent);
    });
}

void GameFpsService::Stop() {
    if (alive) *alive = false;
    // Closing the pipe first makes the helper's next write fail, so it exits on its own and stops
    // its ETW session cleanly (killing it outright would orphan a system-wide trace session).
    if (reader.joinable()) reader.join();
    if (pipe) {
        DisconnectNamedPipe(static_cast<HANDLE>(pipe));
        CloseHandle(static_cast<HANDLE>(pipe));
        pipe = nullptr;
    }
    if (helperProcess) {
        HANDLE h = static_cast<HANDLE>(helperProcess);
        if (WaitForSingleObject(h, 500) == WAIT_TIMEOUT) TerminateProcess(h, 1);
        CloseHandle(h);
        helperProcess = nullptr;
    }
    alive.reset();
    activePid = 0;
    waitingForGame = false;
}

int GameFpsService::RunHelper(uint32_t pid, const std::wstring& pipeName, const std::vector<std::wstring>& processNames) {
    if (!System::IsElevated()) return 2;
    const std::wstring fullName = L"\\\\.\\pipe\\" + pipeName;
    HANDLE pipe = INVALID_HANDLE_VALUE;
    const ULONGLONG deadline = GetTickCount64() + 3000;
    while (GetTickCount64() < deadline) {
        pipe = CreateFileW(fullName.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (pipe != INVALID_HANDLE_VALUE) break;
        WaitNamedPipeW(fullName.c_str(), 200);
    }
    if (pipe == INVALID_HANDLE_VALUE) return 3;
    const auto write = [pipe](const char* text) {
        DWORD written = 0;
        return WriteFile(pipe, text, static_cast<DWORD>(strlen(text)), &written, nullptr) != FALSE;
    };

    // Started from PLAY (pid 0): wait up to 3 minutes for the game, preferring the first name
    // (the *-Win64-Shipping renderer). Empty lines keep checking that the launcher is still there.
    if (pid == 0) {
        const ULONGLONG started = GetTickCount64();
        const ULONGLONG giveUp = started + 180000;
        ULONGLONG nextPing = 0;
        while (pid == 0 && GetTickCount64() < giveUp) {
            const auto processes = Process::List();
            // The launcher stub starts first and never renders: only settle for a later name when
            // the first one (the shipping renderer) hasn't shown up within 30 s
            const size_t candidates = GetTickCount64() - started < 30000 ? std::min<size_t>(1, processNames.size()) : processNames.size();
            for (size_t i = 0; i < candidates; ++i) {
                const auto found = Process::FindByName(processes, processNames[i]);
                if (!found.empty()) {
                    pid = found[0];
                    break;
                }
            }
            if (pid) break;
            if (GetTickCount64() >= nextPing) {
                if (!write("\n")) break;
                nextPing = GetTickCount64() + 1000;
            }
            Sleep(250);
        }
        if (pid == 0) {
            CloseHandle(pipe);
            return 4;
        }
    }

    FrameTimeWindow window(800.0);   // ~1 s of frames: precise, still responsive
    DxgiPresentWatcher watcher(pid, [&window](double ms) { window.AddFrame(ms); });
    watcher.Start();
    // Anti-cheat may refuse a handle even to an elevated process: then check the process table
    HANDLE game = OpenProcess(SYNCHRONIZE, FALSE, pid);
    ULONGLONG nextListCheck = 0;
    const auto gameAlive = [&] {
        if (game) return WaitForSingleObject(game, 0) == WAIT_TIMEOUT;
        if (GetTickCount64() < nextListCheck) return true;
        nextListCheck = GetTickCount64() + 1000;
        for (const auto& p : Process::List())
            if (p.pid == pid) return true;
        return false;
    };
    while (true) {
        Sleep(100);
        if (!gameAlive()) break;
        char text[32];
        _locale_t c = _create_locale(LC_NUMERIC, "C");
        _sprintf_s_l(text, sizeof(text), "%.1f\n", c, window.CurrentFps());
        _free_locale(c);
        if (!write(text)) break;
    }
    if (game) CloseHandle(game);
    watcher.Stop();
    CloseHandle(pipe);
    return 0;
}

} // namespace SpeedrunLauncher::Services
