// Hand Mods → mod list: fetch (cached per chapter), rows with info / preview / extras buttons and
// install / uninstall, plus the dialogs those buttons open.
#include <algorithm>
#include <memory>
#include "Core/Async.h"
#include "Core/Text.h"
#include "Graphics/Images.h"
#include "Localization/Localization.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Dialogs/DialogParts.h"
#include "Windows/Dialogs/DialogWindow.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/HandMods/HandModsOverlay.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
namespace HM = Services::HandMods;
namespace P = Dialogs::Parts;

namespace {

constexpr Color kRowText = Rgba(210, 220, 230), kRowSub = Rgba(45, 90, 120), kRemove = Rgba(204, 51, 51, 200);

// Decoded preview images, keyed by colour (decoded once per dialog).
struct Previews {
    std::vector<HM::HandImage> raw;
    std::vector<ImagePtr> decoded;
    ImagePtr For(const std::wstring& color) {
        if (decoded.size() != raw.size()) {
            decoded.clear();
            for (const auto& image : raw) decoded.push_back(Images::FromMemory(image.data));
        }
        for (size_t i = 0; i < raw.size(); ++i)
            if (_wcsicmp(raw[i].color.c_str(), color.c_str()) == 0) return decoded[i];
        return nullptr;
    }
};

void Note(Ui& ui, Column& col, const std::wstring& text, Color color) {
    col.Gap(12.f);
    col.Gap(Paragraph(ui, col.x + 4.f, col.y, col.w - 8.f, text, Mono(11.f), color));
}

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
    if (!loadError.empty()) {
        Note(ui, col, L"Failed to load mods:\n" + loadError, Rgba(204, 51, 51, 220));
        return;
    }
    if (!mods) {
        Note(ui, col, L"Fetching mods\x2026", Palette::TealDim);
        ui.KeepAnimatingThrottled();
        return;
    }
    if (mods->empty()) {
        Note(ui, col, L"No hand mods available for this chapter yet.", Rgba(160, 180, 200, 160));
        return;
    }
    const float nameH = g.Measure(L"Ag", Mono(13.f).SemiBold()).h, subH = g.Measure(L"Ag", Mono(10.f)).h;
    const std::vector<HM::Mod> list = *mods;   // rows can trigger reloads
    for (size_t i = 0; i < list.size(); ++i) {
        const HM::Mod& mod = list[i];
        const bool installed = paks && HM::IsInstalled(*paks, mod.baseName);
        const Rect r = col.Next(8.f + std::max(nameH + subH, 28.f) + 8.f, 2.f);
        g.FillRound(r, 4.f, installed ? Rgba(0, 204, 170, 30) : Rgba(255, 255, 255, 12));
        const Rect inner = r.Inset(10.f, 8.f);
        ui.PushId(static_cast<int64_t>(i));

        // Actions (right to left): Install / Uninstall / "unavailable", extras, preview, info
        RowFromRight right{ inner.R(), inner.y, inner.h };
        const float cy = inner.CY();
        if (installed) {
            const float w = P::SmallButtonWidth(ui, L"Uninstall", 90.f);
            const Rect b = right.Next(w, 6.f);
            if (P::SmallButton(ui, ui.MakeId("uninstall"), { b.x, cy - 14.f, w, 28.f }, L"Uninstall", kRemove)) Uninstall(mod);
        } else if (!paks) {
            const float w = g.Measure(L"unavailable", Mono(10.f)).w + 1.f;
            g.Text(L"unavailable", right.Next(w, 6.f), Rgba(160, 180, 200, 140), Mono(10.f));
        } else {
            const float w = P::SmallButtonWidth(ui, L"Install", 90.f);
            const Rect b = right.Next(w, 6.f);
            if (P::SmallButton(ui, ui.MakeId("install"), { b.x, cy - 14.f, w, 28.f }, L"Install", Palette::Teal)) StartInstall(mod);
        }
        const struct {
            const char* id;
            wchar_t glyph;
            void (HandModsOverlay::*action)(const HM::Mod&);
        } buttons[] = { { "extras", 0xE8B7, &HandModsOverlay::ShowExtras },
                        { "preview", 0xEB9F, &HandModsOverlay::ShowPreview },
                        { "info", 0xE946, &HandModsOverlay::ShowInfo } };
        for (const auto& b : buttons) {
            const Rect slot = right.Next(26.f, 6.f);
            const bool busy = busyButtons.count(mod.baseName + Text::Widen(b.id)) != 0;
            const ButtonStyle s{ Rgba(100, 130, 160, 40), Colors::Transparent, 0.f, 3.f, { 0.f }, 0.f, 26.f };
            if (TextButton(ui, ui.MakeId(b.id), { slot.x, cy - 13.f, 26.f, 26.f }, std::wstring(1, b.glyph), IconFont(13.f), Rgba(160, 180, 200, 220), s,
                           !busy))
                (this->*b.action)(mod);
        }

        const float textW = std::max(0.f, right.right - 8.f - inner.x);
        const float top = cy - (nameH + subH) * 0.5f;
        g.Text(mod.name, { inner.x, top, textW, nameH }, kRowText, Mono(13.f).SemiBold().Ellipsis());
        g.Text(Text::FileSize(mod.size), { inner.x, top + nameH, textW, subH }, kRowSub, Mono(10.f));
        ui.PopId();
    }
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
        if (!zip) return L"Couldn't check " + mod.name + L"'s info:\nThe download failed.";
        const auto hands = HM::ReadDeclaredHands(*zip);
        return hands ? mod.name + L" changes: " + Text::Join(*hands, L", ") : mod.name + L" hasn't declared its hand color yet.";
    }, [this, key](std::wstring message) {
        busyButtons.erase(key);
        ShowMessage(message);
    });
}

void HandModsOverlay::ShowPreview(const HM::Mod& mod) {
    const std::wstring key = mod.baseName + L"preview";
    busyButtons.insert(key);
    const int number = chapter;
    Async::Run([mod, number]() -> std::optional<std::vector<HM::HandImage>> {
        const auto zip = HM::Download(mod, number, {});
        if (!zip) return std::nullopt;
        return HM::ReadHandImages(*zip);
    }, [this, key, mod](std::optional<std::vector<HM::HandImage>> images) {
        busyButtons.erase(key);
        if (!images) {
            ShowMessage(L"Couldn't load " + mod.name + L"'s preview:\nThe download failed.");
            return;
        }
        auto previews = std::make_shared<Previews>();
        previews->raw = *images;
        Dialogs::Options o;
        o.title = mod.name + L" preview";
        o.close = L"OK";
        o.contentWidth = images->empty() ? 360.f : static_cast<float>(images->size()) * (260.f + 16.f);
        Dialogs::Show(main.Hwnd(), o, [previews](Ui& ui, const Rect& area) -> float {
            auto& g = ui.g;
            if (previews->raw.empty()) return Paragraph(ui, area.x, area.y, area.w, L"No preview available.", Mono(12.f), Rgba(160, 180, 200, 200));
            float x = area.x;
            const float labelH = g.Measure(L"Ag", Mono(12.f)).h;
            for (const auto& image : previews->raw) {
                const Rect card{ x + 8.f, area.y, 260.f, 260.f };
                g.FillRound(card, 6.f, Rgba(14, 42, 78));
                if (auto img = previews->For(image.color)) g.DrawImageRounded(*img, card, 6.f);
                g.Text(image.color, { card.x, card.B() + 6.f, card.w, labelH }, Rgba(160, 180, 200, 200), Mono(12.f).Center());
                x += 276.f;
            }
            return 260.f + 6.f + labelH;
        });
    });
}

void HandModsOverlay::ShowExtras(const HM::Mod& mod) {
    const std::wstring key = mod.baseName + L"extras";
    busyButtons.insert(key);
    const int number = chapter;
    struct Loaded {
        std::optional<std::wstring> zip;
        std::vector<HM::Extra> extras;
        std::vector<HM::HandImage> images;
    };
    Async::Run([mod, number] {
        Loaded l;
        l.zip = HM::Download(mod, number, {});
        if (l.zip) {
            l.extras = HM::ReadExtras(*l.zip);
            if (!l.extras.empty()) l.images = HM::ReadHandImages(*l.zip);
        }
        return l;
    }, [this, key, mod, number](Loaded loaded) {
        busyButtons.erase(key);
        if (!loaded.zip) {
            ShowMessage(L"Couldn't load " + mod.name + L"'s extras:\nThe download failed.");
            return;
        }
        if (loaded.extras.empty()) {
            ShowMessage(mod.name + L" doesn't ship any individual hand options.");
            return;
        }
        struct State {
            std::wstring zip;
            std::vector<HM::Extra> extras;
            Previews previews;
            std::optional<std::wstring> paks;
            int chapter = 0;
            int busy = -1;
        };
        auto state = std::make_shared<State>();
        state->zip = *loaded.zip;
        state->extras = loaded.extras;
        state->previews.raw = loaded.images;
        state->paks = paks;
        state->chapter = number;
        HWND owner = main.Hwnd();
        HandModsOverlay* self = this;

        Dialogs::Options o;
        o.title = mod.name + L" \x2014 Individual Hands";
        o.close = L"Close";
        o.contentWidth = 380.f;
        Dialogs::Show(owner, o, [state, owner, self](Ui& ui, const Rect& area) -> float {
            auto& g = ui.g;
            const float nameH = g.Measure(L"Ag", Mono(13.f).SemiBold()).h, subH = g.Measure(L"Ag", Mono(10.f)).h;
            const float rowH = 8.f + std::max(44.f, nameH + subH) + 8.f;
            const float listH = state->extras.size() * (rowH + 6.f);
            const Rect viewport{ area.x, area.y, area.w, std::min(listH, 420.f) };
            ScrollArea scroll(ui, ui.MakeId("extras-scroll"), viewport, false, 48.f, ScrollArea::Bar::Classic);
            Column col(viewport.x, viewport.y, scroll.ContentWidth());
            for (size_t i = 0; i < state->extras.size(); ++i) {
                const HM::Extra& extra = state->extras[i];
                const bool installed = state->paks && HM::IsInstalled(*state->paks, extra.baseName);
                const Rect r = col.Next(rowH, 6.f);
                g.FillRound(r, 4.f, installed ? Rgba(0, 204, 170, 30) : Rgba(255, 255, 255, 12));
                const Rect inner = r.Inset(10.f, 8.f);
                const Rect thumb{ inner.x, inner.CY() - 22.f, 44.f, 44.f };
                g.FillRound(thumb, 4.f, Rgba(14, 42, 78));
                if (auto img = state->previews.For(extra.color)) g.DrawImageRounded(*img, thumb, 4.f);

                RowFromRight right{ inner.R(), inner.y, inner.h };
                const float cy = inner.CY();
                const bool busy = state->busy == static_cast<int>(i);
                const auto finish = [state, self](int) {
                    state->busy = -1;
                    ++self->listRevision;
                };
                if (installed) {
                    const float w = P::SmallButtonWidth(ui, L"Uninstall", 90.f);
                    const Rect b = right.Next(w);
                    if (P::SmallButton(ui, ui.MakeId("extra-uninstall", static_cast<int64_t>(i)), { b.x, cy - 14.f, w, 28.f }, L"Uninstall", kRemove, !busy)) {
                        state->busy = static_cast<int>(i);
                        const std::wstring dir = *state->paks, baseName = extra.baseName;
                        Async::Run([dir, baseName] {
                            HM::UninstallByBaseName(dir, baseName);
                            return 0;
                        }, finish);
                    }
                } else if (!state->paks) {
                    const float w = g.Measure(L"unavailable", Mono(10.f)).w + 1.f;
                    g.Text(L"unavailable", right.Next(w), Rgba(160, 180, 200, 140), Mono(10.f));
                } else {
                    const float w = P::SmallButtonWidth(ui, L"Install", 90.f);
                    const Rect b = right.Next(w);
                    if (P::SmallButton(ui, ui.MakeId("extra-install", static_cast<int64_t>(i)), { b.x, cy - 14.f, w, 28.f }, L"Install", Palette::Teal, !busy)) {
                        state->busy = static_cast<int>(i);
                        // Extract the nested zip, then resolve hand-colour conflicts like a normal install
                        const HM::Extra e = extra;
                        Async::Run([state, e] { return HM::ExtractExtra(state->zip, e, state->chapter); },
                                   [state, e, owner, self, finish](std::optional<std::wstring> extraZip) {
                                       if (!extraZip) {
                                           finish(0);
                                           self->ShowMessage(L"Error installing " + e.color + L":\nCould not extract it.");
                                           return;
                                       }
                                       const std::vector<std::wstring> hands{ e.color };
                                       std::vector<std::wstring> conflicting;
                                       for (const auto& [baseName, markerHands] : HM::AllInstalledHandMarkers(*state->paks))
                                           if (_wcsicmp(baseName.c_str(), e.baseName.c_str()) != 0 && HM::HandsConflict(hands, markerHands))
                                               conflicting.push_back(baseName);
                                       const std::wstring dir = *state->paks, zipPath = *extraZip;
                                       const auto doInstall = [dir, zipPath, e, hands, conflicting, finish] {
                                           Async::Run([dir, zipPath, e, hands, conflicting] {
                                               for (const auto& c : conflicting) HM::UninstallByBaseName(dir, c);
                                               HM::Install(dir, zipPath, e.baseName, hands);
                                               return 0;
                                           }, finish);
                                       };
                                       if (conflicting.empty()) {
                                           doInstall();
                                           return;
                                       }
                                       Dialogs::Options c;
                                       c.title = L"Hand Conflict";
                                       c.primary = L"Continue";
                                       c.close = L"Cancel";
                                       Dialogs::ShowText(owner, c,
                                                         L"\x26A0 " + e.color + L" changes the " + e.color + L" hand.\n" + Text::Join(conflicting, L", ") +
                                                             L" will be removed first.",
                                                         Rgba(160, 180, 200, 200), [doInstall, finish](Dialogs::Result r) {
                                                             if (r == Dialogs::Result::Primary) doInstall();
                                                             else finish(0);
                                                         });
                                   });
                    }
                }
                const float textX = thumb.R() + 10.f, textW = std::max(0.f, right.right - 8.f - textX);
                const float top = cy - (nameH + subH) * 0.5f;
                g.Text(extra.color, { textX, top, textW, nameH }, kRowText, Mono(13.f).SemiBold());
                g.Text(Text::FileSize(extra.size), { textX, top + nameH, textW, subH }, kRowSub, Mono(10.f));
            }
            scroll.SetContentSize(listH);
            return viewport.h;
        });
    });
}

} // namespace SpeedrunLauncher::Main
