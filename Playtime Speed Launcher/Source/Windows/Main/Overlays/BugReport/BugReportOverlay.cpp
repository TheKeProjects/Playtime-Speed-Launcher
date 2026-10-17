#include "Windows/Main/Overlays/BugReport/BugReportOverlay.h"
#include <algorithm>
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/Community/BugReportService.h"
#include "UI/Layout.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "UI/Widgets/TextBox.h"
#include "Graphics/Geometry.h"
#include "Graphics/Images.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

using namespace UI;

namespace {

constexpr Color kPurple = Hex(0x9944CC), kPurpleDim = Hex(0x5522AA), kPurpleBorder = Hex(0x2A1050);

PanelStyle PurplePanel(const std::wstring& title) {
    PanelStyle s;
    s.border = kPurpleBorder;
    s.headerBackground = Hex(0x07101A);
    s.headerBorder = Hex(0x1A0838);
    s.footerBackground = Hex(0x07101A);
    s.footerBorder = Hex(0x1A0838);
    s.titleColor = kPurple;
    s.title = title;
    return s;
}

TextBoxStyle FormBox(Color foreground, bool multiline, size_t maxLength) {
    TextBoxStyle s;
    s.text = Mono(11.f);
    s.foreground = foreground;
    s.background = Hex(0x080C18);
    s.border = kPurpleBorder;
    s.focusBorder = kPurpleBorder;
    s.caret = kPurple;
    s.selection = Rgba(153, 68, 204, 90);
    s.padding = { 10.f, 8.f };
    s.radius = 6.f;
    s.multiline = multiline;
    s.maxLength = maxLength;
    return s;
}

} // namespace

void BugReportOverlay::Open() {
    title.clear();
    description.clear();
    image.reset();
    status.clear();
    sending = false;
    infoOpen = false;
    discord.Reset();
    openedAt = Async::Now() + 0.06;   // the form follows the panel in
    popup.Open();
}

void BugReportOverlay::Send() {
    const std::wstring t = Text::Trim(title), d = Text::Trim(description);
    if (t.empty() || d.empty()) {
        status = Loc::Get("bug_report_err_empty");
        statusOk = false;
        return;
    }
    if (!discord.User()) {
        ShowDiscordRequired(main.Hwnd(), "bug_report_err_discord_required", [this] { discord.Connect(); });
        return;
    }
    sending = true;
    status.clear();
    const auto user = *discord.User();
    const auto shot = image;
    Async::Run([user, t, d, shot] { return Services::BugReport::Send(user.id, user.username, t, d, shot); }, [this](bool ok) {
        sending = false;
        statusOk = ok;
        status = Loc::Get(ok ? "bug_report_success" : "bug_report_err_send");
        if (ok) {
            sending = true;   // keep the button disabled until the panel closes
            Async::After(2000, [this] {
                sending = false;
                popup.Close();
            });
        }
    });
}

float BugReportOverlay::DrawForm(Ui& ui, const Rect& body) {
    auto& g = ui.g;
    ScrollArea scroll(ui, ui.MakeId("bug-scroll"), body, false, 48.f, ScrollArea::Bar::Classic);
    Column col(body.x + 24.f, body.y + 18.f, scroll.ContentWidth() - 48.f);
    int reveal = 0;   // blocks slide in one after another when the panel opens

    {
        StaggerItem in(ui, openedAt, reveal++, 30.f);
        col.Gap(Paragraph(ui, col.x, col.y, col.w, Loc::Get("bug_report_subtitle"), Mono(10.f), Rgba(150, 120, 200, 200)));
        col.Gap(16.f);
    }
    {
        StaggerItem in(ui, openedAt, reveal++, 30.f);
        Form::Label(ui, col, 0xE77B, Loc::Get("bug_report_discord_label"), kPurpleDim);   // person
        discord.Draw(ui, col, [this] { infoOpen = true; });
    }
    {
        StaggerItem in(ui, openedAt, reveal++, 30.f);
        Form::Label(ui, col, 0xEBE8, Loc::Get("bug_report_etitle_label"), kPurple, true);   // bug
        const TextBoxStyle titleStyle = FormBox(Hex(0xCCEEFF), false, 100);
        Form::Field(ui, ui.MakeId("bug-title"), col.Next(TextBoxHeight(ui, titleStyle), 18.f), title, titleStyle, kPurple);
    }
    {
        StaggerItem in(ui, openedAt, reveal++, 30.f);
        Form::Label(ui, col, 0xE8BD, Loc::Get("bug_report_desc_label"), kPurple, true);   // message
        Form::Field(ui, ui.MakeId("bug-desc"), col.Next(110.f, 18.f), description, FormBox(Hex(0xAABBCC), true, 1500), kPurple);
    }

    // Screenshot (optional): a dashed "attach" card, or a preview of the picked image with a remove button
    {
        StaggerItem in(ui, openedAt, reveal++, 30.f);
        Form::Label(ui, col, 0xEB9F, Loc::Get("bug_report_image_label"), kPurpleDim);   // photo
        const Rect card = col.Next(image ? 76.f : 58.f);
        const Interaction it = ui.Interact(ui.MakeId("bug-image"), card);
        const float hover = ui.Animate(ui.MakeId("bug-image-hover"), it.hovered ? 1.f : 0.f, 14.f);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        g.FillRound(card, 8.f, Lerp(Hex(0x0A0818), Hex(0x140A28), hover));
        if (!image) {
            // Dashed outline
            static ComPtr<ID2D1StrokeStyle> dashed;
            if (!dashed) {
                const float dashes[] = { 4.f, 3.f };
                Device::Get().D2DFactory()->CreateStrokeStyle(
                    D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND, 10.f,
                                                D2D1_DASH_STYLE_CUSTOM),
                    dashes, 2, &dashed);
            }
            auto outline = Geometry::RoundedRect(card.Inset(0.5f).D2D(), 8.f, 8.f);
            g.Context()->DrawGeometry(outline.Get(), g.Brush(Lerp(kPurpleBorder, kPurple, 0.3f + 0.5f * hover)), 1.f, dashed.Get());
            Glyph(ui, 0xE898, { card.x, card.y + 6.f, card.w, 24.f }, 16.f, Lerp(kPurpleDim, kPurple, hover));   // upload
            g.Text(Loc::Get("bug_report_image_hint"), { card.x, card.y + 30.f, card.w, 18.f }, Lerp(Rgba(120, 90, 170), Rgba(190, 160, 240), hover),
                   Mono(10.f, true).Center());
        } else {
            g.StrokeRound(card, 8.f, Lerp(kPurpleBorder, kPurple, 0.4f + 0.4f * hover), 1.f);
            const Rect thumb{ card.x + 8.f, card.y + 8.f, 90.f, card.h - 16.f };
            if (auto preview = Images::Load(*image, 256)) g.DrawImageRounded(*preview, thumb, 5.f, 1.f, Stretch::UniformToFill);
            else g.FillRound(thumb, 5.f, Hex(0x1A0830));
            g.Text(Files::FileName(*image), { thumb.R() + 12.f, card.y + 18.f, card.w - 160.f, 18.f }, Rgba(200, 175, 240), Mono(11.f, true).Ellipsis());
            g.Text(Loc::Get("bug_report_image_btn"), { thumb.R() + 12.f, card.y + 40.f, card.w - 160.f, 16.f }, Rgba(120, 90, 170), Mono(9.f));
        }
        // The remove button registers after the card, so it gets the click instead of the card
        bool removed = false;
        if (image) {
            const Rect x{ card.R() - 36.f, card.CY() - 14.f, 28.f, 28.f };
            if (TextButton(ui, ui.MakeId("bug-image-remove"), x, L"\x2715", Mono(11.f, true).WithFont(Font::Ui).Center(), Rgba(200, 120, 140),
                           { Hex(0x1A0818), kPurpleBorder, 1.f, 14.f, { 0.f }, 0.f, 28.f })) {
                image.reset();
                removed = true;
            }
        }
        if (it.clicked && !removed) {
            const std::wstring file = System::PickFile(main.Hwnd(), L"", { { L"Images", L"*.jpg;*.jpeg;*.png;*.gif;*.webp;*.bmp" } });
            if (!file.empty()) image = file;
        }
    }

    col.Gap(14.f);
    Form::StatusBanner(ui, ui.MakeId("bug-status"), col, status, statusOk ? Form::Status::Success : Form::Status::Error);
    scroll.SetContentSize(col.Height() + 32.f);
    return col.Height() + 32.f;
}

void BugReportOverlay::Draw(Ui& ui, const Rect& screen) {
    {
        OverlayFrame frame(ui, ui.MakeId("bugreport"), screen, 500.f, 0.f, PurplePanel(Loc::Get("bug_report_title")), popup.Opacity(), popup.Scale());
        frame.SetContentHeight(DrawForm(ui, frame.body));
        // Footer: SEND (left), BACK (right)
        const Rect row = frame.footer.Inset(14.f, 0.f);
        const bool sent = statusOk && !status.empty();
        const Form::SendState state = sent ? Form::SendState::Sent : sending ? Form::SendState::Sending : Form::SendState::Idle;
        const std::wstring send = Loc::Get(state == Form::SendState::Sending ? "bug_report_sending" : "bug_report_send_btn");
        const float sw = std::max(190.f, ui.g.Measure(send, Mono(12.f, true)).w + 60.f);
        if (Form::SendButton(ui, ui.MakeId("bug-send"), { row.x, row.CY() - 18.f, sw, 36.f }, send, Hex(0x5A2A90), Colors::White, state)) Send();
        const std::wstring back = Loc::Get("back");
        const float bw = ButtonWidth(ui, back, Mono(11.f, true), Styles::Footer());
        if (TextButton(ui, ui.MakeId("bug-close"), { row.R() - bw, row.CY() - 18.f, bw, 36.f }, back, Mono(11.f, true).Center(), Palette::Dim,
                       Styles::Footer())) {
            discord.CancelAuth();
            popup.Close();
        }
    }
    if (infoOpen) DrawDiscordInfoPopup(ui, screen, "bug_report_discord_info_why_text", infoOpen);
}

} // namespace SpeedrunLauncher::Main
