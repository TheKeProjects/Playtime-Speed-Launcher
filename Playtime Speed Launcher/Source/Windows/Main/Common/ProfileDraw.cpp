#include "Windows/Main/Common/ProfileDraw.h"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include "App/Launcher.h"
#include "Graphics/AnimatedImages.h"
#include "Localization/Localization.h"
#include "Services/Chapters/ChapterPlaytimeStore.h"
#include "Seasonal/SeasonalTheme.h"
#include "UI/Style.h"

namespace SpeedrunLauncher::Main::ProfileDraw {

namespace {

Color Hue(float h) {
    h = h - std::floor(h);
    const float r = std::clamp(std::fabs(h * 6.f - 3.f) - 1.f, 0.f, 1.f);
    const float g = std::clamp(2.f - std::fabs(h * 6.f - 2.f), 0.f, 1.f);
    const float b = std::clamp(2.f - std::fabs(h * 6.f - 4.f), 0.f, 1.f);
    // Pastel-ish so it reads on the dark panels
    return Lerp(Color{ r, g, b, 1.f }, Colors::White, 0.18f);
}

bool IsScript(const Profile& p) { return p.font == "script" || p.font == "gabriola" || p.font == "ink" || p.font == "print"; }

// Where the text actually sits inside `r` (for gradients)
Rect TextBox(UI::Ui& ui, const std::wstring& text, const Rect& r, const TextStyle& style) {
    const TextSize size = ui.g.Measure(text, style);
    const float w = std::min(r.w, size.w), h = std::min(r.h, size.h);
    const float x = style.align == HAlign::Center ? r.CX() - w * 0.5f : style.align == HAlign::Right ? r.R() - w : r.x;
    return { x, r.CY() - h * 0.5f, std::max(w, 1.f), std::max(h, 1.f) };
}

void DrawEffect(UI::Ui& ui, const std::wstring& text, const Rect& r, const Profile& p, const std::string& effect, TextStyle st) {
    auto& g = ui.g;
    if (text.empty() || r.Empty()) return;
    st.valign = VAlign::Center;
    st.ellipsis = true;
    NoColorFilter keep(g);   // the user's colours stay theirs whatever the season
    const Color c1 = Hex(p.color1), c2 = Hex(p.color2);
    const float unit = std::max(1.f, st.size * 0.07f);

    if (effect == "gradient") {
        const Rect box = TextBox(ui, text, r, st);
        g.Text(text, r, g.LinearBrush({ { 0.f, c1 }, { 1.f, c2 } }, { box.x, box.y }, { box.R(), box.y }), st);
    } else if (effect == "neon") {
        g.GlowText(text, r, st, c1, st.size * 0.55f, 0.95f);
        g.GlowText(text, r, st, c1, st.size * 0.2f, 0.8f);
        g.Text(text, r, Lerp(c1, Colors::White, 0.6f), st);
    } else if (effect == "toon") {
        // Thick outline in the second colour (a cartoon title), the name on top with a light top edge
        static const float dirs[8][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 }, { 0.7f, 0.7f }, { -0.7f, 0.7f }, { 0.7f, -0.7f }, { -0.7f, -0.7f } };
        for (const auto& d : dirs) g.Text(text, r.Offset(d[0] * unit * 1.6f, d[1] * unit * 1.6f + unit), c2, st);
        for (const auto& d : dirs) g.Text(text, r.Offset(d[0] * unit * 1.6f, d[1] * unit * 1.6f), c2, st);
        const Rect box = TextBox(ui, text, r, st);
        g.Text(text, r, g.LinearBrush({ { 0.f, Lerp(c1, Colors::White, 0.45f) }, { 0.5f, c1 }, { 1.f, c1 } }, { box.x, box.y }, { box.x, box.B() }), st);
    } else if (effect == "pop") {
        g.Text(text, r.Offset(unit * 1.8f, unit * 1.8f), c2, st);
        g.Text(text, r, c1, st);
    } else if (effect == "gummy") {
        // Glossy jelly: soft glow, vertical shine from the first colour into the second
        g.GlowText(text, r, st, c2, st.size * 0.35f, 0.55f);
        const Rect box = TextBox(ui, text, r, st);
        g.Text(text, r.Offset(0.f, unit), Lerp(c2, Colors::Black, 0.35f).WithAlpha(0.8f), st);
        g.Text(text, r,
               g.LinearBrush({ { 0.f, Lerp(c1, Colors::White, 0.55f) }, { 0.45f, c1 }, { 1.f, Lerp(c1, c2, 0.75f) } }, { box.x, box.y },
                             { box.x, box.B() }),
               st);
    } else if (effect == "prism") {
        // A rainbow sliding through the letters
        const Rect box = TextBox(ui, text, r, st);
        std::vector<GradientStop> stops;
        for (int k = 0; k <= 12; ++k) stops.push_back({ k / 12.f, Hue(k / 6.f) });
        const float phase = static_cast<float>(std::fmod(ui.Time() * 0.35, 1.0));
        const float start = box.x - phase * box.w;
        g.GlowText(text, r, st, Hue(static_cast<float>(ui.Time() * 0.35)), st.size * 0.3f, 0.35f);
        g.Text(text, r, g.LinearBrush(stops, { start, box.y }, { start + box.w * 2.f, box.y + box.h * 0.3f }), st);
        ui.KeepAnimatingThrottled();
    } else {
        g.Text(text, r, c1, st);
    }
}

std::wstring Initial(const std::wstring& name) {
    for (wchar_t c : name)
        if (!std::iswspace(c)) return std::wstring(1, static_cast<wchar_t>(std::towupper(c)));
    return L"?";
}

} // namespace

TextStyle NameStyle(const Profile& profile, float size) {
    TextStyle st;
    st.font = Fonts::ProfileFont(profile.FontIndex());
    // Script fonts have no bold and look better a little bigger
    st.size = IsScript(profile) ? size * 1.12f : size;
    st.weight = IsScript(profile) || profile.font == "impact" ? DWRITE_FONT_WEIGHT_NORMAL : DWRITE_FONT_WEIGHT_BOLD;
    return st;
}

float NameWidth(UI::Ui& ui, const std::wstring& text, const Profile& profile, float size) {
    return ui.g.Measure(text, NameStyle(profile, size)).w + size * 0.25f;
}

void Name(UI::Ui& ui, const std::wstring& text, const Rect& r, const Profile& profile, float size, HAlign align) {
    TextStyle st = NameStyle(profile, size);
    st.align = align;
    DrawEffect(ui, text, r, profile, profile.Effect().id, st);
}

void NameWithEffect(UI::Ui& ui, const std::wstring& text, const Rect& r, const Profile& profile, const char* effect, float size, HAlign align) {
    TextStyle st = NameStyle(profile, size);
    st.align = align;
    DrawEffect(ui, text, r, profile, effect, st);
}

void Avatar(UI::Ui& ui, const std::wstring& source, const Rect& r, const Profile& profile, const std::wstring& name, float ring, Color ringColor,
            bool animate) {
    auto& g = ui.g;
    NoColorFilter keep(g);
    const float radius = r.w * 0.5f;
    if (ring > 0.f) g.FillEllipse(r.CX(), r.CY(), radius + ring, radius + ring, ringColor);
    bool drawn = false;
    if (!source.empty()) {
        if (auto animation = AnimatedImages::Get(source, kAvatarSide)) {
            if (auto frame = animation->At(animate ? ui.Time() : 0.0)) {
                g.DrawImageRounded(*frame, r, radius, 1.f, Stretch::UniformToFill);
                drawn = true;
            }
            if (animate && animation->Animated()) ui.KeepAnimatingThrottled();
        }
    }
    if (!drawn) {
        const Color c = Hex(profile.color1);
        g.FillEllipse(r.CX(), r.CY(), radius, radius, Lerp(Hex(profile.bannerColor), Colors::Black, 0.25f));
        g.StrokeEllipse(r.CX(), r.CY(), radius - 0.5f, radius - 0.5f, c.WithAlpha(0.35f), 1.f);
        TextStyle st = NameStyle(profile, r.h * 0.46f);
        st.align = HAlign::Center;
        g.Text(Initial(name), r, c, st);
    }
    if (r.w >= 20.f) AvatarDecoration(ui, r, profile, animate);
}

void Banner(UI::Ui& ui, const std::wstring& source, const Rect& r, const Profile& profile, float radius) {
    auto& g = ui.g;
    NoColorFilter keep(g);
    const Color base = Hex(profile.bannerColor);
    g.FillLinear(r, radius, { { 0.f, Lerp(base, Colors::White, 0.12f) }, { 1.f, Lerp(base, Colors::Black, 0.3f) } }, { 0.f, 0.f }, { 1.f, 1.f });
    if (source.empty()) return;
    if (auto animation = AnimatedImages::Get(source, kBannerSide)) {
        if (auto frame = animation->At(ui.Time())) g.DrawImageRounded(*frame, r, radius, 1.f, Stretch::UniformToFill);
        if (animation->Animated()) ui.KeepAnimatingThrottled();
    }
}

void WideBanner(UI::Ui& ui, const std::wstring& source, const Rect& r, const Profile& profile) {
    auto& g = ui.g;
    NoColorFilter keep(g);
    const Color base = Hex(profile.bannerColor);
    g.FillLinear(r, 0.f, { { 0.f, Lerp(base, Colors::White, 0.12f) }, { 1.f, Lerp(base, Colors::Black, 0.3f) } }, { 0.f, 0.f }, { 1.f, 1.f });
    if (source.empty()) return;
    auto animation = AnimatedImages::Get(source, kBannerSide);
    if (!animation) return;
    if (animation->Animated()) ui.KeepAnimatingThrottled();
    auto frame = animation->At(ui.Time());
    if (!frame || frame->w <= 0.f || frame->h <= 0.f) return;
    // The whole picture at its own aspect, centred; the sides get a dimmed, stretched copy of it
    const float scale = std::min(r.w / frame->w, r.h / frame->h);
    const Rect fit{ r.CX() - frame->w * scale * 0.5f, r.CY() - frame->h * scale * 0.5f, frame->w * scale, frame->h * scale };
    if (fit.w < r.w - 1.f || fit.h < r.h - 1.f) {
        g.DrawImageRounded(*frame, r, 0.f, 0.35f, Stretch::UniformToFill);
        g.FillRect(r, Rgba(0, 0, 0, 90));
    }
    g.DrawImageRounded(*frame, fit, 0.f, 1.f, Stretch::Fill);
    // Soft edges where the picture meets the sides
    if (fit.w < r.w - 1.f) {
        const float fade = std::min(60.f, fit.w * 0.15f);
        g.FillLinear({ fit.x - 1.f, fit.y, fade, fit.h }, 0.f, { { 0.f, Rgba(0, 0, 0, 90) }, { 1.f, Rgba(0, 0, 0, 0) } }, { 0.f, 0.f }, { 1.f, 0.f });
        g.FillLinear({ fit.R() - fade + 1.f, fit.y, fade, fit.h }, 0.f, { { 0.f, Rgba(0, 0, 0, 0) }, { 1.f, Rgba(0, 0, 0, 90) } }, { 0.f, 0.f }, { 1.f, 0.f });
    }
}

float CardHeight(float width) {
    // banner · the part of the picture under it · name · two lines · bottom padding
    const float avatarSize = std::min(84.f, width * 0.26f);
    return width * 0.36f + avatarSize * 0.45f + 6.f + 34.f + 36.f + 14.f;
}

Color CardColor(const Profile& profile) {
    return Hex(profile.cardColor == Services::Account::kAutoColor ? Services::Account::kDefaultCardColor : profile.cardColor);
}

void Card(UI::Ui& ui, const Rect& r, const Profile& profile, const CardInfo& info) {
    auto& g = ui.g;
    const float bannerH = r.w * 0.36f;
    // The user's colour, or the default dark navy (never re-tinted by the season)
    const Color body = CardColor(profile);
    {
        NoColorFilter keep(g);
        g.Shadow(r, 12.f, 18.f, Rgba(0, 0, 0, 120));
        g.FillRound(r, 12.f, body);
        g.FillLinear(r, 12.f, { { 0.f, Rgba(255, 255, 255, 10) }, { 1.f, Rgba(0, 0, 0, 40) } }, { 0.f, 0.f }, { 0.f, 1.f });
    }
    // Texts readable on light card colours too
    const float lum = 0.2126f * body.r + 0.7152f * body.g + 0.0722f * body.b;
    const bool light = lum > 0.55f;
    // Rounded at the card's top corners only: square where the body starts
    g.PushRoundClip(r, 12.f);
    Banner(ui, info.bannerSource, { r.x, r.y, r.w, bannerH }, profile, 0.f);
    g.PopLayer();
    const float avatarSize = std::min(84.f, r.w * 0.26f);
    const Rect avatar{ r.x + 18.f, r.y + bannerH - avatarSize * 0.55f, avatarSize, avatarSize };
    Avatar(ui, info.avatarSource, avatar, profile, info.name, 5.f, body);
    const float textX = r.x + 18.f;
    const float nameY = avatar.B() + 6.f;
    Name(ui, info.name, { textX, nameY, r.w - 36.f, 34.f }, profile, 22.f);
    {
        NoColorFilter keep(g);
        const Color line1 = light ? Lerp(info.line1Color, Colors::Black, 0.55f) : info.line1Color;
        const Color line2 = light ? Rgba(30, 30, 36, 200) : Rgba(200, 210, 220, 170);
        if (!info.line1.empty()) g.Text(info.line1, { textX, nameY + 34.f, r.w - 36.f, 16.f }, line1, UI::Mono(10.5f, true).Ellipsis());
        if (!info.line2.empty()) g.Text(info.line2, { textX, nameY + 52.f, r.w - 36.f, 16.f }, line2, UI::Mono(10.f).Ellipsis());
    }
    {
        NoColorFilter keep(g);
        g.StrokeRound(r, 12.f, Rgba(255, 255, 255, 18), 1.f);
    }
    ProfileFrame(ui, r, 12.f, profile);
}

namespace {
constexpr float kHourRowH = 22.f;
std::wstring HoursLabel(int id) {
    for (const auto& game : App::L().multiplayers)
        if (game.playtimeId == id) return Loc::Get("mp_" + game.key + "_title");
    return Loc::Fmt("fr_chapter", id);
}
} // namespace

double TotalSeconds(const Profile& profile) {
    double total = 0.0;
    for (const auto& [id, seconds] : profile.playtime)
        if (App::L().CountsPlaytime(id)) total += seconds;
    return total;
}

float HoursListHeight(const Profile& profile) {
    size_t rows = 0;
    for (const auto& [id, seconds] : profile.playtime)
        if (seconds >= 60.0 && App::L().CountsPlaytime(id)) ++rows;
    return rows == 0 ? kHourRowH : rows * kHourRowH;
}

float HoursList(UI::Ui& ui, const Rect& r, const Profile& profile, Color accent) {
    auto& g = ui.g;
    double most = 0.0;
    for (const auto& [id, seconds] : profile.playtime)
        if (App::L().CountsPlaytime(id)) most = std::max(most, seconds);
    float y = r.y;
    bool any = false;
    // Chapters (low ids) first, then the multiplayer games: the map is already in id order
    for (const auto& [id, seconds] : profile.playtime) {
        if (seconds < 60.0 || !App::L().CountsPlaytime(id)) continue;
        if (y + kHourRowH > r.B() + 0.5f) break;
        any = true;
        const Rect row{ r.x, y, r.w, kHourRowH };
        const float labelW = std::min(118.f, r.w * 0.38f), timeW = 70.f;
        g.Text(HoursLabel(id), { row.x, row.y, labelW, row.h }, UI::Palette::Label, UI::Mono(9.5f).Ellipsis());
        const Rect track{ row.x + labelW + 8.f, row.CY() - 3.f, std::max(10.f, row.w - labelW - timeW - 16.f), 6.f };
        g.FillRound(track, 3.f, UI::Palette::ButtonBorder);
        const float fill = most > 0.0 ? static_cast<float>(seconds / most) : 0.f;
        g.FillRound({ track.x, track.y, std::max(6.f, track.w * fill), track.h }, 3.f, accent);
        g.Text(Services::ChapterPlaytimeStore::Format(seconds), { row.R() - timeW, row.y, timeW, row.h }, UI::Palette::Bright, UI::Mono(10.f, true).Right());
        y += kHourRowH;
    }
    if (!any) {
        g.Text(L"\x2014", { r.x, r.y, r.w, kHourRowH }, UI::Palette::Hint, UI::Mono(10.f));
        y += kHourRowH;
    }
    return y - r.y;
}

} // namespace SpeedrunLauncher::Main::ProfileDraw
