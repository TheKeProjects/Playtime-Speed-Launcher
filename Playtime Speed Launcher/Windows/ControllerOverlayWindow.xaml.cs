using System.IO;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using SpeedrunLauncher.Services.App;
using Windows.Gaming.Input;

namespace SpeedrunLauncher;

public partial class ControllerOverlayWindow : Window
{
    // ── Element types ─────────────────────────────────────────────────────────
    private const int TYPE_BODY     = 0;
    private const int TYPE_KEY      = 1;  // keyboard key (same rendering as TYPE_BUTTON)
    private const int TYPE_BUTTON   = 2;
    private const int TYPE_MOUSEBTN = 3;  // mouse button (same rendering as TYPE_BUTTON)
    private const int TYPE_WHEEL    = 4;  // mouse wheel: 4-state horizontal strip
    private const int TYPE_STICK    = 5;
    private const int TYPE_TRIGGER  = 6;
    private const int TYPE_GUIDE    = 7;
    private const int TYPE_DPAD     = 8;

    // ── Win32 interop (keyboard + mouse polling, mouse-wheel hook) ─────────────
    [DllImport("user32.dll")]
    private static extern short GetAsyncKeyState(int vKey);

    [DllImport("user32.dll")]
    private static extern uint MapVirtualKey(uint uCode, uint uMapType);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern IntPtr SetWindowsHookEx(int idHook, LowLevelMouseProc lpfn, IntPtr hMod, uint dwThreadId);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool UnhookWindowsHookEx(IntPtr hhk);

    [DllImport("user32.dll")]
    private static extern IntPtr CallNextHookEx(IntPtr hhk, int nCode, IntPtr wParam, IntPtr lParam);

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr GetModuleHandle(string? lpModuleName);

    private delegate IntPtr LowLevelMouseProc(int nCode, IntPtr wParam, IntPtr lParam);

    [StructLayout(LayoutKind.Sequential)]
    private struct POINT { public int X; public int Y; }

    [StructLayout(LayoutKind.Sequential)]
    private struct MSLLHOOKSTRUCT
    {
        public POINT pt;
        public uint  mouseData;
        public uint  flags;
        public uint  time;
        public IntPtr dwExtraInfo;
    }

    private const int   WH_MOUSE_LL    = 14;
    private const int   WM_MOUSEWHEEL  = 0x020A;
    private const uint  MAPVK_VSC_TO_VK = 1;
    private const int   VK_LBUTTON  = 0x01;
    private const int   VK_RBUTTON  = 0x02;
    private const int   VK_MBUTTON  = 0x04;
    private const int   VK_XBUTTON1 = 0x05;
    private const int   VK_XBUTTON2 = 0x06;

    // Each button's active (cyan) sprite is immediately below its inactive (white) sprite
    // in the sheet: active_y = mapping_y + mapping_h (sprite height is the row step).

    // DPad sprite strip at y=1013, verified against the sheet pixels:
    //   [0]=inactive, [1]=Left, [2]=Right, [3]=Up, [4]=Down,
    //   [5]=UpLeft,   [6]=UpRight, [7]=DownLeft, [8]=DownRight
    // GameControllerSwitchPosition enum: Center=0, Up=1, UpRight=2, Right=3, DownRight=4,
    //                                    Down=5, DownLeft=6, Left=7, UpLeft=8
    // This table maps WGI enum value → correct sprite strip index.
    private static readonly int[] DpadSpriteIndex = { 0, 3, 6, 2, 8, 4, 7, 1, 5 };

    private record OverlayElement(
        string Id,
        int    Type,
        int[]  Mapping,
        int[]  Pos,
        int    Code,
        int    Side,
        int    StickRadius,
        int    Direction
    );

    // ── Sony VendorId + HID button → SDL2 OBS code map ───────────────────────
    private const ushort VendorSony = 0x054C;

    private static bool IsSonyController(RawGameController r) =>
        r.HardwareVendorId == VendorSony;

    // DualSense HID button report order:
    //   [0]=Square, [1]=Cross, [2]=Circle, [3]=Triangle,
    //   [4]=L1, [5]=R1, [6]=L2btn, [7]=R2btn,
    //   [8]=Create, [9]=Options, [10]=L3, [11]=R3
    private static readonly (int raw, int code)[] PsButtonMap =
    {
        (0,  2),  // Square   → code 2
        (1,  0),  // Cross    → code 0
        (2,  1),  // Circle   → code 1
        (3,  3),  // Triangle → code 3
        (4,  9),  // L1
        (5,  10), // R1
        (8,  4),  // Create / Share
        (9,  6),  // Options
        (10, 7),  // L3
        (11, 8),  // R3
    };

    // ── State ─────────────────────────────────────────────────────────────────
    private BitmapSource?              _sheet;
    private List<OverlayElement>       _elements = [];
    private string                     _controllerFolder = "";

    // Active button/trigger images (hidden when not pressed)
    private readonly Dictionary<string, Image> _activeImgs = [];
    // DPad image (single image, Source is swapped per hat-switch state)
    private readonly Dictionary<string, Image>              _dpadImg   = [];
    // Pre-computed DPad crops: 9 states (Center=0, Up=1 … UpLeft=8)
    private readonly Dictionary<string, CroppedBitmap[]>   _dpadCrops = [];
    // Mouse wheel image (single image, Source is swapped per wheel state)
    private readonly Dictionary<string, Image>              _wheelImg   = [];
    // Pre-computed wheel crops: 4 states (0=neutral, 1=middle-click, 2=scroll-up, 3=scroll-down)
    private readonly Dictionary<string, CroppedBitmap[]>   _wheelCrops = [];
    // Keyboard scancode (el.Code) → virtual-key, cached to avoid per-frame MapVirtualKey calls
    private readonly Dictionary<int, uint> _keyVk = [];

    private readonly DispatcherTimer _poll = new() { Interval = TimeSpan.FromMilliseconds(16) };

    // Reusable arrays for RawGameController to avoid per-frame allocation
    private bool[]                         _rawButtons  = [];
    private GameControllerSwitchPosition[] _rawSwitches = [];
    private double[]                       _rawAxes     = [];

    // Low-level mouse hook (only installed for the keyboard+mouse skin — needed because
    // wheel-scroll has no GetAsyncKeyState equivalent to poll)
    private IntPtr             _mouseHookHandle;
    private LowLevelMouseProc? _mouseHookProc;
    private long _wheelFlashUntil;
    private int  _wheelFlashIndex; // 2=scroll-up, 3=scroll-down

    // ── Construction ──────────────────────────────────────────────────────────

    public ControllerOverlayWindow(string controllerFolderName)
    {
        InitializeComponent();
        _controllerFolder = controllerFolderName;
        LoadController(controllerFolderName);
        if (_controllerFolder == "keyboard")
            InstallMouseWheelHook();
        _poll.Tick += Poll;
        _poll.Start();
    }

    // ── Controller loading ────────────────────────────────────────────────────

    private void LoadController(string folderName)
    {
        if (folderName == "keyboard")
        {
            LoadKeyboardMouseController();
            return;
        }

        var controllerDir = System.IO.Path.Combine(
            ResourceExtractor.TempDir, "Assets", "Controllers", folderName);

        if (!Directory.Exists(controllerDir)) return;

        var jsonFile = Directory.GetFiles(controllerDir, "*.json").FirstOrDefault();
        var pngFile  = Directory.GetFiles(controllerDir, "*.png")
                                .FirstOrDefault(f =>
                                {
                                    var n = System.IO.Path.GetFileNameWithoutExtension(f).ToLower();
                                    return !n.EndsWith("black") && !n.EndsWith("white")
                                        && !n.EndsWith("clear")  && !n.EndsWith("show-player");
                                });

        if (jsonFile == null || pngFile == null) return;

        _sheet = LoadBitmap(pngFile);

        using var doc = JsonDocument.Parse(File.ReadAllText(jsonFile));
        var root = doc.RootElement;
        int ow   = root.GetProperty("overlay_width").GetInt32();
        int oh   = root.GetProperty("overlay_height").GetInt32();

        var parsed = new List<(int z, OverlayElement el)>();
        foreach (var el in root.GetProperty("elements").EnumerateArray())
            parsed.Add(ParseElement(el));

        _elements = parsed.OrderBy(e => e.z).Select(e => e.el).ToList();

        FinishLoad(ow, oh);
    }

    private static (int z, OverlayElement el) ParseElement(
        JsonElement el, int sheetXOffset = 0, int canvasXOffset = 0, int canvasYOffset = 0)
    {
        int    type      = el.GetProperty("type").GetInt32();
        string id        = el.GetProperty("id").GetString() ?? "";
        int[]  mapping   = el.GetProperty("mapping").EnumerateArray().Select(v => v.GetInt32()).ToArray();
        int[]  pos       = el.GetProperty("pos").EnumerateArray().Select(v => v.GetInt32()).ToArray();
        int    code      = el.TryGetProperty("code",         out var cv) ? cv.GetInt32() : 0;
        int    side      = el.TryGetProperty("side",         out var sv) ? sv.GetInt32() : 0;
        int    radius    = el.TryGetProperty("stick_radius", out var rv) ? rv.GetInt32() : 0;
        int    direction = el.TryGetProperty("direction",    out var dv) ? dv.GetInt32() : 1;

        mapping[0] += sheetXOffset;
        pos[0]     += canvasXOffset;
        pos[1]     += canvasYOffset;

        int z = 0;
        if (el.TryGetProperty("z_level", out var zp))
        {
            if (zp.ValueKind == JsonValueKind.Number) z = zp.GetInt32();
            else int.TryParse(zp.GetString(), out z);
        }

        return (z, new OverlayElement(id, type, mapping, pos, code, side, radius, direction));
    }

    private static BitmapSource LoadBitmap(string path)
    {
        var bmp = new BitmapImage();
        bmp.BeginInit();
        bmp.UriSource     = new Uri(path);
        bmp.CacheOption   = BitmapCacheOption.OnLoad;
        bmp.CreateOptions = BitmapCreateOptions.IgnoreImageCache;
        bmp.EndInit();
        bmp.Freeze();
        return bmp;
    }

    // Combines the keyboard (left) and mouse (right) skins — two separate JSON+PNG asset
    // pairs — into a single sprite sheet and element list so the rest of the rendering
    // pipeline (which assumes one sheet) doesn't need to change.
    private const int KeyboardMouseGap = 30;

    private void LoadKeyboardMouseController()
    {
        var baseDir       = System.IO.Path.Combine(
            ResourceExtractor.TempDir, "Assets", "Controllers", "keyboard");
        var kbJsonPath    = System.IO.Path.Combine(baseDir, "wasd", "wasd-full.json");
        var kbPngPath     = System.IO.Path.Combine(baseDir, "wasd", "wasd.png");
        var mouseJsonPath = System.IO.Path.Combine(baseDir, "mouse", "mouse-no-movement.json");
        var mousePngPath  = System.IO.Path.Combine(baseDir, "mouse", "mouse.png");

        if (!File.Exists(kbJsonPath) || !File.Exists(kbPngPath) ||
            !File.Exists(mouseJsonPath) || !File.Exists(mousePngPath)) return;

        var kbBmp    = LoadBitmap(kbPngPath);
        var mouseBmp = LoadBitmap(mousePngPath);

        int combinedSheetW = kbBmp.PixelWidth + mouseBmp.PixelWidth;
        int combinedSheetH = Math.Max(kbBmp.PixelHeight, mouseBmp.PixelHeight);

        var dv = new DrawingVisual();
        using (var dc = dv.RenderOpen())
        {
            dc.DrawImage(kbBmp,    new Rect(0,                  0, kbBmp.PixelWidth,    kbBmp.PixelHeight));
            dc.DrawImage(mouseBmp, new Rect(kbBmp.PixelWidth,    0, mouseBmp.PixelWidth, mouseBmp.PixelHeight));
        }
        var rtb = new RenderTargetBitmap(combinedSheetW, combinedSheetH, 96, 96, PixelFormats.Pbgra32);
        rtb.Render(dv);
        rtb.Freeze();
        _sheet = rtb;

        using var kbDoc    = JsonDocument.Parse(File.ReadAllText(kbJsonPath));
        using var mouseDoc = JsonDocument.Parse(File.ReadAllText(mouseJsonPath));
        var kbRoot    = kbDoc.RootElement;
        var mouseRoot = mouseDoc.RootElement;

        int kbOw    = kbRoot.GetProperty("overlay_width").GetInt32();
        int kbOh    = kbRoot.GetProperty("overlay_height").GetInt32();
        int mouseOw = mouseRoot.GetProperty("overlay_width").GetInt32();
        int mouseOh = mouseRoot.GetProperty("overlay_height").GetInt32();

        int ow = kbOw + KeyboardMouseGap + mouseOw;
        int oh = Math.Max(kbOh, mouseOh);

        var parsed = new List<(int z, OverlayElement el)>();
        foreach (var el in kbRoot.GetProperty("elements").EnumerateArray())
            parsed.Add(ParseElement(el, 0, 0, (oh - kbOh) / 2));
        foreach (var el in mouseRoot.GetProperty("elements").EnumerateArray())
            parsed.Add(ParseElement(el, kbBmp.PixelWidth, kbOw + KeyboardMouseGap, (oh - mouseOh) / 2));

        _elements = parsed.OrderBy(e => e.z).Select(e => e.el).ToList();

        FinishLoad(ow, oh);
    }

    private void FinishLoad(int ow, int oh)
    {
        OverlayCanvas.Width  = ow;
        OverlayCanvas.Height = oh;
        BuildCanvas();

        double ratio = (double)ow / oh;
        Width  = 360;
        Height = 360 / ratio;
    }

    private CroppedBitmap Crop(int x, int y, int w, int h) =>
        new(_sheet!, new Int32Rect(x, y, w, h));

    private Image MakeImage(int mx, int my, int mw, int mh, int px, int py)
    {
        var img = new Image
        {
            Width   = mw,
            Height  = mh,
            Source  = Crop(mx, my, mw, mh),
            Stretch = Stretch.Fill,
        };
        RenderOptions.SetBitmapScalingMode(img, BitmapScalingMode.HighQuality);
        Canvas.SetLeft(img, px);
        Canvas.SetTop(img,  py);
        return img;
    }

    private void BuildCanvas()
    {
        OverlayCanvas.Children.Clear();
        _activeImgs.Clear();
        _dpadImg.Clear();
        _dpadCrops.Clear();
        _wheelImg.Clear();
        _wheelCrops.Clear();
        _keyVk.Clear();

        foreach (var el in _elements)
        {
            if (el.Type == TYPE_KEY && !_keyVk.ContainsKey(el.Code))
                _keyVk[el.Code] = MapVirtualKey((uint)el.Code, MAPVK_VSC_TO_VK);

            switch (el.Type)
            {
                case TYPE_WHEEL:
                {
                    // Horizontal strip of 4 states: 0=neutral, 1=middle-click, 2=scroll-up, 3=scroll-down.
                    // Same 3px inter-frame gap as the DPad strip (verified against the sheet pixels).
                    int step  = el.Mapping[2] + 3;
                    var crops = new CroppedBitmap[4];
                    for (int n = 0; n < 4; n++)
                        crops[n] = Crop(el.Mapping[0] + n * step, el.Mapping[1],
                                        el.Mapping[2], el.Mapping[3]);

                    var wimg = MakeImage(el.Mapping[0], el.Mapping[1],
                                         el.Mapping[2], el.Mapping[3],
                                         el.Pos[0], el.Pos[1]);
                    OverlayCanvas.Children.Add(wimg);
                    _wheelImg[el.Id]   = wimg;
                    _wheelCrops[el.Id] = crops;
                    break;
                }

                case TYPE_DPAD:
                {
                    // TYPE_8 DPad: one image whose Source is swapped among 9 hat-switch states.
                    // The sprite sheet has a horizontal strip of 9 full-DPad sprites at the same y;
                    // see DpadSpriteIndex above for the index → direction layout.
                    // Each sprite is mapping[2] wide with a 3px gap → step = mapping[2]+3
                    // (verified against the sheet pixels — a 1px gap was assumed previously,
                    // which under-stepped by 2px per frame and made every pressed sprite
                    // drift further right than the last).
                    int step   = el.Mapping[2] + 3;
                    var crops  = new CroppedBitmap[9];
                    for (int n = 0; n < 9; n++)
                        crops[n] = Crop(el.Mapping[0] + n * step, el.Mapping[1],
                                        el.Mapping[2], el.Mapping[3]);

                    var img = MakeImage(el.Mapping[0], el.Mapping[1],
                                        el.Mapping[2], el.Mapping[3],
                                        el.Pos[0], el.Pos[1]);
                    OverlayCanvas.Children.Add(img);
                    _dpadImg[el.Id]   = img;
                    _dpadCrops[el.Id] = crops;
                    break;
                }

                case TYPE_BUTTON:
                case TYPE_TRIGGER:
                case TYPE_KEY:
                case TYPE_MOUSEBTN:
                {
                    // Row 1 (inactive / white outline): from JSON mapping — always visible.
                    var inactive = MakeImage(el.Mapping[0], el.Mapping[1],
                                            el.Mapping[2], el.Mapping[3],
                                            el.Pos[0], el.Pos[1]);
                    inactive.Opacity = 1;
                    OverlayCanvas.Children.Add(inactive);

                    // Row 2 (active / cyan): y + sprite_height — immediately below inactive.
                    var active = MakeImage(el.Mapping[0], el.Mapping[1] + el.Mapping[3],
                                          el.Mapping[2], el.Mapping[3],
                                          el.Pos[0], el.Pos[1]);
                    active.Opacity = 0;
                    OverlayCanvas.Children.Add(active);
                    _activeImgs[el.Id] = active;
                    break;
                }

                default:
                {
                    var img = MakeImage(el.Mapping[0], el.Mapping[1],
                                        el.Mapping[2], el.Mapping[3],
                                        el.Pos[0], el.Pos[1]);
                    OverlayCanvas.Children.Add(img);

                    // Stick images need to be accessible for position updates
                    if (el.Type == TYPE_STICK)
                        _activeImgs[el.Id] = img;
                    break;
                }
            }
        }
    }

    // ── Input polling ─────────────────────────────────────────────────────────

    private void Poll(object? sender, EventArgs e)
    {
        try
        {
            if (_controllerFolder == "keyboard")
            {
                PollKeyboardMouse();
                return;
            }

            var rawList = RawGameController.RawGameControllers;
            var sony    = rawList.FirstOrDefault(IsSonyController);

            if (sony != null)
            {
                PollSony(sony);
                return;
            }

            var gamepads = Gamepad.Gamepads;
            if (gamepads.Count == 0) return;

            var r     = gamepads[0].GetCurrentReading();
            var codes = new HashSet<int>();

            void AddIf(bool c, int code) { if (c) codes.Add(code); }
            AddIf((r.Buttons & GamepadButtons.A)               != 0, 0);
            AddIf((r.Buttons & GamepadButtons.B)               != 0, 1);
            AddIf((r.Buttons & GamepadButtons.X)               != 0, 2);
            AddIf((r.Buttons & GamepadButtons.Y)               != 0, 3);
            AddIf((r.Buttons & GamepadButtons.View)            != 0, 4);
            AddIf((r.Buttons & GamepadButtons.Menu)            != 0, 6);
            AddIf((r.Buttons & GamepadButtons.LeftThumbstick)  != 0, 7);
            AddIf((r.Buttons & GamepadButtons.RightThumbstick) != 0, 8);
            AddIf((r.Buttons & GamepadButtons.LeftShoulder)    != 0, 9);
            AddIf((r.Buttons & GamepadButtons.RightShoulder)   != 0, 10);

            bool du = (r.Buttons & GamepadButtons.DPadUp)    != 0;
            bool dd = (r.Buttons & GamepadButtons.DPadDown)  != 0;
            bool dl = (r.Buttons & GamepadButtons.DPadLeft)  != 0;
            bool dright = (r.Buttons & GamepadButtons.DPadRight) != 0;
            AddIf(du,     11);
            AddIf(dd,     12);
            AddIf(dl,     13);
            AddIf(dright, 14);

            // Convert DPad buttons to hat-switch equivalent for TYPE_8 skins
            GameControllerSwitchPosition dpad;
            if      (du && dright) dpad = GameControllerSwitchPosition.UpRight;
            else if (dright && dd) dpad = GameControllerSwitchPosition.DownRight;
            else if (dd && dl)     dpad = GameControllerSwitchPosition.DownLeft;
            else if (dl && du)     dpad = GameControllerSwitchPosition.UpLeft;
            else if (du)           dpad = GameControllerSwitchPosition.Up;
            else if (dright)       dpad = GameControllerSwitchPosition.Right;
            else if (dd)           dpad = GameControllerSwitchPosition.Down;
            else if (dl)           dpad = GameControllerSwitchPosition.Left;
            else                   dpad = GameControllerSwitchPosition.Center;

            ApplyUnified(codes, dpad,
                r.LeftThumbstickX,  r.LeftThumbstickY,
                r.RightThumbstickX, r.RightThumbstickY,
                r.LeftTrigger,      r.RightTrigger);
        }
        catch { }
    }

    private void PollSony(RawGameController ctrl)
    {
        if (_rawButtons.Length  != ctrl.ButtonCount) _rawButtons  = new bool[ctrl.ButtonCount];
        if (_rawSwitches.Length != ctrl.SwitchCount) _rawSwitches = new GameControllerSwitchPosition[ctrl.SwitchCount];
        if (_rawAxes.Length     != ctrl.AxisCount)   _rawAxes     = new double[ctrl.AxisCount];

        ctrl.GetCurrentReading(_rawButtons, _rawSwitches, _rawAxes);

        // Buttons via raw HID order
        var codes = new HashSet<int>();
        foreach (var (raw, code) in PsButtonMap)
            if (_rawButtons.Length > raw && _rawButtons[raw])
                codes.Add(code);

        // Hat switch → individual direction codes (for TYPE_2 DPad) + raw position (for TYPE_8)
        var hatPos = _rawSwitches.Length > 0 ? _rawSwitches[0] : GameControllerSwitchPosition.Center;
        if (hatPos is GameControllerSwitchPosition.Up or GameControllerSwitchPosition.UpRight or GameControllerSwitchPosition.UpLeft)   codes.Add(11);
        if (hatPos is GameControllerSwitchPosition.Down or GameControllerSwitchPosition.DownRight or GameControllerSwitchPosition.DownLeft) codes.Add(12);
        if (hatPos is GameControllerSwitchPosition.Left or GameControllerSwitchPosition.UpLeft or GameControllerSwitchPosition.DownLeft)  codes.Add(13);
        if (hatPos is GameControllerSwitchPosition.Right or GameControllerSwitchPosition.UpRight or GameControllerSwitchPosition.DownRight) codes.Add(14);

        // Analog values via Gamepad view (correct regardless of USB/BT axis order)
        double lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
        var gp = Gamepad.FromGameController(ctrl);
        if (gp != null)
        {
            var r = gp.GetCurrentReading();
            lx = r.LeftThumbstickX;
            ly = r.LeftThumbstickY;
            rx = r.RightThumbstickX;
            ry = r.RightThumbstickY;
            lt = r.LeftTrigger;
            rt = r.RightTrigger;
        }

        ApplyUnified(codes, hatPos, lx, ly, rx, ry, lt, rt);
    }

    private void PollKeyboardMouse()
    {
        var codes = new HashSet<int>();

        // Keyboard keys: scancode (el.Code) → cached virtual-key → live system-wide state
        foreach (var (scanCode, vk) in _keyVk)
            if ((GetAsyncKeyState((int)vk) & 0x8000) != 0)
                codes.Add(scanCode);

        // Mouse buttons (asset convention: 1=left, 2=right, 3=middle, 4=side2, 5=side1)
        // smb1 (code 5) is the upper side button, smb2 (code 4) the lower one — that's the
        // opposite of Windows' XBUTTON1/XBUTTON2 ordering, hence the swap below.
        void AddMouseIf(int vk, int code) { if ((GetAsyncKeyState(vk) & 0x8000) != 0) codes.Add(code); }
        AddMouseIf(VK_LBUTTON,  1);
        AddMouseIf(VK_RBUTTON,  2);
        AddMouseIf(VK_XBUTTON1, 4);
        AddMouseIf(VK_XBUTTON2, 5);

        // Wheel sprite: middle-click held, else a brief flash on the last scroll direction
        int wheelIndex = 0;
        if ((GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0)
            wheelIndex = 1;
        else if (Environment.TickCount64 < _wheelFlashUntil)
            wheelIndex = _wheelFlashIndex;

        ApplyUnified(codes, GameControllerSwitchPosition.Center, 0, 0, 0, 0, 0, 0, wheelIndex);
    }

    private void InstallMouseWheelHook()
    {
        _mouseHookProc = MouseHookCallback;
        using var curModule = System.Diagnostics.Process.GetCurrentProcess().MainModule;
        _mouseHookHandle = SetWindowsHookEx(WH_MOUSE_LL, _mouseHookProc,
            GetModuleHandle(curModule?.ModuleName), 0);
    }

    private void UninstallMouseWheelHook()
    {
        if (_mouseHookHandle != IntPtr.Zero)
        {
            UnhookWindowsHookEx(_mouseHookHandle);
            _mouseHookHandle = IntPtr.Zero;
        }
    }

    private IntPtr MouseHookCallback(int nCode, IntPtr wParam, IntPtr lParam)
    {
        if (nCode >= 0 && wParam.ToInt32() == WM_MOUSEWHEEL)
        {
            var data  = Marshal.PtrToStructure<MSLLHOOKSTRUCT>(lParam);
            short delta = (short)((data.mouseData >> 16) & 0xFFFF);
            _wheelFlashIndex = delta > 0 ? 2 : 3;
            _wheelFlashUntil = Environment.TickCount64 + 150;
        }
        return CallNextHookEx(_mouseHookHandle, nCode, wParam, lParam);
    }

    // ── Unified state application ─────────────────────────────────────────────

    private void ApplyUnified(
        HashSet<int> codes,
        GameControllerSwitchPosition dpadState,
        double lx, double ly, double rx, double ry,
        double lt, double rt,
        int wheelIndex = 0)
    {
        foreach (var el in _elements)
        {
            switch (el.Type)
            {
                case TYPE_BUTTON:
                case TYPE_KEY:
                case TYPE_MOUSEBTN:
                    // Active (cyan) image is shown on top of always-visible inactive (white) image
                    if (_activeImgs.TryGetValue(el.Id, out var btn))
                        btn.Opacity = codes.Contains(el.Code) ? 1.0 : 0.0;
                    break;

                case TYPE_WHEEL:
                    if (_wheelImg.TryGetValue(el.Id, out var wheelImage) &&
                        _wheelCrops.TryGetValue(el.Id, out var wcrops) &&
                        wheelIndex < wcrops.Length)
                        wheelImage.Source = wcrops[wheelIndex];
                    break;

                case TYPE_DPAD:
                    // Swap the DPad image source to the sprite for the current hat-switch state.
                    // State enum values: Center=0, Up=1, UpRight=2, Right=3, DownRight=4,
                    //                   Down=5, DownLeft=6, Left=7, UpLeft=8
                    if (_dpadImg.TryGetValue(el.Id, out var dpadImage) &&
                        _dpadCrops.TryGetValue(el.Id, out var crops))
                    {
                        int wgi = (int)dpadState;
                        int idx = wgi < DpadSpriteIndex.Length ? DpadSpriteIndex[wgi] : 0;
                        if (idx < crops.Length)
                            dpadImage.Source = crops[idx];
                    }
                    break;

                case TYPE_TRIGGER:
                    // Active (cyan) image opacity follows trigger depth
                    if (_activeImgs.TryGetValue(el.Id, out var trig))
                        trig.Opacity = el.Side == 0 ? lt : rt;
                    break;

                case TYPE_STICK:
                    if (_activeImgs.TryGetValue(el.Id, out var stick))
                    {
                        double nx = el.Side == 0 ? lx : rx;
                        double ny = el.Side == 0 ? ly : ry;
                        // WGI Y is math (+up); canvas Y is +down → negate
                        Canvas.SetLeft(stick, el.Pos[0] + nx *  el.StickRadius);
                        Canvas.SetTop( stick, el.Pos[1] + ny * -el.StickRadius);
                    }
                    break;
            }
        }
    }

    // ── UI ────────────────────────────────────────────────────────────────────

    // Makes the overlay click-through: without this the overlay is a real, hit-testable
    // window sitting on top of the game, so the OS reports it (not the game) as the window
    // under the cursor on hover — which some games read as a focus/foreground loss and
    // respond to by pausing. Same fix as FpsOverlayWindow.
    protected override void OnSourceInitialized(EventArgs e)
    {
        base.OnSourceInitialized(e);

        var hwnd = new WindowInteropHelper(this).Handle;
        var ex   = NativeMethods.GetWindowLong(hwnd, NativeMethods.GWL_EXSTYLE);
        NativeMethods.SetWindowLong(hwnd, NativeMethods.GWL_EXSTYLE,
            ex | NativeMethods.WS_EX_TRANSPARENT | NativeMethods.WS_EX_LAYERED);
    }

    protected override void OnClosed(EventArgs e)
    {
        _poll.Stop();
        UninstallMouseWheelHook();
        base.OnClosed(e);
    }

    private static class NativeMethods
    {
        public const int GWL_EXSTYLE       = -20;
        public const int WS_EX_LAYERED     = 0x80000;
        public const int WS_EX_TRANSPARENT = 0x20;

        [DllImport("user32.dll")]
        public static extern int GetWindowLong(nint hWnd, int nIndex);

        [DllImport("user32.dll")]
        public static extern int SetWindowLong(nint hWnd, int nIndex, int dwNewLong);
    }
}
