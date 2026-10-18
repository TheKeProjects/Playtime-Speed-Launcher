// Hand Mods → submission form: Discord identity, chapter, name, hand colours and the mod files,
// sent (zipped, split if needed) to the review webhook.
#include <algorithm>
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/GameFiles/HandModSubmission.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/Stagger.h"
#include "UI/Widgets/TextBox.h"
#include "Windows/Dialogs/DialogParts.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/HandMods/HandModsKit.h"
#include "Windows/Main/Overlays/HandMods/HandModsOverlay.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
namespace P = Dialogs::Parts;
namespace Submission = Services::HandModSubmission;

namespace {

// WrapPanel of MakeSmallButton chips (Margin 0,0,6,6). Returns the clicked index or -1.
int ChipWrap(Ui& ui, Column& col, std::string_view idBase, const std::vector<std::wstring>& texts, const std::vector<bool>& selected) {
    int clicked = -1;
    float x = col.x, y = col.y;
    for (size_t i = 0; i < texts.size(); ++i) {
        const float w = P::SmallButtonWidth(ui, texts[i]);
        if (x > col.x && x + w > col.x + col.w) {
            x = col.x;
            y += 28.f + 6.f;
        }
        ButtonStyle s{ selected[i] ? Rgba(0, 120, 100) : Rgba(8, 30, 55), selected[i] ? Palette::Teal : Rgba(0, 120, 100, 180), 1.f, 3.f,
                       { 8.f, 0.f }, 80.f, 28.f };
        if (TextButton(ui, ui.MakeId(idBase, static_cast<int64_t>(i)), { x, y, w, 28.f }, texts[i], Mono(10.f, true).Center(), Palette::Teal, s))
            clicked = static_cast<int>(i);
        x += w + 6.f;
    }
    col.Gap(y + 28.f + 6.f - col.y);
    return clicked;
}

void Caption(Ui& ui, Column& col, const std::wstring& text) {
    const float h = ui.g.Measure(L"Ag", Mono(10.f, true)).h;
    ui.g.Text(text, col.Next(h, 6.f), Palette::Dim, Mono(10.f, true));
}

} // namespace

void HandModsOverlay::OpenSubmit() {
    submitChapter = chapter > 0 ? chapter : 1;
    submitColors.clear();
    submitFiles.clear();
    submitName.clear();
    submitStatus.clear();
    sending = false;
    discord.Reset();
    GoTo(Screen::Submit);
}

void HandModsOverlay::Send() {
    const std::wstring name = Text::Trim(submitName);
    submitOk = false;
    if (name.empty()) {
        submitStatus = Loc::Get("handmods_submit_err_name");
        return;
    }
    if (submitFiles.empty()) {
        submitStatus = Loc::Get("handmods_submit_err_files");
        return;
    }
    if (!discord.User()) {
        ShowDiscordRequired(main.Hwnd(), "handmods_submit_err_discord_required", [this] { discord.Connect(); });
        return;
    }
    sending = true;
    submitStatus.clear();
    const auto user = *discord.User();
    const int ch = submitChapter;
    const auto colors = submitColors;
    const auto files = submitFiles;
    Async::Run([name, ch, colors, files, user] {
        return Submission::Submit(name, ch, colors, files, user.username + L" \x00B7 ID: " + user.id, user.id);
    }, [this](std::wstring error) {
        if (error.empty()) {
            submitOk = true;
            submitStatus = Loc::Get("handmods_submit_success");
            Async::After(2000, [this] {
                sending = false;
                if (screen == Screen::Submit) GoTo(Screen::Hub, -1);
            });
        } else {
            sending = false;
            submitStatus = error;
        }
    });
}

void HandModsOverlay::DrawSubmit(Ui& ui, Column& col) {
    auto& g = ui.g;
    // Each section rises in one after another
    const auto section = [&](int index) { return StaggerItem(ui, screenChangedAt + 0.04, index, 0.f, 14.f, 0.05, 0.38); };

    // Discord identity
    {
        auto in = section(0);
        Caption(ui, col, Loc::Get("handmods_submit_discord_label"));
        discord.Draw(ui, col, [this] { infoOpen = true; }, DiscordIdentityStyle::HandMods());
    }

    // Chapter
    {
        auto in = section(1);
        Caption(ui, col, Loc::Get("handmods_submit_chapter_label"));
        std::vector<std::wstring> texts;
        std::vector<bool> selected;
        for (int n = 1; n <= 5; ++n) {
            texts.push_back(L"CH " + std::to_wstring(n));
            selected.push_back(n == submitChapter);
        }
        const int clicked = ChipWrap(ui, col, "submit-chapter", texts, selected);
        if (clicked >= 0) submitChapter = clicked + 1;
        col.Gap(16.f - 6.f);
    }

    // Name
    {
        auto in = section(2);
        Caption(ui, col, Loc::Get("handmods_submit_name_label"));
        TextBoxStyle s;
        s.text = Mono(12.f);
        s.foreground = Palette::Bright;
        s.background = Palette::HeaderBg;
        s.border = Palette::ButtonBorder;
        s.focusBorder = Palette::ButtonBorder;
        s.caret = Palette::Teal;
        s.padding = { 8.f, 6.f };
        TextBox(ui, ui.MakeId("submit-name"), col.Next(TextBoxHeight(ui, s), 16.f), submitName, s);
    }

    // Hand colours
    {
        auto in = section(3);
        Caption(ui, col, Loc::Get("handmods_submit_colors_label"));
        const auto& colors = Submission::Colors();
        std::vector<bool> selected;
        for (const auto& c : colors) selected.push_back(std::find(submitColors.begin(), submitColors.end(), c) != submitColors.end());
        const int clicked = ChipWrap(ui, col, "submit-color", colors, selected);
        if (clicked >= 0) {
            const std::wstring& c = colors[static_cast<size_t>(clicked)];
            auto it = std::find(submitColors.begin(), submitColors.end(), c);
            if (it != submitColors.end()) submitColors.erase(it);
            else submitColors.push_back(c);
        }
        col.Gap(16.f - 6.f);
    }

    // Files
    {
        auto in = section(4);
        Caption(ui, col, Loc::Get("handmods_submit_files_label"));
        const std::wstring text = Loc::Get("handmods_submit_files_btn");
        const ButtonStyle s{ Palette::ButtonBg, Palette::ButtonBorder, 1.f, 3.f, { 12.f, 6.f }, 0.f, 0.f };
        const float h = g.Measure(L"Ag", Mono(11.f, true)).h + 12.f + 2.f;
        const Rect r = col.Next(h);
        if (TextButton(ui, ui.MakeId("submit-files"), { r.x, r.y, ButtonWidth(ui, text, Mono(11.f, true), s), h }, text, Mono(11.f, true).Center(),
                       Palette::Dim, s)) {
            auto picked = System::PickFiles(main.Hwnd(), L"", { { Loc::Get("hm_mod_files"), L"*.zip;*.pak;*.ucas;*.utoc" } });
            if (!picked.empty()) submitFiles = picked;
        }
        std::vector<std::wstring> names;
        for (const auto& f : submitFiles) names.push_back(Files::FileName(f));
        col.Gap(6.f);
        col.Gap(Paragraph(ui, col.x, col.y, col.w, names.empty() ? Loc::Get("handmods_submit_no_files") : Text::Join(names, L", "), Mono(10.f),
                          Hex(0x5A7A9A)));
        col.Gap(16.f);
    }

    // Status + SEND
    auto in = section(5);
    if (!submitStatus.empty()) {
        // The message slides down into place when it changes
        const float shown = ui.Animate(ui.MakeId("submit-status"), 1.f, 12.f, 0.f);
        g.PushOpacity(shown);
        col.Gap(Paragraph(ui, col.x, col.y - (1.f - shown) * 8.f, col.w, submitStatus, Mono(10.f, true), submitOk ? Rgba(0, 200, 140) : Rgba(200, 60, 60)));
        g.PopLayer();
        col.Gap(10.f);
    } else {
        ui.SetValue(ui.MakeId("submit-status"), 0.f);
    }
    const std::wstring send = Loc::Get(sending && !submitOk ? "handmods_submit_sending" : "handmods_submit_send_btn");
    const ButtonStyle s{ Palette::ConfirmBg, Palette::ConfirmBorder, 1.f, 6.f, { 0.f }, 0.f, 38.f };
    const Rect b = col.Next(38.f);
    const double now = Async::Now();
    if (submitOk) g.GlowRound(b, 6.f, HandKit::kGreen, 16.f, 0.25f + 0.2f * Anim::PingPong(now, 1.4));
    else if (!sending) g.GlowRound(b, 6.f, Palette::Teal, 14.f, 0.16f);   // static: an idle form doesn't redraw
    if (TextButton(ui, ui.MakeId("submit-send"), b, send, Mono(11.f, true).Center(), Palette::Teal, s, !sending)) Send();
    if (sending && !submitOk) {
        const float tw = g.Measure(send, Mono(11.f, true)).w;
        Form::Spinner(ui, b.CX() - tw * 0.5f - 16.f, b.CY(), 6.f, Palette::Teal);
    }
    if (sending) HandKit::Shimmer(g, b, 6.f, now, 1.1, 1.6f);
    if (sending || submitOk) ui.KeepAnimatingThrottled();
}

} // namespace SpeedrunLauncher::Main
