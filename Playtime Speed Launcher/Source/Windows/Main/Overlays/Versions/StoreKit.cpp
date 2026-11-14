#include "Windows/Main/Overlays/Versions/StoreKit.h"
#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <unordered_map>
#include <vector>
#include "App/Launcher.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Text.h"
#include "Graphics/Images.h"
#include "Localization/Localization.h"
#include "UI/Widgets/Button.h"

namespace SpeedrunLauncher::Main::StoreKit {

using namespace UI;

// Chapter name in the launcher's language (the chapter data itself is in Spanish)
std::wstring ChapterTitle(const Models::Chapter& ch) { return Loc::Get("ch" + std::to_string(ch.number) + "_title"); }

std::vector<Game> Games() {
    const auto& l = App::L();
    std::vector<Game> games;
    games.reserve(l.chapters.size() + l.multiplayers.size());
    for (const auto& ch : l.chapters) games.push_back({ ch.number, "", ch.isAvailable, ch.presets });
    for (const auto& mp : l.multiplayers)
        if (!mp.presets.empty()) games.push_back({ mp.playtimeId, mp.key, mp.isAvailable, mp.presets });
    return games;
}

int GameIndexOf(int number) {
    const auto games = Games();
    for (size_t i = 0; i < games.size(); ++i)
        if (games[i].number == number) return static_cast<int>(i);
    return -1;
}

std::wstring GameTitle(const Game& game) {
    if (game.Multiplayer()) return Loc::Get("mp_" + game.mpKey + "_title");
    return Loc::Get("ch" + std::to_string(game.number) + "_title");
}

std::wstring GameTag(const Game& game) {
    return game.Multiplayer() ? Loc::Get("store_multiplayer_tag") : Loc::Fmt("store_chapter", game.number);
}

bool HasMultiplayers() {
    for (const auto& game : Games())
        if (game.Multiplayer() && game.isAvailable) return true;
    return false;
}

int FirstGameOf(bool multiplayer, int preferred) {
    const auto games = Games();
    const auto fits = [&](int i) {
        return i >= 0 && i < static_cast<int>(games.size()) && games[static_cast<size_t>(i)].Multiplayer() == multiplayer &&
               games[static_cast<size_t>(i)].isAvailable;
    };
    if (fits(preferred)) return preferred;
    for (int i = 0; i < static_cast<int>(games.size()); ++i)
        if (fits(i)) return i;
    return -1;
}

float SectionSwitchWidth(Ui& ui) {
    const TextStyle style = Mono(9.5f, true);
    const float widest = std::max(ui.g.Measure(Loc::Get("store_chapters"), style).w, ui.g.Measure(Loc::Get("store_multiplayers"), style).w);
    return (widest + 28.f) * 2.f + 6.f;
}

bool SectionSwitch(Ui& ui, Id id, const Rect& r, bool& multiplayers) {
    auto& g = ui.g;
    const float segW = (r.w - 6.f) * 0.5f;
    g.Border(r, r.h * 0.5f, Rgba(9, 20, 30, 190), Palette::Teal.WithAlpha(0.35f), 1.f);
    // The lit half slides to the picked section
    const float knob = ui.Animate(HashId(id, 1), multiplayers ? 1.f : 0.f, 16.f);
    const Rect k{ r.x + 3.f + knob * segW, r.y + 3.f, segW, r.h - 6.f };
    g.GlowRound(k, k.h * 0.5f, Palette::Teal, 10.f, 0.4f);
    g.FillRound(k, k.h * 0.5f, Palette::Teal);
    bool changed = false;
    const char* keys[2] = { "store_chapters", "store_multiplayers" };
    for (int i = 0; i < 2; ++i) {
        const Rect seg{ r.x + 3.f + i * segW, r.y + 3.f, segW, r.h - 6.f };
        const bool on = (i == 1) == multiplayers;
        const Interaction it = ui.Interact(HashId(id, 10 + i), seg);
        if (it.hovered && !on) ui.SetCursor(IDC_HAND);
        if (it.clicked && !on) {
            multiplayers = i == 1;
            changed = true;
        }
        const Color text = on ? Rgba(3, 17, 13) : it.hovered ? kTitle : kDimText;
        g.Text(Loc::Get(keys[i]), seg, text, Mono(9.5f, true).Center());
    }
    return changed;
}

std::wstring GameCrumb(const Game& game) { return game.Multiplayer() ? GameTitle(game) : GameTag(game); }

std::wstring GameShort(const Game& game) {
    if (!game.Multiplayer()) return std::to_wstring(game.number);
    std::wstring s(game.mpKey.begin(), game.mpKey.end());   // ASCII key
    for (auto& c : s) c = static_cast<wchar_t>(std::towupper(c));
    return s;
}

std::wstring VersionsCount(size_t n) {
    return n == 1 ? Loc::Get("store_versions_one") : Loc::Fmt("store_versions_count", static_cast<int>(n));
}

namespace {

// First of `candidates` that exists (asked every frame, so the answer is remembered per key)
std::wstring FirstExisting(const std::wstring& key, const std::vector<std::wstring>& candidates, const std::wstring& fallback) {
    static std::unordered_map<std::wstring, std::wstring> resolved;
    if (auto it = resolved.find(key); it != resolved.end()) return it->second;
    std::wstring found = fallback;
    for (const auto& c : candidates)
        if (Files::Exists(c)) {
            found = c;
            break;
        }
    resolved.emplace(key, found);
    return found;
}

std::wstring StoreAsset(const std::wstring& relative) { return Paths::Asset(L"Store\\" + relative); }

// Version name as a file name: characters Windows doesn't allow are dropped ("Any% <1.1" → "Any% 1.1")
std::wstring FileSafe(const std::wstring& name) {
    std::wstring out;
    for (wchar_t c : name)
        if (c >= 32 && !wcschr(L"<>:\"/\\|?*", c)) out += c;
    while (out.find(L"  ") != std::wstring::npos) out.replace(out.find(L"  "), 2, L" ");
    while (!out.empty() && (out.back() == L' ' || out.back() == L'.')) out.pop_back();
    while (!out.empty() && out.front() == L' ') out.erase(out.begin());
    return out;
}

} // namespace

// Games on the front page: the ones with a store cover or something to install
bool Listed(const Game& game) {
    if (!game.isAvailable) return false;   // not out yet (chapter 6)
    if (game.Multiplayer()) return !game.presets.empty();
    return !game.presets.empty() || CoverPath(game.number).find(L"\\Store\\") != std::wstring::npos;
}

namespace {

// "EFP" for Escape From Playtime (its banner's name); empty for a chapter
std::wstring MultiplayerArt(int number) {
    for (const auto& mp : App::L().multiplayers)
        if (mp.playtimeId == number) return Files::FileNameWithoutExtension(mp.banner);
    return L"";
}

} // namespace

std::wstring CoverPath(int chapterNumber) {
    if (const std::wstring art = MultiplayerArt(chapterNumber); !art.empty())
        return FirstExisting(L"cover|" + art, { StoreAsset(L"Multiplayers\\" + art + L".jpg"), StoreAsset(L"Multiplayers\\" + art + L".png") },
                             Paths::Asset(L"Banners\\Multiplayers\\" + art + L".jpg"));
    const std::wstring n = std::to_wstring(chapterNumber);
    return FirstExisting(L"cover|" + n, { StoreAsset(L"Chapters\\Chapter" + n + L".jpg"), StoreAsset(L"Chapters\\Chapter" + n + L".png"),
                                          StoreAsset(L"Chapter" + n + L".jpg") },
                         Paths::Asset(L"Images\\Chapter " + n + L".png"));
}

std::wstring BannerPath(int chapterNumber) {
    if (const std::wstring art = MultiplayerArt(chapterNumber); !art.empty())
        return FirstExisting(L"banner|" + art, { StoreAsset(L"Banners\\" + art + L".png"), StoreAsset(L"Banners\\" + art + L".jpg") },
                             CoverPath(chapterNumber));
    const std::wstring n = std::to_wstring(chapterNumber);
    return FirstExisting(L"banner|" + n, { StoreAsset(L"Banners\\Chapter " + n + L".png"), StoreAsset(L"Banners\\Chapter " + n + L".jpg"),
                                           StoreAsset(L"Banners\\Chapter" + n + L".png"), StoreAsset(L"Banners\\Chapter" + n + L".jpg") },
                         CoverPath(chapterNumber));
}

std::wstring VersionCoverPath(int chapterNumber, const std::wstring& versionName) {
    const std::wstring n = std::to_wstring(chapterNumber);
    const std::wstring art = MultiplayerArt(chapterNumber);
    const std::wstring folder = art.empty() ? L"Chapter " + n : art;
    std::vector<std::wstring> names{ FileSafe(versionName), FileSafe(Text::Replace(versionName, L"Parche", L"Patch")),
                                     FileSafe(Text::Replace(versionName, L"Patch", L"Parche")) };
    std::vector<std::wstring> candidates;
    for (const auto& name : names) {
        if (name.empty()) continue;
        for (const wchar_t* ext : { L".png", L".jpg", L".jpeg" }) candidates.push_back(StoreAsset(L"Versions\\" + folder + L"\\" + name + ext));
    }
    return FirstExisting(L"version|" + n + L"|" + versionName, candidates, CoverPath(chapterNumber));
}

// Image filling r (cropped, never stretched), zoomed around its centre by `zoom`
void DrawCoverImage(Ui& ui, const std::wstring& path, const Rect& r, float zoom, unsigned maxSide) {
    if (auto img = Images::Load(path, maxSide)) {
        const float gx = r.w * (zoom - 1.f) * 0.5f, gy = r.h * (zoom - 1.f) * 0.5f;
        ui.g.DrawImage(*img, r.Inset(-gx, -gy), 1.f, Stretch::UniformToFill);
    }
}

// Store cover tile: lifts, zooms its art and lights its border while hovered
Tile CoverTile(Ui& ui, Id id, const Rect& slot, const std::wstring& image, bool enabled, float lit, bool keepHover) {
    auto& g = ui.g;
    Tile t;
    t.it = ui.Interact(id, slot, enabled);
    if (t.it.hovered && enabled) ui.SetCursor(IDC_HAND);
    t.hover = ui.Animate(HashId(id, 0x7173), (t.it.hovered || keepHover) && enabled ? 1.f : 0.f, 14.f);
    const float press = ui.Animate(HashId(id, 0x7174), t.it.held ? 1.f : 0.f, 30.f);
    t.cover = slot.Offset(0.f, -7.f * t.hover + 2.f * press);

    g.Shadow(t.cover.Offset(0.f, 6.f + 6.f * t.hover), kRadius, 10.f + 10.f * t.hover, Argb(0x80000000));
    const float glow = std::max(t.hover, lit);
    if (glow > 0.01f) g.GlowRound(t.cover, kRadius, Palette::Teal, 20.f, 0.55f * glow);
    g.PushRoundClip(t.cover, kRadius);
    g.FillRect(t.cover, Hex(0x0A1420));
    DrawCoverImage(ui, image, t.cover, 1.f + 0.07f * t.hover);
    g.FillLinear(t.cover.Bottom(t.cover.h * 0.45f), 0.f, { { 0.f, Rgba(0, 0, 0, 0) }, { 1.f, Rgba(0, 0, 0, 170) } }, { 0.f, 0.f }, { 0.f, 1.f });
    g.PopLayer();
    g.StrokeRound(t.cover.Inset(0.5f), kRadius, glow > 0.01f ? Palette::Teal.WithAlpha(0.25f + 0.75f * glow) : Rgba(255, 255, 255, 22),
                  1.f + glow);
    return t;
}

TextBoxStyle FieldStyle(bool password, const std::wstring& placeholder) {
    TextBoxStyle s;
    s.text = Mono(11.f);
    s.foreground = Hex(0xC8D8E4);
    s.background = Hex(0x060D18);
    s.border = Hex(0x123049);
    s.focusBorder = Hex(0x123049);
    s.caret = Palette::Teal;
    s.selection = Rgba(0, 204, 170, 100);
    s.padding = { 12.f, 9.f };
    s.radius = 8.f;
    s.password = password;
    s.placeholder = placeholder;
    return s;
}

// Rounded check box + wrapped caption; returns its height
float CheckRow(Ui& ui, Id id, float x, float y, float w, const std::wstring& text, bool& value) {
    TextStyle style = Mono(10.f);
    style.wrap = true;
    style.valign = VAlign::Top;
    const float textH = ui.g.Measure(text, style, w - 28.f).h;
    const float h = std::max(18.f, textH);
    const Interaction it = ui.Interact(id, { x, y, w, h });
    if (it.hovered) ui.SetCursor(IDC_HAND);
    if (it.clicked) value = !value;
    const float on = ui.Animate(HashId(id, 1), value ? 1.f : 0.f, 18.f);
    const Rect box{ x, y, 18.f, 18.f };
    ui.g.Border(box, 5.f, Palette::Teal.WithAlpha(0.08f + 0.9f * on), value || it.hovered ? Palette::Teal : Rgba(255, 255, 255, 60), 1.f);
    if (on > 0.01f) Glyph(ui, 0xE73E, box, 10.f * on, Rgba(3, 17, 13));   // check mark
    ui.g.Text(text, { x + 28.f, y + 1.f, w - 28.f, textH }, Rgba(160, 185, 205, 220), style);
    return h;
}

// Wide primary action (glyph + label), glowing while hovered
bool BigButton(Ui& ui, Id id, const Rect& b, wchar_t glyph, const std::wstring& label, bool enabled) {
    auto& g = ui.g;
    const TextStyle bs = Mono(13.f, true);
    const float glow = ui.Animate(HashId(id, 1), ui.Hot(id) && enabled ? 1.f : 0.f, 14.f);
    if (enabled) g.GlowRound(b, 10.f, Palette::Teal, 18.f, 0.25f + 0.45f * glow);
    ButtonScope button(ui, id, b, { Palette::Teal, Colors::Transparent, 0.f, 10.f, { 0.f }, 0.f, b.h }, enabled);
    const float lw = g.Measure(label, bs).w;
    const float gx = b.CX() - (16.f + 10.f + lw) * 0.5f;
    Glyph(ui, glyph, { gx, b.y, 16.f, b.h }, 14.f, Rgba(3, 17, 13));
    g.Text(label, { gx + 26.f, b.y, lw + 2.f, b.h }, Rgba(3, 17, 13), bs);
    return button.Clicked();
}

// Text box in a mono-spaced chip (commands, folders)
float CodeBox(Ui& ui, float x, float y, float w, const std::wstring& text, Color accent, float size) {
    TextStyle style = Mono(size);
    style.wrap = true;
    style.valign = VAlign::Top;
    const float th = ui.g.Measure(text, style, w - 24.f).h;
    const Rect box{ x, y, w, th + 18.f };
    ui.g.Border(box, 8.f, accent.WithAlpha(0.10f), accent.WithAlpha(0.35f), 1.f);
    ui.g.Text(text, { x + 12.f, y + 9.f, w - 24.f, th }, accent, style);
    return box.h;
}

// Small label on a cover's top-left corner
void Badge(Ui& ui, const Rect& cover, const std::wstring& text, Color background, Color foreground) {
    const TextStyle style = Mono(9.f, true).Center();
    const float w = ui.g.Measure(text, style).w + 16.f;
    const Rect r{ cover.x + 8.f, cover.y + 8.f, w, 20.f };
    ui.g.FillRound(r, 10.f, background);
    ui.g.Text(text, r, foreground, style);
}

float SectionTitle(Ui& ui, float x, float y, float w, const std::wstring& title, const std::wstring& hint) {
    const float h = ui.g.Measure(L"Ag", Mono(13.f, true)).h;
    ui.g.FillRound({ x, y + h * 0.5f - 7.f, 3.f, 14.f }, 1.5f, Palette::Teal);
    ui.g.Text(title, { x + 12.f, y, w - 12.f, h }, kTitle, Mono(13.f, true));
    if (!hint.empty()) {
        const float tw = ui.g.Measure(title, Mono(13.f, true)).w;
        ui.g.Text(hint, { x + 12.f + tw + 14.f, y, w - tw - 26.f, h }, kDimText, Mono(10.f).Ellipsis());
    }
    return h + 14.f;
}

} // namespace SpeedrunLauncher::Main::StoreKit
