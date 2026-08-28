using System.Runtime.InteropServices;

namespace SpeedrunLauncher.Services.Hotkeys;

public enum F11RemapInputType { None, Keyboard, Mouse }

/// <summary>A single input (keyboard key or mouse button) that triggers a synthetic F11.</summary>
public readonly record struct F11RemapBinding(F11RemapInputType Type, uint KeyVk, int MouseButton);

/// <summary>
/// While enabled, blocks the real F11 key system-wide and lets one or more
/// configured keys / mouse side-buttons (e.g. Mouse 4/5, "0", "9") act as F11
/// instead. Active for as long as the launcher process is running, regardless
/// of which window has focus.
/// </summary>
public sealed class F11RemapService : IDisposable
{
    // Mouse button identifiers used for persistence/UI.
    public const int MouseXButton1 = 1; // "Mouse 4"
    public const int MouseXButton2 = 2; // "Mouse 5"
    public const int MouseMiddle   = 3; // "Mouse 3"

    private const int WH_KEYBOARD_LL = 13;
    private const int WH_MOUSE_LL    = 14;

    private const int WM_KEYDOWN     = 0x0100;
    private const int WM_KEYUP       = 0x0101;
    private const int WM_SYSKEYDOWN  = 0x0104;
    private const int WM_SYSKEYUP    = 0x0105;
    private const int WM_XBUTTONDOWN = 0x020B;
    private const int WM_MBUTTONDOWN = 0x0207;

    private const uint LLKHF_INJECTED = 0x10;
    private const uint VK_F11         = 0x7A;

    private const int XBUTTON1 = 0x0001;
    private const int XBUTTON2 = 0x0002;

    private const uint INPUT_KEYBOARD  = 1;
    private const uint KEYEVENTF_KEYUP = 0x0002;

    private static readonly string ConfigFile =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "SpeedrunLauncher", "f11remap.cfg");

    public bool Enabled { get; private set; }

    /// <summary>All inputs configured to trigger a synthetic F11.</summary>
    public List<F11RemapBinding> Bindings { get; } = [];

    private nint _keyboardHook;
    private nint _mouseHook;
    private LowLevelProc? _keyboardProc;
    private LowLevelProc? _mouseProc;
    private readonly HashSet<uint> _heldKeys = [];

    public void Load()
    {
        try
        {
            if (!File.Exists(ConfigFile)) return;
            var parts = File.ReadAllText(ConfigFile).Trim().Split(',');
            if (parts.Length == 0) return;

            Enabled = parts[0] == "1";
            Bindings.Clear();

            // Legacy single-binding format: "enabled,K|M|N,value"
            if (parts.Length == 3 && parts[1].Length == 1)
            {
                if (parts[1] == "K" && uint.TryParse(parts[2], out var legacyVk))
                    Bindings.Add(new F11RemapBinding(F11RemapInputType.Keyboard, legacyVk, 0));
                else if (parts[1] == "M" && int.TryParse(parts[2], out var legacyMb))
                    Bindings.Add(new F11RemapBinding(F11RemapInputType.Mouse, 0, legacyMb));
                return;
            }

            // Current format: "enabled,K<vk>,M<button>,..."
            for (var i = 1; i < parts.Length; i++)
            {
                var p = parts[i];
                if (p.Length < 2) continue;

                if (p[0] == 'K' && uint.TryParse(p[1..], out var vk))
                    Bindings.Add(new F11RemapBinding(F11RemapInputType.Keyboard, vk, 0));
                else if (p[0] == 'M' && int.TryParse(p[1..], out var mb))
                    Bindings.Add(new F11RemapBinding(F11RemapInputType.Mouse, 0, mb));
            }
        }
        catch { }
    }

    private void Save()
    {
        try
        {
            var dir = Path.GetDirectoryName(ConfigFile)!;
            Directory.CreateDirectory(dir);

            var tokens = new List<string> { Enabled ? "1" : "0" };
            foreach (var b in Bindings)
            {
                if (b.Type == F11RemapInputType.Keyboard)
                    tokens.Add($"K{b.KeyVk}");
                else if (b.Type == F11RemapInputType.Mouse)
                    tokens.Add($"M{b.MouseButton}");
            }

            File.WriteAllText(ConfigFile, string.Join(',', tokens));
        }
        catch { }
    }

    public void SetEnabled(bool enabled)
    {
        Enabled = enabled;
        Save();
        Refresh();
    }

    /// <summary>Adds a new trigger binding, ignoring exact duplicates.</summary>
    public void AddBinding(F11RemapInputType type, uint keyVk = 0, int mouseButton = 0)
    {
        var binding = new F11RemapBinding(type, keyVk, mouseButton);
        if (Bindings.Contains(binding)) return;

        Bindings.Add(binding);
        Save();
        Refresh();
    }

    /// <summary>Replaces the binding at the given index (used when re-capturing a slot).</summary>
    public void ReplaceBinding(int index, F11RemapInputType type, uint keyVk = 0, int mouseButton = 0)
    {
        if (index < 0 || index >= Bindings.Count) return;

        Bindings[index] = new F11RemapBinding(type, keyVk, mouseButton);
        Save();
        Refresh();
    }

    public void RemoveBinding(int index)
    {
        if (index < 0 || index >= Bindings.Count) return;

        Bindings.RemoveAt(index);
        Save();
        Refresh();
    }

    /// <summary>Installs/removes the low-level hooks to match the current settings.</summary>
    public void Refresh()
    {
        Uninstall();
        if (!Enabled || Bindings.Count == 0) return;

        try
        {
            using var curProcess = System.Diagnostics.Process.GetCurrentProcess();
            using var curModule  = curProcess.MainModule!;
            var hMod = GetModuleHandle(curModule.ModuleName);

            _keyboardProc = KeyboardHookProc;
            _keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, _keyboardProc, hMod, 0);

            if (Bindings.Any(b => b.Type == F11RemapInputType.Mouse))
            {
                _mouseProc = MouseHookProc;
                _mouseHook = SetWindowsHookEx(WH_MOUSE_LL, _mouseProc, hMod, 0);
            }
        }
        catch { }
    }

    private void Uninstall()
    {
        if (_keyboardHook != 0) { UnhookWindowsHookEx(_keyboardHook); _keyboardHook = 0; }
        if (_mouseHook    != 0) { UnhookWindowsHookEx(_mouseHook);    _mouseHook    = 0; }
        _keyboardProc = null;
        _mouseProc    = null;
        _heldKeys.Clear();
    }

    private nint KeyboardHookProc(int nCode, nint wParam, nint lParam)
    {
        if (nCode >= 0)
        {
            var data     = Marshal.PtrToStructure<KBDLLHOOKSTRUCT>(lParam);
            var msg      = (int)wParam;
            var isDown   = msg is WM_KEYDOWN or WM_SYSKEYDOWN;
            var isUp     = msg is WM_KEYUP   or WM_SYSKEYUP;
            var injected = (data.flags & LLKHF_INJECTED) != 0;

            // Swallow the real F11 key so it never reaches the foreground app —
            // but let our own SendF11() (which is reported as "injected") through.
            if (data.vkCode == VK_F11 && !injected)
                return 1;

            // Any configured trigger key sends a synthetic F11 (debounced on hold).
            if (!injected && Bindings.Any(b => b.Type == F11RemapInputType.Keyboard && b.KeyVk == data.vkCode))
            {
                if (isDown && _heldKeys.Add(data.vkCode)) SendF11();
                else if (isUp) _heldKeys.Remove(data.vkCode);
            }
        }
        return CallNextHookEx(_keyboardHook, nCode, wParam, lParam);
    }

    private nint MouseHookProc(int nCode, nint wParam, nint lParam)
    {
        if (nCode >= 0 && (wParam == WM_XBUTTONDOWN || wParam == WM_MBUTTONDOWN))
        {
            var data    = Marshal.PtrToStructure<MSLLHOOKSTRUCT>(lParam);
            var xButton = (int)(data.mouseData >> 16);
            var pressed = (int)wParam switch
            {
                WM_MBUTTONDOWN => MouseMiddle,
                WM_XBUTTONDOWN => xButton == XBUTTON1 ? MouseXButton1
                                : xButton == XBUTTON2 ? MouseXButton2 : 0,
                _ => 0,
            };

            if (pressed != 0 && Bindings.Any(b => b.Type == F11RemapInputType.Mouse && b.MouseButton == pressed))
                SendF11();
        }
        return CallNextHookEx(_mouseHook, nCode, wParam, lParam);
    }

    private static void SendF11()
    {
        var inputs = new INPUT[2];
        inputs[0].type      = INPUT_KEYBOARD;
        inputs[0].u.ki.wVk  = (ushort)VK_F11;
        inputs[1].type      = INPUT_KEYBOARD;
        inputs[1].u.ki.wVk  = (ushort)VK_F11;
        inputs[1].u.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, inputs, Marshal.SizeOf<INPUT>());
    }

    public void Dispose() => Uninstall();

    // ── Win32 interop ────────────────────────────────────────────────────────

    private delegate nint LowLevelProc(int nCode, nint wParam, nint lParam);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern nint SetWindowsHookEx(int idHook, LowLevelProc lpfn, nint hMod, uint dwThreadId);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool UnhookWindowsHookEx(nint hhk);

    [DllImport("user32.dll")]
    private static extern nint CallNextHookEx(nint hhk, int nCode, nint wParam, nint lParam);

    [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    private static extern nint GetModuleHandle(string? lpModuleName);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);

    [StructLayout(LayoutKind.Sequential)]
    private struct KBDLLHOOKSTRUCT
    {
        public uint vkCode;
        public uint scanCode;
        public uint flags;
        public uint time;
        public nint dwExtraInfo;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct MSLLHOOKSTRUCT
    {
        public int  ptX;
        public int  ptY;
        public uint mouseData;
        public uint flags;
        public uint time;
        public nint dwExtraInfo;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct INPUT
    {
        public uint type;
        public InputUnion u;
    }

    [StructLayout(LayoutKind.Explicit)]
    private struct InputUnion
    {
        [FieldOffset(0)] public KEYBDINPUT ki;
        [FieldOffset(0)] public MOUSEINPUT mi;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct KEYBDINPUT
    {
        public ushort wVk;
        public ushort wScan;
        public uint   dwFlags;
        public uint   time;
        public nint   dwExtraInfo;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct MOUSEINPUT
    {
        public int  dx;
        public int  dy;
        public uint mouseData;
        public uint dwFlags;
        public uint time;
        public nint dwExtraInfo;
    }
}
