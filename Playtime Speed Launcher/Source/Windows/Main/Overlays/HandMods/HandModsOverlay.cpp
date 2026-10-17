#include "Windows/Main/Overlays/HandMods/HandModsOverlay.h"
#include "App/Launcher.h"
#include "Core/Files.h"
#include "Localization/Localization.h"
#include "Services/GameFiles/ModFiles.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/ScrollArea.h"
#include "Windows/Dialogs/DialogWindow.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

using namespace UI;

void HandModsOverlay::OpenHub() {
    screen = Screen::Hub;
    popup.Open();
}

void HandModsOverlay::OpenChapter(int number) {
    chapter = number;
    win64.reset();
    paks.reset();
    mods.reset();
    screen = Screen::Versions;
    popup.Open();
}

void HandModsOverlay::ShowMessage(const std::wstring& message) {
    Dialogs::Options o;
    o.title = L"Hand Mods";
    o.close = L"OK";
    Dialogs::ShowText(main.Hwnd(), o, message, Rgba(160, 180, 200, 200));
}

// ── Hub ─────────────────────────────────────────────────────────────────────

void HandModsOverlay::DrawHub(Ui& ui, Column& col) {
    const float labelH = ui.g.Measure(L"Ag", Mono(12.f, true)).h;
    ui.g.Text(Loc::Get("handmods_hub_label"), col.Next(labelH, 10.f), Palette::Teal, Mono(12.f, true));
    const float h = OptionButtonHeight(ui);
    for (int n = 1; n <= 5; ++n) {
        const std::string key = "chapter" + std::to_string(n) + "_section";
        if (OptionButton(ui, ui.MakeId("hand-chapter", n), col.Next(h, n < 5 ? 8.f : 0.f), Loc::Get(key), false)) OpenChapter(n);
    }
    col.Gap(14.f);
    const ButtonStyle submit{ Hex(0x0C1F14), Hex(0x1A4A2E), 1.f, 4.f, { 12.f, 10.f }, 0.f, 0.f };
    TextStyle text = Mono(11.f, true);
    if (TextButton(ui, ui.MakeId("hand-submit-entry"), col.Next(h), Loc::Get("handmods_submit_entry_btn"), text, Hex(0x00CC88), submit))
        OpenSubmit();
}

// ── Version picker ──────────────────────────────────────────────────────────

void HandModsOverlay::SelectVersion(const std::wstring& exePath) {
    const auto found = App::Launcher::FindWin64Dir(Files::DirectoryName(exePath));
    if (!found) {
        ShowMessage(L"No Content\\Paks folder found for this version.");
        return;
    }
    win64 = found;
    paks = Services::ModFiles::PaksDir(*found);
    screen = Screen::Mods;
    LoadMods();
}

void HandModsOverlay::DrawVersions(Ui& ui, Column& col) {
    auto& g = ui.g;
    auto& l = App::L();
    const auto* ch = l.ChapterByNumber(chapter);
    if (!ch) return;
    std::vector<std::pair<std::wstring, std::wstring>> rows;   // name, exe
    const auto autoExe = l.epic.IsEnabled() ? l.epic.ExePath(chapter) : ch->gameExePath;
    if (autoExe && !autoExe->empty() && Files::Exists(*autoExe)) rows.emplace_back(Loc::Get("auto_name"), *autoExe);
    for (const auto& custom : l.store.Customs(chapter))
        if (Files::Exists(custom.exePath)) rows.emplace_back(custom.name, custom.exePath);

    if (rows.empty()) {
        col.Gap(12.f);
        col.Gap(Paragraph(ui, col.x + 4.f, col.y, col.w - 8.f, L"No installed version found for this chapter.", Mono(11.f), Rgba(160, 180, 200, 160)));
        return;
    }
    const float nameH = g.Measure(L"Ag", Mono(13.f).SemiBold()).h, subH = g.Measure(L"Ag", Mono(10.f)).h;
    int index = 0;
    for (const auto& [name, exe] : rows) {
        const Rect r = col.Next(8.f + nameH + subH + 8.f, 2.f);
        const Interaction it = ui.Interact(ui.MakeId("hand-version", index++), r);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        g.FillRound(r, 4.f, it.hovered ? Rgba(255, 255, 255, 28) : Rgba(255, 255, 255, 12));
        g.Text(name, { r.x + 10.f, r.y + 8.f, r.w - 20.f, nameH }, Rgba(210, 220, 230), Mono(13.f).SemiBold().Ellipsis());
        g.Text(exe, { r.x + 10.f, r.y + 8.f + nameH, r.w - 20.f, subH }, Rgba(45, 90, 120), Mono(10.f).Ellipsis());
        if (it.held && ui.In().pressed[0]) SelectVersion(exe);
    }
}

// ── Frame ───────────────────────────────────────────────────────────────────

void HandModsOverlay::Draw(Ui& ui, const Rect& screenRect) {
    std::wstring title = L"\x270B Hand Mods";
    if (screen == Screen::Versions) title = L"\x270B Hand Mods \x2014 Chapter " + std::to_wstring(chapter);
    else if (screen == Screen::Mods) title = L"\x270B Hand Mods \x2014 Select a Mod";
    else if (screen == Screen::Submit) title = Loc::Get("handmods_submit_header");
    else if (screen == Screen::Installing) title = L"\x270B Installing\x2026";

    {
        PanelStyle style;
        style.title = title;
        OverlayFrame frame(ui, ui.MakeId("handmods"), screenRect, 480.f, 440.f, style, popup.Opacity(), popup.Scale());

        // ← back (every screen but the hub and the install progress)
        if (screen != Screen::Hub && screen != Screen::Installing) {
            TextStyle arrow = Mono(20.f);
            arrow.font = Font::Consolas;
            arrow.align = HAlign::Center;
            const Rect row = frame.header.Inset(6.f, 0.f);
            if (TextButton(ui, ui.MakeId("hand-back"), { row.x, row.CY() - 19.f, 38.f, 38.f }, L"\x2190", arrow, Palette::Teal, Styles::Flat())) {
                if (screen == Screen::Mods) screen = Screen::Versions;
                else screen = Screen::Hub;
                infoOpen = false;
                discord.CancelAuth();
            }
        }

        if (screen == Screen::Installing) {
            DrawInstalling(ui, frame.body);
        } else {
            const bool wide = screen == Screen::Hub || screen == Screen::Submit;   // Margin 24,18 vs 12,10
            ScrollArea scroll(ui, ui.MakeId("hand-scroll", static_cast<int>(screen)), frame.body, false, 48.f, ScrollArea::Bar::Classic);
            Column col(frame.body.x + (wide ? 24.f : 12.f), frame.body.y + (wide ? 18.f : 10.f), scroll.ContentWidth() - (wide ? 48.f : 24.f));
            switch (screen) {
                case Screen::Hub: DrawHub(ui, col); break;
                case Screen::Versions: DrawVersions(ui, col); break;
                case Screen::Mods: DrawMods(ui, col); break;
                case Screen::Submit: DrawSubmit(ui, col); break;
                default: break;
            }
            scroll.SetContentSize(col.Height() + (wide ? 36.f : 20.f));
        }

        const Rect row = frame.footer.Inset(14.f, 0.f);
        const TextStyle text = Mono(11.f, true).Center();
        const float w = ButtonWidth(ui, L"Close", text, Styles::Footer());
        if (TextButton(ui, ui.MakeId("hand-close"), { row.R() - w, row.CY() - 18.f, w, 36.f }, L"Close", text, Palette::Dim, Styles::Footer())) {
            discord.CancelAuth();
            infoOpen = false;
            popup.Close();
        }
    }
    if (infoOpen) DrawDiscordInfoPopup(ui, screenRect, "handmods_submit_discord_info_why_text", infoOpen);
}

} // namespace SpeedrunLauncher::Main
