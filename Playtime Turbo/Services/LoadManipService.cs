using System.Diagnostics;
using System.Runtime.InteropServices;
using SpeedrunLauncher.Models;
using IOPath = System.IO.Path;

namespace SpeedrunLauncher.Services;

public enum LoadManipMode { Normal, Slower, Freeze }

public sealed class LoadManipService
{
    private LoadManipMode _currentMode = LoadManipMode.Normal;
    private int _targetPid;
    private nint _savedAffinity;
    private ProcessPriorityClass _savedPriority = ProcessPriorityClass.Normal;

    public LoadManipMode CurrentMode => _currentMode;
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

    public string? ApplyMode(LoadManipMode mode, IReadOnlyList<ChapterInfo> chapters)
    {
        if (mode == _currentMode && _targetPid != 0)
            return DetectedProcessName;

        if (_currentMode != LoadManipMode.Normal && _targetPid != 0)
            RestoreCurrentMode();

        if (mode == LoadManipMode.Normal)
        {
            var result = DetectedProcessName;
            _currentMode = LoadManipMode.Normal;
            _targetPid = 0;
            return result;
        }

        var (proc, chapter) = FindGameProcess(chapters);
        if (proc == null)
        {
            DetectedChapter = 0;
            DetectedProcessName = null;
            _currentMode = LoadManipMode.Normal;
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
            try { _savedPriority = proc.PriorityClass; }
            catch { _savedPriority = ProcessPriorityClass.Normal; }

            switch (mode)
            {
                case LoadManipMode.Slower:
                    SetSingleCoreAffinity(proc);
                    break;
                case LoadManipMode.Freeze:
                    SetSingleCoreAffinity(proc);
                    try { proc.PriorityClass = ProcessPriorityClass.High; } catch { }
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
        if (_currentMode != LoadManipMode.Normal && _targetPid != 0)
        {
            RestoreCurrentMode();
            _currentMode = LoadManipMode.Normal;
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
                case LoadManipMode.Freeze:
                    RestoreAffinity(proc);
                    try { proc.PriorityClass = _savedPriority; } catch { }
                    break;
                case LoadManipMode.Slower:
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
    private static readonly nint INVALID_HANDLE = -1;

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
