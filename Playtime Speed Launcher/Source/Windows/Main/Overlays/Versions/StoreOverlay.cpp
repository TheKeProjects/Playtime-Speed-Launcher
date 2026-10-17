#include "Windows/Main/Overlays/Versions/StoreOverlay.h"
#include <algorithm>
#include <cmath>
#include "App/Launcher.h"
#include "App/Paths.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Graphics/Animation.h"
#include "Graphics/Images.h"
#include "Localization/Localization.h"
#include "Services/Platforms/SteamCmdRunner.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/TextBox.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/Versions/PresetInstaller.h"
#include "Windows/Main/Overlays/Versions/StoreKit.h"

namespace SpeedrunLauncher::Main {

using namespace UI;

using namespace StoreKit;


void StoreOverlay::Open(int chapterIndex) {
    const auto& chapters = App::L().chapters;
    if (chapterIndex < 0 || chapterIndex >= static_cast<int>(chapters.size()) || !chapters[static_cast<size_t>(chapterIndex)].isAvailable) chapterIndex = 0;
    featured = chapterIndex;
    chapter = -1;
    pageShownAt = Async::Now() + 0.15;   // after the panel's entrance
    pageFromX = 40.f;
    popup.Open();
}

void StoreOverlay::ShowChapter(int index) {
    if (product >= 0 && chapter == index) pageFromX = -40.f;   // back from a product page
    else pageFromX = 40.f;
    chapter = index;
    product = -1;
    pageShownAt = Async::Now();
}

void StoreOverlay::SetStep(int step) {
    form.step = step;
    form.shownAt = Async::Now();
}

void StoreOverlay::ShowProduct(int presetIndex) {
    product = presetIndex;
    method = 0;
    form = InstallForm{};
    pageShownAt = Async::Now();
    pageFromX = 40.f;
}

void StoreOverlay::ShowReady(int chapterNumber, const std::wstring& manifestId) {
    auto& l = App::L();
    for (size_t ci = 0; ci < l.chapters.size(); ++ci) {
        if (l.chapters[ci].number != chapterNumber || !l.chapters[ci].isAvailable) continue;
        const auto& presets = l.chapters[ci].presets;
        for (size_t pi = 0; pi < presets.size(); ++pi) {
            if (presets[pi].manifestId != manifestId) continue;
            if (!popup.IsOpen()) popup.Open();
            if (chapter == static_cast<int>(ci) && product == static_cast<int>(pi)) return;   // already there
            chapter = static_cast<int>(ci);
            product = static_cast<int>(pi);
            method = 0;
            form = InstallForm{};
            pageShownAt = Async::Now();
            pageFromX = 40.f;
            return;
        }
    }
}

void StoreOverlay::GoBack() {
    if (product >= 0 && form.step > 0) SetStep(form.step == 3 ? 2 : 0);
    else if (product >= 0) ShowChapter(chapter);
    else if (chapter >= 0) ShowFront();
    else popup.Close();
}

void StoreOverlay::ShowFront() {
    featured = chapter >= 0 ? chapter : featured;
    chapter = -1;
    product = -1;
    pageShownAt = Async::Now();
    pageFromX = -40.f;
}

// ── Banner ──────────────────────────────────────────────────────────────────

void StoreOverlay::DrawBanner(Ui& ui, const Rect& r, int chapterIndex, bool front) {
    auto& g = ui.g;
    auto& l = App::L();
    const auto& ch = l.chapters[static_cast<size_t>(chapterIndex)];

    g.Shadow(r.Offset(0.f, 8.f), 12.f, 16.f, Argb(0x70000000));
    g.PushRoundClip(r, 12.f);
    g.FillRect(r, Hex(0x0A1420));
    // Art on the right two thirds, fading into the dark side where the text sits
    const float artX = std::round(r.x + r.w * 0.30f);   // whole pixel: no half-covered seam column
    const Rect art{ artX, r.y, r.R() - artX, r.h };
    g.PushClip(art);
    DrawCoverImage(ui, BannerPath(ch.number), art, 1.f, 2048);
    g.PopClip();
    g.FillRect({ r.x, r.y, art.x - r.x + 3.f, r.h }, Hex(0x0A1420));   // hides the art's antialiased left edge
    g.FillLinear(art, 0.f, { { 0.f, Hex(0x0A1420) }, { 0.45f, Hex(0x0A1420, 0.55f) }, { 1.f, Hex(0x0A1420, 0.f) } }, { 0.f, 0.f }, { 1.f, 0.f });
    g.FillLinear(r, 0.f, { { 0.f, Palette::Teal.WithAlpha(0.14f) }, { 0.6f, Palette::Teal.WithAlpha(0.f) } }, { 0.f, 1.f }, { 0.6f, 0.f });
    g.PopLayer();
    g.StrokeRound(r.Inset(0.5f), 12.f, Rgba(255, 255, 255, 24), 1.f);

    // Text block
    const float x = r.x + 28.f, w = r.w * 0.5f;
    float y = r.y + 24.f;
    {
        const std::wstring tag = front ? Loc::Get("store_featured") : Loc::Fmt("store_chapter", ch.number);
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

    std::wstring line = VersionsCount(ch.presets.size());
    if (front) line = Loc::Fmt("store_chapter", ch.number) + L"  \x00B7  " + line;
    g.Text(line, { x, y, w, 18.f }, kDimText, Mono(11.f).Ellipsis());

    if (!front) return;
    // BROWSE button
    const std::wstring browse = Loc::Get("store_browse") + L"  \x203A";
    const TextStyle bs = Mono(11.f, true).Center();
    const float bw = std::max(130.f, g.Measure(browse, bs).w + 36.f);
    const Rect b{ x, r.B() - 24.f - 36.f, bw, 36.f };
    ButtonStyle style{ Palette::Teal, Colors::Transparent, 0.f, 18.f, { 0.f }, 0.f, 36.f };
    const Id id = ui.MakeId("store-browse");
    const float glow = ui.Animate(HashId(id, 1), ui.Hot(id) ? 1.f : 0.f, 14.f);
    if (glow > 0.01f) g.GlowRound(b, 18.f, Palette::Teal, 16.f, 0.6f * glow);
    if (TextButton(ui, id, b, browse, bs, Rgba(3, 17, 13), style)) ShowChapter(chapterIndex);
}

// ── Pages ───────────────────────────────────────────────────────────────────

float StoreOverlay::DrawFront(Ui& ui, float x, float y, float w) {
    auto& g = ui.g;
    auto& l = App::L();
    const float top = y;
    int reveal = 0;
    {
        StaggerItem in(ui, pageShownAt, reveal++, pageFromX);
        DrawBanner(ui, { x, y, w, 210.f }, featured, true);
    }
    y += 210.f + 26.f;
    {
        StaggerItem in(ui, pageShownAt, reveal++, pageFromX);
        y += SectionTitle(ui, x, y, w, Loc::Get("store_chapters"), Loc::Get("store_chapters_hint"));
    }

    const float tileW = (w - kGap * (kColumns - 1.f)) / kColumns, coverH = tileW * kCoverAspect;
    const float nameH = g.Measure(L"Ag", Mono(12.f).SemiBold()).h, subH = g.Measure(L"Ag", Mono(10.f)).h;
    const float rowH = coverH + 10.f + nameH + 2.f + subH + 22.f;
    int position = 0;
    for (size_t i = 0; i < l.chapters.size(); ++i) {
        const auto& ch = l.chapters[i];
        if (!Listed(ch)) continue;
        const int col = position % static_cast<int>(kColumns), row = position / static_cast<int>(kColumns);
        ++position;
        const Rect slot{ x + col * (tileW + kGap), y + row * rowH, tileW, coverH };
        StaggerItem in(ui, pageShownAt, reveal++, 0.f, 26.f, 0.06);
        const Tile t = CoverTile(ui, ui.MakeId("store-chapter", static_cast<int64_t>(i)), slot, CoverPath(ch.number));
        if (static_cast<int>(i) == featured) Badge(ui, t.cover, Loc::Get("store_featured"), Palette::Teal, Rgba(3, 17, 13));
        // BROWSE hint rising over the art on hover
        if (t.hover > 0.01f) {
            const std::wstring hint = Loc::Get("store_browse") + L"  \x203A";
            const Rect h{ t.cover.x, t.cover.B() - 36.f - 8.f * (1.f - t.hover), t.cover.w, 26.f };
            g.Text(hint, h, Palette::Teal.Fade(t.hover), Mono(10.f, true).Center());
        }
        const float ty = slot.B() + 10.f;
        g.Text(ChapterTitle(ch), { slot.x + 2.f, ty, slot.w - 4.f, nameH }, t.hover > 0.5f ? Palette::Teal : kTitle, Mono(12.f).SemiBold().Ellipsis());
        g.Text(Loc::Fmt("store_chapter", ch.number) + L"  \x00B7  " + VersionsCount(ch.presets.size()),
               { slot.x + 2.f, ty + nameH + 2.f, slot.w - 4.f, subH }, kDimText, Mono(10.f).Ellipsis());
        if (t.it.clicked) ShowChapter(static_cast<int>(i));
    }
    const int rows = (position + static_cast<int>(kColumns) - 1) / static_cast<int>(kColumns);
    y += rows * rowH;
    return y - top;
}

float StoreOverlay::DrawChapter(Ui& ui, float x, float y, float w) {
    auto& g = ui.g;
    auto& l = App::L();
    const auto& ch = l.chapters[static_cast<size_t>(chapter)];
    const float top = y;
    int reveal = 0;
    {
        StaggerItem in(ui, pageShownAt, reveal++, pageFromX);
        DrawBanner(ui, { x, y, w, 170.f }, chapter, false);
    }
    y += 170.f + 26.f;
    {
        StaggerItem in(ui, pageShownAt, reveal++, pageFromX);
        y += SectionTitle(ui, x, y, w, Loc::Get("store_versions"), Loc::Get("store_versions_hint"));
    }
    if (ch.presets.empty()) {
        g.Text(Loc::Get("store_no_versions"), { x, y + 10.f, w, 40.f }, kDimText, Mono(11.f).Center());
        return y + 60.f - top;
    }

    const float tileW = (w - kGap * (kColumns - 1.f)) / kColumns, coverH = tileW * kCoverAspect;
    const float nameH = g.Measure(L"Ag", Mono(12.f).SemiBold()).h, subH = g.Measure(L"Ag", Mono(10.f)).h;
    const float buttonH = 30.f, logH = 16.f;
    const float rowH = coverH + 10.f + nameH + 2.f + subH + 10.f + buttonH + 4.f + logH + 18.f;
    const int chapterNumber = ch.number;
    bool anyDownloading = false;

    for (size_t i = 0; i < ch.presets.size(); ++i) {
        const auto preset = ch.presets[i];   // copy: installing can refresh the chapter list
        const int col = static_cast<int>(i) % static_cast<int>(kColumns), row = static_cast<int>(i) / static_cast<int>(kColumns);
        const Rect slot{ x + col * (tileW + kGap), y + row * rowH, tileW, coverH };
        StaggerItem in(ui, pageShownAt, reveal++, 0.f, 26.f, 0.05);

        // Every version can always be installed (again, or into another folder) — the store doesn't
        // track what's installed; which copy PLAY uses is chosen in the versions menu
        const bool downloading = installer.IsDownloading(preset.manifestId);
        anyDownloading |= downloading;

        const Id tileId = ui.MakeId("store-version", static_cast<int64_t>(i));
        const Tile t = CoverTile(ui, tileId, slot, VersionCoverPath(chapterNumber, preset.name), !downloading);
        if (downloading) {
            const float pulse = 0.6f + 0.4f * static_cast<float>(std::sin(Async::Now() * 4.0));
            Badge(ui, t.cover, Loc::Get("downloading"), kAmber.WithAlpha(pulse), Rgba(20, 12, 0));
        } else if (installer.ReadyFor(preset.manifestId) || installer.MovingStatus(preset.manifestId)) {
            Badge(ui, t.cover, L"\x2713  " + Loc::Get("store_ready_badge"), Palette::Teal, Rgba(3, 17, 13));
        }

        // Name and size under the cover (like a store's title + price)
        float ty = slot.B() + 10.f;
        g.Text(TranslatePresetName(preset.name), { slot.x + 2.f, ty, slot.w - 4.f, nameH }, t.hover > 0.5f ? Palette::Teal : kTitle,
               Mono(12.f).SemiBold().Ellipsis());
        ty += nameH + 2.f;
        g.Text(preset.downloadSize.empty() ? std::wstring(L"\x2014") : preset.downloadSize, { slot.x + 2.f, ty, slot.w - 4.f, subH }, kDimText,
               Mono(10.f).Ellipsis());
        ty += subH + 10.f;

        // Action
        const Rect b{ slot.x, ty, slot.w, buttonH };
        const TextStyle bs = Mono(10.f, true).Center();
        bool act = t.it.clicked;
        if (downloading) {
            g.FillRound(b, 6.f, kAmber.WithAlpha(0.12f));
            g.Text(Loc::Get("downloading"), b, kAmber, bs);
            const auto tail = installer.LogTail(preset.manifestId, 1);
            if (!tail.empty()) g.Text(tail.back(), { slot.x, b.B() + 4.f, slot.w, logH }, kAmber.WithAlpha(0.7f), Mono(9.f).Ellipsis());
            act = false;
        } else {
            const ButtonStyle style{ Palette::Teal, Colors::Transparent, 0.f, 6.f, { 0.f }, 0.f, buttonH };
            if (TextButton(ui, ui.MakeId("store-action", static_cast<int64_t>(i)), b, Loc::Get("install_btn"), bs, Rgba(3, 17, 13), style))
                act = true;
        }
        if (act) ShowProduct(static_cast<int>(i));
    }
    if (anyDownloading) ui.KeepAnimatingThrottled();   // badge pulse + log line
    const int rows = (static_cast<int>(ch.presets.size()) + static_cast<int>(kColumns) - 1) / static_cast<int>(kColumns);
    y += rows * rowH;
    return y - top;
}

// ── Product page ────────────────────────────────────────────────────────────

float StoreOverlay::DrawProduct(Ui& ui, float x, float y, float w) {
    auto& g = ui.g;
    auto& l = App::L();
    const auto& ch = l.chapters[static_cast<size_t>(chapter)];
    const auto preset = ch.presets[static_cast<size_t>(product)];   // copy: installing can refresh the chapter list
    const bool downloading = installer.IsDownloading(preset.manifestId);
    const float top = y;
    int reveal = 0;

    // ── Cover (left) ──
    const float coverW = 300.f, coverH = coverW * kCoverAspect;
    {
        StaggerItem in(ui, pageShownAt, reveal++, -30.f);
        const Rect cover{ x, y, coverW, coverH };
        g.Shadow(cover.Offset(0.f, 10.f), kRadius, 22.f, Argb(0x90000000));
        g.GlowRound(cover, kRadius, Palette::Teal, 26.f, 0.35f);
        g.PushRoundClip(cover, kRadius);
        DrawCoverImage(ui, VersionCoverPath(ch.number, preset.name), cover, 1.f);
        g.FillLinear(cover.Bottom(cover.h * 0.35f), 0.f, { { 0.f, Rgba(0, 0, 0, 0) }, { 1.f, Rgba(0, 0, 0, 150) } }, { 0.f, 0.f }, { 0.f, 1.f });
        g.PopLayer();
        g.StrokeRound(cover.Inset(0.5f), kRadius, Palette::Teal.WithAlpha(0.5f), 1.f);
        if (downloading) {
            const float pulse = 0.6f + 0.4f * static_cast<float>(std::sin(Async::Now() * 4.0));
            Badge(ui, cover, Loc::Get("downloading"), kAmber.WithAlpha(pulse), Rgba(20, 12, 0));
        }
    }

    // ── Details (right) ──
    const float rx = x + coverW + 34.f, rw = w - coverW - 34.f;
    float ry = y;
    {
        StaggerItem in(ui, pageShownAt, reveal++, 30.f);
        const std::wstring tag = Loc::Fmt("store_chapter", ch.number) + L"  \x00B7  " + ChapterTitle(ch);
        const TextStyle tagStyle = Mono(9.f, true).Center();
        const float tw = g.Measure(tag, tagStyle).w + 18.f;
        g.FillRound({ rx, ry, tw, 20.f }, 10.f, Palette::Teal.WithAlpha(0.18f));
        g.StrokeRound({ rx + 0.5f, ry + 0.5f, tw - 1.f, 19.f }, 9.5f, Palette::Teal.WithAlpha(0.6f), 1.f);
        g.Text(tag, { rx, ry, tw, 20.f }, Palette::Teal, tagStyle);
        ry += 20.f + 12.f;
        const TextStyle nameStyle = Mono(28.f, true).Ellipsis();
        const float nameH = g.Measure(L"Ag", nameStyle).h;
        const std::wstring name = TranslatePresetName(preset.name);
        g.GlowText(name, { rx, ry, rw, nameH }, nameStyle, Palette::Teal, 14.f, 0.4f);
        g.Text(name, { rx, ry, rw, nameH }, kTitle, nameStyle);
        ry += nameH + 4.f;
        const std::wstring size = preset.downloadSize.empty() ? std::wstring(L"\x2014") : preset.downloadSize;
        g.Text(size + L"  \x00B7  SteamCMD  \x00B7  depot " + std::to_wstring(preset.depotId), { rx, ry, rw, 18.f }, kDimText, Mono(11.f).Ellipsis());
        ry += 18.f + 18.f;
        g.FillLinear({ rx, ry, rw, 1.f }, 0.f, { { 0.f, Palette::Teal.WithAlpha(0.5f) }, { 1.f, Palette::Teal.WithAlpha(0.f) } }, { 0.f, 0.f }, { 1.f, 0.f });
        ry += 1.f + 18.f;
    }

    // Install steps (or the download's progress, then naming / moving the files), one at a time
    const auto movingStatus = installer.MovingStatus(preset.manifestId);
    const bool isReady = installer.ReadyFor(preset.manifestId) != nullptr;
    const bool notFound = !downloading && installer.NotFoundFor(preset.manifestId);
    const bool phoneCheck = downloading && installer.PhoneCheck(preset.manifestId);
    const bool phoneWaiting = downloading && !phoneCheck && installer.PhoneWaiting(preset.manifestId);
    {
        // Where we are: AUTO  method → account → confirm (phone) → download → save
        //               MANUAL method → steps → wait for Steam → save
        const bool manual = !downloading && (form.step == 2 || form.step == 3 || ((isReady || movingStatus || notFound) && method == 1));
        std::vector<std::wstring> labels;
        int current = 0;
        if (manual) {
            labels = { Loc::Get("store_step_method"), Loc::Get("store_step_steps"), Loc::Get("store_step_wait"), Loc::Get("store_step_save") };
            current = (isReady || movingStatus) ? 3 : (notFound || form.step == 3) ? 2 : form.step == 2 ? 1 : 0;
        } else {
            labels = { Loc::Get("store_step_method"), Loc::Get("store_step_account"), Loc::Get("store_step_confirm"), Loc::Get("store_step_phone"),
                       Loc::Get("store_step_download"), Loc::Get("store_step_save") };
            current = (isReady || movingStatus) ? 5 : phoneCheck ? 2 : phoneWaiting ? 3 : (downloading || notFound) ? 4 : form.step == 1 ? 1 : 0;
        }
        StaggerItem in(ui, pageShownAt, reveal++, 30.f);
        ry = DrawStepper(ui, rx, ry, rw, labels, current, notFound) + 22.f;
    }
    if (downloading) {
        StaggerItem in(ui, pageShownAt, reveal++, 30.f);
        if (phoneCheck) {
            // First: Steam will ask the phone to approve the sign-in
            ry = DrawPhoneCard(ui, preset, rx, ry, rw);
        } else if (phoneWaiting) {
            // Then: approve it in the Steam app (the download takes over by itself)
            ry = DrawPhoneWaitCard(ui, preset, rx, ry, rw);
        } else {
            TextStyle logStyle = Mono(9.f);
            logStyle.wrap = true;
            logStyle.valign = VAlign::Top;
            std::wstring log;
            for (const auto& line : installer.LogTail(preset.manifestId, 6)) log += (log.empty() ? L"" : L"\n") + line;
            if (log.empty()) log = Loc::Get("steamcmd_initializing");
            const float logH = std::min(g.Measure(log, logStyle, rw - 28.f).h, 96.f);
            const Rect box{ rx, ry, rw, 34.f + logH + 14.f };
            g.Border(box, 10.f, kAmber.WithAlpha(0.08f), kAmber.WithAlpha(0.4f), 1.f);
            const float pulse = 0.6f + 0.4f * static_cast<float>(std::sin(Async::Now() * 4.0));
            g.FillEllipse(box.x + 20.f, box.y + 18.f, 4.f, 4.f, kAmber.WithAlpha(pulse));
            g.Text(Loc::Get("downloading"), { box.x + 32.f, box.y + 8.f, rw - 44.f, 20.f }, kAmber, Mono(11.f, true));
            g.PushClip({ box.x + 14.f, box.y + 34.f, rw - 28.f, logH });
            const float fullH = g.Measure(log, logStyle, rw - 28.f).h;
            g.Text(log, { box.x + 14.f, box.y + 34.f - (fullH - logH), rw - 28.f, fullH }, kAmber.WithAlpha(0.75f), logStyle);
            g.PopClip();
            ui.KeepAnimatingThrottled();
            ry += box.h;
        }
    } else if (notFound) {
        ry = DrawNotFoundStep(ui, preset, rx, ry, rw);
    } else if (movingStatus) {
        ry = DrawMovingStep(ui, *movingStatus, rx, ry, rw);
    } else if (isReady) {
        ry = DrawReadyStep(ui, preset, ch.number, rx, ry, rw);
    } else {
        switch (form.step) {
        case 1: ry = DrawAccountStep(ui, preset, ch.number, rx, ry, rw); break;
        case 2: ry = DrawManualStep(ui, preset, rx, ry, rw); break;
        case 3: ry = DrawWaitingStep(ui, preset, ch.number, rx, ry, rw); break;
        default: {
            StaggerItem in(ui, pageShownAt, reveal++, 30.f);
            ry = DrawMethodStep(ui, preset, rx, ry, rw);
        }
        }
    }
    return std::max(coverH, ry - y) + (y - top);
}

float StoreOverlay::StepHeader(Ui& ui, float x, float y, float w, const std::wstring& title) {
    auto& g = ui.g;
    // "‹ METHOD" goes back to the method choice
    const std::wstring back = L"\x2039  " + Loc::Get("store_method_short");
    const TextStyle bs = Mono(9.f, true).Center();
    const float bw = g.Measure(back, bs).w + 20.f;
    if (TextButton(ui, ui.MakeId("store-step-back"), { x, y, bw, 22.f }, back, bs, kDimText,
                   { Rgba(255, 255, 255, 12), Rgba(255, 255, 255, 30), 1.f, 11.f, { 0.f }, 0.f, 22.f }))
        SetStep(form.step == 3 ? 2 : 0);
    g.Text(title, { x + bw + 12.f, y, w - bw - 12.f, 22.f }, Palette::Teal, Mono(10.f, true));
    return y + 22.f + 14.f;
}

float StoreOverlay::DrawMethodStep(Ui& ui, const Models::ChapterPreset& preset, float rx, float ry, float rw) {
    auto& g = ui.g;
    g.Text(Loc::Get("store_method"), { rx, ry, rw, 16.f }, kDimText, Mono(9.f, true));
    ry += 16.f + 8.f;
    const float cardW = (rw - 14.f) * 0.5f;
    TextStyle desc = Mono(9.5f);
    desc.wrap = true;
    desc.valign = VAlign::Top;
    const float descW = cardW - 28.f;
    const float cardH = 14.f + 30.f + 10.f +
                        std::max(g.Measure(Loc::Get("install_mode_auto_desc"), desc, descW).h, g.Measure(Loc::Get("install_mode_manual_desc"), desc, descW).h) +
                        14.f;
    const struct {
        wchar_t glyph;
        const char* title;
        const char* desc;
        bool recommended;
    } cards[2] = { { 0xE896, "install_mode_auto_title", "install_mode_auto_desc", true },
                   { 0xE756, "install_mode_manual_title", "install_mode_manual_desc", false } };
    for (int i = 0; i < 2; ++i) {
        const Rect r{ rx + i * (cardW + 14.f), ry, cardW, cardH };
        const Id id = ui.MakeId("store-method", i);
        const Interaction it = ui.Interact(id, r);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        if (it.clicked) method = i;
        const bool on = method == i;
        const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 14.f);
        const float sel = ui.Animate(HashId(id, 2), on ? 1.f : 0.f, 14.f);
        if (sel > 0.01f) g.GlowRound(r, 10.f, Palette::Teal, 16.f, 0.45f * sel);
        g.Border(r, 10.f, Palette::Teal.WithAlpha(0.04f + 0.08f * sel + 0.04f * hover),
                 sel > 0.5f ? Palette::Teal : Rgba(255, 255, 255, static_cast<int>(25 + 30 * hover)), 1.f + sel);
        const Rect icon{ r.x + 14.f, r.y + 14.f, 30.f, 30.f };
        g.FillRound(icon, 15.f, Palette::Teal.WithAlpha(0.12f + 0.12f * sel));
        Glyph(ui, cards[i].glyph, icon, 13.f, on ? Palette::Teal : kDimText);
        g.Text(Loc::Get(cards[i].title), { icon.R() + 10.f, icon.y, cardW - 100.f, 30.f }, on ? kTitle : kDimText, Mono(13.f, true));
        if (cards[i].recommended) {
            const std::wstring rec = Loc::Get("store_recommended");
            const TextStyle rs = Mono(8.f, true).Center();
            const float recW = g.Measure(rec, rs).w + 12.f;
            const float tx = icon.R() + 10.f + g.Measure(Loc::Get(cards[i].title), Mono(13.f, true)).w + 8.f;
            g.FillRound({ tx, icon.CY() - 8.f, recW, 16.f }, 8.f, kAmber.WithAlpha(0.18f));
            g.Text(rec, { tx, icon.CY() - 8.f, recW, 16.f }, kAmber, rs);
        }
        const float rcx = r.R() - 22.f, rcy = icon.CY();
        g.StrokeEllipse(rcx, rcy, 8.f, 8.f, on ? Palette::Teal : Rgba(255, 255, 255, 60), 1.5f);
        if (sel > 0.01f) g.FillEllipse(rcx, rcy, 4.5f * sel, 4.5f * sel, Palette::Teal);
        g.Text(Loc::Get(cards[i].desc), { r.x + 14.f, icon.B() + 10.f, descW, r.B() - icon.B() - 10.f }, Rgba(150, 175, 195, on ? 230 : 150), desc);
    }
    ry += cardH + 24.f;

    const std::wstring label = preset.downloadSize.empty() ? Loc::Get("install_btn") : Loc::Fmt("install_with_size", preset.downloadSize);
    if (BigButton(ui, ui.MakeId("store-install"), { rx, ry, rw, 48.f }, 0xE896, label)) {
        if (method == 0) {
            // AUTO: ask for the Steam account here (saved accounts first)
            form.accounts = Services::SteamCredentials::LoadAll();
            form.account = form.accounts.empty() ? -1 : 0;
            form.user = Services::SteamCmd::LoggedInUsername().value_or(App::L().store.SteamUsername().value_or(L""));
            form.pass.clear();
            form.savePassword = false;
            SetStep(1);
        } else {
            form.depotFolder = PresetInstaller::ManualDepotFolder(preset);
            form.confirmed = false;
            SetStep(2);
        }
    }
    return ry + 48.f;
}

float StoreOverlay::DrawAccountStep(Ui& ui, const Models::ChapterPreset& preset, int chapterNumber, float rx, float ry, float rw) {
    auto& g = ui.g;
    int item = 0;
    {
        StaggerItem in(ui, form.shownAt, item++, 30.f);
        ry = StepHeader(ui, rx, ry, rw, Loc::Get("store_steam_account"));
    }

    // Saved accounts (+ "use another account")
    if (!form.accounts.empty()) {
        {
            StaggerItem in(ui, form.shownAt, item++, 30.f);
            g.Text(Loc::Get("saved_accounts"), { rx, ry, rw, 16.f }, kDimText, Mono(9.f, true));
        }
        ry += 16.f + 8.f;
        const auto accountCard = [&](int index, wchar_t glyph, const std::wstring& name) {
            StaggerItem in(ui, form.shownAt, item++, 30.f);
            const Rect r{ rx, ry, rw, 44.f };
            const Id id = ui.MakeId("store-account", index + 1);
            const Interaction it = ui.Interact(id, r);
            if (it.hovered) ui.SetCursor(IDC_HAND);
            if (it.clicked) {
                form.account = index;
                if (index < 0) form.pass.clear();
            }
            const bool on = form.account == index;
            const float sel = ui.Animate(HashId(id, 2), on ? 1.f : 0.f, 14.f);
            const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 14.f);
            if (sel > 0.01f) g.GlowRound(r, 10.f, Palette::Teal, 14.f, 0.4f * sel);
            g.Border(r, 10.f, Palette::Teal.WithAlpha(0.04f + 0.08f * sel + 0.04f * hover),
                     sel > 0.5f ? Palette::Teal : Rgba(255, 255, 255, static_cast<int>(25 + 30 * hover)), 1.f + sel);
            const Rect icon{ r.x + 10.f, r.CY() - 13.f, 26.f, 26.f };
            g.FillRound(icon, 13.f, Palette::Teal.WithAlpha(0.12f + 0.12f * sel));
            Glyph(ui, glyph, icon, 11.f, on ? Palette::Teal : kDimText);
            g.Text(name, { icon.R() + 12.f, r.y, r.w - 90.f, r.h }, on ? kTitle : kDimText, Mono(11.f, true).Ellipsis());
            g.StrokeEllipse(r.R() - 22.f, r.CY(), 8.f, 8.f, on ? Palette::Teal : Rgba(255, 255, 255, 60), 1.5f);
            if (sel > 0.01f) g.FillEllipse(r.R() - 22.f, r.CY(), 4.5f * sel, 4.5f * sel, Palette::Teal);
            ry += r.h + 8.f;
        };
        for (size_t i = 0; i < form.accounts.size(); ++i) accountCard(static_cast<int>(i), 0xE77B, form.accounts[i].username);
        accountCard(-1, 0xE710, Loc::Get("use_another_account"));
        ry += 6.f;
    }

    // Typed-in account
    if (form.account < 0) {
        StaggerItem in(ui, form.shownAt, item++, 30.f);
        Column col(rx, ry, rw);
        Form::Label(ui, col, 0xE77B, Loc::Get("username_placeholder"), Palette::Teal, true);
        Form::Field(ui, ui.MakeId("store-user"), col.Next(TextBoxHeight(ui, FieldStyle()), 12.f), form.user, FieldStyle(), Palette::Teal);
        Form::Label(ui, col, 0xE72E, Loc::Get("password_placeholder"), Palette::Teal);
        Form::Field(ui, ui.MakeId("store-pass"), col.Next(TextBoxHeight(ui, FieldStyle(true)), 12.f), form.pass, FieldStyle(true), Palette::Teal);
        col.Gap(CheckRow(ui, ui.MakeId("store-save-pass"), col.x, col.y, col.w, Loc::Get("save_password"), form.savePassword) + 6.f);
        ry = col.y + 10.f;
    }

    // START DOWNLOAD
    StaggerItem in(ui, form.shownAt, item++, 30.f);
    const std::wstring user = form.account >= 0 ? form.accounts[static_cast<size_t>(form.account)].username : std::wstring(form.user);
    const bool ready = !user.empty() && user.find_first_not_of(L' ') != std::wstring::npos;
    if (BigButton(ui, ui.MakeId("store-start"), { rx, ry, rw, 48.f }, 0xE768, Loc::Get("start_download"), ready)) {
        if (form.account >= 0) {
            const auto& account = form.accounts[static_cast<size_t>(form.account)];
            installer.StartAutoWith(preset, chapterNumber, account.username, Services::SteamCredentials::Decrypt(account));
        } else {
            std::wstring name = form.user;
            name.erase(0, name.find_first_not_of(L' '));
            name.erase(name.find_last_not_of(L' ') + 1);
            std::optional<std::wstring> pass;
            if (!form.pass.empty()) pass = form.pass;
            if (form.savePassword && pass) Services::SteamCredentials::Save(name, *pass);
            installer.StartAutoWith(preset, chapterNumber, name, pass);
        }
        form.pass.clear();
        SetStep(0);   // the page now shows the download
    }
    return ry + 48.f;
}

float StoreOverlay::DrawManualStep(Ui& ui, const Models::ChapterPreset& preset, float rx, float ry, float rw) {
    auto& g = ui.g;
    int item = 0;
    {
        StaggerItem in(ui, form.shownAt, item++, 30.f);
        ry = StepHeader(ui, rx, ry, rw, Text::Upper(Loc::Get("install_manual_title_plain")));
    }
    TextStyle body = Mono(10.5f);
    body.wrap = true;
    body.valign = VAlign::Top;
    const auto step = [&](int number, const std::wstring& text, const std::wstring& code, Color codeColor) {
        StaggerItem in(ui, form.shownAt, item++, 30.f);
        const float textX = rx + 40.f, textW = rw - 40.f;
        const float th = g.Measure(text, body, textW).h;
        // Number bubble + line to the next step
        g.FillRound({ rx, ry, 26.f, 26.f }, 13.f, Palette::Teal.WithAlpha(0.16f));
        g.StrokeRound({ rx + 0.5f, ry + 0.5f, 25.f, 25.f }, 12.5f, Palette::Teal.WithAlpha(0.6f), 1.f);
        g.Text(std::to_wstring(number), { rx, ry, 26.f, 26.f }, Palette::Teal, Mono(11.f, true).Center());
        g.Text(text, { textX, ry + 4.f, textW, th }, Rgba(170, 195, 215, 230), body);
        float h = std::max(26.f, th + 4.f);
        if (!code.empty()) h += 8.f + CodeBox(ui, textX, ry + h + 8.f, textW, code, codeColor);
        ry += h + 14.f;
    };
    step(1, Loc::Get("install_manual_step1"), L"", Palette::Teal);
    step(2, Loc::Get("install_manual_step2"), preset.Command(), Palette::Teal);
    step(3, Loc::Get("install_manual_step3"), form.depotFolder, Rgba(90, 160, 230));
    step(4, Loc::Get("install_manual_step4"), L"", Palette::Teal);
    step(5, Loc::Get("install_manual_step5"), L"", Palette::Teal);

    StaggerItem in(ui, form.shownAt, item++, 30.f);
    ry += 4.f + CheckRow(ui, ui.MakeId("store-manual-ok"), rx, ry + 4.f, rw, Loc::Get("install_manual_confirm"), form.confirmed) + 18.f;
    if (BigButton(ui, ui.MakeId("store-console"), { rx, ry, rw, 48.f }, 0xE756, Loc::Get("install_manual_open_btn"), form.confirmed)) {
        installer.OpenManualConsole(preset);
        SetStep(3);
    }
    return ry + 48.f;
}

float StoreOverlay::DrawWaitingStep(Ui& ui, const Models::ChapterPreset& preset, int chapterNumber, float rx, float ry, float rw) {
    auto& g = ui.g;
    int item = 0;
    {
        StaggerItem in(ui, form.shownAt, item++, 30.f);
        ry = StepHeader(ui, rx, ry, rw, Loc::Get("store_waiting_steam"));
    }
    {
        StaggerItem in(ui, form.shownAt, item++, 30.f);
        TextStyle body = Mono(10.5f);
        body.wrap = true;
        body.valign = VAlign::Top;
        const std::wstring text = Loc::Get("install_manual_waiting");
        const float th = g.Measure(text, body, rw - 84.f).h;
        const Rect box{ rx, ry, rw, std::max(74.f, th + 36.f) };
        g.Border(box, 10.f, kAmber.WithAlpha(0.07f), kAmber.WithAlpha(0.35f), 1.f);
        Form::Spinner(ui, box.x + 34.f, box.CY(), 12.f, kAmber);
        g.Text(text, { box.x + 64.f, box.CY() - th * 0.5f, rw - 84.f, th }, Rgba(230, 200, 150, 230), body);
        ui.KeepAnimatingThrottled();
        ry += box.h + 24.f;
    }
    StaggerItem in(ui, form.shownAt, item++, 30.f);
    if (BigButton(ui, ui.MakeId("store-manual-done"), { rx, ry, rw, 48.f }, 0xE73E, Loc::Get("store_download_finished"))) {
        installer.ManualDownloaded(preset, chapterNumber);
        SetStep(0);
    }
    return ry + 48.f;
}

float StoreOverlay::DrawStepper(Ui& ui, float x, float y, float w, const std::vector<std::wstring>& labels, int current, bool failed) {
    auto& g = ui.g;
    const int n = static_cast<int>(labels.size());
    if (n < 2) return y;
    const float r = 12.f, seg = (w - 2.f * r) / static_cast<float>(n - 1), cy = y + r;
    // Track + the part already walked (eases to the current step)
    const float progress = ui.Animate(ui.MakeId("store-stepper"), static_cast<float>(current), 9.f);
    g.FillRound({ x + r, cy - 1.5f, w - 2.f * r, 3.f }, 1.5f, Rgba(255, 255, 255, 22));
    const float walked = seg * std::clamp(progress, 0.f, static_cast<float>(n - 1));
    if (walked > 0.5f) g.FillRound({ x + r, cy - 1.5f, walked, 3.f }, 1.5f, Palette::Teal);
    for (int i = 0; i < n; ++i) {
        const float cx = x + r + seg * static_cast<float>(i);
        const Rect dot{ cx - r, cy - r, 2.f * r, 2.f * r };
        const bool done = i < current, active = i == current;
        if (active && failed) {
            const Color red = Rgba(230, 70, 70);
            g.GlowRound(dot, r, red, 12.f, 0.8f);
            g.FillRound(dot, r, red);
            g.Text(L"!", dot, Colors::White, Mono(12.f, true).Center());
        } else if (active) {
            g.GlowRound(dot, r, Palette::Teal, 12.f, 0.7f);
            g.FillRound(dot, r, Hex(0x0A1420));
            g.StrokeRound(dot.Inset(1.f), r - 1.f, Palette::Teal, 2.f);
            g.Text(std::to_wstring(i + 1), dot, Palette::Teal, Mono(10.f, true).Center());
        } else if (done) {
            g.FillRound(dot, r, Palette::Teal);
            Glyph(ui, 0xE73E, dot, 10.f, Rgba(3, 17, 13));   // check
        } else {
            g.FillRound(dot, r, Hex(0x0A1420));
            g.StrokeRound(dot.Inset(0.5f), r - 0.5f, Rgba(255, 255, 255, 50), 1.f);
            g.Text(std::to_wstring(i + 1), dot, kDimText, Mono(10.f, true).Center());
        }
        const Color labelColor = active ? (failed ? Rgba(240, 110, 110) : Palette::Teal) : done ? kTitle.WithAlpha(0.75f) : kDimText.WithAlpha(0.7f);
        g.Text(labels[static_cast<size_t>(i)], { cx - 60.f, cy + r + 6.f, 120.f, 14.f }, labelColor, Mono(8.5f, true).Center());
    }
    return cy + r + 6.f + 14.f;
}

float StoreOverlay::DrawPhoneCard(Ui& ui, const Models::ChapterPreset& preset, float rx, float ry, float rw) {
    auto& g = ui.g;
    TextStyle body = Mono(10.f);
    body.wrap = true;
    body.valign = VAlign::Top;
    const std::wstring text = Loc::Get("steamguard_phone_body");
    const float textW = rw - 28.f - 44.f;
    const float th = g.Measure(text, body, textW).h;
    const Rect card{ rx, ry, rw, 16.f + 24.f + 8.f + th + 14.f + 32.f + 16.f };
    g.GlowRound(card, 12.f, Palette::Teal, 14.f, 0.3f);
    g.Border(card, 12.f, Palette::Teal.WithAlpha(0.08f), Palette::Teal.WithAlpha(0.6f), 1.f);
    const Rect icon{ card.x + 16.f, card.y + 16.f, 32.f, 32.f };
    g.FillRound(icon, 16.f, Palette::Teal.WithAlpha(0.18f));
    Glyph(ui, 0xE8EA, icon, 15.f, Palette::Teal);   // phone
    const float tx = icon.R() + 12.f;
    g.Text(Text::Upper(Loc::Get("steamguard_phone_title")), { tx, card.y + 16.f, textW, 24.f }, kTitle, Mono(12.f, true));
    g.Text(text, { tx, card.y + 16.f + 24.f + 8.f, textW, th }, Rgba(170, 195, 215, 230), body);
    const std::wstring ok = Loc::Get("steamguard_phone_continue");
    const TextStyle bs = Mono(10.f, true).Center();
    const float bw = std::max(120.f, g.Measure(ok, bs).w + 30.f);
    if (TextButton(ui, ui.MakeId("store-phone-ok"), { card.R() - 16.f - bw, card.B() - 16.f - 32.f, bw, 32.f }, ok, bs, Rgba(3, 17, 13),
                   { Palette::Teal, Colors::Transparent, 0.f, 8.f, { 0.f }, 0.f, 32.f }))
        installer.DismissPhoneCheck(preset.manifestId);
    return card.B();
}

float StoreOverlay::DrawPhoneWaitCard(Ui& ui, const Models::ChapterPreset& preset, float rx, float ry, float rw) {
    auto& g = ui.g;
    TextStyle body = Mono(10.5f);
    body.wrap = true;
    body.valign = VAlign::Top;
    const std::wstring text = Loc::Get("store_phone_wait_body");
    const float textW = rw - 40.f - 110.f;
    const float th = g.Measure(text, body, textW).h;
    const Rect card{ rx, ry, rw, std::max(150.f, 20.f + 22.f + 10.f + th + 18.f + 34.f + 20.f) };
    g.GlowRound(card, 12.f, Palette::Teal, 16.f, 0.35f);
    g.Border(card, 12.f, Palette::Teal.WithAlpha(0.08f), Palette::Teal.WithAlpha(0.6f), 1.f);

    // A phone with rings pulsing out of it
    const float t = static_cast<float>(std::fmod(Async::Now(), 1.6) / 1.6);
    const float cx = card.x + 20.f + 40.f, cy = card.y + card.h * 0.5f;
    for (int i = 0; i < 2; ++i) {
        const float k = std::fmod(t + 0.5f * static_cast<float>(i), 1.f);
        const float radius = 22.f + 22.f * k;
        g.StrokeEllipse(cx, cy, radius, radius, Palette::Teal.WithAlpha(0.55f * (1.f - k)), 1.5f);
    }
    const Rect phone{ cx - 15.f, cy - 24.f, 30.f, 48.f };
    g.FillRound(phone, 6.f, Hex(0x0A1420));
    g.StrokeRound(phone.Inset(1.f), 5.f, Palette::Teal, 2.f);
    g.FillRound({ phone.x + 5.f, phone.y + 7.f, phone.w - 10.f, phone.h - 18.f }, 2.f, Palette::Teal.WithAlpha(0.25f + 0.2f * (1.f - t)));
    Glyph(ui, 0xE73E, { phone.x + 5.f, phone.y + 7.f, phone.w - 10.f, phone.h - 18.f }, 11.f, Palette::Teal);   // approve
    g.FillRound({ phone.CX() - 3.f, phone.B() - 8.f, 6.f, 3.f }, 1.5f, Palette::Teal);

    const float tx = card.x + 20.f + 110.f;
    float y = card.y + 20.f;
    g.Text(Loc::Get("store_phone_wait_title"), { tx, y, textW, 22.f }, kTitle, Mono(12.f, true).Ellipsis());
    y += 22.f + 10.f;
    g.Text(text, { tx, y, textW, th }, Rgba(170, 195, 215, 230), body);
    y += th + 18.f;
    // Waiting dots + "I've approved it"
    Form::Spinner(ui, tx + 9.f, y + 17.f, 8.f, Palette::Teal);
    const std::wstring done = Loc::Get("store_phone_wait_done");
    const TextStyle bs = Mono(10.f, true).Center();
    const float bw = std::max(150.f, g.Measure(done, bs).w + 30.f);
    if (TextButton(ui, ui.MakeId("store-phone-done"), { card.R() - 20.f - bw, y, bw, 34.f }, done, bs, Palette::Teal,
                   { Palette::Teal.WithAlpha(0.12f), Palette::Teal.WithAlpha(0.6f), 1.f, 8.f, { 0.f }, 0.f, 34.f }))
        installer.SkipPhoneWaiting(preset.manifestId);
    ui.KeepAnimatingThrottled();
    return card.B();
}

float StoreOverlay::DrawReadyStep(Ui& ui, const Models::ChapterPreset& preset, int chapterNumber, float rx, float ry, float rw) {
    auto& g = ui.g;
    if (form.readyFor != preset.manifestId) {
        form.readyFor = preset.manifestId;
        form.readyName = TranslatePresetName(preset.name);
        form.readyIcon.reset();
        form.shownAt = Async::Now();
    }
    int item = 0;
    // ✓ DOWNLOAD READY
    {
        StaggerItem in(ui, form.shownAt, item++, 30.f);
        TextStyle body = Mono(10.5f);
        body.wrap = true;
        body.valign = VAlign::Top;
        const std::wstring text = Loc::Get("files_downloaded_msg");
        const float th = g.Measure(text, body, rw - 84.f).h;
        const Rect box{ rx, ry, rw, std::max(70.f, 18.f + 22.f + 4.f + th + 16.f) };
        g.GlowRound(box, 12.f, Palette::Teal, 14.f, 0.35f);
        g.Border(box, 12.f, Palette::Teal.WithAlpha(0.10f), Palette::Teal.WithAlpha(0.65f), 1.f);
        const Rect icon{ box.x + 16.f, box.y + 18.f, 34.f, 34.f };
        g.FillRound(icon, 17.f, Palette::Teal);
        Glyph(ui, 0xE73E, icon, 15.f, Rgba(3, 17, 13));   // check
        g.Text(Loc::Get("store_ready_title"), { icon.R() + 14.f, box.y + 16.f, rw - 84.f, 22.f }, kTitle, Mono(13.f, true));
        g.Text(text, { icon.R() + 14.f, box.y + 16.f + 22.f + 4.f, rw - 84.f, th }, Rgba(170, 195, 215, 230), body);
        ry += box.h + 20.f;
    }
    // Name
    {
        StaggerItem in(ui, form.shownAt, item++, 30.f);
        Column col(rx, ry, rw);
        Form::Label(ui, col, 0xE70F, Loc::Get("name_label"), Palette::Teal, true);
        Form::Field(ui, ui.MakeId("store-ready-name"), col.Next(TextBoxHeight(ui, FieldStyle()), 16.f), form.readyName, FieldStyle(), Palette::Teal);
        ry = col.y;
    }
    // Icon: preview + choose
    {
        StaggerItem in(ui, form.shownAt, item++, 30.f);
        Column col(rx, ry, rw);
        Form::Label(ui, col, 0xEB9F, Loc::Get("icon_label"), Palette::Teal);
        const Rect row = col.Next(56.f, 22.f);
        const Rect preview{ row.x, row.y, 56.f, 56.f };
        g.Shadow(preview.Offset(0.f, 4.f), 10.f, 8.f, Argb(0x80000000));
        g.FillRound(preview, 10.f, Hex(0x0A1420));
        if (auto img = Images::Load(form.readyIcon.value_or(VersionCoverPath(chapterNumber, preset.name)), 256)) g.DrawImageRounded(*img, preview, 10.f);
        g.StrokeRound(preview.Inset(0.5f), 9.5f, Palette::Teal.WithAlpha(0.5f), 1.f);
        const std::wstring choose = Loc::Get("choose_icon");
        const TextStyle cs = Mono(10.f, true).Center();
        const float cw = std::max(150.f, g.Measure(choose, cs).w + 30.f);
        if (TextButton(ui, ui.MakeId("store-ready-icon"), { preview.R() + 14.f, row.CY() - 18.f, cw, 36.f }, choose, cs, Palette::Teal,
                       { Palette::Teal.WithAlpha(0.12f), Palette::Teal.WithAlpha(0.6f), 1.f, 8.f, { 0.f }, 0.f, 36.f })) {
            const std::wstring file = System::PickFile(main.Hwnd(), L"", { { L"Images", L"*.jpg;*.jpeg;*.png;*.bmp" } });
            if (!file.empty()) form.readyIcon = file;
        }
        if (form.readyIcon) {
            const std::wstring reset = L"\x2715";
            if (TextButton(ui, ui.MakeId("store-ready-icon-reset"), { preview.R() + 14.f + cw + 8.f, row.CY() - 18.f, 36.f, 36.f }, reset,
                           Mono(11.f, true).WithFont(Font::Ui).Center(), kDimText, { Rgba(255, 255, 255, 12), Rgba(255, 255, 255, 40), 1.f, 8.f, { 0.f }, 0.f, 36.f }))
                form.readyIcon.reset();
        }
        ry = col.y;
    }
    // CHOOSE FOLDER AND SAVE  ·  later
    StaggerItem in(ui, form.shownAt, item++, 30.f);
    if (BigButton(ui, ui.MakeId("store-ready-save"), { rx, ry, rw, 48.f }, 0xE8B7, Loc::Get("store_save_to_folder"))) {   // folder
        const std::wstring folder = System::PickFolder(main.Hwnd(), L"");
        if (!folder.empty()) installer.Finish(preset.manifestId, form.readyName, form.readyIcon, folder);
    }
    ry += 48.f + 10.f;
    const std::wstring later = Loc::Get("later");
    const TextStyle ls = Mono(10.f, true).Center();
    const float lw = g.Measure(later, ls).w + 30.f;
    if (TextButton(ui, ui.MakeId("store-ready-later"), { rx + rw * 0.5f - lw * 0.5f, ry, lw, 28.f }, later, ls, kDimText, Styles::Flat())) {
        installer.Later(preset.manifestId);
        form.readyFor.clear();
    }
    return ry + 28.f;
}

float StoreOverlay::DrawNotFoundStep(Ui& ui, const Models::ChapterPreset& preset, float rx, float ry, float rw) {
    auto& g = ui.g;
    const Color red = Rgba(230, 70, 70);
    // A little shake when "look again" didn't find them either
    const float since = static_cast<float>(Async::Now() - retriedAt);
    float shake = 0.f;
    if (since < 0.45f) {
        shake = 7.f * (1.f - since / 0.45f) * static_cast<float>(std::sin(since * 55.0));
        ui.KeepAnimating();
    }
    ui.PushTranslate(shake, 0.f);

    TextStyle body = Mono(10.5f);
    body.wrap = true;
    body.valign = VAlign::Top;
    const std::wstring text = Loc::Get("files_not_found_content");
    const float textW = rw - 40.f - 58.f;
    const float th = g.Measure(text, body, textW).h;
    const Rect card{ rx, ry, rw, 20.f + 24.f + 10.f + th + 20.f };
    g.GlowRound(card, 12.f, red, 16.f, 0.35f);
    g.Border(card, 12.f, red.WithAlpha(0.08f), red.WithAlpha(0.6f), 1.f);
    const Rect icon{ card.x + 20.f, card.y + 20.f, 40.f, 40.f };
    g.FillRound(icon, 20.f, red.WithAlpha(0.18f));
    g.StrokeRound(icon.Inset(0.5f), 19.5f, red.WithAlpha(0.8f), 1.f);
    Glyph(ui, 0xE7BA, icon, 17.f, Rgba(245, 120, 120));   // warning
    const float tx = icon.R() + 18.f;
    g.Text(Text::Upper(Loc::Get("files_not_found_title")), { tx, card.y + 20.f, textW, 24.f }, kTitle, Mono(13.f, true));
    g.Text(text, { tx, card.y + 20.f + 24.f + 10.f, textW, th }, Rgba(175, 198, 218, 235), body);
    ry = card.B() + 20.f;
    ui.PopTransform();

    // Pick the folder (main) · look again · cancel
    if (BigButton(ui, ui.MakeId("store-nf-folder"), { rx, ry, rw, 48.f }, 0xE8B7, Text::Upper(Loc::Get("select_folder_manually")))) {   // folder
        const std::wstring folder = System::PickFolder(main.Hwnd(), L"");
        if (!folder.empty()) installer.UseFolder(preset.manifestId, folder);
    }
    ry += 48.f + 12.f;
    const float half = (rw - 12.f) * 0.5f;
    {
        const Rect b{ rx, ry, half, 38.f };
        ButtonScope button(ui, ui.MakeId("store-nf-retry"), b, { Palette::Teal.WithAlpha(0.12f), Palette::Teal.WithAlpha(0.6f), 1.f, 8.f, { 0.f }, 0.f, 38.f });
        const std::wstring label = Loc::Get("store_search_again");
        const TextStyle ls = Mono(10.f, true);
        const float lw = g.Measure(label, ls).w;
        const float gx = b.CX() - (14.f + 8.f + lw) * 0.5f;
        Glyph(ui, 0xE72C, { gx, b.y, 14.f, b.h }, 12.f, Palette::Teal);   // refresh
        g.Text(label, { gx + 22.f, b.y, lw + 2.f, b.h }, Palette::Teal, ls);
        if (button.Clicked()) {
            installer.RetryLocate(preset.manifestId);
            retriedAt = Async::Now();
        }
    }
    if (TextButton(ui, ui.MakeId("store-nf-cancel"), { rx + half + 12.f, ry, half, 38.f }, Loc::Get("cancel"), Mono(10.f, true).Center(), kDimText,
                   { Rgba(255, 255, 255, 10), Rgba(255, 255, 255, 40), 1.f, 8.f, { 0.f }, 0.f, 38.f }))
        installer.DismissNotFound(preset.manifestId);
    return ry + 38.f;
}

float StoreOverlay::DrawMovingStep(Ui& ui, const std::wstring& status, float rx, float ry, float rw) {
    auto& g = ui.g;
    const Rect box{ rx, ry, rw, 118.f };
    g.GlowRound(box, 12.f, Palette::Teal, 14.f, 0.3f);
    g.Border(box, 12.f, Palette::Teal.WithAlpha(0.08f), Palette::Teal.WithAlpha(0.55f), 1.f);
    Form::Spinner(ui, box.x + 36.f, box.y + 40.f, 13.f, Palette::Teal);
    g.Text(Text::Upper(Loc::Get("preparing_version_title")), { box.x + 66.f, box.y + 20.f, rw - 86.f, 22.f }, kTitle, Mono(13.f, true).Ellipsis());
    g.Text(status, { box.x + 66.f, box.y + 44.f, rw - 86.f, 18.f }, Rgba(170, 195, 215, 230), Mono(10.5f).Ellipsis());
    ProgressBarIndeterminate(ui, { box.x + 20.f, box.B() - 20.f - 8.f, rw - 40.f, 8.f });
    ui.KeepAnimatingThrottled();
    return box.B();
}

// ── Panel ───────────────────────────────────────────────────────────────────

void StoreOverlay::DrawHeader(Ui& ui, const Rect& header) {
    auto& g = ui.g;
    const Rect row = header.Inset(20.f, 0.f);
    // Cart badge + STORE (› CHAPTER N)
    const Rect icon{ row.x, row.CY() - 17.f, 34.f, 34.f };
    g.FillRound(icon, 17.f, Palette::Teal.WithAlpha(0.14f));
    g.StrokeRound(icon.Inset(0.5f), 16.5f, Palette::Teal.WithAlpha(0.55f), 1.f);
    Glyph(ui, 0xE7BF, icon, 15.f, Palette::Teal);   // shopping cart
    const TextStyle titleStyle = Mono(16.f, true);
    const std::wstring title = Loc::Get("store_title");
    const float tw = g.Measure(title, titleStyle).w + 2.f;
    const Rect titleRect{ icon.R() + 12.f, row.y, tw, row.h };
    if (chapter >= 0) {
        // The title goes back to the front page
        const Interaction it = ui.Interact(ui.MakeId("store-home"), titleRect);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        g.Text(title, titleRect, it.hovered ? kTitle : Palette::Teal, titleStyle);
        const auto& ch = App::L().chapters[static_cast<size_t>(chapter)];
        const TextStyle crumbStyle = Mono(12.f, true);
        float cx = titleRect.R() + 12.f;
        g.Text(L"\x203A", { cx, row.y, 12.f, row.h }, kDimText, crumbStyle);
        cx += 22.f;
        const std::wstring chapterCrumb = Loc::Fmt("store_chapter", ch.number);
        const float cw = g.Measure(chapterCrumb, crumbStyle).w + 2.f;
        if (product >= 0) {
            // The chapter crumb goes back to the chapter page
            const Interaction ci = ui.Interact(ui.MakeId("store-crumb-chapter"), { cx, row.y, cw, row.h });
            if (ci.hovered) ui.SetCursor(IDC_HAND);
            g.Text(chapterCrumb, { cx, row.y, cw, row.h }, ci.hovered ? kTitle : kDimText, crumbStyle);
            cx += cw + 12.f;
            g.Text(L"\x203A", { cx, row.y, 12.f, row.h }, kDimText, crumbStyle);
            cx += 22.f;
            const std::wstring version = Text::Upper(TranslatePresetName(ch.presets[static_cast<size_t>(product)].name));
            g.Text(version, { cx, row.y, 260.f, row.h }, Palette::Teal, crumbStyle);
            if (ci.clicked) ShowChapter(chapter);
        } else {
            g.Text(chapterCrumb, { cx, row.y, cw, row.h }, Palette::Teal, crumbStyle);
        }
        if (it.clicked) ShowFront();
    } else {
        g.GlowText(title, titleRect, titleStyle, Palette::Teal, 10.f, 0.45f);
        g.Text(title, titleRect, Palette::Teal, titleStyle);
    }
    if (TextButton(ui, ui.MakeId("store-close"), { row.R() - 30.f, row.CY() - 15.f, 30.f, 30.f }, L"\x2715", Mono(12.f, true).WithFont(Font::Ui).Center(),
                   Palette::Dim, { Palette::ButtonBg, Palette::ButtonBorder, 1.f, 6.f, { 0.f }, 0.f, 30.f }))
        popup.Close();
}

void StoreOverlay::Draw(Ui& ui, const Rect& screen) {
    auto& l = App::L();
    if (l.chapters.empty()) return;
    featured = std::clamp(featured, 0, static_cast<int>(l.chapters.size()) - 1);
    if (chapter >= static_cast<int>(l.chapters.size())) chapter = -1;
    if (chapter < 0 || product >= static_cast<int>(l.chapters[static_cast<size_t>(chapter)].presets.size())) product = -1;
    if (popup.IsOpen() && ui.KeyPressed(VK_ESCAPE)) GoBack();

    PanelStyle style;
    style.headerHeight = 60.f;
    style.radius = 10.f;
    OverlayFrame frame(ui, ui.MakeId("store"), screen, 1080.f, 720.f, style, popup.Opacity(), popup.Scale());
    DrawHeader(ui, frame.header);
    {
        const Id scrollId = ui.MakeId("store-scroll", chapter * 1000 + product);
        ScrollArea scroll(ui, scrollId, frame.body, false, 60.f);
        const float x = frame.body.x + kPad, w = scroll.ContentWidth() - kPad * 2.f;
        const float by = frame.body.y + kPad;
        const float h = product >= 0 ? DrawProduct(ui, x, by, w) : chapter >= 0 ? DrawChapter(ui, x, by, w) : DrawFront(ui, x, by, w);
        scroll.SetContentSize(h + kPad * 2.f);
    }

    // Footer: BACK (to the front page, or closes the store)
    const Rect footer = frame.footer.Inset(20.f, 0.f);
    const TextStyle text = Mono(11.f, true).Center();
    const std::wstring back = Loc::Get("back");
    const float bw = ButtonWidth(ui, back, text, Styles::Footer());
    if (TextButton(ui, ui.MakeId("store-back"), { footer.R() - bw, footer.CY() - 18.f, bw, 36.f }, back, text, Palette::Dim, Styles::Footer()))
        GoBack();
    if (frame.BackdropClicked()) popup.Close();
}

} // namespace SpeedrunLauncher::Main
