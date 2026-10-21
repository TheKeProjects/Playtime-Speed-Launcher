#include "Windows/Main/Overlays/BugReport/BugReportOverlay.h"
#include <algorithm>
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/Community/BugReportService.h"
#include "UI/Layout.h"
#include "Windows/Main/Common/ReportKit.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "UI/Widgets/TextBox.h"
#include "Graphics/Images.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

using namespace UI;

namespace {

constexpr Color kPurple = Hex(0x9944CC), kPurpleBorder = Hex(0x2A1050), kDiscord = Rgba(0x58, 0x65, 0xF2);
constexpr float kRightW = 330.f;
constexpr size_t kTitleMax = 100, kDescriptionMax = 1500;

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

TextBoxStyle FormBox(Color foreground, bool multiline) {
    TextBoxStyle s;
    s.text = Mono(11.f);
    s.foreground = foreground;
    s.background = Hex(0x080C18);
    s.border = kPurpleBorder;
    s.focusBorder = kPurpleBorder;
    s.caret = kPurple;
    s.selection = Rgba(153, 68, 204, 90);
    s.padding = { 12.f, 10.f };
    s.radius = 8.f;
    s.multiline = multiline;
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

bool BugReportOverlay::Ready() const {
    return discord.User().has_value() && !Text::Trim(title).empty() && Text::Trim(description).size() >= 20;
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

// The form card: "YOUR REPORT" with a hint, the title, the description (fills the card), the
// screenshot zone and the send row (status pill on the left, the send button on the right)
void BugReportOverlay::DrawForm(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    const Rect c = ReportKit::Card(ui, r, kPurple, 20.f);
    int reveal = 0;
    float y = c.y;
    {
        StaggerItem in(ui, openedAt + 0.1, reveal++, 0.f, 10.f);
        ReportKit::Caption(ui, c.x, y, c.w, 0xE70F, Loc::Get("bug_report_your_report"), kPurple);   // edit
        g.Text(Loc::Get("bug_report_subtitle"), { c.x, y + 26.f, c.w, 16.f }, Rgba(150, 130, 190), Mono(10.f).Ellipsis());
        y += 54.f;
    }
    {
        StaggerItem in(ui, openedAt + 0.1, reveal++, 0.f, 10.f);
        ReportKit::FieldLabel(ui, { c.x, y, c.w, 16.f }, Loc::Get("bug_report_etitle_label"), title.size(), kTitleMax, kPurple, true);
        const TextBoxStyle style = FormBox(Hex(0xCCEEFF), false);
        const float h = TextBoxHeight(ui, style) + 4.f;
        Form::Field(ui, ui.MakeId("bug-title"), { c.x, y + 22.f, c.w, h }, title, style, kPurple);
        if (title.size() > kTitleMax) title.resize(kTitleMax);
        y += 22.f + h + 16.f;
    }
    constexpr float kShotH = 84.f, kSendH = 46.f;
    const float descH = std::max(90.f, c.B() - y - 22.f - 16.f - kShotH - 16.f - kSendH);
    {
        StaggerItem in(ui, openedAt + 0.1, reveal++, 0.f, 10.f);
        ReportKit::FieldLabel(ui, { c.x, y, c.w, 16.f }, Loc::Get("bug_report_desc_label"), description.size(), kDescriptionMax, kPurple, true);
        Form::Field(ui, ui.MakeId("bug-desc"), { c.x, y + 22.f, c.w, descH }, description, FormBox(Hex(0xAABBCC), true), kPurple);
        if (description.size() > kDescriptionMax) description.resize(kDescriptionMax);
        y += 22.f + descH + 16.f;
    }
    {
        StaggerItem in(ui, openedAt + 0.1, reveal++, 0.f, 10.f);
        ReportKit::Screenshot(ui, ui.MakeId("bug-image"), { c.x, y, c.w, kShotH }, image, kPurple, main.Hwnd(), Loc::Get("bug_report_screenshot_title"),
                              Loc::Get("bug_report_screenshot_formats"), Loc::Get("bug_report_screenshot_attached"));
        y += kShotH + 16.f;
    }
    {
        StaggerItem in(ui, openedAt + 0.1, reveal++, 0.f, 10.f);
        const Rect row{ c.x, y, c.w, kSendH };
        constexpr float kButtonW = 210.f;
        ReportKit::StatusPill(ui, ui.MakeId("bug-status"), { row.x, row.y, row.w - kButtonW - 14.f, row.h }, status,
                              statusOk ? Form::Status::Success : Form::Status::Error);
        const bool sent = statusOk && !status.empty();
        const Form::SendState state = sent ? Form::SendState::Sent : sending ? Form::SendState::Sending : Form::SendState::Idle;
        const std::wstring label = Loc::Get(state == Form::SendState::Sending ? "bug_report_sending" : "bug_report_send_btn");
        if (ReportKit::SendButton(ui, ui.MakeId("bug-send"), { row.R() - kButtonW, row.y, kButtonW, row.h }, label, kPurple, Ready(), state)) Send();
    }
}

// Discord card: caption, why it's needed, then the connect button / waiting line / connected user
float BugReportOverlay::DrawDiscord(Ui& ui, const Rect& r, bool draw) {
    auto& g = ui.g;
    constexpr float kPad = 18.f;
    TextStyle why = Mono(9.5f);
    why.wrap = true;
    why.valign = VAlign::Top;
    const std::wstring& text = Loc::Get("bug_report_discord_why");
    const float whyH = g.Measure(text, why, r.w - kPad * 2.f).h;
    const float stateH = discord.User() ? 46.f : discord.Waiting() ? 26.f : 36.f;
    const float h = kPad * 2.f + 22.f + 10.f + whyH + 14.f + stateH;
    if (!draw) return h;
    const Rect c = ReportKit::Card(ui, { r.x, r.y, r.w, h }, kDiscord, kPad);
    ReportKit::Caption(ui, c.x, c.y, c.w - 30.f, Glyphs::People, Loc::Get("bug_report_discord_label"), kDiscord);
    if (MenuKit::FooterButton(ui, ui.MakeId("bug-discord-info"), { c.R() - 26.f, c.y - 2.f, 26.f, 26.f }, L"", MenuKit::Kind::Secondary, true, Glyphs::Info,
                              kDiscord))
        infoOpen = true;
    g.Text(text, { c.x, c.y + 32.f, c.w, whyH }, Rgba(150, 160, 200), why);
    Column col(c.x, c.y + 32.f + whyH + 14.f, c.w);
    DiscordIdentityStyle style;
    style.buttonBackground = Hex(0x1A1640);
    style.border = Lerp(kDiscord, Colors::Black, 0.4f);
    style.glyphText = Rgba(210, 215, 255);
    style.accent = kDiscord;
    style.marginBottom = 0.f;
    discord.Draw(ui, col, [this] { infoOpen = true; }, style);
    return h;
}

void BugReportOverlay::Draw(Ui& ui, const Rect& screen) {
    {
        OverlayFrame frame(ui, ui.MakeId("bugreport"), screen, 1000.f, 700.f, PurplePanel(Loc::Get("bug_report_title")), popup.Opacity(), popup.Scale());
        const Rect inner = frame.body.Inset(20.f, 16.f);
        {
            StaggerItem in(ui, openedAt, 0, 0.f, 12.f);
            ReportKit::Banner(ui, inner.Top(ReportKit::kBannerH), 0xEBE8, Loc::Get("bug_report_banner_title"), Loc::Get("bug_report_banner_sub"),
                              Loc::Get("bug_report_banner_pill"), kPurple);   // bug
        }
        const float top = inner.y + ReportKit::kBannerH + ReportKit::kGap;
        const Rect left{ inner.x, top, inner.w - kRightW - ReportKit::kGap, inner.B() - top };
        const Rect right{ left.R() + ReportKit::kGap, top, kRightW, inner.B() - top };
        {
            StaggerItem in(ui, openedAt + 0.06, 0, 0.f, 16.f);
            DrawForm(ui, left);
        }
        const float discordH = DrawDiscord(ui, right, false);
        {
            StaggerItem in(ui, openedAt + 0.12, 0, 26.f);
            DrawDiscord(ui, right, true);
        }
        {
            StaggerItem in(ui, openedAt + 0.18, 0, 26.f);
            const std::vector<ReportKit::Check> checks = {
                { Loc::Get("bug_check_discord"), Loc::Get("bug_check_discord_hint"), discord.User().has_value(), false },
                { Loc::Get("bug_check_title"), Loc::Get("bug_check_title_hint"), !Text::Trim(title).empty(), false },
                { Loc::Get("bug_check_describe"), Loc::Get("bug_check_describe_hint"), Text::Trim(description).size() >= 20, false },
                { Loc::Get("bug_check_screenshot"), Loc::Get("bug_check_screenshot_hint"), image.has_value(), true },
            };
            const float y = right.y + discordH + ReportKit::kGap;
            ReportKit::Checklist(ui, { right.x, y, right.w, right.B() - y }, checks, kPurple, openedAt + 0.3, Loc::Get("reports_before_send"),
                                 Loc::Get("reports_ready"), Loc::Get("reports_pending"), Loc::Get("reports_optional"), Loc::Get("bug_report_tip"));
        }

        // Footer: BACK on the right
        const Rect row = frame.footer.Inset(14.f, 0.f);
        const std::wstring back = Loc::Get("back");
        const float bw = MenuKit::FooterButtonWidth(ui, back, Glyphs::Back, 120.f);
        if (MenuKit::FooterButton(ui, ui.MakeId("bug-close"), { row.R() - bw, row.CY() - 18.f, bw, 36.f }, back, MenuKit::Kind::Secondary, true, Glyphs::Back,
                                  kPurple) ||
            (popup.IsOpen() && !infoOpen && ui.KeyPressed(VK_ESCAPE))) {
            discord.CancelAuth();
            popup.Close();
        }
    }
    if (infoOpen) DrawDiscordInfoPopup(ui, screen, "bug_report_discord_info_why_text", infoOpen);
}

} // namespace SpeedrunLauncher::Main
