#include "App/SteamShortcutHelper.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <tuple>
#include <vector>
#include "App/MessageLoop.h"
#include "App/Paths.h"
#include "Core/Async.h"
#include "Graphics/Animation.h"
#include "Graphics/Images.h"
#include "Localization/Localization.h"
#include "Seasonal/SeasonalCardArt.h"
#include "Seasonal/SeasonalTheme.h"
#include "Services/Platforms/SteamShortcutService.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Form.h"
#include "UI/Window.h"

namespace SpeedrunLauncher::App {

namespace {

using namespace UI;
namespace S = Services::SteamShortcuts;

// Same frame as the launcher's dialogs (DialogWindow.cpp): title bar, content, button bar
constexpr Color kTeal = Rgba(0, 204, 170);
constexpr Color kGreen = Hex(0x00CC88);
constexpr Color kRed = Hex(0xCC3333);
constexpr Color kAmber = Rgba(255, 176, 32);
constexpr Color kDim = Rgba(120, 160, 190, 160);
constexpr float kContentW = 420.f, kPadX = 20.f, kPadY = 18.f;
constexpr float kTitleH = 48.f, kButtonsH = 52.f;
constexpr float kDecorH = 36.f;     // transparent strip above the window for the seasonal decorations
constexpr float kTile = 96.f, kSmallTile = 64.f, kStepH = 28.f;
constexpr DWORD kMinStepMs = 450;   // every step stays on screen at least this long (some are instant)

// "Added to Steam!\nRestart Steam if..." -> headline + body
std::pair<std::wstring, std::wstring> SplitFirstLine(const std::wstring& text) {
    const size_t nl = text.find(L'\n');
    if (nl == std::wstring::npos) return { text, L"" };
    return { text.substr(0, nl), text.substr(nl + 1) };
}

TextStyle Wrapped(float size, bool bold = false) {
    TextStyle s = Mono(size, bold);
    s.wrap = true;
    s.valign = VAlign::Top;
    return s;
}

// One window that goes Confirm -> Working (live steps) -> Done / Failed, instead of a dialog per stage
class SteamHelperWindow : public UI::Window {
public:
    SteamHelperWindow(std::wstring exe, std::wstring icon, bool running) : exePath(std::move(exe)), iconPath(std::move(icon)), wasRunning(running) {}

    bool Open() {
        UI::Window::Options w;
        w.title = Loc::Get("add_to_steam_btn");
        w.style = WS_POPUP;
        w.topmost = true;
        w.transparent = true;   // fades in / out (and holds the seasonal decorations above it)
        w.width = static_cast<int>(kContentW + kPadX * 2.f + 2.f);
        w.height = 400;
        w.showCommand = SW_HIDE;
        decor = Seasonal::IsSeasonal() ? kDecorH : 0.f;
        if (!Create(w)) return false;
        g.SetColorFilter(&Seasonal::Filter);   // follows the seasonal theme like the launcher's dialogs
        SetContinuous(true, true);              // the tile, the spinners and the particles never stop
        Layout(400.f);
        Show(SW_SHOW);
        Activate();
        return true;
    }

protected:
    Color ClearColor() const override { return Colors::Transparent; }

    // Mid-operation the window can't be closed (Steam would be left shut down)
    void OnClosing(bool& cancel) override { cancel = phase == Phase::Working; }

    // Drag the window by its title bar
    bool OnMessage(UINT msg, WPARAM, LPARAM lp, LRESULT& result) override {
        if (msg != WM_NCHITTEST) return false;
        POINT pt{ static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)) };
        ScreenToClient(hwnd, &pt);
        const float x = pt.x / DpiScale(), y = pt.y / DpiScale() - decor;
        const Rect title{ 0.f, 0.f, g.Width(), kTitleH };
        if (!title.Contains(x, y) || (CanClose() && CloseRect().Contains(x, y))) return false;
        result = HTCAPTION;
        return true;
    }

    void OnFrame() override {
        const double now = ui.Time();
        if (openedAt < 0.0) openedAt = phaseAt = now;

        // Open: fade + a slight zoom; close: quick fade, then the window goes
        float fade, zoom;
        if (closingAt >= 0.0) {
            const float c = static_cast<float>(now - closingAt);
            fade = 1.f - Anim::EaseInCubic(Anim::Clamp01(c / 0.18f));
            zoom = 1.f - 0.03f * Anim::EaseInCubic(Anim::Clamp01(c / 0.18f));
            if (c >= 0.18f) {
                Close();
                return;
            }
        } else {
            const float o = static_cast<float>(now - openedAt);
            fade = Anim::EaseOutCubic(Anim::Clamp01(o / 0.22f));
            zoom = 0.96f + 0.04f * Anim::EaseOutBack(Anim::Clamp01(o / 0.4f), 1.2f);
        }
        ui.KeepAnimatingThrottled();
        if (now - phaseAt < 1.5 || closingAt >= 0.0 || now - openedAt < 0.6) ui.KeepAnimating();

        const float w = g.Width();
        const float bodyH = BodyHeight();
        const float h = kTitleH + 1.f + kPadY + bodyH + kPadY + kButtonsH + 2.f;
        if (std::abs(h + decor - g.Height()) > 0.5f) Layout(h + decor);

        // The accent follows the stage: teal, then green / red at the end
        const float mix = Anim::EaseOutCubic(Anim::Clamp01(static_cast<float>(now - phaseAt) / 0.5f));
        accent = Lerp(accentFrom, PhaseAccent(), mix);

        g.PushOpacity(fade);
        ui.PushTranslate(0.f, decor);
        ui.PushScale(zoom, w * 0.5f, h * 0.5f);
        g.FillRect({ 0.f, 0.f, w, h }, Palette::PanelBg);

        const Rect body{ 1.f, kTitleH + 1.f, w - 2.f, h - kTitleH - kButtonsH - 2.f };
        Seasonal::CardArt::DrawPanelParticles(g, body, 0.f, now, 0.5f);
        DrawTitle(w);

        const Rect content{ kPadX + 1.f, kTitleH + 1.f + kPadY, kContentW, bodyH };
        const Rect bar{ 1.f, h - kButtonsH - 1.f, w - 2.f, kButtonsH };
        g.FillRect(bar, Palette::HeaderBg);
        g.FillRect({ bar.x, bar.y, bar.w, 1.f }, Palette::Divider);
        switch (phase) {
            case Phase::Confirm: DrawConfirm(content, bar); break;
            case Phase::Working: DrawWorking(content, bar); break;
            default: DrawResult(content, bar); break;
        }

        // Outer border, lit by the accent while something is happening
        g.StrokeRect({ 0.f, 0.f, w, h }, Lerp(Rgba(21, 48, 72), accent, phase == Phase::Confirm ? 0.f : 0.35f * mix), 1.f);
        Seasonal::CardArt::DrawPanelEdge(g, { 0.f, 0.f, w, h }, now);
        ui.PopTransform();
        ui.PopTransform();
        g.PopLayer();

        if (closingAt < 0.0 && ui.KeyPressed(VK_ESCAPE) && CanClose()) Dismiss();
    }

private:
    enum class Phase { Confirm, Working, Done, Failed };
    enum class StepState { Pending, Active, Done, Failed };
    struct Step {
        std::string key;
        StepState state = StepState::Pending;
        double changedAt = 0.0;
    };

    bool CanClose() const { return phase != Phase::Working && closingAt < 0.0; }
    Rect CloseRect() const { return { g.Width() - 12.f - 28.f, (kTitleH - 28.f) * 0.5f, 28.f, 28.f }; }

    Color PhaseAccent() const {
        switch (phase) {
            case Phase::Done: return kGreen;
            case Phase::Failed: return kRed;
            default: return kTeal;
        }
    }

    void SetPhase(Phase next) {
        accentFrom = accent;
        phase = next;
        phaseAt = ui.Time();
        Invalidate();
    }

    void Dismiss() {
        if (closingAt >= 0.0) return;
        closingAt = ui.Time();
        Invalidate();
    }

    // The tallest stage decides the window's size, so it never jumps between stages
    float BodyHeight() {
        const float confirm = 16.f + 12.f + kTile + 12.f + 20.f + 16.f + NoteHeight();
        const float working = kSmallTile + 40.f + 14.f + 20.f + 18.f + kStepH * 3.f + 14.f + 8.f + 8.f + 16.f;
        return std::max(confirm, working);
    }
    const std::wstring& Note() const { return Loc::Get(wasRunning ? "add_to_steam_warning_running" : "add_to_steam_warning_not_running"); }
    float NoteHeight() { return g.Measure(Note(), Wrapped(10.f), kContentW - 52.f).h + 26.f; }

    // ── Title bar: Steam logo, "ADD TO STEAM", the X; its bottom edge is the progress line ──────
    void DrawTitle(float w) {
        const float t = static_cast<float>(ui.Time());
        const Rect title{ 1.f, 1.f, w - 2.f, kTitleH };
        g.FillRect(title, Palette::HeaderBg);

        // Logo with a ring pulsing out of it
        const Rect logo{ title.x + 18.f, title.CY() - 12.f, 24.f, 24.f };
        const float ring = std::fmod(t, 1.8f) / 1.8f;
        g.StrokeRound(logo.Inset(-5.f * ring), 6.f + 5.f * ring, accent.WithAlpha(0.5f * (1.f - ring)), 1.5f);
        if (auto img = Images::Load(Paths::Asset(L"Images\\Steam.jpg"))) g.DrawImageRounded(*img, logo, 6.f);
        g.StrokeRound(logo.Inset(0.5f), 5.5f, accent.WithAlpha(0.6f), 1.f);
        const float tx = logo.R() + 12.f;
        g.Text(Loc::Get("add_to_steam_btn"), { tx, title.y, CloseRect().x - 8.f - tx, title.h }, kTeal, Mono(14.f, true).Ellipsis());

        // Progress: share of steps done, eased; a light runs along it while working
        const float target = phase == Phase::Working ? Progress() : 1.f;
        progress = phase == Phase::Confirm ? 0.f : Anim::Approach(progress, target, 6.f, ui.Dt());
        g.FillRect({ title.x, title.B() - 1.f, title.w, 1.f }, Palette::Divider);
        if (phase != Phase::Confirm) {
            const Rect line{ title.x, title.B() - 2.f, title.w * progress, 2.f };
            g.GlowRound(line, 0.f, accent, 8.f, 0.5f);
            g.FillRect(line, accent);
            if (phase == Phase::Working) {
                const float run = std::fmod(t, 1.2f) / 1.2f;
                g.PushClip(line);
                g.FillLinear({ line.x + line.w * run - 50.f, line.y, 100.f, line.h }, 0.f,
                             { { 0.f, Rgba(255, 255, 255, 0) }, { 0.5f, Rgba(255, 255, 255, 180) }, { 1.f, Rgba(255, 255, 255, 0) } }, { 0.f, 0.f }, { 1.f, 0.f });
                g.PopClip();
            }
        }

        // X: hidden while working
        const float shown = ui.Tween(ui.MakeId("x-shown"), CanClose() ? 1.f : 0.f, 0.2f, 1.f);
        if (shown > 0.01f) {
            const Rect x = CloseRect();
            const Id id = ui.MakeId("x");
            const Interaction it = ui.Interact(id, x, CanClose());
            if (it.hovered) ui.SetCursor(IDC_HAND);
            const float hover = ui.Animate(ui.MakeId("x-hover"), it.hovered ? 1.f : 0.f, 14.f, 0.f);
            g.PushOpacity(shown);
            g.Border(x, 3.f, Lerp(Colors::Transparent, Palette::DangerBg, hover), Lerp(Palette::ButtonBorder, Palette::DangerBorder, hover), 1.f);
            Glyph(ui, Glyphs::Cancel, x, 10.f, Lerp(Palette::Dim, kRed, hover));
            g.PopLayer();
            if (it.clicked) Dismiss();
        }
    }

    float Reveal(int& row) {
        const float k = Anim::Clamp01(static_cast<float>(ui.Time() - phaseAt - 0.08 - 0.06 * row++) / 0.3f);
        return Anim::EaseOutCubic(k);
    }
    // Fades a row in while it rises `dy` px into place (pair with EndReveal)
    void BeginReveal(float k, float dy = 8.f) {
        g.PushOpacity(k);
        ui.PushTranslate(0.f, dy * (1.f - k));
    }
    void EndReveal() {
        ui.PopTransform();
        g.PopLayer();
    }

    // The shortcut's icon in a glowing tile, a shine passing over it every few seconds
    void DrawTile(const Rect& tile, float scale) {
        const float t = static_cast<float>(ui.Time());
        const float radius = tile.w * 0.18f;
        ui.PushScale(scale, tile.CX(), tile.CY());
        g.GlowRound(tile, radius, accent, 22.f, 0.35f + 0.15f * std::sin(t * 2.f));
        g.FillLinear(tile, radius, { { 0.f, Hex(0x0D2535) }, { 1.f, Palette::HeaderBg } }, { 0.f, 0.f }, { 1.f, 1.f });
        if (auto icon = Images::Load(iconPath, 256)) {
            NoColorFilter keep(g);   // the icon keeps its own colors
            g.DrawImage(*icon, tile.Inset(tile.w * 0.13f), 1.f, Stretch::Uniform);
        }
        const float sweep = std::fmod(t, 3.5f) / 0.9f;
        if (sweep < 1.f) {
            g.FillLinear(tile, radius,
                         { { std::clamp(sweep - 0.25f, 0.f, 1.f), Rgba(255, 255, 255, 0) }, { std::clamp(sweep, 0.f, 1.f), Rgba(255, 255, 255, 60) },
                           { std::clamp(sweep + 0.25f, 0.f, 1.f), Rgba(255, 255, 255, 0) } },
                         { 0.f, 0.f }, { 1.f, 1.f });
            ui.KeepAnimating();
        }
        g.StrokeRound(tile.Inset(0.75f), radius - 0.75f, accent.WithAlpha(0.7f), 1.5f);
        ui.PopTransform();
    }

    // Dialog-style button in the bar (right aligned from `right`); returns its left edge
    bool BarButton(const char* id, const Rect& bar, float& right, const std::wstring& text, bool primary, bool enabled) {
        const TextStyle style = Mono(11.f, true).Center();
        const ButtonStyle look{ primary ? Rgba(0, 80, 60) : Rgba(10, 24, 37), primary ? Rgba(0, 180, 130, 180) : Rgba(13, 37, 53), 1.f, 3.f,
                                { 12.f, 0.f }, primary ? 150.f : 100.f, 36.f };
        const float bw = ButtonWidth(ui, text, style, look);
        right -= bw;
        const Rect r{ right, bar.CY() - 18.f, bw, 36.f };
        if (primary && enabled) g.GlowRound(r, 3.f, accent, 12.f, 0.15f + 0.1f * std::sin(static_cast<float>(ui.Time()) * 2.4f));
        const bool clicked = TextButton(ui, ui.MakeId(id), r, text, style, primary ? kTeal : Rgba(58, 106, 138, 200), look, enabled);
        right -= 8.f;
        return clicked;
    }

    // ── Confirm: the library preview (the icon floating in a glowing tile), what happens to Steam,
    // Cancel / Add to Steam ──────────────────────────────────────────────────────────────────────
    void DrawConfirm(const Rect& body, const Rect& bar) {
        const float t = static_cast<float>(ui.Time());
        int row = 0;
        float y = body.y;
        {
            BeginReveal(Reveal(row));
            g.Text(Loc::Get("add_to_steam_preview"), { body.x, y, body.w, 16.f }, kDim, Mono(9.f, true).Center());
            EndReveal();
            y += 16.f + 12.f;
        }
        {
            const float k = Reveal(row);
            BeginReveal(k, 0.f);
            DrawTile({ body.CX() - kTile * 0.5f, y + 3.f * std::sin(t * 2.f), kTile, kTile }, 0.7f + 0.3f * Anim::EaseOutBack(k, 1.6f));
            y += kTile + 12.f;
            g.Text(S::AppName, { body.x, y, body.w, 20.f }, Palette::Bright, Mono(13.f, true).Center());
            EndReveal();
            y += 20.f + 16.f;
        }
        {
            // Amber with a turning arrow when Steam has to restart, teal otherwise
            BeginReveal(Reveal(row));
            const Color c = wasRunning ? kAmber : kTeal;
            const Rect box{ body.x, y, body.w, NoteHeight() };
            g.Border(box, 4.f, c.WithAlpha(20.f / 255.f), c.WithAlpha(70.f / 255.f), 1.f);
            g.FillRect({ box.x, box.y + 8.f, 2.f, box.h - 16.f }, c.WithAlpha(0.8f));
            const Rect icon{ box.x + 10.f, box.CY() - 12.f, 24.f, 24.f };
            g.PushTransform(D2D1::Matrix3x2F::Rotation(wasRunning ? std::fmod(t * 90.f, 360.f) : 0.f, D2D1::Point2F(icon.CX(), icon.CY())));
            Glyph(ui, wasRunning ? Glyphs::Refresh : Glyphs::Info, icon, 13.f, c);
            g.PopTransform();
            const float nh = box.h - 26.f;
            g.Text(Note(), { box.x + 42.f, box.CY() - nh * 0.5f, box.w - 52.f, nh }, Lerp(c, Palette::Bright, 0.35f), Wrapped(10.f));
            EndReveal();
        }

        float right = bar.R() - 14.f;
        const bool live = closingAt < 0.0;
        if (BarButton("add", bar, right, Loc::Get("add_to_steam_btn"), true, live) || (live && ui.KeyPressed(VK_RETURN))) Start();
        else if (BarButton("cancel", bar, right, Loc::Get("cancel"), false, live)) Dismiss();
    }

    // ── Working: the tile in a spinning ring, then the steps (like the SteamCMD download) and a
    // progress bar ───────────────────────────────────────────────────────────────────────────────
    void DrawWorking(const Rect& body, const Rect& bar) {
        const double now = ui.Time();
        const float t = static_cast<float>(now);
        int row = 0;
        float y = body.y;
        {
            const float k = Reveal(row);
            BeginReveal(k, 0.f);
            const float cx = body.CX(), cy = y + (kSmallTile + 40.f) * 0.5f;
            constexpr float kRing = kSmallTile * 0.5f + 14.f;
            g.FillEllipse(cx, cy, kRing + 8.f, kRing + 8.f, accent.WithAlpha(0.05f));
            g.StrokeEllipse(cx, cy, kRing, kRing, Palette::ButtonBorder, 3.f);
            // Two arcs chasing each other (segments with round caps)
            const float spin = std::fmod(t * 4.2f, 6.2831853f);
            const auto Arc = [&](float start, float sweep, Color c) {
                constexpr int kSegments = 24;
                const float step = sweep / kSegments;
                for (int i = 0; i < kSegments; ++i) {
                    const float a0 = start + i * step, a1 = a0 + step;
                    g.Line(cx + std::cos(a0) * kRing, cy + std::sin(a0) * kRing, cx + std::cos(a1) * kRing, cy + std::sin(a1) * kRing, c, 3.f, true);
                }
            };
            Arc(spin, 1.6f, accent);
            Arc(spin + 3.1415927f, 0.7f, accent.WithAlpha(0.5f));
            DrawTile({ cx - kSmallTile * 0.5f, cy - kSmallTile * 0.5f, kSmallTile, kSmallTile }, 0.8f + 0.2f * Anim::EaseOutBack(k, 1.6f));
            EndReveal();
            y += kSmallTile + 40.f + 14.f;
        }
        {
            BeginReveal(Reveal(row));
            g.Text(Loc::Get("add_to_steam_working"), { body.x, y, body.w, 20.f }, Palette::Bright, Mono(13.f, true).Center());
            EndReveal();
            y += 20.f + 18.f;
        }

        // Steps: done (✓), running (spinner), waiting (ring), failed (✕); centered as a block
        float labelW = 0.f;
        for (const auto& s : steps) labelW = std::max(labelW, g.Measure(Loc::Get(s.key), Mono(11.f, true)).w);
        const float blockW = 20.f + 12.f + labelW;
        const float cx = body.CX() - blockW * 0.5f + 10.f;
        for (size_t i = 0; i < steps.size(); ++i) {
            const Step& s = steps[i];
            // Each step slides in from the left
            const float k = Reveal(row);
            BeginReveal(k, 0.f);
            ui.PushTranslate(-12.f * (1.f - k), 0.f);
            const float cy = y + kStepH * (static_cast<float>(i) + 0.5f);
            const float since = static_cast<float>(now - s.changedAt);
            // Connector to the next step, filling in once this one is done
            if (i + 1 < steps.size()) {
                const float y1 = cy + 11.f, y2 = cy + kStepH - 11.f;
                g.FillRect({ cx - 0.5f, y1, 1.f, y2 - y1 }, Rgba(58, 106, 138, 70));
                if (s.state == StepState::Done)
                    g.FillRect({ cx - 0.5f, y1, 1.f, (y2 - y1) * Anim::EaseOutCubic(Anim::Clamp01(since / 0.3f)) }, kTeal.WithAlpha(0.6f));
            }
            Color text = Rgba(90, 120, 145);
            bool bold = false;
            switch (s.state) {
                case StepState::Pending: g.StrokeEllipse(cx, cy, 8.f, 8.f, Rgba(58, 106, 138, 140), 1.f); break;
                case StepState::Active:
                    Form::Spinner(ui, cx, cy, 8.f, kTeal);
                    text = Rgba(214, 228, 238);
                    bold = true;
                    break;
                case StepState::Done:
                case StepState::Failed: {
                    const bool ok = s.state == StepState::Done;
                    const Color c = ok ? kTeal : kRed;
                    const float pop = Anim::EaseOutBack(Anim::Clamp01(since / 0.35f), 2.f);
                    const float burst = Anim::Clamp01(since / 0.5f);
                    if (burst < 1.f) g.StrokeEllipse(cx, cy, 9.f + 8.f * burst, 9.f + 8.f * burst, c.WithAlpha(0.6f * (1.f - burst)), 1.5f);
                    g.FillEllipse(cx, cy, 9.f * pop, 9.f * pop, c.WithAlpha(40.f / 255.f));
                    g.StrokeEllipse(cx, cy, 9.f * pop, 9.f * pop, c, 1.f);
                    Glyph(ui, ok ? Glyphs::CheckMark : Glyphs::Cancel, { cx - 9.f, cy - 9.f, 18.f, 18.f }, 9.f * pop, c);
                    text = ok ? Rgba(120, 200, 180) : kRed;
                    break;
                }
            }
            g.Text(Loc::Get(s.key), { cx + 10.f + 12.f, cy - 12.f, labelW + 4.f, 24.f }, text, Mono(11.f, bold));
            ui.PopTransform();
            EndReveal();
        }
        y += kStepH * 3.f + 14.f;

        // Progress bar with a light sweeping along the filled part, "2 / 3 · 66%" and the time
        {
            BeginReveal(Reveal(row));
            const Rect track{ body.x, y, body.w, 8.f };
            g.FillRound(track, 4.f, Hex(0x0D2535));
            if (progress > 0.f) {
                const Rect fill{ track.x, track.y, std::max(8.f, track.w * progress), track.h };
                g.GlowRound(fill, 4.f, accent, 10.f, 0.35f);
                g.FillLinear(fill, 4.f, { { 0.f, Rgba(0, 150, 125) }, { 1.f, accent } }, { 0.f, 0.f }, { 1.f, 0.f });
                const float sweep = std::fmod(t * 0.9f, 1.3f) - 0.15f;
                g.PushRoundClip(fill, 4.f);
                g.FillLinear({ fill.x + fill.w * sweep - 40.f, fill.y, 80.f, fill.h }, 0.f,
                             { { 0.f, Rgba(255, 255, 255, 0) }, { 0.5f, Rgba(255, 255, 255, 70) }, { 1.f, Rgba(255, 255, 255, 0) } }, { 0.f, 0.f }, { 1.f, 0.f });
                g.PopLayer();
            }
            y += 8.f + 8.f;
            size_t done = 0;
            for (const auto& s : steps) done += s.state == StepState::Done ? 1 : 0;
            const Rect status{ body.x, y, body.w, 16.f };
            g.Text(std::to_wstring(done) + L" / " + std::to_wstring(steps.size()) + L"  \x00B7  " + std::to_wstring(static_cast<int>(std::round(progress * 100.f))) + L"%",
                   status, kTeal, Mono(10.f, true));
            const int secs = static_cast<int>(now - phaseAt);
            wchar_t clock[16];
            swprintf_s(clock, L"%02d:%02d", secs / 60, secs % 60);
            g.Text(clock, status, kDim, Mono(10.f).Right());
            EndReveal();
        }

        // The bar explains why there's no button
        const Rect hint = bar.Inset(Edges{ 20.f, 0.f });
        Form::Spinner(ui, hint.x + 7.f, hint.CY(), 6.f, kAmber);
        g.Text(Loc::Get("add_to_steam_working_hint"), hint.CutLeft(22.f), kAmber.WithAlpha(0.85f), Mono(10.f, true).Ellipsis());
    }

    // ── Done / Failed: a big badge that bursts in (success) or shakes (error), the message, Close ─
    void DrawResult(const Rect& body, const Rect& bar) {
        const float t = static_cast<float>(ui.Time());
        const bool ok = phase == Phase::Done;
        const float since = static_cast<float>(ui.Time() - phaseAt);
        TextStyle headStyle = Wrapped(14.f, true);
        headStyle.align = HAlign::Center;
        TextStyle textStyle = Wrapped(10.5f);
        textStyle.align = HAlign::Center;
        const float headH = g.Measure(resultHead, headStyle, body.w).h;
        const float textH = resultBody.empty() ? 0.f : g.Measure(resultBody, textStyle, body.w).h;
        constexpr float kBadge = 84.f;
        const float total = kBadge + 24.f + headH + (textH > 0.f ? 8.f + textH : 0.f);
        float y = body.y + std::max(0.f, (body.h - total) * 0.5f);
        int row = 0;
        {
            const float k = Reveal(row);
            const float shake = ok ? 0.f : 9.f * std::sin(since * 40.f) * (1.f - Anim::Clamp01(since / 0.5f));
            const float cx = body.CX() + shake, cy = y + kBadge * 0.5f;
            const float r = kBadge * 0.5f * Anim::EaseOutBack(k, 1.8f);
            // Rings expanding out, then a slow halo
            for (int i = 0; i < 2; ++i) {
                const float b = Anim::Clamp01((since - 0.12f * i) / 0.7f);
                const float rr = kBadge * 0.5f + 30.f * Anim::EaseOutCubic(b);
                if (b > 0.f && b < 1.f) g.StrokeEllipse(cx, cy, rr, rr, accent.WithAlpha(0.55f * (1.f - b)), 2.f);
            }
            const float halo = r + 9.f + 3.f * std::sin(t * 2.f);
            g.FillEllipse(cx, cy, halo, halo, accent.WithAlpha(0.08f));
            // Success: sparks flying out
            if (ok && since < 1.f) {
                for (int i = 0; i < 12; ++i) {
                    const float a = 6.2831853f * (static_cast<float>(i) / 12.f) + 0.3f;
                    const float d = kBadge * 0.5f + 8.f + 44.f * Anim::EaseOutCubic(since);
                    const float s = 3.f * (1.f - since);
                    g.FillEllipse(cx + std::cos(a) * d, cy + std::sin(a) * d, s, s, i % 2 ? kTeal : kGreen);
                }
            }
            g.FillEllipse(cx, cy, r, r, accent.WithAlpha(40.f / 255.f));
            g.StrokeEllipse(cx, cy, r, r, accent, 2.f);
            // The glyph pops in (glowing) a beat after the badge
            const float ik = Anim::EaseOutBack(Anim::Clamp01((since - 0.15f) / 0.35f), 2.f);
            if (ik > 0.f) {
                const Rect glyph{ cx - r, cy - r, r * 2.f, r * 2.f };
                const std::wstring mark(1, ok ? Glyphs::CheckMark : Glyphs::Cancel);
                g.GlowText(mark, glyph, IconFont(30.f), accent, 16.f, 0.6f * std::min(1.f, ik));
                Glyph(ui, mark[0], glyph, 30.f * ik, accent);
            }
            y += kBadge + 24.f;
        }
        {
            BeginReveal(Reveal(row));
            g.Text(resultHead, { body.x, y, body.w, headH }, ok ? Palette::Bright : kRed, headStyle);
            y += headH + 8.f;
            if (textH > 0.f) g.Text(resultBody, { body.x, y, body.w, textH }, kDim, textStyle);
            EndReveal();
        }

        float right = bar.R() - 14.f;
        const bool live = closingAt < 0.0;
        if (BarButton("ok", bar, right, Loc::Get("close"), true, live) || (live && ui.KeyPressed(VK_RETURN))) Dismiss();
    }

    float Progress() const {
        if (steps.empty()) return 0.f;
        float done = 0.f;
        for (const auto& s : steps) done += s.state == StepState::Done ? 1.f : s.state == StepState::Active ? 0.4f : 0.f;
        return done / static_cast<float>(steps.size());
    }

    void SetStep(size_t index, StepState state) {
        if (index >= steps.size()) return;
        steps[index].state = state;
        steps[index].changedAt = ui.Time();
        Invalidate();
    }

    // Closing / restarting Steam takes seconds: off the UI thread, reporting every step
    void Start() {
        if (phase != Phase::Confirm || closingAt >= 0.0) return;
        steps.clear();
        if (wasRunning) steps.push_back({ "add_to_steam_step_close" });
        steps.push_back({ "add_to_steam_step_write" });
        if (wasRunning) steps.push_back({ "add_to_steam_step_start" });
        SetPhase(Phase::Working);
        Async::Run([this, exe = exePath, icon = iconPath, restart = wasRunning] {
            const auto Mark = [this](size_t i, StepState state) { Async::Post([this, i, state] { SetStep(i, state); }); };
            const auto Paced = [](const auto& work) {
                const ULONGLONG begin = GetTickCount64();
                work();
                const ULONGLONG spent = GetTickCount64() - begin;
                if (spent < kMinStepMs) Sleep(static_cast<DWORD>(kMinStepMs - spent));
            };
            size_t i = 0;
            std::wstring error;
            const auto steamPath = S::FindSteamPath();
            if (!steamPath) {
                error = Loc::Get("add_to_steam_error_no_steam");
                Mark(0, StepState::Failed);
            }
            if (error.empty() && restart) {
                Mark(i, StepState::Active);
                Paced([&] { S::CloseSteam(*steamPath); });
                Mark(i++, StepState::Done);
            }
            if (error.empty()) {
                Mark(i, StepState::Active);
                Paced([&] { error = S::AddOrUpdateShortcut(exe, icon); });
                Mark(i++, error.empty() ? StepState::Done : StepState::Failed);
            }
            // Steam is opened again either way: it was running before
            if (restart && steamPath) {
                Mark(i, StepState::Active);
                Paced([&] { S::StartSteam(*steamPath); });
                Mark(i++, StepState::Done);
            }
            // A beat for the last check to land before the result
            Sleep(350);
            Async::Post([this, error] { Finish(error); });
        });
    }

    void Finish(const std::wstring& error) {
        const bool ok = error.empty();
        std::tie(resultHead, resultBody) = SplitFirstLine(ok ? Loc::Get("add_to_steam_success") : Loc::Fmt("add_to_steam_error_generic", error));
        MessageBeep(ok ? MB_ICONINFORMATION : MB_ICONERROR);
        SetPhase(ok ? Phase::Done : Phase::Failed);
    }

    // Centered on the screen's work area the first time, then only resized (it may have been dragged)
    void Layout(float heightDips) {
        const float scale = DpiScale();
        const int w = static_cast<int>((kContentW + kPadX * 2.f + 2.f) * scale);
        const int h = static_cast<int>(heightDips * scale);
        if (!placed) {
            RECT area{};
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
            SetWindowPos(hwnd, HWND_TOPMOST, area.left + (area.right - area.left - w) / 2, area.top + (area.bottom - area.top - h) / 2, w, h, SWP_NOACTIVATE);
            placed = layoutHeight > 0.f;
        } else {
            SetWindowPos(hwnd, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
        layoutHeight = heightDips;
    }

    std::wstring exePath, iconPath;
    bool wasRunning;
    Phase phase = Phase::Confirm;
    std::vector<Step> steps;
    std::wstring resultHead, resultBody;
    double openedAt = -1.0, phaseAt = 0.0, closingAt = -1.0;
    Color accent = kTeal, accentFrom = kTeal;
    float progress = 0.f;
    float layoutHeight = 0.f;
    float decor = 0.f;   // height of the transparent decorations strip (0 = none)
    bool placed = false;
};

} // namespace

int RunSteamShortcutHelper(int argc, wchar_t** argv) {
    if (argc < 4) return 1;
    SteamHelperWindow window(argv[2], argv[3], S::IsSteamRunning());
    window.onClosed = [] { PostQuitMessage(0); };
    if (!window.Open()) return 1;
    return App::RunMessageLoop();
}

} // namespace SpeedrunLauncher::App
