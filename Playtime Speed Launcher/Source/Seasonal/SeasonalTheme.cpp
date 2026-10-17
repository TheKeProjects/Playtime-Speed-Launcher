#include "Seasonal/SeasonalTheme.h"
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include "Settings/IconThemeSettings.h"

namespace SpeedrunLauncher::Seasonal {

namespace {

Season Resolve() {
    const std::wstring t = Settings::IconTheme::Current().EffectiveTheme();
    if (t == L"halloween") return Season::Halloween;
    if (t == L"summer") return Season::Summer;
    if (t == L"christmas") return Season::Christmas;
    return Season::Classic;
}

Season active = Season::Classic;
bool resolved = false;
unsigned revision = 0;
std::unordered_map<uint32_t, Color> cache;

// Classic UI colors collected from the old MainWindow.xaml / .cs
const std::unordered_set<uint32_t>& ClassicColors() {
    static const std::unordered_set<uint32_t> colors = {
        0x00CCAA, 0x3A6A8A, 0x060F18, 0x8AAABB, 0x0D2535, 0x0A1825, 0x1A3A55, 0x0D2030, 0x09141E, 0xA0B4C8, 0x153048, 0x2A5A7A,
        0x00503C, 0x00B482, 0x00281E, 0xD2DCE6, 0x1E3C5A, 0x112233, 0x00CC88, 0x2D5A78, 0x007864, 0x5A8AA0, 0x07101A, 0xA0BED2,
        0x5082C8, 0x0A1420, 0xC8D2DC, 0x1A4060, 0x060C12, 0xA0C8D8, 0x50AAE6, 0x081E37, 0x0E2A4E, 0x00C88C, 0x78A0BE, 0xA0BEDC,
        0x0A283C, 0x6482A0, 0x08111A, 0x007755, 0x1A3A50, 0x2A4A6A, 0x080C18, 0x4A5A7A, 0x334444, 0x050A12, 0x142637, 0x50A0E6,
        0xA0C8E6, 0x96B4C8, 0x00785A, 0x070C12, 0x00CC66, 0x003322, 0x050B12, 0x0A1D2E, 0x123049, 0x0F2438, 0xCCEEFF, 0xAABBCC,
        0x060B14, 0x1A2A38, 0x5A7A9A, 0x0D2A3B, 0xE6F0FF, 0x123C6E, 0x008CC8, 0x64AADC, 0xC8D2E1, 0x3C78A0, 0x64A0DC, 0x00C896,
        0x005064, 0x648CA0, 0x64A0C8, 0x8CAAC8, 0x1565C0, 0xB4D2E6,
        // Standalone menus (checkpoint picker, skips and glitches, skip report)
        0x0D3020, 0x05090F, 0xC4D2DE, 0x0C1C2A, 0x28506A, 0x96B9CD, 0x5A82A0, 0xD6E4EE, 0x143044, 0x285A78, 0x50738C, 0x03070D,
        0xC8D8E4, 0x04080E, 0x8AABBD, 0x060D18, 0x648296, 0x0A1E2E, 0x040810, 0xAAC8D7, 0x03110D,
    };
    return colors;
}

void RgbToHsl(double r, double g, double b, double& h, double& s, double& l) {
    const double mx = std::max({ r, g, b }), mn = std::min({ r, g, b });
    l = (mx + mn) / 2;
    if (mx == mn) {
        h = s = 0;
        return;
    }
    const double d = mx - mn;
    s = l > 0.5 ? d / (2 - mx - mn) : d / (mx + mn);
    if (mx == r) h = (g - b) / d + (g < b ? 6 : 0);
    else if (mx == g) h = (b - r) / d + 2;
    else h = (r - g) / d + 4;
    h *= 60;
}

double Hue(double p, double q, double t) {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0 / 6) return p + (q - p) * 6 * t;
    if (t < 0.5) return q;
    if (t < 2.0 / 3) return p + (q - p) * (2.0 / 3 - t) * 6;
    return p;
}

void HslToRgb(double h, double s, double l, double& r, double& g, double& b) {
    if (s == 0) {
        r = g = b = l;
        return;
    }
    const double q = l < 0.5 ? l * (1 + s) : l + s - l * s;
    const double p = 2 * l - q;
    const double hk = h / 360;
    r = Hue(p, q, hk + 1.0 / 3);
    g = Hue(p, q, hk);
    b = Hue(p, q, hk - 1.0 / 3);
}

} // namespace

Season Active() {
    if (!resolved) {
        active = Resolve();
        resolved = true;
    }
    return active;
}

bool IsSeasonal() { return Active() != Season::Classic; }

std::wstring Name(Season season) {
    switch (season) {
        case Season::Halloween: return L"halloween";
        case Season::Summer: return L"summer";
        case Season::Christmas: return L"christmas";
        default: return L"classic";
    }
}

void Refresh() {
    const Season season = Resolve();
    resolved = true;
    if (season == active) return;
    active = season;
    cache.clear();
    ++revision;
}

unsigned Revision() { return revision; }

const Palette& Current() {
    static const Palette halloween{ Rgba(14, 8, 22), Rgba(16, 8, 26, 225), Rgba(70, 34, 100), Rgba(255, 140, 50),
                                    Rgba(255, 122, 26), Rgba(18, 10, 28), Rgba(195, 155, 255), Rgba(170, 140, 205) };
    static const Palette summer{ Rgba(6, 24, 31), Rgba(10, 22, 46, 225), Rgba(120, 60, 110), Rgba(255, 196, 70),
                                 Rgba(255, 182, 39), Rgba(30, 16, 40), Rgba(255, 220, 130), Rgba(255, 205, 190) };
    static const Palette christmas{ Rgba(10, 22, 15), Rgba(6, 14, 26, 225), Rgba(40, 80, 70), Rgba(255, 201, 40),
                                    Rgba(214, 40, 40), Rgba(255, 255, 255), Rgba(255, 213, 79), Rgba(170, 200, 225) };
    static const Palette classic{ Rgba(7, 12, 18), Rgba(8, 17, 26), Rgba(13, 32, 48), Rgba(0, 204, 170),
                                  Rgba(0, 204, 170), Rgba(6, 12, 18), Rgba(255, 170, 0), Rgba(42, 90, 122) };
    switch (Active()) {
        case Season::Halloween: return halloween;
        case Season::Summer: return summer;
        case Season::Christmas: return christmas;
        default: return classic;
    }
}

Color Map(Color c) {
    if (Active() == Season::Classic) return c;
    const uint32_t packed = c.Packed();
    const uint32_t rgb = packed & 0xFFFFFF;
    if (!ClassicColors().count(rgb)) return c;
    auto it = cache.find(packed);
    if (it != cache.end()) return it->second;

    double h, s, l;
    RgbToHsl(((rgb >> 16) & 0xFF) / 255.0, ((rgb >> 8) & 0xFF) / 255.0, (rgb & 0xFF) / 255.0, h, s, l);
    const bool teal = h < 185;
    double nh = h, ns = s, nl = l;
    switch (Active()) {
        case Season::Halloween:
            if (teal) { nh = 26; nl = std::min(0.72, l * 1.12); }
            else { nh = 272 + (h - 205) * 0.4; ns = std::min(1.0, s * 1.05); }
            break;
        case Season::Summer:
            if (teal) { nh = 40; nl = std::min(0.72, l * 1.15); }
            else nh = 248 + (h - 205) * 0.4;
            break;
        case Season::Christmas:
            if (teal) { nh = 45; nl = std::min(0.72, l * 1.15); }
            else { nh = 152 + (h - 205) * 0.3; ns = s * 0.9; }
            break;
        default:
            break;
    }
    nh = std::fmod(std::fmod(nh, 360.0) + 360.0, 360.0);
    double r, g, b;
    HslToRgb(nh, ns, nl, r, g, b);
    const auto byte = [](double v) { return static_cast<float>(std::round(std::clamp(v, 0.0, 1.0) * 255.0) / 255.0); };
    const Color mapped{ byte(r), byte(g), byte(b), c.a };
    cache.emplace(packed, mapped);
    return mapped;
}

Color Filter(Color c) { return Map(c); }

namespace {
double clockReal = -1.0, clockTime = 0.0;
float clockRamp = 1.f;   // 1 = full speed, 0 = stopped
} // namespace

double AnimationTime(double realTime, bool paused) {
    constexpr float kRampSeconds = 1.6f;
    const float dt = clockReal < 0.0 ? 0.f : std::clamp(static_cast<float>(realTime - clockReal), 0.f, 0.1f);
    clockReal = realTime;
    clockRamp = std::clamp(clockRamp + (paused ? -dt : dt) / kRampSeconds, 0.f, 1.f);
    // Smoothstep on the speed: eases out of full speed and settles gently at the stop (and back)
    const float speed = clockRamp * clockRamp * (3.f - 2.f * clockRamp);
    clockTime += dt * speed;
    return clockTime;
}

double LastAnimationTime() { return clockTime; }
bool AnimationStopped() { return clockRamp <= 0.f; }

} // namespace SpeedrunLauncher::Seasonal
