#include "Windows/Main/Common/DiscordIdentity.h"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/Panel.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Dialogs/DialogParts.h"
#include "Windows/Dialogs/DialogWindow.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
namespace OAuth = Services::DiscordOAuth;
namespace P = Dialogs::Parts;

void DiscordIdentity::Reset() {
    CancelAuth();
    user = OAuth::LoadCached();
}

void DiscordIdentity::Connect() {
    cancel.Request();
    cancel = Async::Cancel();
    waiting = true;
    const Async::Cancel token = cancel;
    auto guard = alive;
    Async::Run([token] { return OAuth::Authenticate(token); }, [this, guard, token](std::optional<OAuth::User> result) {
        if (!*guard || token.Requested()) return;
        waiting = false;
        if (!result) return;
        user = result;
        OAuth::SaveCached(*result);
    });
}

void DiscordIdentity::CancelAuth() {
    cancel.Request();
    waiting = false;
}

void DiscordIdentity::Disconnect() {
    OAuth::ClearCached();
    user.reset();
}

// Connected: a card with the user's initial in a glowing green circle (it pops in when the login
// lands) and a ✕ that turns red on hover. Waiting: a spinner, the "waiting for the browser" line
// and Cancel. Otherwise: the Connect button glowing in the block's colour, and the "?" info button.
void DiscordIdentity::Draw(Ui& ui, Column& col, const std::function<void()>& onInfo, const DiscordIdentityStyle& st) {
    auto& g = ui.g;
    float& stateAt = ui.Slot(ui.MakeId("discord-state-at"), -10.f);
    int& shown = ui.Data<int>(ui.MakeId("discord-state"));
    const int state = user ? 2 : waiting ? 1 : 0;
    const float now = static_cast<float>(ui.Time());
    if (shown != state) {
        shown = state;
        stateAt = now;
    }
    const float since = now - stateAt;
    const float enter = Anim::EaseOutCubic(Anim::Clamp01(since / 0.3f));
    if (enter < 1.f) ui.KeepAnimating();
    g.PushOpacity(enter);
    ui.PushTranslate(0.f, 6.f * (1.f - enter));

    if (user) {
        const Rect r = col.Next(46.f, st.marginBottom);
        g.GlowRound(r, 8.f, MenuKit::kGreen, 12.f, 0.15f);
        g.Border(r, 8.f, Hex(0x061408), Hex(0x1A4A2A), 1.f);
        const float pop = Anim::EaseOutBack(Anim::Clamp01(since / 0.4f), 1.8f);
        const float cx = r.x + 24.f, cy = r.CY();
        ui.PushScale(std::max(0.01f, pop), cx, cy);   // (scaled, not resized: text formats are cached per size)
        g.FillEllipse(cx, cy, 18.f, 18.f, MenuKit::kGreen.WithAlpha(0.12f));
        g.FillEllipse(cx, cy, 14.f, 14.f, MenuKit::kGreen);
        const std::wstring initial = user->username.empty() ? L"?" : std::wstring(1, static_cast<wchar_t>(towupper(user->username[0])));
        g.Text(initial, { cx - 14.f, cy - 14.f, 28.f, 28.f }, Rgba(3, 17, 13), Mono(12.f, true).Center());
        ui.PopTransform();
        g.Text(user->username, { r.x + 48.f, r.y + 7.f, r.w - 90.f, 18.f }, MenuKit::kGreen, Mono(11.f, true).Ellipsis());
        g.Text(L"Discord", { r.x + 48.f, r.y + 24.f, r.w - 90.f, 14.f }, Hex(0x336644), Mono(9.f, true));
        const Rect x{ r.R() - 34.f, r.CY() - 12.f, 24.f, 24.f };
        const Id xId = ui.MakeId("discord-disconnect");
        const float hover = ui.Animate(HashId(xId, 1), ui.Hot(xId) ? 1.f : 0.f, 16.f, 0.f);
        ButtonScope b(ui, xId, x, { Lerp(Colors::Transparent, Palette::DangerBg, hover), Lerp(Hex(0x2A4A2A), Palette::DangerBorder, hover), 1.f, 12.f, { 0.f }, 0.f, 24.f });
        g.PushTransform(D2D1::Matrix3x2F::Rotation(90.f * hover, D2D1::Point2F(x.CX(), x.CY())));
        Glyph(ui, Glyphs::Cancel, x, 8.f, Lerp(Hex(0x336633), MenuKit::kRed, hover));
        g.PopTransform();
        const bool clicked = b.Clicked();
        ui.PopTransform();
        g.PopLayer();
        if (clicked) Disconnect();
        return;
    }
    if (waiting) {
        const Rect row = col.Next(26.f, st.marginBottom);
        Form::Spinner(ui, row.x + 8.f, row.CY(), 7.f, st.accent);
        const std::wstring text = Loc::Get("bug_report_discord_waiting");
        const float w = g.Measure(text, Mono(9.f)).w + 1.f;
        g.Text(text, { row.x + 24.f, row.y, w, row.h }, st.accent, Mono(9.f));
        const std::wstring cancelText = Loc::Get("bug_report_discord_cancel");
        const ButtonStyle s{ Colors::Transparent, st.border, 1.f, 11.f, { 10.f, 0.f }, 0.f, 22.f };
        const bool clicked = TextButton(ui, ui.MakeId("discord-cancel"), { row.x + 24.f + w + 12.f, row.CY() - 11.f, ButtonWidth(ui, cancelText, Mono(9.f, true), s), 22.f },
                                        cancelText, Mono(9.f, true).Center(), st.accent, s);
        ui.PopTransform();
        g.PopLayer();
        if (clicked) CancelAuth();
        return;
    }
    // Connect + "?"
    const Rect row = col.Next(36.f, st.marginBottom);
    const std::wstring text = Loc::Get("bug_report_discord_connect_btn");
    const Id connectId = ui.MakeId("discord-connect");
    const float hover = ui.Animate(HashId(connectId, 1), ui.Hot(connectId) ? 1.f : 0.f, 14.f, 0.f);
    const float breathe = 0.5f + 0.5f * static_cast<float>(std::sin(ui.Time() * 2.4));
    const ButtonStyle s{ st.buttonBackground, Lerp(st.border, st.accent, hover), 1.f, 8.f, { 14.f, 0.f }, 0.f, 36.f };
    const float w = ButtonWidthFor(13.f + 8.f + g.Measure(text, Mono(10.f, true)).w, s);
    const Rect button{ row.x, row.y, w, 36.f };
    g.GlowRound(button, 8.f, st.accent, 14.f, 0.12f + 0.08f * breathe + 0.3f * hover);
    ui.KeepAnimatingThrottled();
    bool connect = GlyphTextButton(ui, connectId, button, 0xE8BD, 13.f, st.glyphText, text, Mono(10.f, true), st.glyphText, s, 8.f);
    bool info = TextButton(ui, ui.MakeId("discord-info"), { row.x + w + 8.f, row.CY() - 13.f, 26.f, 26.f }, L"?", Mono(11.f, true).Center(), st.accent,
                           { Colors::Transparent, st.border, 1.f, 13.f, { 0.f }, 0.f, 26.f });
    ui.PopTransform();
    g.PopLayer();
    if (connect) Connect();
    if (info && onInfo) onInfo();
}

void ShowDiscordRequired(HWND owner, const char* introKey, std::function<void()> onConnect) {
    Dialogs::Options o;
    o.title = Loc::Get("bug_report_discord_required_title");
    o.primary = Loc::Get("bug_report_discord_connect_btn");
    o.close = Loc::Get("back");
    o.seasonal = true;
    const std::string key = introKey;
    Dialogs::Show(owner, o, [key](Ui& ui, const Rect& area) {
        auto& g = ui.g;
        Column col(area);
        MenuKit::Hero(ui, col.Next(MenuKit::kHeroH, 14.f), Glyphs::People, Loc::Get("bug_report_discord_required_title"), L"Discord", Rgba(119, 85, 204));
        P::Label(ui, col, Loc::Get(key), 12.f, Rgba(160, 180, 200), false, 16.f);
        P::Label(ui, col, Loc::Get("bug_report_discord_info_perms_title"), 10.f, Hex(0x9944CC), true, 8.f);
        // Permission card
        TextStyle desc = Mono(9.f);
        desc.wrap = true;
        desc.valign = VAlign::Top;
        const float inner = col.w - 22.f;
        const float nameH = g.Measure(L"Ag", Mono(10.f, true)).h;
        const float descH = g.Measure(Loc::Get("bug_report_discord_info_perm_desc"), desc, inner).h;
        const Rect card = col.Next(1.f + 8.f + nameH + 3.f + descH + 8.f + 1.f, 10.f);
        g.Border(card, 4.f, Rgba(119, 85, 204, 40), Rgba(119, 85, 204, 90), 1.f);
        g.Text(Loc::Get("bug_report_discord_info_perm_name"), { card.x + 11.f, card.y + 9.f, inner, nameH }, Rgba(119, 85, 204), Mono(10.f, true));
        g.Text(Loc::Get("bug_report_discord_info_perm_desc"), { card.x + 11.f, card.y + 9.f + nameH + 3.f, inner, descH }, Rgba(74, 90, 122), desc);
        P::Label(ui, col, Loc::Get("bug_report_discord_info_no_access"), 9.f, Rgba(51, 68, 68));
        return col.Height();
    }, [onConnect](Dialogs::Result r) {
        if (r == Dialogs::Result::Primary && onConnect) onConnect();
    });
}

void DrawDiscordInfoPopup(Ui& ui, const Rect& screen, const char* whyKey, bool& open) {
    auto& g = ui.g;
    PanelStyle style;
    style.border = Hex(0x2A1050);
    style.headerBackground = Hex(0x07101A);
    style.headerBorder = Hex(0x1A0838);
    style.footerBackground = Hex(0x07101A);
    style.footerBorder = Hex(0x1A0838);
    style.titleColor = Hex(0x9944CC);
    style.title = Loc::Get("bug_report_discord_info_header");
    style.backdrop = Palette::BackdropDark;
    style.titleSize = 12.f;
    style.headerHeight = 44.f;
    style.footerHeight = 48.f;
    OverlayFrame frame(ui, ui.MakeId("discord-info"), screen, 400.f, 0.f, style);
    Column col(frame.body.x + 22.f, frame.body.y + 18.f, frame.body.w - 44.f);
    int row = 0;
    const auto paragraph = [&](const std::wstring& text, float size, Color color, bool bold, float marginBottom) {
        StaggerItem reveal(ui, frame.OpenedAt() + 0.15, row++, 0.f, 8.f);
        col.Gap(Paragraph(ui, col.x, col.y, col.w, text, Mono(size, bold), color));
        col.Gap(marginBottom);
    };
    paragraph(Loc::Get("bug_report_discord_info_why_title"), 10.f, Hex(0x9944CC), true, 6.f);
    paragraph(Loc::Get(whyKey), 10.f, Palette::Label, false, 16.f);
    paragraph(Loc::Get("bug_report_discord_info_perms_title"), 10.f, Hex(0x9944CC), true, 8.f);
    {
        TextStyle desc = Mono(9.f);
        desc.wrap = true;
        desc.valign = VAlign::Top;
        const float inner = col.w - 26.f - 20.f;
        const float nameH = g.Measure(L"Ag", Mono(10.f, true)).h;
        const float descH = g.Measure(Loc::Get("bug_report_discord_info_perm_desc"), desc, inner).h;
        const Rect card = col.Next(1.f + 8.f + nameH + 3.f + descH + 8.f + 1.f);
        g.Border(card, 3.f, Hex(0x060B14), Hex(0x1A1A38), 1.f);
        g.Text(Loc::Get("bug_report_discord_info_perm_name"), { card.x + 13.f, card.y + 9.f, inner, nameH }, Hex(0x7755CC), Mono(10.f, true));
        g.Text(Loc::Get("bug_report_discord_info_perm_desc"), { card.x + 13.f, card.y + 9.f + nameH + 3.f, inner, descH }, Hex(0x4A5A7A), desc);
        g.Text(std::wstring(1, Glyphs::CheckMark), { card.R() - 13.f - 14.f, card.y, 14.f, card.h }, Hex(0x336644), IconFont(12.f));
    }
    col.Gap(10.f);
    paragraph(Loc::Get("bug_report_discord_info_no_access"), 9.f, Hex(0x334444), false, 0.f);
    frame.SetContentHeight(18.f + col.Height() + 18.f);

    const std::wstring back = Loc::Get("back");
    const float w = MenuKit::FooterButtonWidth(ui, back, Glyphs::Back, 110.f);
    if (MenuKit::FooterButton(ui, ui.MakeId("discord-info-close"), { frame.footer.R() - 16.f - w, frame.footer.CY() - 16.f, w, 32.f }, back,
                              MenuKit::Kind::Secondary, true, Glyphs::Back, Hex(0x9944CC)) ||
        ui.KeyPressed(VK_ESCAPE))
        open = false;
}

} // namespace SpeedrunLauncher::Main
