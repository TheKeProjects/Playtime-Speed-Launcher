#include "Settings/CustomNameSettings.h"
#include <algorithm>
#include "Core/Text.h"
#include "Settings/ConfigFile.h"

namespace SpeedrunLauncher::Settings {

namespace {
constexpr const wchar_t* kFile = L"custom_name.json";

std::wstring HexColor(uint32_t rgb) { return Text::Hex(rgb & 0xFFFFFF, 6); }
} // namespace

CustomName& CustomName::Current() {
    static CustomName current = [] {
        CustomName c;
        const Json::Value json = Config::ReadJson(kFile);
        if (json.IsObject()) {
            c.name = json["Name"].WStr().substr(0, kMaxName);
            c.color = static_cast<uint32_t>(json["Color"].Int(0xFFFFFF)) & 0xFFFFFF;
            c.color2 = static_cast<uint32_t>(json["Color2"].Int(0x33CCFF)) & 0xFFFFFF;
            c.animation = json["Animation"].IsString() ? json["Animation"].WStr() : L"none";
            c.speed = std::clamp(static_cast<float>(json["Speed"].Number(1.0)), 0.25f, 3.f);
            c.size = std::clamp(static_cast<float>(json["Size"].Number(1.0)), 0.8f, 1.6f);
            c.shadow = json["Shadow"].Bool(true);
            c.outline = json["Outline"].Bool(false);
            c.bold = json["Bold"].Bool(false);
            c.style = json["Style"].Bool(true);
            c.typeface = json["Typeface"].IsString() ? json["Typeface"].WStr() : (c.bold ? L"Bold" : L"Regular");
            // Light / Black were offered before: the game's font hasn't got them
            c.typeface = Text::Equals(c.typeface, L"Bold") || Text::Equals(c.typeface, L"Black") ? L"Bold" : L"Regular";
            c.italic = json["Italic"].Bool(false);
            c.letterCase = json["Case"].IsString() ? json["Case"].WStr() : (json["Upper"].Bool(false) ? L"upper" : L"typed");
            if (c.letterCase != L"upper" && c.letterCase != L"lower") c.letterCase = L"typed";
            c.spacing = json["Spacing"].Bool(false);
        }
        return c;
    }();
    return current;
}

void CustomName::Save() const {
    Json::Value json = Json::Value::MakeObject();
    json.Set("Name", name);
    json.Set("Color", static_cast<long long>(color));
    json.Set("Color2", static_cast<long long>(color2));
    json.Set("Animation", animation);
    json.Set("Speed", static_cast<double>(speed));
    json.Set("Size", static_cast<double>(size));
    json.Set("Shadow", shadow);
    json.Set("Outline", outline);
    json.Set("Style", style);
    json.Set("Typeface", typeface);
    json.Set("Italic", italic);
    json.Set("Case", letterCase);
    json.Set("Spacing", spacing);
    Config::WriteJson(kFile, json);
}

std::wstring CustomName::ToGameConfig() const {
    // One line per key: the name can't break the file
    std::wstring clean;
    for (wchar_t ch : name)
        if (ch >= 0x20 && ch != 0x7F) clean += ch;
    std::wstring s = L"; Written by Playtime Speed Launcher (Custom Name). Edit it from the launcher.\r\n[CustomName]\r\n";
    s += L"Name=" + Text::Trim(clean) + L"\r\n";
    s += L"Style=" + std::wstring(style ? L"1" : L"0") + L"\r\n";
    s += L"Color=" + HexColor(color) + L"\r\n";
    s += L"Color2=" + HexColor(color2) + L"\r\n";
    s += L"Animation=" + animation + L"\r\n";
    s += L"Speed=" + Text::FromDouble(speed, 2) + L"\r\n";
    s += L"Size=" + Text::FromDouble(size, 2) + L"\r\n";
    s += L"Shadow=" + std::wstring(shadow ? L"1" : L"0") + L"\r\n";
    s += L"Outline=" + std::wstring(outline ? L"1" : L"0") + L"\r\n";
    s += L"Typeface=" + typeface + L"\r\n";
    s += L"Italic=" + std::wstring(italic ? L"1" : L"0") + L"\r\n";
    s += L"Case=" + letterCase + L"\r\n";
    s += L"Spacing=" + std::wstring(spacing ? L"1" : L"0") + L"\r\n";
    return s;
}

} // namespace SpeedrunLauncher::Settings
