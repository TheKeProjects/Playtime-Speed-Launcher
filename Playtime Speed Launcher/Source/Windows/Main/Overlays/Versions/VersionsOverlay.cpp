#include "Windows/Main/Overlays/Versions/VersionsOverlay.h"
#include <algorithm>
#include <windows.h>
#include <shellapi.h>
#include <cmath>
#include <functional>
#include <vector>
#include "App/Launcher.h"
#include "App/Paths.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Graphics/Animation.h"
#include "Graphics/Images.h"
#include "Localization/Localization.h"
#include "Services/Audio/SoundEffects.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Dialogs/DialogParts.h"
#include "Windows/Dialogs/DialogWindow.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/Versions/StoreKit.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
using namespace StoreKit;
namespace P = Dialogs::Parts;

namespace {

// Round icon button over a cover (move / edit / delete): solid colour, glows and grows while
// hovered, with a tooltip. Drawn scaled, hit-tested at its real size so it never jitters.
bool CoverAction(Ui& ui, Id id, const Rect& r, wchar_t glyph, Color color, const std::wstring& tip, float alpha, bool enabled = true) {
    auto& g = ui.g;
    const bool hot = ui.Hot(id) && enabled;
    const float grow = ui.Animate(HashId(id, 1), hot ? 1.f : 0.f, 18.f);
    g.PushOpacity(alpha * (enabled ? 1.f : 0.4f));
    g.Shadow(r.Offset(0.f, 3.f), r.h * 0.5f, 6.f, Argb(0x90000000));
    if (grow > 0.01f) g.GlowRound(r, r.h * 0.5f, color, 12.f, 0.8f * grow);
    ui.PushScale(1.f + 0.14f * grow, r.CX(), r.CY());
    g.FillRound(r, r.h * 0.5f, enabled ? color.WithAlpha(0.88f + 0.12f * grow) : Rgba(60, 70, 80, 230));
    g.StrokeRound(r.Inset(0.5f), r.h * 0.5f - 0.5f, Rgba(255, 255, 255, static_cast<int>(70 + 120 * grow)), 1.f);
    Glyph(ui, glyph, r, 12.f, Colors::White);
    ui.PopTransform();
    g.PopLayer();
    const Interaction it = ui.Interact(id, r, enabled && alpha > 0.2f);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    if (enabled) ui.Tooltip(id, tip);
    return it.clicked;
}

// The folder an installation lives in, worked out from its exe: the packaged game folder (the one
// with an Engine folder next to the project), then up through parents that hold nothing else
// (e.g. "Patch 1.3\WindowsNoEditor\…" → "Patch 1.3"). Empty when it can't be told safely.
std::wstring InstallRoot(const std::wstring& exePath) {
    std::wstring dir = Files::DirectoryName(exePath);
    std::wstring package;
    for (std::wstring probe = dir; !probe.empty() && package.empty(); ) {
        if (Files::DirectoryExists(Files::Combine(probe, L"Engine"))) package = probe;
        const std::wstring up = Files::DirectoryName(probe);
        if (up == probe || up.size() < 4) break;
        probe = up;
    }
    // Not a packaged game (e.g. an exe on the Desktop): don't guess, never offer to delete
    if (package.empty()) return L"";
    std::wstring root = package;
    for (int i = 0; i < 2; ++i) {
        const std::wstring parent = Files::DirectoryName(root);
        if (parent.empty() || parent.size() < 4 || parent == root) break;
        if (Files::ListDirectories(parent).size() != 1 || !Files::ListFiles(parent).empty()) break;   // the parent holds other things
        root = parent;
    }
    // Never a drive root or a folder right under it
    const size_t depth = static_cast<size_t>(std::count(root.begin(), root.end(), L'\\'));
    if (root.size() < 4 || depth < 2) return L"";
    return root;
}

bool StartsWithFolder(const std::wstring& path, const std::wstring& folder) {
    const std::wstring p = Text::Lower(path), f = Text::Lower(folder) + L"\\";
    return p.compare(0, f.size(), f) == 0;
}

// To the Recycle Bin (Windows warns first if something is too big for it)
bool RecycleFolder(const std::wstring& folder) {
    std::wstring from = folder;
    from.push_back(L'\0');   // double-null terminated list
    SHFILEOPSTRUCTW op{};
    op.wFunc = FO_DELETE;
    op.pFrom = from.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI | FOF_WANTNUKEWARNING;
    return SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted;
}

} // namespace

VersionsOverlay::VersionsOverlay(MainWindow& window)
    : Overlay(window), installer(std::make_unique<PresetInstaller>(window)), store(std::make_unique<StoreOverlay>(window, *installer)) {
    installer->onInstalled = [this] { main.RefreshInfo(); };
    const auto chapterIndexOf = [](int chapterNumber) {
        const auto& chapters = App::L().chapters;
        for (size_t i = 0; i < chapters.size(); ++i)
            if (chapters[i].number == chapterNumber) return static_cast<int>(i);
        return -1;
    };
    // Downloaded: the store shows that version's page to name it and pick its folder
    installer->onReady = [this, chapterIndexOf](int chapterNumber, const std::wstring& manifestId) {
        const int index = chapterIndexOf(chapterNumber);
        if (index < 0) return;
        if (!popup.IsOpen()) Open(index);
        store->ShowReady(chapterNumber, manifestId);
    };
    installer->onNotFound = installer->onReady;   // same: go to that version's page in the store
    // Moved and registered: back to the versions menu, on that chapter
    installer->onFinished = [this, chapterIndexOf](int chapterNumber) {
        const int index = chapterIndexOf(chapterNumber);
        if (index < 0) return;
        chapterIndex = index;
        if (!popup.IsOpen()) Open(index);
        store->Close();   // the panel plays its entrance again as the store goes away
    };
}

void VersionsOverlay::Open(int index) {
    const auto& chapters = App::L().chapters;
    if (index < 0 || index >= static_cast<int>(chapters.size()) || !chapters[static_cast<size_t>(index)].isAvailable) return;   // not out yet
    chapterIndex = index;
    popup.Open();
    Reveal(40.f);
    revealAt += 0.1;   // after the panel's entrance
    switchedAt = -10.0;
}

void VersionsOverlay::Reveal(float fromX) {
    revealAt = Async::Now();
    revealFromX = fromX;
}

void VersionsOverlay::SwitchChapter(int delta) {
    // Next / previous released chapter (wrapping around; chapters that aren't out are skipped)
    const auto& chapters = App::L().chapters;
    const int count = static_cast<int>(chapters.size());
    if (count == 0) return;
    int index = chapterIndex;
    for (int step = 0; step < count; ++step) {
        index = (index + (delta < 0 ? -1 : 1) + count) % count;
        if (chapters[static_cast<size_t>(index)].isAvailable) break;
    }
    SwitchTo(index, delta);
}

void VersionsOverlay::SwitchTo(int index, int direction) {
    if (index == chapterIndex) return;
    if (drag.from >= 0) {
        if (drag.active) Services::Sfx::Stop();
        drag = Drag{};
    }
    pendingDelete.reset();
    chapterIndex = index;
    switchDir = direction < 0 ? -1.f : 1.f;
    switchedAt = Async::Now();
    Reveal(40.f * switchDir);   // the tiles come in from the arrow's side
}

void VersionsOverlay::Select(const std::optional<std::wstring>& exePath) {
    App::L().store.SetSelected(App::L().chapters[static_cast<size_t>(chapterIndex)].number, exePath);
    main.RefreshInfo();
}

void VersionsOverlay::AddManually() {
    auto& l = App::L();
    const std::wstring exe = System::PickFile(main.Hwnd(), L"", { { L"Executables", L"*.exe" } });
    if (exe.empty()) return;
    l.store.AddCustom(l.chapters[static_cast<size_t>(chapterIndex)].number, Files::FileNameWithoutExtension(exe), exe);
    main.RefreshInfo();
}

// ── Banner: chapter + copy in use, with the STORE button ────────────────────

void VersionsOverlay::DrawBanner(Ui& ui, const Rect& r, const std::wstring& inUse, size_t count) {
    auto& g = ui.g;
    const auto& ch = App::L().chapters[static_cast<size_t>(chapterIndex)];

    g.Shadow(r.Offset(0.f, 8.f), 12.f, 16.f, Argb(0x70000000));
    g.PushRoundClip(r, 12.f);
    g.FillRect(r, Hex(0x0A1420));
    const float artX = std::round(r.x + r.w * 0.25f);   // whole pixel: no half-covered seam column
    const Rect art{ artX, r.y, r.R() - artX, r.h };
    // After a chapter change the art settles from a slight zoom — clipped to its area, so the zoomed
    // image never spills past the gradient's edge (that showed as a split in the banner)
    const float settle = 1.f - Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>((ui.Time() - switchedAt) / 0.8)));
    if (settle > 0.f) ui.KeepAnimating();
    g.PushClip(art);
    DrawCoverImage(ui, BannerPath(ch.number), art, 1.f + 0.10f * settle, 2048);
    g.PopClip();
    // Solid over the art's left edge, so its antialiased border never shows as a thin line
    g.FillRect({ r.x, r.y, art.x - r.x + 3.f, r.h }, Hex(0x0A1420));
    g.FillLinear(art, 0.f, { { 0.f, Hex(0x0A1420) }, { 0.45f, Hex(0x0A1420, 0.6f) }, { 1.f, Hex(0x0A1420, 0.15f) } }, { 0.f, 0.f }, { 1.f, 0.f });
    g.FillLinear(r, 0.f, { { 0.f, Palette::Teal.WithAlpha(0.14f) }, { 0.6f, Palette::Teal.WithAlpha(0.f) } }, { 0.f, 1.f }, { 0.6f, 0.f });
    g.PopLayer();
    g.StrokeRound(r.Inset(0.5f), 12.f, Rgba(255, 255, 255, 24), 1.f);

    // Text block (left)
    const float x = r.x + 28.f, w = r.w * 0.5f;
    float y = r.y + 24.f;
    {
        const std::wstring tag = Loc::Fmt("store_chapter", ch.number);
        const TextStyle tagStyle = Mono(9.f, true).Center();
        const float tw = g.Measure(tag, tagStyle).w + 18.f;
        g.FillRound({ x, y, tw, 20.f }, 10.f, Palette::Teal.WithAlpha(0.18f));
        g.StrokeRound({ x + 0.5f, y + 0.5f, tw - 1.f, 19.f }, 9.5f, Palette::Teal.WithAlpha(0.6f), 1.f);
        g.Text(tag, { x, y, tw, 20.f }, Palette::Teal, tagStyle);
        y += 20.f + 12.f;
    }
    const TextStyle titleStyle = Mono(24.f, true).Ellipsis();
    const float titleH = g.Measure(L"Ag", titleStyle).h;
    const std::wstring title = ChapterTitle(ch);
    g.GlowText(title, { x, y, w, titleH }, titleStyle, Palette::Teal, 12.f, 0.35f);
    g.Text(title, { x, y, w, titleH }, kTitle, titleStyle);
    y += titleH + 4.f;
    g.Text(VersionsCount(count), { x, y, w, 18.f }, kDimText, Mono(11.f));
    y += 18.f + 14.f;
    // In use: ▶ name
    {
        const std::wstring label = Loc::Get("versions_in_use");
        const TextStyle ls = Mono(9.f, true);
        const float lw = g.Measure(label, ls).w;
        g.FillEllipse(x + 4.f, y + 9.f, 4.f, 4.f, Palette::Teal);
        g.Text(label, { x + 14.f, y, lw + 2.f, 18.f }, Palette::Teal, ls);
        g.Text(inUse, { x + 14.f + lw + 10.f, y, w - lw - 24.f, 18.f }, kTitle, Mono(11.f, true).Ellipsis());
    }

    // STORE (right): the loudest thing on the page
    const float bw = 250.f, bh = 64.f;
    const Rect b{ r.R() - 28.f - bw, r.CY() - bh * 0.5f, bw, bh };
    const Id id = ui.MakeId("versions-store");
    const float hover = ui.Animate(HashId(id, 1), ui.Hot(id) ? 1.f : 0.f, 14.f);
    g.GlowRound(b, 14.f, Palette::Teal, 24.f, 0.55f + 0.35f * hover);
    bool clicked = false;
    {
        ButtonScope button(ui, id, b.Offset(0.f, -2.f * hover), { Palette::Teal, Colors::Transparent, 0.f, 14.f, { 0.f }, 0.f, bh });
        const Rect inner = b.Offset(0.f, -2.f * hover);
        const Rect icon{ inner.x + 12.f, inner.CY() - 20.f, 40.f, 40.f };
        g.FillRound(icon, 20.f, Rgba(3, 17, 13, 60));
        Glyph(ui, 0xE7BF, icon, 17.f, Rgba(3, 17, 13));   // shopping cart
        const float tx = icon.R() + 14.f, tw = inner.R() - 14.f - tx;
        g.Text(Loc::Get("store_button"), { tx, inner.y + 12.f, tw, 22.f }, Rgba(3, 17, 13), Mono(16.f, true));
        g.Text(Loc::Get("versions_store_cta"), { tx, inner.y + 34.f, tw, 16.f }, Rgba(3, 17, 13, 200), Mono(9.5f, true).Ellipsis());
        g.Text(L"\x203A", { inner.R() - 28.f, inner.y, 16.f, inner.h }, Rgba(3, 17, 13), Mono(20.f, true));
        clicked = button.Clicked();
    }
    if (clicked) store->Open(chapterIndex);
}

// ── Tiles ───────────────────────────────────────────────────────────────────

void VersionsOverlay::DrawTile(Ui& ui, const Rect& slot, const Entry& entry, int index, int reveal, bool floating) {
    auto& g = ui.g;
    auto& l = App::L();
    const int chapterNumber = l.chapters[static_cast<size_t>(chapterIndex)].number;
    StaggerItem in(ui, revealAt, reveal, revealFromX, 0.f, 0.05);

    const Id tileId = ui.MakeId("version-tile", index);
    const Id delId = ui.MakeId("version-del", index), editId = ui.MakeId("version-edit", index);
    // The tile stays up (and its buttons visible) while the mouse is on one of its buttons; the
    // dragged copy is always up
    const bool onButton = floating || (!entry.isAuto && (ui.Hot(delId) || ui.Hot(editId) || ui.Active(delId) || ui.Active(editId)));
    // The copy in use stays lit (glow under the art, outline, badge)
    const float sel = ui.Animate(HashId(tileId, 9), entry.selected ? 1.f : 0.f, 14.f);
    const Tile t = CoverTile(ui, tileId, slot, entry.isAuto ? CoverPath(chapterNumber) : VersionCoverPath(chapterNumber, entry.name), true, sel, onButton);
    const bool tileHot = ui.HotId() == tileId;
    if (sel > 0.5f) Badge(ui, t.cover, L"\x2713  " + Loc::Get("versions_in_use"), Palette::Teal, Rgba(3, 17, 13));
    // Platform logo / custom icon on the bottom-left of the cover
    if (!entry.logo.empty()) {
        if (auto img = Images::Load(entry.logo, 128)) {
            const Rect logo{ t.cover.x + 10.f, t.cover.B() - 10.f - 36.f, 36.f, 36.f };
            g.Shadow(logo.Offset(0.f, 3.f), 9.f, 6.f, Argb(0x90000000));
            g.DrawImageRounded(*img, logo, 9.f);
            g.StrokeRound(logo.Inset(0.5f), 8.5f, Rgba(255, 255, 255, 60), 1.f);
        }
    }

    // Actions over the cover (custom installations): ◀ ▶  ✎ ✕, shown while hovered
    bool actionUsed = false;
    if (!entry.isAuto && !floating && !drag.active) {
        const float alpha = Anim::Clamp01(t.hover * 1.4f);
        if (alpha > 0.01f) {
            const float s = 34.f, gap = 8.f;
            // Edit + delete on the top-right corner
            float x = t.cover.R() - 10.f - s;
            const float y = t.cover.y + 10.f;
            if (CoverAction(ui, delId, { x, y, s, s }, 0xE74D, Rgba(225, 65, 65), Loc::Get("versions_delete"), alpha)) {   // trash
                AskDelete(entry);
                actionUsed = true;
            }
            x -= s + gap;
            if (CoverAction(ui, editId, { x, y, s, s }, 0xE70F, Rgba(55, 140, 235), Loc::Get("versions_edit"), alpha)) {   // pencil
                EditInstall(entry);
                actionUsed = true;
            }
        }
    }

    // Name + subtitle under the cover
    const float nameH = g.Measure(L"Ag", Mono(12.f).SemiBold()).h, subH = g.Measure(L"Ag", Mono(9.5f)).h;
    const float ty = slot.B() + 10.f;
    g.Text(entry.name, { slot.x + 2.f, ty, slot.w - 4.f, nameH }, entry.selected || t.hover > 0.5f ? Palette::Teal : kTitle,
           Mono(12.f).SemiBold().Ellipsis());
    g.Text(entry.subtitle, { slot.x + 2.f, ty + nameH + 2.f, slot.w - 4.f, subH }, kDimText, Mono(9.5f).Ellipsis());

    // Clicking the cover (not one of its buttons) selects it
    if (!floating && !dragEnded && !drag.active && !actionUsed && t.it.clicked && tileHot && !entry.selected)
        Select(entry.isAuto ? std::nullopt : std::optional<std::wstring>(entry.exePath));
}

void VersionsOverlay::DrawAddTile(Ui& ui, const Rect& slot, int reveal) {
    auto& g = ui.g;
    StaggerItem in(ui, revealAt, reveal, revealFromX, 0.f, 0.05);
    const Id id = ui.MakeId("version-add-tile");
    const Interaction it = ui.Interact(id, slot);
    if (it.hovered) ui.SetCursor(IDC_HAND);
    const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 14.f);
    const Rect r = slot.Offset(0.f, -5.f * hover);
    if (hover > 0.01f) g.GlowRound(r, kRadius, Palette::Teal, 16.f, 0.35f * hover);
    g.FillRound(r, kRadius, Palette::Teal.WithAlpha(0.04f + 0.06f * hover));
    // Dashed-looking border: short segments around the edge
    const Color edge = Palette::Teal.WithAlpha(0.35f + 0.5f * hover);
    for (float x = r.x + kRadius; x < r.R() - kRadius; x += 14.f) {
        g.Line(x, r.y + 0.5f, std::min(x + 7.f, r.R() - kRadius), r.y + 0.5f, edge, 1.f);
        g.Line(x, r.B() - 0.5f, std::min(x + 7.f, r.R() - kRadius), r.B() - 0.5f, edge, 1.f);
    }
    for (float y = r.y + kRadius; y < r.B() - kRadius; y += 14.f) {
        g.Line(r.x + 0.5f, y, r.x + 0.5f, std::min(y + 7.f, r.B() - kRadius), edge, 1.f);
        g.Line(r.R() - 0.5f, y, r.R() - 0.5f, std::min(y + 7.f, r.B() - kRadius), edge, 1.f);
    }
    const Rect plus{ r.CX() - 26.f, r.CY() - 40.f, 52.f, 52.f };
    g.FillRound(plus, 26.f, Palette::Teal.WithAlpha(0.12f + 0.2f * hover));
    g.StrokeRound(plus.Inset(0.5f), 25.5f, Palette::Teal.WithAlpha(0.6f + 0.4f * hover), 1.f);
    Glyph(ui, 0xE710, plus, 18.f, Palette::Teal);
    g.Text(Loc::Get("versions_add_manual"), { r.x + 10.f, plus.B() + 14.f, r.w - 20.f, 18.f }, Palette::Teal, Mono(11.f, true).Center());
    g.Text(Loc::Get("versions_add_hint"), { r.x + 10.f, plus.B() + 34.f, r.w - 20.f, 16.f }, kDimText, Mono(9.f).Center());
    if (it.clicked) AddManually();
}

float VersionsOverlay::DrawTiles(Ui& ui, float x, float y, float w) {
    auto& g = ui.g;
    auto& l = App::L();
    const auto& ch = l.chapters[static_cast<size_t>(chapterIndex)];
    const auto sel = l.store.SelectedPath(ch.number);
    const float top = y;

    // Entries: automatic first, then the user's installations
    std::vector<Entry> entries;
    {
        Entry e;
        e.name = Loc::Get("auto_name");
        e.isAuto = true;
        e.selected = !sel;
        if (!l.epic.IsEnabled()) {
            e.subtitle = Loc::Get("auto_subtitle");
            e.exePath = ch.gameExePath.value_or(L"");
            e.logo = Paths::Asset(L"Images\\Steam.jpg");
        } else {
            e.subtitle = Loc::Get("version_auto_epic");
            e.exePath = l.epic.ExePath(ch.number).value_or(L"");
            e.logo = Paths::Asset(L"Images\\Epic.png");
        }
        entries.push_back(e);
    }
    // Copy: tiles can remove / reorder installations while drawing
    const std::vector<Models::Installation> customs = l.store.Customs(ch.number);
    for (size_t i = 0; i < customs.size(); ++i) {
        Entry e;
        e.name = customs[i].name;
        e.subtitle = customs[i].exePath;
        e.exePath = customs[i].exePath;
        e.icon = customs[i].iconPath;
        if (e.icon && Files::Exists(*e.icon)) e.logo = *e.icon;
        e.selected = sel && *sel == customs[i].exePath;
        e.first = i == 0;
        e.last = i + 1 == customs.size();
        entries.push_back(e);
    }

    std::wstring inUse = entries.front().name;
    for (const auto& e : entries)
        if (e.selected) inUse = e.name;

    int reveal = 0;
    // Switching chapter: the new page slides in from the arrow's side
    const float sp = Anim::Clamp01(static_cast<float>((ui.Time() - switchedAt) / 0.38));
    const bool sliding = sp < 1.f;
    if (sliding) {
        g.PushOpacity(Anim::EaseOutCubic(std::min(1.f, sp * 1.6f)));
        ui.PushTranslate((1.f - Anim::EaseOutCubic(sp)) * 70.f * switchDir, 0.f);
        ui.KeepAnimating();
    }
    {
        StaggerItem in(ui, revealAt, reveal++, revealFromX);
        DrawBanner(ui, { x, y, w, 190.f }, inUse, entries.size());
    }
    y += 190.f + 26.f;
    {
        StaggerItem in(ui, revealAt, reveal++, revealFromX);
        y += SectionTitle(ui, x, y, w, Loc::Get("versions_title"), Loc::Get("versions_hint"));
    }

    const int columns = static_cast<int>(kColumns);
    const float tileW = (w - kGap * (kColumns - 1.f)) / kColumns, coverH = tileW * kCoverAspect;
    const float nameH = g.Measure(L"Ag", Mono(12.f).SemiBold()).h, subH = g.Measure(L"Ag", Mono(9.5f)).h;
    const float rowH = coverH + 10.f + nameH + 2.f + subH + 24.f;
    const auto slotAt = [&](int position) {
        return Rect{ x + (position % columns) * (tileW + kGap), y + (position / columns) * rowH, tileW, coverH };
    };
    const int customCount = static_cast<int>(entries.size()) - 1;   // entries[0] is the automatic one
    const float mx = ui.MouseX(), my = ui.MouseY();
    dragEnded = false;

    // ── Drag to reorder the custom installations ──
    if (drag.from >= 0 && (drag.from >= customCount || entries[static_cast<size_t>(drag.from) + 1].exePath != drag.exe)) {   // list changed
        if (drag.active) Services::Sfx::Stop();
        drag = Drag{};
    }
    if (drag.from < 0 && ui.In().pressed[0] && !store->Visible()) {
        for (int i = 0; i < customCount; ++i) {
            if (ui.HotId() != ui.MakeId("version-tile", i + 1)) continue;   // pressed on the tile itself (not one of its buttons)
            const Rect s = slotAt(i + 1);
            drag = Drag{ entries[static_cast<size_t>(i) + 1].exePath, i, mx - s.x, my - s.y, mx, my, false, 0.0, false };
        }
    }
    int target = drag.from;
    if (drag.from >= 0) {
        if (!drag.active && ui.In().down[0] && std::hypot(mx - drag.startX, my - drag.startY) > 6.f) {
            drag.active = true;
            drag.grabbedAt = ui.Time();
            Services::Sfx::Play(Services::Sfx::Grab);
        }
        // Once the grab sound is over, it trembles while held
        if (drag.active && !drag.trembling && ui.Time() - drag.grabbedAt > 0.14) {
            drag.trembling = true;
            Services::Sfx::PlayLoop(Services::Sfx::Drag);
        }
        if (drag.active) {
            // Drop slot: the custom slot nearest to the dragged tile's centre
            const float cx = mx - drag.grabX + tileW * 0.5f, cy = my - drag.grabY + coverH * 0.5f;
            float best = 1e30f;
            for (int i = 0; i < customCount; ++i) {
                const Rect s = slotAt(i + 1);
                const float dist = (s.CX() - cx) * (s.CX() - cx) + (s.CY() - cy) * (s.CY() - cy);
                if (dist < best) {
                    best = dist;
                    target = i;
                }
            }
            ui.SetCursor(IDC_SIZEALL);
            ui.KeepAnimating();
        }
    }
    // Display order: the dragged one sits at its drop slot, the others make room
    std::vector<int> order;
    for (int i = 0; i < customCount; ++i) order.push_back(i);
    if (drag.active) {
        order.erase(order.begin() + drag.from);
        order.insert(order.begin() + target, drag.from);
    }

    // Tiles glide to their place (positions remembered per installation)
    const auto glide = [&](const std::wstring& key, const Rect& to) {
        const int64_t h = static_cast<int64_t>(std::hash<std::wstring>{}(key));
        return Rect{ ui.Animate(ui.MakeId("version-x", h), to.x, 16.f), ui.Animate(ui.MakeId("version-y", h), to.y, 16.f), to.w, to.h };
    };
    DrawTile(ui, slotAt(0), entries[0], 0, reveal++);
    for (int k = 0; k < customCount; ++k) {
        const int ci = order[static_cast<size_t>(k)];
        const Entry& entry = entries[static_cast<size_t>(ci) + 1];
        const Rect s = slotAt(k + 1);
        if (drag.active && ci == drag.from) {
            // Where it will land
            StaggerItem in(ui, revealAt, reveal++, revealFromX, 0.f, 0.05);
            g.FillRound(s, kRadius, Palette::Teal.WithAlpha(0.08f));
            g.StrokeRound(s.Inset(1.f), kRadius, Palette::Teal.WithAlpha(0.7f), 2.f);
            continue;
        }
        DrawTile(ui, glide(entry.exePath, s), entry, ci + 1, reveal++);
    }
    int position = customCount + 1;
    DrawAddTile(ui, slotAt(position), reveal++);
    ++position;

    // The dragged copy follows the mouse, on top of everything
    if (drag.active) {
        const Entry& entry = entries[static_cast<size_t>(drag.from) + 1];
        // Held tile: follows the mouse and trembles a little (in time with the sound)
        const double held = ui.Time() - drag.grabbedAt;
        const float shake = drag.trembling ? 1.f : 0.f;
        const double tau = 2.0 * 3.14159265;
        // Slow, smooth sway (a gentle drift plus the sound's ~4 Hz wobble)
        const float jx = shake * static_cast<float>(0.7 * std::sin(held * tau * 1.3) + 0.25 * std::sin(held * tau * 4.0));
        const float jy = shake * static_cast<float>(0.5 * std::cos(held * tau * 0.9) + 0.15 * std::sin(held * tau * 4.0 + 1.0));
        const Rect f{ mx - drag.grabX + jx, my - drag.grabY + jy, tileW, coverH };
        const int64_t h = static_cast<int64_t>(std::hash<std::wstring>{}(entry.exePath));
        ui.SetValue(ui.MakeId("version-x", h), f.x);   // on drop it glides from here to its slot
        ui.SetValue(ui.MakeId("version-y", h), f.y);
        ui.PushScale(1.06f, f.CX(), f.CY());
        g.GlowRound(f, kRadius, Palette::Teal, 26.f, 0.6f);
        DrawTile(ui, f, entry, drag.from + 1, 0, true);
        ui.PopTransform();
    }

    // Drop
    if (drag.from >= 0 && !ui.In().down[0]) {
        if (drag.active && target != drag.from) {
            const int chapterNumber = ch.number;
            const int step = target > drag.from ? 1 : -1;
            for (int k = drag.from; k != target; k += step) l.store.MoveCustom(chapterNumber, drag.exe, step);
            main.RefreshInfo();
        }
        if (drag.active) Services::Sfx::Play(Services::Sfx::Drop);   // also ends the trembling loop
        dragEnded = drag.active;
        drag = Drag{};
    }

    if (sliding) {
        ui.PopTransform();
        g.PopLayer();
    }
    y += ((position + columns - 1) / columns) * rowH;
    return y - top;
}

void VersionsOverlay::AskDelete(const Entry& entry) {
    auto& l = App::L();
    PendingDelete p;
    p.chapterNumber = l.chapters[static_cast<size_t>(chapterIndex)].number;
    p.name = entry.name;
    p.exePath = entry.exePath;
    p.folder = InstallRoot(entry.exePath);
    p.shownAt = Async::Now();
    const std::wstring lower = Text::Lower(p.folder);
    if (p.folder.empty() || !Files::DirectoryExists(p.folder)) {
        p.blockedReason = Loc::Get("versions_delete_blocked_root");
    } else if (lower.find(L"\\steamapps\\") != std::wstring::npos || lower.find(L"\\epic games\\") != std::wstring::npos) {
        p.blockedReason = Loc::Get("versions_delete_blocked_steam");   // the store's own copy of the game
    } else {
        // Another registered installation inside this folder (any chapter): leave the files alone
        for (const auto& ch : l.chapters)
            for (const auto& c : l.store.Customs(ch.number))
                if (!Text::Equals(c.exePath, entry.exePath, true) && StartsWithFolder(c.exePath, p.folder)) p.blockedReason = Loc::Get("versions_delete_blocked_shared");
    }
    pendingDelete = p;
}

void VersionsOverlay::DrawDeleteConfirm(Ui& ui, const Rect& panel) {
    auto& g = ui.g;
    const PendingDelete p = *pendingDelete;
    const float t = Anim::Clamp01(static_cast<float>((Async::Now() - p.shownAt) / 0.3));
    if (t < 1.f) ui.KeepAnimating();
    const float fade = Anim::EaseOutCubic(std::min(1.f, t * 1.6f));

    // Dim the panel and block everything under the card
    g.PushOpacity(fade);
    g.FillRound(panel, 10.f, Argb(0xB8000000));
    ui.Block(panel);

    TextStyle body = Mono(10.5f);
    body.wrap = true;
    body.valign = VAlign::Top;
    const float cw = 560.f, innerW = cw - 48.f;
    const std::wstring question = Loc::Get("versions_delete_question");
    const std::wstring note = p.blockedReason.empty() ? Loc::Get("versions_delete_bin") : p.blockedReason;
    const float qh = g.Measure(question, body, innerW).h;
    TextStyle pathStyle = Mono(9.5f);
    pathStyle.wrap = true;
    pathStyle.valign = VAlign::Top;
    const float ph = p.folder.empty() ? 0.f : g.Measure(p.folder, pathStyle, innerW - 24.f).h + 18.f;
    const float nh = g.Measure(note, Mono(9.5f).Ellipsis(), innerW).h;
    const float ch = 24.f + 44.f + 14.f + qh + 14.f + ph + (ph > 0.f ? 10.f : 0.f) + nh + 22.f + 44.f + 10.f + 30.f + 18.f;
    const Rect card = panel.Center(cw, ch);
    ui.PushScale(Anim::Lerp(0.92f, 1.f, Anim::EaseOutBack(t, 0.7f)), card.CX(), card.CY());
    ui.Block(card);
    g.Shadow(card.Offset(0.f, 12.f), 14.f, 26.f, Argb(0xA0000000));
    g.GlowRound(card, 14.f, Rgba(225, 65, 65), 22.f, 0.35f);
    g.Border(card, 14.f, Hex(0x0B1622), Rgba(225, 65, 65, 150), 1.f);

    float y = card.y + 24.f;
    const float x = card.x + 24.f;
    // Trash icon + title + version name
    const Rect icon{ x, y, 44.f, 44.f };
    g.FillRound(icon, 22.f, Rgba(225, 65, 65, 40));
    g.StrokeRound(icon.Inset(0.5f), 21.5f, Rgba(225, 65, 65, 200), 1.f);
    Glyph(ui, 0xE74D, icon, 18.f, Rgba(240, 100, 100));
    g.Text(Loc::Get("versions_delete_title"), { icon.R() + 14.f, y + 2.f, innerW - 58.f, 22.f }, kTitle, Mono(14.f, true));
    g.Text(p.name, { icon.R() + 14.f, y + 24.f, innerW - 58.f, 18.f }, Rgba(240, 120, 120), Mono(11.f, true).Ellipsis());
    y += 44.f + 14.f;
    g.Text(question, { x, y, innerW, qh }, Rgba(175, 198, 218, 235), body);
    y += qh + 14.f;
    if (ph > 0.f) {
        const Rect box{ x, y, innerW, ph };
        g.Border(box, 8.f, Rgba(225, 65, 65, 18), Rgba(225, 65, 65, 70), 1.f);
        Glyph(ui, 0xE8B7, { box.x + 8.f, box.y + 6.f, 14.f, 14.f }, 11.f, Rgba(240, 120, 120));   // folder
        g.Text(p.folder, { box.x + 28.f, box.y + 9.f, innerW - 40.f, ph - 18.f }, Rgba(235, 170, 170), pathStyle);
        y += ph + 10.f;
    }
    g.Text(note, { x, y, innerW, nh }, p.blockedReason.empty() ? kDimText : kAmber, Mono(9.5f).Ellipsis());
    y += nh + 22.f;

    // Actions: delete files too (red) · only from the list · cancel
    enum class Choice { None, Files, ListOnly, Cancel } choice = Choice::None;
    const float half = (innerW - 12.f) * 0.5f;
    {
        const Rect b{ x, y, half, 44.f };
        const bool enabled = p.blockedReason.empty();
        const Id id = ui.MakeId("delete-files");
        const float hover = ui.Animate(HashId(id, 1), ui.Hot(id) && enabled ? 1.f : 0.f, 14.f);
        if (enabled) g.GlowRound(b, 10.f, Rgba(225, 65, 65), 16.f, 0.25f + 0.45f * hover);
        ButtonScope button(ui, id, b, { Rgba(215, 55, 55), Colors::Transparent, 0.f, 10.f, { 0.f }, 0.f, 44.f }, enabled);
        const std::wstring label = Loc::Get("versions_delete_files");
        const TextStyle ls = Mono(10.5f, true);
        const float lw = g.Measure(label, ls).w;
        const float gx = b.CX() - (14.f + 8.f + lw) * 0.5f;
        Glyph(ui, 0xE74D, { gx, b.y, 14.f, b.h }, 12.f, Colors::White);
        g.Text(label, { gx + 22.f, b.y, lw + 2.f, b.h }, Colors::White, ls);
        if (button.Clicked()) choice = Choice::Files;
    }
    {
        const Rect b{ x + half + 12.f, y, half, 44.f };
        if (TextButton(ui, ui.MakeId("delete-list"), b, Loc::Get("versions_delete_list_only"), Mono(10.5f, true).Center(), kTitle,
                       { Rgba(255, 255, 255, 14), Rgba(255, 255, 255, 60), 1.f, 10.f, { 0.f }, 0.f, 44.f }))
            choice = Choice::ListOnly;
    }
    y += 44.f + 10.f;
    {
        const std::wstring cancel = Loc::Get("cancel");
        const TextStyle cs = Mono(10.f, true).Center();
        const float bw = g.Measure(cancel, cs).w + 40.f;
        if (TextButton(ui, ui.MakeId("delete-cancel"), { card.CX() - bw * 0.5f, y, bw, 30.f }, cancel, cs, kDimText, Styles::Flat()))
            choice = Choice::Cancel;
    }
    ui.PopTransform();
    g.PopLayer();

    if (ui.KeyPressed(VK_ESCAPE)) choice = Choice::Cancel;
    if (choice == Choice::None) return;
    pendingDelete.reset();
    if (choice == Choice::Cancel) return;
    auto& l = App::L();
    l.store.RemoveCustom(p.chapterNumber, p.exePath);
    main.RefreshInfo();
    if (choice == Choice::Files) {
        const std::wstring folder = p.folder;
        HWND owner = main.Hwnd();
        Async::Run([folder] { return RecycleFolder(folder); }, [folder, owner](bool ok) {
            if (ok) return;
            Dialogs::Options o;
            o.title = Loc::Get("error_title");
            o.close = Loc::Get("close");
            Dialogs::ShowText(owner, o, Loc::Get("error_delete_files") + L"\n" + folder, Rgba(160, 180, 200));
        });
    }
}

void VersionsOverlay::EditInstall(const Entry& entry) {
    struct State {
        std::wstring name;
        std::optional<std::wstring> icon;
    };
    auto state = std::make_shared<State>();
    state->name = entry.name;
    state->icon = entry.icon;
    const int chapterNumber = App::L().chapters[static_cast<size_t>(chapterIndex)].number;
    Dialogs::Options o;
    o.title = Loc::Get("edit_install_title");
    o.primary = Loc::Get("save_btn");
    o.close = Loc::Get("cancel");
    o.contentWidth = 260.f;
    Dialogs::Show(main.Hwnd(), o, [state](Ui& ui, const Rect& area) {
        Column col(area);
        P::Label(ui, col, Loc::Get("edit_install_title"), 10.f, Rgba(0, 204, 170, 180), true, 8.f);
        P::Input(ui, ui.MakeId("edit-name"), col, state->name);
        if (P::SmallButton(ui, ui.MakeId("edit-icon"), col.Next(28.f), Loc::Get("icon_btn"), Rgba(80, 170, 230))) {
            const std::wstring file = System::PickFile(GetActiveWindow(), L"", { { L"Images", L"*.jpg;*.jpeg;*.png;*.bmp" } });
            if (!file.empty()) state->icon = file;
        }
        return col.Height();
    }, [this, state, entry, chapterNumber](Dialogs::Result r) {
        if (r != Dialogs::Result::Primary) return;
        const std::wstring name = Text::Trim(state->name);
        App::L().store.UpdateCustom(chapterNumber, entry.exePath, name.empty() ? entry.name : name, state->icon);
        main.RefreshInfo();
    });
}

// ── Panel ───────────────────────────────────────────────────────────────────

void VersionsOverlay::DrawHeader(Ui& ui, const Rect& header) {
    auto& g = ui.g;
    const Rect row = header.Inset(20.f, 0.f);
    // Icon + VERSIONS
    const Rect icon{ row.x, row.CY() - 17.f, 34.f, 34.f };
    g.FillRound(icon, 17.f, Palette::Teal.WithAlpha(0.14f));
    g.StrokeRound(icon.Inset(0.5f), 16.5f, Palette::Teal.WithAlpha(0.55f), 1.f);
    Glyph(ui, 0xE8F1, icon, 15.f, Palette::Teal);   // library
    const TextStyle titleStyle = Mono(16.f, true);
    const std::wstring title = Loc::Get("versions_title");
    const Rect titleRect{ icon.R() + 12.f, row.y, g.Measure(title, titleStyle).w + 2.f, row.h };
    g.GlowText(title, titleRect, titleStyle, Palette::Teal, 10.f, 0.45f);
    g.Text(title, titleRect, Palette::Teal, titleStyle);

    // Chapter switcher (centre): ‹ [1][2][3][4][5] › — one chip per chapter with its cover; the
    // current one opens up to show its name, the others are dimmed thumbnails with their number
    auto& l = App::L();
    const int count = static_cast<int>(l.chapters.size());
    const float chipH = 34.f, thumb = 26.f, gap = 6.f, arrowS = 30.f;
    const TextStyle chipText = Mono(10.5f, true);
    const auto chipLabel = [&](int i) {
        const auto& c = l.chapters[static_cast<size_t>(i)];
        return Loc::Fmt("store_chapter", c.number) + L"  \x00B7  " + ChapterTitle(c);
    };
    std::vector<float> widths(static_cast<size_t>(count));
    float total = 0.f;
    const auto shown = [&](int i) { return l.chapters[static_cast<size_t>(i)].isAvailable; };   // not-yet-released chapters get no chip
    for (int i = 0; i < count; ++i) {
        if (!shown(i)) continue;
        const float open = 4.f + thumb + 10.f + g.Measure(chipLabel(i), chipText).w + 16.f;
        widths[static_cast<size_t>(i)] = ui.Animate(ui.MakeId("versions-chip-w", i), i == chapterIndex ? open : 4.f + thumb + 4.f, 15.f);
        total += widths[static_cast<size_t>(i)] + (total > 0.f ? gap : 0.f);
    }
    const float barW = arrowS + 10.f + total + 10.f + arrowS;
    const float cy = row.CY();
    float x = row.CX() - barW * 0.5f;
    const auto arrowButton = [&](const char* name, float ax, wchar_t glyph, int delta) {
        const Rect r{ ax, cy - arrowS * 0.5f, arrowS, arrowS };
        const Id id = ui.MakeId(name);
        const float hover = ui.Animate(HashId(id, 1), ui.Hot(id) ? 1.f : 0.f, 16.f);
        if (hover > 0.01f) g.GlowRound(r, arrowS * 0.5f, Palette::Teal, 10.f, 0.7f * hover);
        ButtonScope button(ui, id, r, { Palette::Teal.WithAlpha(0.85f + 0.15f * hover), Colors::Transparent, 0.f, arrowS * 0.5f, { 0.f }, 0.f, arrowS });
        Glyph(ui, glyph, r, 12.f, Rgba(3, 17, 13));
        if (button.Clicked()) SwitchChapter(delta);
    };
    arrowButton("versions-prev", x, 0xE76B, -1);   // chevron left
    x += arrowS + 10.f;
    for (int i = 0; i < count; ++i) {
        if (!shown(i)) continue;
        const auto& c = l.chapters[static_cast<size_t>(i)];
        const bool current = i == chapterIndex;
        const Rect r{ x, cy - chipH * 0.5f, widths[static_cast<size_t>(i)], chipH };
        const Id id = ui.MakeId("versions-chip", i);
        const Interaction it = ui.Interact(id, r);
        const float sel = ui.Animate(HashId(id, 2), current ? 1.f : 0.f, 14.f);
        const float hover = ui.Animate(HashId(id, 1), it.hovered && !current ? 1.f : 0.f, 16.f);
        if (it.hovered && !current) {
            ui.SetCursor(IDC_HAND);
            ui.Tooltip(id, chipLabel(i));
        }
        if (sel > 0.01f) g.GlowRound(r, chipH * 0.5f, Palette::Teal, 12.f, 0.45f * sel);
        g.Border(r, chipH * 0.5f, Palette::Teal.WithAlpha(0.05f + 0.13f * sel + 0.07f * hover),
                 Palette::Teal.WithAlpha(0.18f + 0.62f * sel + 0.35f * hover), 1.f);
        const Rect th{ r.x + 4.f, cy - thumb * 0.5f, thumb, thumb };
        if (auto img = Images::Load(CoverPath(c.number), 128)) g.DrawImageRounded(*img, th, thumb * 0.5f);
        const float dim = (1.f - sel) * (1.f - 0.6f * hover);
        if (dim > 0.01f) {
            g.FillRound(th, thumb * 0.5f, Rgba(0, 0, 0, static_cast<int>(150 * dim)));
            g.Text(std::to_wstring(c.number), th, Rgba(255, 255, 255, static_cast<int>(235 * dim)), Mono(11.f, true).Center());
        }
        g.StrokeRound(th.Inset(0.5f), thumb * 0.5f - 0.5f, Palette::Teal.WithAlpha(0.3f + 0.7f * std::max(sel, hover)), 1.f);
        if (r.w > thumb + 16.f) {   // the name, revealed as the chip opens
            g.PushClip({ th.R() + 4.f, r.y, std::max(0.f, r.R() - th.R() - 12.f), r.h });
            g.Text(chipLabel(i), { th.R() + 10.f, r.y, 400.f, r.h }, kTitle.Fade(sel), chipText);
            g.PopClip();
        }
        if (it.clicked && !current) SwitchTo(i, i > chapterIndex ? 1 : -1);
        x += r.w + gap;
    }
    arrowButton("versions-next", x - gap + 10.f, 0xE76C, 1);   // chevron right

    if (TextButton(ui, ui.MakeId("versions-close-x"), { row.R() - 30.f, row.CY() - 15.f, 30.f, 30.f }, L"\x2715", Mono(12.f, true).WithFont(Font::Ui).Center(),
                   Palette::Dim, { Palette::ButtonBg, Palette::ButtonBorder, 1.f, 6.f, { 0.f }, 0.f, 30.f }))
        popup.Close();
}

void VersionsOverlay::Draw(Ui& ui, const Rect& screen) {
    // Back from the store: this panel plays its entrance again (it wasn't drawn while covered) and
    // the page slides in
    if (storeWasOpen && !store->IsOpen()) {
        Reveal(-40.f);
        revealAt += 0.08;
    }
    storeWasOpen = store->IsOpen();
    if (popup.Visible() && !store->Covering()) DrawPanel(ui, screen);
    if (store->Visible()) store->Draw(ui, screen);
}

void VersionsOverlay::DrawPanel(Ui& ui, const Rect& screen) {
    auto& l = App::L();
    if (l.chapters.empty()) return;
    chapterIndex = std::clamp(chapterIndex, 0, static_cast<int>(l.chapters.size()) - 1);
    if (!l.chapters[static_cast<size_t>(chapterIndex)].isAvailable) {
        // Never show a chapter that isn't out yet
        for (size_t i = 0; i < l.chapters.size(); ++i)
            if (l.chapters[i].isAvailable) {
                chapterIndex = static_cast<int>(i);
                break;
            }
    }
    // Keys (the store on top handles its own)
    if (popup.IsOpen() && !store->Visible() && !pendingDelete && ui.Focus() == 0) {
        if (ui.KeyPressed(VK_ESCAPE)) popup.Close();
        else if (ui.KeyPressed(VK_LEFT)) SwitchChapter(-1);
        else if (ui.KeyPressed(VK_RIGHT)) SwitchChapter(1);
    }

    if (!popup.IsOpen() && drag.from >= 0) {   // closed mid-drag
        if (drag.active) Services::Sfx::Stop();
        drag = Drag{};
    }

    PanelStyle style;
    style.headerHeight = 60.f;
    style.radius = 10.f;
    OverlayFrame frame(ui, ui.MakeId("versions"), screen, 1080.f, 720.f, style, popup.Opacity(), popup.Scale());
    DrawHeader(ui, frame.header);
    {
        ScrollArea scroll(ui, ui.MakeId("versions-scroll", chapterIndex), frame.body, false, 60.f);
        const float x = frame.body.x + kPad, w = scroll.ContentWidth() - kPad * 2.f;
        scroll.SetContentSize(DrawTiles(ui, x, frame.body.y + kPad, w) + kPad * 2.f);
    }

    // Footer: + ADD MANUALLY (left), BACK (right)
    const Rect footer = frame.footer.Inset(20.f, 0.f);
    const TextStyle text = Mono(11.f, true);
    {
        const std::wstring add = Loc::Get("versions_add_manual");
        const float aw = ui.g.Measure(add, text).w + 16.f + 10.f + 36.f;
        const Rect ar{ footer.x, footer.CY() - 18.f, std::max(170.f, aw), 36.f };
        ButtonScope b(ui, ui.MakeId("versions-add"), ar, { Palette::Teal.WithAlpha(0.10f), Palette::Teal.WithAlpha(0.6f), 1.f, 6.f, { 0.f }, 0.f, 36.f });
        const float cw = 16.f + 10.f + ui.g.Measure(add, text).w;
        const float cx = ar.CX() - cw * 0.5f;
        Glyph(ui, 0xE710, { cx, ar.y, 16.f, ar.h }, 12.f, Palette::Teal);
        ui.g.Text(add, { cx + 26.f, ar.y, cw - 26.f + 2.f, ar.h }, Palette::Teal, text);
        if (b.Clicked()) AddManually();
    }
    const std::wstring back = Loc::Get("back");
    const TextStyle backStyle = Mono(11.f, true).Center();
    const float bw = ButtonWidth(ui, back, backStyle, Styles::Footer());
    if (TextButton(ui, ui.MakeId("versions-close"), { footer.R() - bw, footer.CY() - 18.f, bw, 36.f }, back, backStyle, Palette::Dim, Styles::Footer()))
        popup.Close();
    if (pendingDelete) DrawDeleteConfirm(ui, frame.panel);
    if (frame.BackdropClicked() && !store->Visible() && !pendingDelete) popup.Close();
}

} // namespace SpeedrunLauncher::Main
