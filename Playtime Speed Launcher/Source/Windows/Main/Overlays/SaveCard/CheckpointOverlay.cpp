#include "Windows/Main/Overlays/SaveCard/CheckpointOverlay.h"
#include <algorithm>
#include <cmath>
#include "Localization/Localization.h"
#include "UI/Layout.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
namespace K = UI::MenuKit;

namespace {

constexpr float kRowH = 44.f, kRowGap = 6.f, kMaxListH = 340.f;

// "[3] Mini Huggies" -> "3", "Mini Huggies" (unnumbered folders keep their whole name)
std::pair<std::wstring, std::wstring> SplitName(const std::wstring& name) {
    if (name.size() > 2 && name[0] == L'[') {
        const size_t close = name.find(L']');
        if (close != std::wstring::npos) {
            std::wstring rest = name.substr(close + 1);
            while (!rest.empty() && rest[0] == L' ') rest.erase(0, 1);
            return { name.substr(1, close - 1), rest };
        }
    }
    return { L"", name };
}

} // namespace

void CheckpointOverlay::Open(int chapterNumber) {
    chapter = chapterNumber;
    checkpoints = Services::SaveGames::Checkpoints(chapterNumber);
    resetScroll = true;
    focused = -1;
    popup.Open();
}

void CheckpointOverlay::Load(size_t index) {
    if (index >= checkpoints.size()) return;
    Services::SaveGames::LoadCheckpoint(checkpoints[index]);
    popup.Close();
}

// The chapter's heading and how many checkpoints it has, then the list: a numbered badge per
// checkpoint, rows that light up and slide while hovered (or picked with the arrows)
void CheckpointOverlay::Draw(Ui& ui, const Rect& screen) {
    auto& g = ui.g;
    PanelStyle style;
    style.title = Loc::Get("checkpoint_select_header");
    OverlayFrame frame(ui, ui.MakeId("checkpoints"), screen, 460.f, 0.f, style, popup.Opacity(), popup.Scale());
    const double start = frame.OpenedAt() + 0.15;
    const Id scrollId = ui.MakeId("checkpoints-scroll");
    Column col(frame.body.x + 24.f, frame.body.y + 20.f, frame.body.w - 48.f);
    {
        StaggerItem reveal(ui, start, 0, 0.f, 10.f);
        const Rect hero = col.Next(K::kHeroH, 16.f);
        const std::wstring count = Loc::Fmt("checkpoint_count", static_cast<int>(checkpoints.size()));
        const float cw = g.Measure(count, Mono(9.f, true)).w + 18.f;
        K::Hero(ui, hero.CutRight(cw + 10.f), Glyphs::Save, Loc::Fmt("checkpoint_hero_title", chapter), Loc::Get("checkpoint_hero_sub"), Palette::Teal);
        K::Chip(ui, hero.R() - cw, hero.CY(), count, Palette::Teal);
    }

    // Keyboard: arrows move the selection (and keep it in view), Enter loads it
    const int count = static_cast<int>(checkpoints.size());
    if (popup.IsOpen() && count > 0) {
        int moved = 0;
        if (ui.KeyPressed(VK_DOWN)) moved = 1;
        else if (ui.KeyPressed(VK_UP)) moved = -1;
        if (moved) {
            focused = focused < 0 ? (moved > 0 ? 0 : count - 1) : (focused + moved + count) % count;
            const float top = focused * (kRowH + kRowGap);
            ScrollArea::ScrollTo(ui, scrollId, std::max(0.f, top - kMaxListH * 0.5f + kRowH * 0.5f));
        }
        if (focused >= 0 && ui.KeyPressed(VK_RETURN)) Load(static_cast<size_t>(focused));
    }

    const float listH = count == 0 ? 0.f : kRowH * count + kRowGap * (count - 1);
    if (count == 0) {
        StaggerItem reveal(ui, start, 1, 0.f, 10.f);
        const std::wstring text = Loc::Get("checkpoint_empty");
        const float h = K::NoteHeight(ui, text, col.w);
        K::Note(ui, col.Next(h), Glyphs::Info, text, K::kAmber);
    } else {
        // The list has a few px of room on each side so the hover glow isn't clipped
        const float viewH = std::min(listH, kMaxListH) + 8.f;
        const Rect viewport{ col.x - 6.f, col.y, col.w + 12.f, viewH };
        col.Gap(viewH);
        if (resetScroll) {
            ScrollArea::ScrollTo(ui, scrollId, 0.f);
            resetScroll = false;
        }
        ScrollArea scroll(ui, scrollId, viewport, false, 48.f, ScrollArea::Bar::Classic);
        const float x = viewport.x + 6.f, w = scroll.ContentWidth() - 12.f;
        float y = viewport.y + 4.f;
        int clicked = -1;
        for (int i = 0; i < count; ++i) {
            const Rect slot{ x, y, w, kRowH };
            y += kRowH + kRowGap;
            StaggerItem reveal(ui, start + 0.06, i, 22.f);
            const Id id = ui.MakeId("checkpoint", i);
            const Interaction it = ui.Interact(id, slot);
            if (it.hovered) {
                ui.SetCursor(IDC_HAND);
                if (ui.In().mouseMoved) focused = i;
            }
            const bool lit = it.hovered || focused == i;
            const float hover = ui.Animate(HashId(id, 0xC4E1), lit ? 1.f : 0.f, 16.f, 0.f);
            const Rect r = slot.Offset(4.f * hover, 0.f);
            if (hover > 0.01f) g.GlowRound(r, 8.f, Palette::Teal, 14.f, 0.3f * hover);
            g.Border(r, 8.f, Lerp(Palette::OptionBg, Rgba(0, 40, 30), hover), Lerp(Palette::ButtonBorder, Palette::Teal.WithAlpha(0.8f), hover), 1.f);
            if (hover > 0.01f) g.FillRound({ r.x + 1.f, r.y + 8.f, 3.f * hover, r.h - 16.f }, 1.5f, Palette::Teal);

            const auto [number, name] = SplitName(checkpoints[static_cast<size_t>(i)].name);
            const Rect badge{ r.x + 10.f, r.CY() - 13.f, 26.f, 26.f };
            g.Border(badge, 13.f, Palette::Teal.WithAlpha((20.f + 40.f * hover) / 255.f), Palette::Teal.WithAlpha((70.f + 130.f * hover) / 255.f), 1.f);
            if (number.empty()) Glyph(ui, Glyphs::Save, badge, 11.f, Palette::Teal);
            else g.Text(number, badge, Palette::Teal, Mono(10.f, true).Center());
            g.Text(name, { badge.R() + 12.f, r.y, r.R() - 40.f - badge.R() - 12.f, r.h }, Lerp(Hex(0xCCDDEE), Palette::Teal, 0.5f * hover),
                   Mono(11.f, true).Ellipsis());
            // "Load" arrow sliding in
            if (hover > 0.01f) Glyph(ui, Glyphs::Download, { r.R() - 34.f + 4.f * (1.f - hover), r.y, 20.f, r.h }, 11.f, Palette::Teal.WithAlpha(hover));
            if (it.clicked) clicked = i;
        }
        scroll.SetContentSize(listH + 8.f);
        if (clicked >= 0) Load(static_cast<size_t>(clicked));
    }
    frame.SetContentHeight(20.f + col.Height() + 20.f);

    // Footer: the keyboard hint on the left, BACK on the right
    g.Text(L"\x2191 \x2193  \x00B7  ENTER", frame.footer.Inset(Edges{ 20.f, 0.f }), K::kDim, Mono(9.f, true));
    const std::wstring back = Loc::Get("back");
    const float w = K::FooterButtonWidth(ui, back, Glyphs::Back, 120.f);
    if (K::FooterButton(ui, ui.MakeId("checkpoints-close"), { frame.footer.R() - 14.f - w, frame.footer.CY() - 18.f, w, 36.f }, back, K::Kind::Secondary,
                        true, Glyphs::Back) ||
        (popup.IsOpen() && ui.KeyPressed(VK_ESCAPE)))
        popup.Close();
}

} // namespace SpeedrunLauncher::Main
