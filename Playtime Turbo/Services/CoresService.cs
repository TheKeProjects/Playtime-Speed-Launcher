using System.Diagnostics;
using System.Runtime.InteropServices;
using SpeedrunLauncher.Models;
using IOPath = System.IO.Path;

namespace SpeedrunLauncher.Services;

public enum CoresMode { Normal, Slower, Freeze }

public sealed class CoresService
{
    private CoresMode _currentMode = CoresMode.Normal;
    private int _targetPid;
    private nint _savedAffinity;
    private ProcessPriorityClass _savedPriority = ProcessPriorityClass.Normal;

    public CoresMode CurrentMode => _currentMode;
    public int DetectedChapter { get; private set; }
    public string? DetectedProcessName { get; private set; }

    public (Process? process, int chapter) FindGameProcess(IReadOnlyList<ChapterInfo> chapters)
    {
        for (int i = 0; i < chapters.Count; i++)
        {
            var path = chapters[i].GameExePath;
            if (string.IsNullOrEmpty(path)) continue;

            var name = IOPath.GetFileNameWithoutExtension(path);
            try
            {
                var procs = Process.GetProcessesByName(name);
                if (procs.Length > 0)
                {
                    for (int j = 1; j < procs.Length; j++) procs[j].Dispose();
                    return (procs[0], chapters[i].Number);
                }
            }
            catch { }
        }
        return (null, 0);
    }

    public string? ApplyMode(CoresMode mode, IReadOnlyList<ChapterInfo> chapters)
    {
        // Always restore-then-reapply, even if `mode` already matches `_currentMode` — this makes
        // every hotkey press self-healing if a previous affinity/priority change silently failed
        // (or got reset by the game/OS) instead of trusting possibly-stale bookkeeping.
        if (_currentMode != CoresMode.Normal && _targetPid != 0)
            RestoreCurrentMode();

        if (mode == CoresMode.Normal)
        {
            var result = DetectedProcessName;
            _currentMode = CoresMode.Normal;
            _targetPid = 0;
            return result;
        }

        var (proc, chapter) = FindGameProcess(chapters);
        if (proc == null)
        {
            DetectedChapter = 0;
            DetectedProcessName = null;
            _currentMode = CoresMode.Normal;
            _targetPid = 0;
            return null;
        }

        try
        {
            DetectedChapter = chapter;
            DetectedProcessName = proc.ProcessName;
            _targetPid = proc.Id;

            try { _savedAffinity = proc.ProcessorAffinity; }
            catch { _savedAffinity = (nint)((1L << Environment.ProcessorCount) - 1); }
            _savedPriority = GetProcessPriority(proc.Id, ProcessPriorityClass.Normal);

            switch (mode)
            {
                case CoresMode.Slower:
                    SetSingleCoreAffinity(proc);
                    break;
                case CoresMode.Freeze:
                    SetSingleCoreAffinity(proc);
                    SetProcessPriority(proc.Id, ProcessPriorityClass.High);
                    break;
            }

            _currentMode = mode;
            return proc.ProcessName;
        }
        finally
        {
            proc.Dispose();
        }
    }

    public void RestoreIfActive()
    {
        if (_currentMode != CoresMode.Normal && _targetPid != 0)
        {
            RestoreCurrentMode();
            _currentMode = CoresMode.Normal;
            _targetPid = 0;
        }
    }

    private void RestoreCurrentMode()
    {
        try
        {
            using var proc = Process.GetProcessById(_targetPid);
            switch (_currentMode)
            {
                case CoresMode.Freeze:
                    RestoreAffinity(proc);
                    SetProcessPriority(proc.Id, _savedPriority);
                    break;
                case CoresMode.Slower:
                    RestoreAffinity(proc);
                    break;
            }
        }
        catch { }
    }

    private static void SetSingleCoreAffinity(Process proc)
    {
        try { proc.ProcessorAffinity = 1; }
        catch { }
    }

    private void RestoreAffinity(Process proc)
    {
        try
        {
            proc.ProcessorAffinity = _savedAffinity != 0
                ? _savedAffinity
                : (nint)((1L << Environment.ProcessorCount) - 1);
        }
        catch { }
    }

    // Process.PriorityClass opens its handle with PROCESS_QUERY_INFORMATION | PROCESS_SET_INFORMATION
    // combined. Some games' anti-cheat/DRM denies that combined request outright while still allowing
    // affinity changes (which .NET requests the same way, but apparently isn't filtered the same way) —
    // going through a narrower, single-purpose handle here gets past that for the affected chapters.
    //
    // The very first SetPriorityClass call right after changing affinity can still get silently
    // dropped (previously needed a second hotkey press to "take") — retry a few times within the
    // same call so one press is enough.
    private static void SetProcessPriority(int pid, ProcessPriorityClass priority)
    {
        for (int attempt = 0; attempt < 5; attempt++)
        {
            var handle = OpenProcess(PROCESS_SET_INFORMATION, false, pid);
            if (handle == 0) return;
            bool ok;
            try { ok = SetPriorityClass(handle, (uint)priority); }
            finally { CloseHandle(handle); }

            if (ok && GetProcessPriority(pid, priority) == priority) return;
            System.Threading.Thread.Sleep(20);
        }
    }

    private static ProcessPriorityClass GetProcessPriority(int pid, ProcessPriorityClass fallback)
    {
        var handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, pid);
        if (handle == 0) return fallback;
        try
        {
            var value = GetPriorityClass(handle);
            return value != 0 ? (ProcessPriorityClass)value : fallback;
        }
        finally { CloseHandle(handle); }
    }

    private static void SuspendAllThreads(int pid)
    {
        var snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE) return;

        try
        {
            var entry = new THREADENTRY32 { dwSize = (uint)Marshal.SizeOf<THREADENTRY32>() };
            if (!Thread32First(snapshot, ref entry)) return;

            do
            {
                if (entry.th32OwnerProcessID != (uint)pid) continue;
                var hThread = OpenThread(THREAD_SUSPEND_RESUME, false, entry.th32ThreadID);
                if (hThread == 0) continue;
                SuspendThread(hThread);
                CloseHandle(hThread);
            } while (Thread32Next(snapshot, ref entry));
        }
        finally { CloseHandle(snapshot); }
    }

    private static void ResumeAllThreads(int pid)
    {
        var snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE) return;

        try
        {
            var entry = new THREADENTRY32 { dwSize = (uint)Marshal.SizeOf<THREADENTRY32>() };
            if (!Thread32First(snapshot, ref entry)) return;

            do
            {
                if (entry.th32OwnerProcessID != (uint)pid) continue;
                var hThread = OpenThread(THREAD_SUSPEND_RESUME, false, entry.th32ThreadID);
                if (hThread == 0) continue;
                ResumeThread(hThread);
                CloseHandle(hThread);
            } while (Thread32Next(snapshot, ref entry));
        }
        finally { CloseHandle(snapshot); }
    }

    // ── Win32 ─────────────────────────────────────────────────────────────────

    private const uint TH32CS_SNAPTHREAD     = 0x00000004;
    private const uint THREAD_SUSPEND_RESUME = 0x0002;
    private const uint PROCESS_SET_INFORMATION           = 0x0200;
    private const uint PROCESS_QUERY_LIMITED_INFORMATION = 0x1000;
    private static readonly nint INVALID_HANDLE = -1;

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern nint OpenProcess(uint dwDesiredAccess, bool bInheritHandle, int dwProcessId);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool SetPriorityClass(nint hProcess, uint dwPriorityClass);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern uint GetPriorityClass(nint hProcess);

    [DllImport("kernel32.dll")]
    private static extern nint CreateToolhelp32Snapshot(uint dwFlags, uint th32ProcessID);

    [DllImport("kernel32.dll")]
    private static extern bool Thread32First(nint hSnapshot, ref THREADENTRY32 lpte);

    [DllImport("kernel32.dll")]
    private static extern bool Thread32Next(nint hSnapshot, ref THREADENTRY32 lpte);

    [DllImport("kernel32.dll")]
    private static extern nint OpenThread(uint dwDesiredAccess, bool bInheritHandle, uint dwThreadId);

    [DllImport("kernel32.dll")]
    private static extern uint SuspendThread(nint hThread);

    [DllImport("kernel32.dll")]
    private static extern uint ResumeThread(nint hThread);

    [DllImport("kernel32.dll")]
    private static extern bool CloseHandle(nint hObject);

    [StructLayout(LayoutKind.Sequential)]
    private struct THREADENTRY32
    {
        public uint dwSize;
        public uint cntUsage;
        public uint th32ThreadID;
        public uint th32OwnerProcessID;
        public int  tpBasePri;
        public int  tpDeltaPri;
        public uint dwFlags;
    }
}
