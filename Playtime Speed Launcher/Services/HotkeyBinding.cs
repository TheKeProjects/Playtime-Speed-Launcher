namespace SpeedrunLauncher.Services;

public enum HotkeyInputType { None, Keyboard, Mouse }

/// <summary>A single input (keyboard key or mouse button) used to trigger a remappable hotkey.</summary>
public readonly record struct HotkeyBinding(HotkeyInputType Type, uint KeyVk, int MouseButton);

/// <summary>Mouse button identifiers shared by remap features, used for persistence/UI.</summary>
public static class HotkeyMouseButtons
{
    public const int XButton1 = 1; // "Mouse 4"
    public const int XButton2 = 2; // "Mouse 5"
    public const int Middle   = 3; // "Mouse 3"
}
