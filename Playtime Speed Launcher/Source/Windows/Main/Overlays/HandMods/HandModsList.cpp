// Hand Mods → mod list: fetch (cached per chapter), rows with info / preview / extras buttons and
// install / uninstall, plus the dialogs those buttons open.
#include <algorithm>
#include <memory>
#include "Core/Async.h"
#include "Core/Text.h"
#include "Graphics/Animation.h"
#include "Graphics/Images.h"
#include "Localization/Localization.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Dialogs/DialogParts.h"
#include "Windows/Dialogs/DialogWindow.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/HandMods/HandModsKit.h"
#include "Windows/Main/Overlays/HandMods/HandModsOverlay.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
using namespace HandKit;
namespace HM = Services::HandMods;
namespace P = Dialogs::Parts;

namespace {

constexpr Color kRowText = Rgba(210, 220, 230), kRowSub = Rgba(45, 90, 120), kRemove = Rgba(204, 51, 51, 200);

constexpr unsigned kViewerSide = 900;   // big enough for the viewer's stage on a high-DPI screen
constexpr unsigned kThumbSide = 360;    // Individual Hands cards

// Hand pictures decoded on the worker thread (only the GPU upload is left for the UI thread)
struct DecodedHand {
    std::wstring color;
    unsigned w = 0, h = 0;
    std::vector<uint32_t> pixels;
};

std::vector<DecodedHand> DecodeHands(const std::vector<HM::HandImage>& images, unsigned maxSide) {
    std::vector<DecodedHand> out;
    for (const auto& image : images) {
        DecodedHand d;
        d.color = image.color;
        if (!Images::DecodeMemory(image.data, maxSide, d.w, d.h, d.pixels)) d.pixels.clear();
        out.push_back(std::move(d));
    }
    return out;
}

std::vector<HandPreview> UploadHands(const std::vector<DecodedHand>& decoded) {
    std::vector<HandPreview> out;
    for (const auto& d : decoded) out.push_back({ d.color, d.pixels.empty() ? nullptr : Images::FromPixels(d.w, d.h, d.pixels.data()) });
    return out;
}

std::wstring ChapterTitle(int n) { return Loc::Get("ch" + std::to_string(n) + "_title"); }

} // namespace

// ── Loading ─────────────────────────────────────────────────────────────────

void HandModsOverlay::LoadMods() {
    mods.reset();
    loadError.clear();
    if (auto it = cache.find(chapter); it != cache.end()) {
        mods = it->second;
        return;
    }
    loading = true;
    const int number = chapter;
    Async::Run([number]() -> std::pair<std::vector<HM::Mod>, std::wstring> {
        try {
            return { HM::GetMods(number), L"" };
        } catch (const std::exception& e) {
            return { {}, Text::Widen(e.what()) };
        }
    }, [this, number](std::pair<std::vector<HM::Mod>, std::wstring> result) {
        loading = false;
        if (result.second.empty()) cache[number] = result.first;
        if (chapter != number) return;   // the overlay moved on meanwhile
        if (!result.second.empty()) loadError = result.second;
        else mods = result.first;
    });
}

// ── Rows ────────────────────────────────────────────────────────────────────

void HandModsOverlay::DrawMods(Ui& ui, Column& col) {
    auto& g = ui.g;
    const double now = Async::Now();
    const float labelH = g.Measure(L"Ag", Mono(10.f, true)).h;

    // Error: badge, message and a retry button
    if (!loadError.empty()) {
        StaggerItem in(ui, screenChangedAt + 0.04, 0, 0.f, 12.f, 0.0, 0.4);
        const float msgH = g.Measure(loadError, Mono(9.f).Wrap().Center(), col.w - 40.f).h;
        const Rect r = col.Next(150.f + msgH);
        Badge(ui, r.CX(), r.y + 34.f, 22.f, std::wstring(1, Glyphs::Warning), IconFont(16.f), Palette::DangerText);
        g.Text(Loc::Get("hm_load_failed"), { r.x, r.y + 68.f, r.w, 18.f }, Palette::DangerText, Mono(11.f, true).Center());
        g.Text(loadError, { r.x + 20.f, r.y + 90.f, r.w - 40.f, msgH }, kSub, Mono(9.f).Wrap().Center());
        const float w = P::SmallButtonWidth(ui, Loc::Get("hm_retry"), 90.f);
        if (P::SmallButton(ui, ui.MakeId("hand-retry"), { r.CX() - w * 0.5f, r.y + 102.f + msgH, w, 28.f }, Loc::Get("hm_retry"), Palette::Teal)) {
            cache.erase(chapter);
            LoadMods();
            screenChangedAt = now;   // cascade the skeleton in again
        }
        return;
    }
    // Loading: placeholder cards with a light sweeping over them
    if (!mods) {
        g.Text(Loc::Get("hm_fetching"), col.Next(labelH, 10.f), kSub.Fade(0.6f + 0.4f * Anim::PingPong(now, 1.2)), Mono(10.f, true));
        for (int i = 0; i < 4; ++i) {
            StaggerItem in(ui, screenChangedAt + 0.05, i, 0.f, 10.f, 0.06, 0.4);
            const Rect r = col.Next(64.f, 8.f);
            g.Border(r, 8.f, kCardBg, kCardBorder, 1.f);
            g.FillEllipse(r.x + 32.f, r.CY(), 17.f, 17.f, Rgba(255, 255, 255, 14));
            g.FillRound({ r.x + 62.f, r.y + 17.f, r.w * (0.32f + 0.08f * (i % 3)), 10.f }, 5.f, Rgba(255, 255, 255, 18));
            g.FillRound({ r.x + 62.f, r.y + 35.f, r.w * 0.16f, 8.f }, 4.f, Rgba(255, 255, 255, 12));
            g.FillRound({ r.R() - 104.f, r.CY() - 14.f, 90.f, 28.f }, 4.f, Rgba(255, 255, 255, 12));
            Shimmer(g, r, 8.f, now + i * 0.12, 1.4, 1.4f);
        }
        ui.KeepAnimatingThrottled();
        return;
    }
    if (mods->empty()) {
        StaggerItem in(ui, screenChangedAt + 0.04, 0, 0.f, 12.f, 0.0, 0.4);
        const Rect r = col.Next(130.f);
        // The hand waves for a few seconds while nobody has made one yet
        const float calm = Calm(now - screenChangedAt, 3.0);
        const float wave = std::sin(static_cast<float>(now) * 5.f) * 14.f * calm;
        g.FillEllipse(r.CX(), r.y + 36.f, 24.f, 24.f, Palette::Teal.WithAlpha(0.10f));
        g.PushTransform(D2D1::Matrix3x2F::Rotation(wave, D2D1::Point2F(r.CX(), r.y + 48.f)));
        g.Text(L"\x270B", { r.CX() - 24.f, r.y + 12.f, 48.f, 48.f }, Palette::Teal, Mono(24.f).WithFont(Font::Ui).Center());
        g.PopTransform();
        g.Text(Loc::Get("hm_none"), { r.x, r.y + 72.f, r.w, 18.f }, kText, Mono(11.f, true).Center());
        g.Text(Loc::Get("hm_none_hint"), { r.x, r.y + 94.f, r.w, 16.f }, kSub, Mono(9.f).Center());
        if (calm > 0.f) ui.KeepAnimatingThrottled();
        return;
    }

    // No row action replaces the list while it's drawn (reloads only come from the screens before it),
    // so no per-frame copy; the actions get their own copy of the mod
    const std::vector<HM::Mod>& list = *mods;
    const std::vector<bool>& isInstalled = InstalledFlags();
    const int installedCount = static_cast<int>(std::count(isInstalled.begin(), isInstalled.end(), true));
    {
        StaggerItem in(ui, screenChangedAt, 0, 0.f, 8.f, 0.0, 0.3);
        const Rect r = col.Next(labelH, 10.f);
        const std::wstring summary = Loc::Fmt(list.size() == 1 ? "hm_mod_count_one" : "hm_mod_count", list.size()) + L"  \x00B7  " + ChapterTitle(chapter);
        g.Text(summary, r, kSub, Mono(10.f, true).Ellipsis());
        if (installedCount > 0) {
            const std::wstring text = Loc::Fmt("hm_installed_count", installedCount);
            Pill(g, r.R() - PillWidth(g, text), r.CY(), text, kGreen, true);
        }
    }

    for (size_t i = 0; i < list.size(); ++i) {
        const HM::Mod& mod = list[i];
        const bool installedNow = isInstalled[i];
        StaggerItem in(ui, screenChangedAt + 0.05, static_cast<int>(i), 26.f * screenDirection, 0.f, 0.045, 0.38);
        ui.PushId(static_cast<int64_t>(i));
        // Installing / removing it fades the green tint in and out
        const float lit = ui.Animate(ui.MakeId("installed"), installedNow ? 1.f : 0.f, 8.f, installedNow ? 1.f : 0.f);
        const Rect slot = col.Next(64.f, 8.f);
        const Card c = DrawCard(ui, ui.MakeId("card"), slot, kGreen, 8.f, lit, true, false);
        const Rect inner = c.r.Inset(14.f, 0.f);
        const float cy = c.r.CY();

        // Hand badge (a ✓ joins it once installed)
        const float bx = inner.x + 18.f;
        const float bob = std::sin(static_cast<float>(now) * 3.f + static_cast<float>(i)) * 1.5f * c.hover;
        g.FillEllipse(bx, cy, 18.f, 18.f, Lerp(Palette::Teal.WithAlpha(0.10f), kGreen.WithAlpha(0.18f), lit));
        g.StrokeEllipse(bx, cy, 18.f, 18.f, Lerp(Palette::Teal.WithAlpha(0.35f), kGreen.WithAlpha(0.7f), lit), 1.f);
        g.Text(L"\x270B", { bx - 18.f, cy - 18.f + bob, 36.f, 36.f }, Palette::Teal, Mono(16.f).WithFont(Font::Ui).Center());
        if (lit > 0.01f) {
            const float pop = Anim::EaseOutBack(lit, 1.4f);
            g.FillEllipse(bx + 13.f, cy + 12.f, 7.f * pop, 7.f * pop, kGreen);
            g.Text(std::wstring(1, Glyphs::CheckMark), { bx + 6.f, cy + 5.f, 14.f, 14.f }, Palette::PanelBg.WithAlpha(lit), IconFont(7.f));
        }

        // Actions (right to left): Install / Uninstall / "unavailable", extras, preview, info
        RowFromRight right{ inner.R(), inner.y, inner.h };
        if (installedNow) {
            const float w = P::SmallButtonWidth(ui, Loc::Get("hm_uninstall"), 90.f);
            const Rect b = right.Next(w, 8.f);
            if (P::SmallButton(ui, ui.MakeId("uninstall"), { b.x, cy - 14.f, w, 28.f }, Loc::Get("hm_uninstall"), kRemove)) Uninstall(HM::Mod(mod));
        } else if (!paks) {
            const float w = g.Measure(Loc::Get("hm_unavailable"), Mono(10.f)).w + 1.f;
            g.Text(Loc::Get("hm_unavailable"), right.Next(w, 8.f), Rgba(160, 180, 200, 140), Mono(10.f));
        } else {
            const float w = P::SmallButtonWidth(ui, Loc::Get("hm_install"), 90.f);
            const Rect b = right.Next(w, 8.f);
            // The install button breathes while the card is hovered
            if (c.hover > 0.01f) g.GlowRound({ b.x, cy - 14.f, w, 28.f }, 3.f, Palette::Teal, 12.f, 0.35f * c.hover);
            if (P::SmallButton(ui, ui.MakeId("install"), { b.x, cy - 14.f, w, 28.f }, Loc::Get("hm_install"), Palette::Teal)) StartInstall(HM::Mod(mod));
        }
        static const struct {
            const char* id;
            const wchar_t* key;   // busyButtons suffix
            wchar_t glyph;
            void (HandModsOverlay::*action)(const HM::Mod&);
        } buttons[] = { { "extras", L"extras", 0xE8B7, &HandModsOverlay::ShowExtras },
                        { "preview", L"preview", 0xEB9F, &HandModsOverlay::ShowPreview },
                        { "info", L"info", 0xE946, &HandModsOverlay::ShowInfo } };
        for (const auto& b : buttons) {
            const Rect slotB = right.Next(28.f, 4.f);
            const Rect br{ slotB.x, cy - 14.f, 28.f, 28.f };
            const bool busy = !busyButtons.empty() && busyButtons.count(mod.baseName + b.key) != 0;
            if (busy) {
                // Waiting on the download: spinner instead of the icon
                g.FillRound(br, 14.f, Rgba(100, 130, 160, 40));
                Form::Spinner(ui, br.CX(), br.CY(), 7.f, Palette::Teal);
                ui.KeepAnimatingThrottled();
                continue;
            }
            const ButtonStyle s{ Rgba(100, 130, 160, static_cast<int>(20 + 30 * c.hover)), Colors::Transparent, 0.f, 14.f, { 0.f }, 0.f, 28.f };
            if (TextButton(ui, ui.MakeId(b.id), br, std::wstring(1, b.glyph), IconFont(12.f), Rgba(160, 180, 200, 220), s))
                (this->*b.action)(HM::Mod(mod));
        }

        const float textX = bx + 30.f + 3.f * c.hover;
        const float textW = std::max(0.f, right.right - 8.f - textX);
        g.Text(mod.name, { textX, c.r.y + 13.f, textW, 20.f }, Lerp(kRowText, Colors::White, c.hover), Mono(12.f).SemiBold().Ellipsis());
        float sx = textX;
        const std::wstring size = Text::FileSize(mod.size);
        const float sizeW = g.Measure(size, Mono(9.f)).w + 1.f;
        g.Text(size, { sx, c.r.y + 34.f, sizeW, 16.f }, kRowSub, Mono(9.f));
        sx += sizeW + 8.f;
        if (lit > 0.01f && sx + 70.f < textX + textW) {
            const bool fading = lit < 0.999f;   // a layer only while it fades
            if (fading) g.PushOpacity(lit);
            Pill(g, sx, c.r.y + 42.f, Loc::Get("hm_installed"), kGreen, true);
            if (fading) g.PopLayer();
        }
        ui.PopId();
    }
}

const std::vector<bool>& HandModsOverlay::InstalledFlags() {
    const void* list = mods ? static_cast<const void*>(mods->data()) : nullptr;
    const size_t count = mods ? mods->size() : 0;
    if (installedRevision != listRevision || installedList != list || installed.size() != count) {
        installed.assign(count, false);
        for (size_t i = 0; i < count; ++i) installed[i] = paks && HM::IsInstalled(*paks, (*mods)[i].baseName);
        installedRevision = listRevision;
        installedList = list;
    }
    return installed;
}

void HandModsOverlay::Uninstall(const HM::Mod& mod) {
    if (!paks) return;
    const std::wstring dir = *paks, baseName = mod.baseName;
    Async::Run([dir, baseName] {
        HM::UninstallByBaseName(dir, baseName);
        return true;
    }, [this](bool) { ++listRevision; });
}

// ── Info / preview / extras (each downloads the zip, cached) ────────────────

void HandModsOverlay::ShowInfo(const HM::Mod& mod) {
    const std::wstring key = mod.baseName + L"info";
    busyButtons.insert(key);
    const int number = chapter;
    Async::Run([mod, number]() -> std::wstring {
        const auto zip = HM::Download(mod, number, {});
        if (!zip) return Loc::Fmt("hm_info_failed", mod.name);
        const auto hands = HM::ReadDeclaredHands(*zip);
        return hands ? Loc::Fmt("hm_info_changes", mod.name, Text::Join(*hands, L", ")) : Loc::Fmt("hm_info_no_color", mod.name);
    }, [this, key](std::wstring message) {
        busyButtons.erase(key);
        ShowMessage(message);
    });
}

void HandModsOverlay::ShowPreview(const HM::Mod& mod) {
    const std::wstring key = mod.baseName + L"preview";
    busyButtons.insert(key);
    const int number = chapter;
    Async::Run([mod, number]() -> std::optional<std::vector<DecodedHand>> {
        const auto zip = HM::Download(mod, number, {});
        if (!zip) return std::nullopt;
        return DecodeHands(HM::ReadHandImages(*zip), kViewerSide);
    }, [this, key, mod](std::optional<std::vector<DecodedHand>> decoded) {
        busyButtons.erase(key);
        if (!decoded) {
            ShowMessage(Loc::Fmt("hm_preview_failed", mod.name));
            return;
        }
        if (!popup.IsOpen()) return;
        viewer = {};
        viewer.open = true;
        viewer.mod = mod;
        viewer.images = UploadHands(*decoded);
        viewer.openedAt = Async::Now();
    });
}

void HandModsOverlay::ShowExtras(const HM::Mod& mod) {
    const std::wstring key = mod.baseName + L"extras";
    busyButtons.insert(key);
    const int number = chapter;
    struct Loaded {
        std::optional<std::wstring> zip;
        std::vector<HM::Extra> extras;
        std::vector<DecodedHand> images;
    };
    Async::Run([mod, number] {
        Loaded l;
        l.zip = HM::Download(mod, number, {});
        if (l.zip) {
            l.extras = HM::ReadExtras(*l.zip);
            if (!l.extras.empty()) l.images = DecodeHands(HM::ReadHandImages(*l.zip), kThumbSide);
        }
        return l;
    }, [this, key, mod](Loaded loaded) {
        busyButtons.erase(key);
        if (!loaded.zip) {
            ShowMessage(Loc::Fmt("hm_extras_failed", mod.name));
            return;
        }
        if (loaded.extras.empty()) {
            ShowMessage(Loc::Fmt("hm_extras_none", mod.name));
            return;
        }
        if (!popup.IsOpen()) return;
        const int generation = extrasPanel.generation + 1;
        extrasPanel = {};
        extrasPanel.generation = generation;
        extrasPanel.open = true;
        extrasPanel.mod = mod;
        extrasPanel.zip = *loaded.zip;
        extrasPanel.extras = std::move(loaded.extras);
        extrasPanel.previews = UploadHands(loaded.images);
        extrasPanel.doneAt.assign(extrasPanel.extras.size(), -10.0);
        extrasPanel.openedAt = Async::Now();
    });
}

} // namespace SpeedrunLauncher::Main
