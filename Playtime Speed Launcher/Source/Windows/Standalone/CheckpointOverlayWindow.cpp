#include "Windows/Standalone/CheckpointOverlayWindow.h"
#include <algorithm>
#include <cmath>
#include "App/Launcher.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Seasonal/SeasonalCardArt.h"
#include "Seasonal/SeasonalTheme.h"
#include "UI/Layout.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Panel.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"

namespace SpeedrunLauncher::Windows {

using namespace UI;
namespace Saves = Services::SaveGames;

namespace {

constexpr float kRadius = 10.f;
constexpr float kHeaderH = 64.f, kFooterH = 62.f;
constexpr float kRowH = 36.f, kRowGap = 4.f, kLabelH = 18.f, kSectionGap = 14.f;
constexpr float kMaxListH = 430.f;
constexpr double kPopIn = 0.34, kPopOut = 0.2, kDoneHold = 0.7;
constexpr Color kPanelBg = Argb(0xF207101A);
constexpr Color kPanelBorder = Hex(0x0D3020);
constexpr Color kRowText = Rgba(196, 210, 222);

// First chapter whose Steam exe is running (by process name), 1-based; 0 = none.
int DetectRunningChapter() {
    const auto& chapters = App::L().chapters;
    for (size_t i = 0; i < chapters.size(); ++i) {
        const auto& exe = chapters[i].gameExePath;
        if (!exe || exe->empty()) continue;
        if (!Process::FindByName(Files::FileNameWithoutExtension(*exe)).empty()) return static_cast<int>(i) + 1;
    }
    return 0;
}

std::wstring ChapterTitle(int n) { return Loc::Get("ch" + std::to_string(n) + "_title"); }
std::wstring ChapterLabel(int n) { return Loc::Fmt("store_chapter", std::to_wstring(n)) + L"  \x00B7  " + ChapterTitle(n); }

// "[3] Mini Huggies" → number "3", label "Mini Huggies"
void SplitName(const std::wstring& name, std::wstring& number, std::wstring& label) {
    number.clear();
    label = name;
    if (name.size() < 3 || name[0] != L'[') return;
    const size_t close = name.find(L']');
    if (close == std::wstring::npos || close < 2) return;
    number = name.substr(1, close - 1);
    size_t start = close + 1;
    while (start < name.size() && name[start] == L' ') ++start;
    label = name.substr(start);
}

// Small keyboard key ("ESC", "↑", "ENTER")
float Keycap(Graphics& g, float x, float cy, const std::wstring& key, float alpha = 1.f) {
    const TextStyle style = Mono(9.f, true).Center();
    const float w = std::max(20.f, g.Measure(key, style).w + 12.f);
    const Rect r{ x, cy - 10.f, w, 20.f };
    g.Border(r, 4.f, Rgba(12, 28, 42).Fade(alpha), Rgba(40, 80, 106).Fade(alpha), 1.f);
    g.FillRect({ r.x + 3.f, r.B() - 2.f, r.w - 6.f, 1.f }, Rgba(40, 80, 106, 120).Fade(alpha));
    g.Text(key, r.Offset(0.f, -1.f), Rgba(150, 185, 205).Fade(alpha), style);
    return w;
}

} // namespace

bool CheckpointOverlayWindow::Open() {
    detected = DetectRunningChapter();
    if (detected >= 4) {
        armed = true;   // no list: LOAD copies the chapter's start saves
    } else {
        for (int n = 1; n <= 3; ++n) {
            if (detected && n != detected) continue;
            for (auto& checkpoint : Saves::Checkpoints(n)) {
                Row row;
                row.chapter = n;
                SplitName(checkpoint.name, row.number, row.label);
                row.checkpoint = std::move(checkpoint);
                rows.push_back(std::move(row));
            }
        }
    }

    // The list never changes while the window is open: lay it out and build its strings once
    for (size_t i = 0; i < rows.size(); ++i) {
        const bool newSection = !detected && (i == 0 || rows[i].chapter != rows[i - 1].chapter);
        if (newSection) {
            listH += (i ? kSectionGap : 0.f) + kLabelH + 6.f;
            rows[i].section = ChapterLabel(rows[i].chapter);
        } else if (i) {
            listH += kRowGap;
        }
        rows[i].y = listH;
        listH += kRowH;
    }
    subtitle = detected ? ChapterLabel(detected) + L"  \x00B7  " : L"";
    subtitle += armed ? Loc::Get("hotkey_start_saves") : Loc::Fmt("hotkey_count", std::to_wstring(rows.size()));

    // WindowState=Maximized over the primary monitor, transparent + topmost
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), &mi);
    UI::Window::Options o;
    o.title = L"Checkpoints";
    o.style = WS_POPUP;
    o.exStyle = WS_EX_TOOLWINDOW;
    o.transparent = true;
    o.topmost = true;
    o.showCommand = SW_HIDE;
    if (!Create(o)) return false;
    g.SetColorFilter(&Seasonal::Filter);   // follows the seasonal theme like the main window
    // Pulses and seasonal particles are slow: idle frames at 30 fps (input / tweens still at full rate)
    SetContinuous(true, true);
    SetIdleFps(30.0);
    SetWindowPos(hwnd, HWND_TOPMOST, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top, 0);
    openedAt = Async::Now();
    Show(SW_SHOW);
    Activate();
    return true;
}

// The hotkey again / Alt+F4 / the owner closing it: pop out first
void CheckpointOverlayWindow::OnClosing(bool& cancel) {
    if (finished) return;
    cancel = true;
    BeginClose();
    Invalidate();
}

void CheckpointOverlayWindow::BeginClose() {
    if (closingAt < 0.0) closingAt = Async::Now();
}

void CheckpointOverlayWindow::Done(const std::wstring& text, bool danger) {
    doneText = text;
    doneDanger = danger;
    doneAt = Async::Now();
}

void CheckpointOverlayWindow::Load() {
    if (armed) Saves::LoadStartSaves(detected);
    else if (selected >= 0 && selected < static_cast<int>(rows.size())) Saves::LoadCheckpoint(rows[static_cast<size_t>(selected)].checkpoint);
    else return;
    Done(Loc::Get("hotkey_loaded"), false);
}

void CheckpointOverlayWindow::Delete() {
    Saves::DeleteSaves(detected);
    Done(Loc::Get("hotkey_deleted"), true);
}

// The confirmation after LOAD / DELETE: a ring that pops in with its glyph, and the message
void CheckpointOverlayWindow::DrawDone(const Rect& body, double now) {
    const float t = static_cast<float>(now - doneAt);
    const float fade = Anim::EaseOutCubic(t / 0.18f);
    const Color accent = doneDanger ? Palette::DangerText : Palette::Teal;
    g.FillRound(body, 0.f, kPanelBg.Fade(0.94f * fade));

    const float pop = Anim::EaseOutBack(t / 0.42f, 1.2f);
    const float cx = body.CX(), cy = body.CY() - 12.f;
    const float r = 26.f * pop;
    if (r > 0.5f) {
        // Ripple expanding out of the ring
        const float ripple = Anim::Clamp01((t - 0.1f) / 0.6f);
        if (ripple < 1.f) g.StrokeEllipse(cx, cy, r + 24.f * Anim::EaseOutCubic(ripple), r + 24.f * Anim::EaseOutCubic(ripple),
                                          accent.WithAlpha(0.5f * (1.f - ripple)), 2.f);
        g.FillEllipse(cx, cy, r, r, accent.WithAlpha(0.16f * fade));
        g.StrokeEllipse(cx, cy, r, r, accent.WithAlpha(fade), 2.f);
        g.Text(std::wstring(1, doneDanger ? Glyphs::Delete : Glyphs::CheckMark), { cx - r, cy - r, r * 2.f, r * 2.f }, accent.WithAlpha(fade),
               IconFont(18.f * pop));
    }
    const float rise = (1.f - Anim::EaseOutCubic((t - 0.12f) / 0.3f)) * 10.f;
    const float textFade = Anim::Clamp01((t - 0.12f) / 0.25f);
    g.Text(doneText, { body.x, cy + 38.f + rise, body.w, 20.f }, accent.WithAlpha(textFade), Mono(12.f, true).Center());
}

void CheckpointOverlayWindow::OnFrame() {
    const Rect screen{ 0.f, 0.f, g.Width(), g.Height() };
    const double now = Async::Now();

    // Closing: pop out, then really close
    float closeT = 0.f;
    if (closingAt >= 0.0) {
        closeT = Anim::Clamp01(static_cast<float>((now - closingAt) / kPopOut));
        if (closeT >= 1.f) {
            finished = true;
            Close();
            return;
        }
        ui.KeepAnimating();
    }
    // After LOAD / DELETE the confirmation stays for a moment
    if (doneAt >= 0.0) {
        if (closingAt < 0.0 && now - doneAt > kDoneHold) BeginClose();
        ui.KeepAnimating();
    }
    const bool interactive = closingAt < 0.0 && doneAt < 0.0;
    const bool canLoad = armed || selected >= 0;

    // Keyboard: Esc closes, ↑ ↓ pick a checkpoint, Enter loads it
    if (interactive) {
        if (ui.KeyPressed(VK_ESCAPE)) BeginClose();
        const int count = static_cast<int>(rows.size());
        if (count > 0 && ui.KeyPressed(VK_DOWN)) {
            selected = selected < 0 ? 0 : std::min(count - 1, selected + 1);
            keyboardMoved = true;
        }
        if (count > 0 && ui.KeyPressed(VK_UP)) {
            selected = selected < 0 ? count - 1 : std::max(0, selected - 1);
            keyboardMoved = true;
        }
        if (canLoad && ui.KeyPressed(VK_RETURN)) Load();
    }

    // Pop-in: fade + scale 0.9 → 1 with an overshoot and a small rise; pop-out: fade + shrink
    const float openT = Anim::Clamp01(static_cast<float>((now - openedAt) / kPopIn));
    if (openT < 1.f) ui.KeepAnimating();
    const float fade = Anim::EaseOutCubic(std::min(1.f, openT * 1.4f)) * (1.f - Anim::EaseInCubic(closeT));
    const float scale = Anim::Lerp(0.9f, 1.f, Anim::EaseOutBack(openT, 0.8f)) * Anim::Lerp(1.f, 0.95f, Anim::EaseInCubic(closeT));
    const float rise = (1.f - Anim::EaseOutCubic(openT)) * 18.f + Anim::EaseInCubic(closeT) * 10.f;
    const bool layered = fade < 0.999f;
    if (layered) g.PushOpacity(fade);

    // Backdrop (click outside the panel closes)
    g.FillRect(screen, Argb(0x99000000));
    g.FillRadial(screen.Inset(-screen.w * 0.2f, -screen.h * 0.2f), { { 0.f, Argb(0x00000000) }, { 0.7f, Argb(0x00000000) }, { 1.f, Argb(0x66000000) } });
    const Interaction backdrop = ui.Interact(ui.MakeId("backdrop"), screen, true, false);

    float bodyH = 0.f;
    if (armed) bodyH = 96.f;
    else if (rows.empty()) bodyH = 90.f;
    else bodyH = std::min(listH, kMaxListH);
    const float width = detected ? 500.f : 540.f;
    const float panelH = kHeaderH + 1.f + 16.f + bodyH + 16.f + kFooterH;
    const Rect panel = screen.Center(width, panelH);
    const float sinceOpen = static_cast<float>(now - openedAt);

    PanelBackdropGlow(ui, panel, Palette::Teal, sinceOpen);
    ui.PushScale(scale, panel.CX(), panel.CY());
    ui.PushTranslate(0.f, rise);
    ui.Block(panel);
    PanelGlow(ui, panel, kRadius, Palette::Teal, sinceOpen);
    g.Border(panel, kRadius, kPanelBg, kPanelBorder, 1.f);
    g.FillLinear(panel.Inset(1.f).Top(std::min(panel.h * 0.5f, 200.f)), kRadius - 1.f,
                 { { 0.f, Palette::Teal.WithAlpha(0.07f) }, { 1.f, Palette::Teal.WithAlpha(0.f) } }, { 0.f, 0.f }, { 0.f, 1.f });

    // ── Header: [save badge] TITLE / subtitle ............ [ESC] [✕] ──────────
    const Rect header{ panel.x + 1.f, panel.y + 1.f, panel.w - 2.f, kHeaderH };
    g.FillRound(header, kRadius - 1.f, kRadius - 1.f, 0.f, 0.f, Hex(0x05090F, 0.85f));
    {
        StaggerItem in(ui, openedAt + 0.06, 0, -14.f, 0.f, 0.0, 0.36);
        const Rect badge{ header.x + 16.f, header.CY() - 18.f, 36.f, 36.f };
        const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(now * 2.6));
        g.GlowRound(badge, 9.f, Palette::Teal, 16.f, 0.18f + 0.22f * pulse);
        g.Border(badge, 9.f, Rgba(0, 204, 170, 30), Rgba(0, 204, 170, 140), 1.f);
        g.Text(std::wstring(1, Glyphs::Save), badge, Palette::Teal, IconFont(16.f));

        const Rect title{ badge.R() + 14.f, header.y + 13.f, header.w - 200.f, 20.f };
        g.Text(Loc::Get("hotkey_title"), title, Palette::Teal, Mono(14.f, true));
        g.Text(subtitle, { title.x, title.B() + 1.f, title.w, 16.f }, Rgba(90, 130, 160), Mono(10.f).Ellipsis());
    }
    const ButtonStyle smallButton{ Palette::ButtonBg, Palette::ButtonBorder, 1.f, 6.f, { 0.f }, 0.f, 30.f };
    const Rect closeR{ header.R() - 16.f - 30.f, header.CY() - 15.f, 30.f, 30.f };
    if (TextButton(ui, ui.MakeId("close"), closeR, L"\x2715", Mono(12.f, true).WithFont(Font::Ui).Center(), Palette::Dim, smallButton) && interactive)
        BeginClose();
    {
        const float escW = std::max(20.f, g.Measure(L"ESC", Mono(9.f, true).Center()).w + 12.f);
        Keycap(g, closeR.x - 10.f - escW, header.CY(), L"ESC", 0.8f);
    }

    // ── Body ────────────────────────────────────────────────────────────────
    const Rect area{ panel.x + 18.f, header.B() + 1.f + 16.f, panel.w - 36.f, bodyH };
    if (armed) {
        // Chapters 4-5: what LOAD / DELETE do
        StaggerItem in(ui, openedAt + 0.12, 0, 0.f, 12.f, 0.0, 0.38);
        g.Border(area, 8.f, Rgba(0, 204, 170, 14), Rgba(0, 204, 170, 50), 1.f);
        const Rect icon{ area.x + 18.f, area.CY() - 22.f, 44.f, 44.f };
        g.FillEllipse(icon.CX(), icon.CY(), 22.f, 22.f, Rgba(0, 204, 170, 26));
        g.StrokeEllipse(icon.CX(), icon.CY(), 22.f, 22.f, Rgba(0, 204, 170, 110), 1.f);
        g.Text(std::wstring(1, Glyphs::Install), icon, Palette::Teal, IconFont(18.f));
        const Rect text{ icon.R() + 16.f, area.y + 18.f, area.R() - icon.R() - 34.f, area.h - 36.f };
        g.Text(ChapterTitle(detected), text.Top(18.f), Rgba(214, 228, 238), Mono(12.f, true));
        g.Text(Loc::Get("hotkey_start_saves_desc"), { text.x, text.y + 22.f, text.w, text.h - 22.f }, Rgba(120, 160, 190), Mono(10.f).Wrap().Top());
    } else if (rows.empty()) {
        StaggerItem in(ui, openedAt + 0.12, 0, 0.f, 10.f, 0.0, 0.38);
        g.Text(std::wstring(1, Glyphs::Info), area.Top(40.f).Offset(0.f, 6.f), Palette::Muted, IconFont(20.f));
        g.Text(Loc::Get("hotkey_empty"), { area.x, area.y + 50.f, area.w, 20.f }, Palette::Hint, Mono(11.f, true).Center());
    } else {
        float offset = 0.f;
        {
            ScrollArea scroll(ui, ui.MakeId("checkpoint-list"), area, false, 48.f, ScrollArea::Bar::Thin);
            offset = scroll.Offset();
            const float rowW = scroll.ContentWidth();
            for (size_t i = 0; i < rows.size(); ++i) {
                const int index = static_cast<int>(i);
                const int stagger = std::min(index, 14);
                // Only what's inside the viewport (its label sits above the row)
                const float top = rows[i].y - (rows[i].section.empty() ? 0.f : kLabelH + 6.f);
                if (rows[i].y + kRowH < offset || top > offset + area.h) continue;
                // Chapter label with a line running to the right edge
                if (!rows[i].section.empty()) {
                    StaggerItem in(ui, openedAt + 0.12, stagger, -12.f, 0.f, 0.03, 0.34);
                    const Rect label{ area.x, area.y + rows[i].y - kLabelH - 6.f, rowW, kLabelH };
                    const TextStyle style = Mono(9.f, true);
                    const std::wstring& text = rows[i].section;
                    const float tw = g.Measure(text, style).w;
                    g.Text(text, label, Palette::Teal, style);
                    const float lineX = label.x + tw + 10.f;
                    if (lineX < label.R()) g.FillLinear({ lineX, label.CY(), label.R() - lineX, 1.f }, 0.f,
                                                        { { 0.f, Rgba(0, 204, 170, 70) }, { 1.f, Rgba(0, 204, 170, 0) } }, { 0.f, 0.f }, { 1.f, 0.f });
                }

                StaggerItem in(ui, openedAt + 0.14, stagger, -18.f, 0.f, 0.03, 0.34);
                const Rect r{ area.x, area.y + rows[i].y, rowW, kRowH };
                const Interaction it = ui.Interact(ui.MakeId("row", index), r);
                const bool isSelected = selected == index;
                const float hover = ui.Animate(ui.MakeId("row-hover", index), it.hovered && interactive ? 1.f : 0.f, 16.f);
                const float pick = ui.Animate(ui.MakeId("row-pick", index), isSelected ? 1.f : 0.f, 18.f);
                const float press = ui.Animate(ui.MakeId("row-press", index), it.held ? 1.f : 0.f, 30.f);

                const Rect card = r.Inset(press * 1.5f, press * 1.f);
                if (pick > 0.01f) g.GlowRound(r, 7.f, Palette::Teal, 14.f, 0.28f * pick);   // r, not card: same size → cached blur mask
                Color bg = Lerp(Rgba(255, 255, 255, 8), Rgba(255, 255, 255, 20), hover);
                bg = Lerp(bg, Rgba(0, 204, 170, 36), pick);
                g.Border(card, 7.f, bg, Lerp(Lerp(Rgba(20, 48, 68, 90), Rgba(40, 90, 120, 160), hover), Rgba(0, 204, 170, 190), pick), 1.f);
                // Accent bar growing from the middle of the left edge
                if (pick > 0.01f) {
                    const float barH = (card.h - 14.f) * Anim::EaseOutCubic(pick);
                    g.FillRound({ card.x + 5.f, card.CY() - barH * 0.5f, 3.f, barH }, 1.5f, Palette::Teal);
                }

                float x = card.x + 14.f + 4.f * std::max(hover, pick);
                if (!rows[i].number.empty()) {
                    const TextStyle chipStyle = Mono(10.f, true).Center();
                    const float chipW = std::max(26.f, g.Measure(rows[i].number, chipStyle).w + 12.f);
                    const Rect chip{ x, card.CY() - 10.f, chipW, 20.f };
                    g.Border(chip, 5.f, Lerp(Rgba(0, 204, 170, 18), Rgba(0, 204, 170, 70), pick), Rgba(0, 204, 170, static_cast<int>(60 + 80 * pick)), 1.f);
                    g.Text(rows[i].number, chip, Lerp(Rgba(0, 204, 170, 200), Colors::White, pick), chipStyle);
                    x += chipW + 12.f;
                } else {
                    g.Text(std::wstring(1, Glyphs::Save), { x, card.y, 20.f, card.h }, Rgba(0, 204, 170, 200), IconFont(12.f));
                    x += 20.f + 12.f;
                }
                g.Text(rows[i].label, { x, card.y, card.R() - x - 40.f, card.h }, Lerp(kRowText, Colors::White, std::max(hover, pick)),
                       Mono(11.f, true).Ellipsis());
                // ✓ on the selected row (slides in), → while hovering another
                if (pick > 0.01f)
                    g.Text(std::wstring(1, Glyphs::CheckMark), { card.R() - 34.f + 8.f * (1.f - pick), card.y, 22.f, card.h },
                           Palette::Teal.WithAlpha(pick), IconFont(12.f));
                else if (hover > 0.01f)
                    g.Text(L"\x2192", { card.R() - 34.f - 4.f * (1.f - hover), card.y, 22.f, card.h }, Palette::Hint.WithAlpha(hover),
                           Mono(12.f, true).Center());

                if (!interactive) continue;
                if (it.hovered) ui.SetCursor(IDC_HAND);
                if (it.held && ui.In().pressed[0]) selected = index;
                if (it.clicked) {
                    if (lastClickRow == index && now - lastClickAt < 0.4) Load();
                    lastClickRow = index;
                    lastClickAt = now;
                }
            }
            scroll.SetContentSize(listH);
        }
        // Keep the row picked with ↑ ↓ in view
        if (keyboardMoved && selected >= 0) {
            const Row& row = rows[static_cast<size_t>(selected)];
            const float top = row.y;
            const float visibleTop = row.section.empty() ? top : top - kLabelH - 6.f;
            if (visibleTop < offset) ScrollArea::ScrollTo(ui, ui.MakeId("checkpoint-list"), std::max(0.f, visibleTop - 2.f));
            else if (top + kRowH > offset + area.h) ScrollArea::ScrollTo(ui, ui.MakeId("checkpoint-list"), top + kRowH - area.h + 2.f);
            ui.KeepAnimating();
        }
        keyboardMoved = false;
    }

    // ── Footer: key hints on the left, BACK / DELETE / LOAD on the right ──────
    const Rect footer{ panel.x + 1.f, panel.B() - 1.f - kFooterH, panel.w - 2.f, kFooterH };
    g.FillRound(footer, 0.f, 0.f, kRadius - 1.f, kRadius - 1.f, Hex(0x05090F, 0.6f));
    {
        StaggerItem in(ui, openedAt + 0.2, 0, 0.f, 8.f, 0.0, 0.34);
        float hx = footer.x + 18.f;
        const TextStyle hint = Mono(9.f);
        const auto label = [&](const std::wstring& text) {
            const float w = g.Measure(text, hint).w;
            g.Text(text, { hx, footer.y, w + 2.f, footer.h }, Rgba(80, 115, 140), hint);
            hx += w + 14.f;
        };
        if (!rows.empty()) {
            hx += Keycap(g, hx, footer.CY(), L"\x2191") + 3.f;
            hx += Keycap(g, hx, footer.CY(), L"\x2193") + 6.f;
            label(Loc::Get("hotkey_nav_hint"));
        }
        if (armed || !rows.empty()) {
            hx += Keycap(g, hx, footer.CY(), L"ENTER") + 6.f;
            label(Loc::Get("hotkey_enter_hint"));
        }
    }

    const float bh = 34.f;
    const float by = footer.CY() - bh * 0.5f;
    float bx = footer.R() - 16.f;
    const TextStyle buttonText = Mono(11.f, true);
    if (armed || !rows.empty()) {
        const std::wstring load = Loc::Get(armed ? "hotkey_load_btn" : "hotkey_load_all_btn");
        const ButtonStyle loadStyle{ Palette::ConfirmBg, Palette::ConfirmBorder, 1.f, 6.f, { 16.f, 0.f }, 110.f, bh };
        const float loadW = ButtonWidthFor(13.f + 8.f + g.Measure(load, buttonText).w, loadStyle);
        bx -= loadW;
        const Rect loadR{ bx, by, loadW, bh };
        // Breathing glow once something can be loaded
        const float ready = ui.Animate(ui.MakeId("load-ready"), canLoad ? 1.f : 0.f, 10.f);
        if (ready > 0.01f) {
            const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(now * 3.2));
            g.GlowRound(loadR, 6.f, Palette::Teal, 18.f, ready * (0.2f + 0.25f * pulse));
        }
        if (GlyphTextButton(ui, ui.MakeId("load"), loadR, Glyphs::Save, 13.f, Palette::Teal, load, buttonText, Palette::Teal, loadStyle, 8.f,
                            canLoad && interactive))
            Load();
        bx -= 8.f;
    }
    if (detected >= 4) {
        const std::wstring del = Loc::Get("hotkey_delete_btn");
        const ButtonStyle delStyle{ Palette::DangerBg, Palette::DangerBorder, 1.f, 6.f, { 14.f, 0.f }, 0.f, bh };
        const float delW = ButtonWidthFor(13.f + 8.f + g.Measure(del, buttonText).w, delStyle);
        bx -= delW;
        if (GlyphTextButton(ui, ui.MakeId("delete"), { bx, by, delW, bh }, Glyphs::Delete, 13.f, Palette::DangerText, del, buttonText,
                            Palette::DangerText, delStyle, 8.f, interactive))
            Delete();
        bx -= 8.f;
    }
    {
        const std::wstring back = Loc::Get("back");
        const ButtonStyle backStyle{ Palette::ButtonBg, Palette::ButtonBorder, 1.f, 6.f, { 14.f, 0.f }, 0.f, bh };
        const float backW = ButtonWidth(ui, back, buttonText, backStyle);
        bx -= backW;
        if (TextButton(ui, ui.MakeId("back"), { bx, by, backW, bh }, back, Mono(11.f, true).Center(), Palette::Dim, backStyle) && interactive)
            BeginClose();
    }

    // Seasonal theme: particles over the content, decorations resting on the top edge
    Seasonal::CardArt::DrawPanelParticles(g, panel, kRadius, now, 0.75f);
    if (doneAt >= 0.0) DrawDone({ panel.x + 1.f, header.B() + 1.f, panel.w - 2.f, footer.y - header.B() - 1.f }, now);

    PanelDecor(ui, panel, kRadius, Palette::Teal, sinceOpen, header.B(), footer.y);
    Seasonal::CardArt::DrawPanelEdge(g, panel, now);
    if (PanelEntranceRunning(sinceOpen)) ui.KeepAnimating();

    ui.PopTransform();
    ui.PopTransform();
    if (layered) g.PopLayer();
    if (interactive && backdrop.held && ui.In().pressed[0] && !ui.MouseOver(panel)) BeginClose();
}

} // namespace SpeedrunLauncher::Windows
