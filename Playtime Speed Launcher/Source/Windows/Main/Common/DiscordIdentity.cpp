#include "Windows/Main/Common/DiscordIdentity.h"
#include <algorithm>
#include "Localization/Localization.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/Panel.h"
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

void DiscordIdentity::Draw(Ui& ui, Column& col, const std::function<void()>& onInfo, const DiscordIdentityStyle& st) {
    auto& g = ui.g;
    if (user) {
        // Connected: username + ✕
        const float h = 1.f + 8.f + std::max(g.Measure(L"\xE8BD", IconFont(14.f)).h, g.Measure(L"Ag", Mono(11.f, true)).h) + 8.f + 1.f;
        const Rect r = col.Next(std::max(h, 22.f + 18.f), st.marginBottom);
        g.Border(r, 3.f, Hex(0x061408), Hex(0x1A3A1A), 1.f);
        const Rect inner = r.Inset(11.f, 9.f);
        g.Text(L"\xE8BD", { inner.x, inner.y, 16.f, inner.h }, Hex(0x00CC88), IconFont(14.f));
        g.Text(user->username, { inner.x + 24.f, inner.y, inner.w - 60.f, inner.h }, Hex(0x00CC88), Mono(11.f, true).Ellipsis());
        if (TextButton(ui, ui.MakeId("discord-disconnect"), { inner.R() - 22.f, inner.CY() - 11.f, 22.f, 22.f }, L"\x2715", Mono(9.f).Center(),
                       Hex(0x336633), { Colors::Transparent, Hex(0x2A4A2A), 1.f, 11.f, { 0.f }, 0.f, 22.f }))
            Disconnect();
        return;
    }
    if (waiting) {
        const Rect row = col.Next(20.f, st.marginBottom);
        const std::wstring text = Loc::Get("bug_report_discord_waiting");
        const float w = g.Measure(text, Mono(9.f)).w + 1.f;
        g.Text(text, { row.x, row.y, w, row.h }, st.accent, Mono(9.f));
        const std::wstring cancelText = Loc::Get("bug_report_discord_cancel");
        const ButtonStyle s{ Colors::Transparent, st.border, 1.f, 2.f, { 8.f, 0.f }, 0.f, 20.f };
        if (TextButton(ui, ui.MakeId("discord-cancel"), { row.x + w + 10.f, row.y, ButtonWidth(ui, cancelText, Mono(9.f, true), s), 20.f }, cancelText,
                       Mono(9.f, true).Center(), st.accent, s))
            CancelAuth();
        ui.KeepAnimatingThrottled();
        return;
    }
    // Connect + "?"
    const Rect row = col.Next(34.f, st.marginBottom);
    const std::wstring text = Loc::Get("bug_report_discord_connect_btn");
    const ButtonStyle s{ st.buttonBackground, st.border, 1.f, 3.f, { 14.f, 0.f }, 0.f, 34.f };
    const float w = ButtonWidthFor(13.f + 8.f + g.Measure(text, Mono(10.f, true)).w, s);
    if (GlyphTextButton(ui, ui.MakeId("discord-connect"), { row.x, row.y, w, 34.f }, 0xE8BD, 13.f, st.glyphText, text, Mono(10.f, true),
                        st.glyphText, s, 8.f))
        Connect();
    if (TextButton(ui, ui.MakeId("discord-info"), { row.x + w + 8.f, row.CY() - 13.f, 26.f, 26.f }, L"?", Mono(11.f, true).Center(), st.accent,
                   { Colors::Transparent, st.border, 1.f, 13.f, { 0.f }, 0.f, 26.f }) &&
        onInfo)
        onInfo();
}

void ShowDiscordRequired(HWND owner, const char* introKey, std::function<void()> onConnect) {
    Dialogs::Options o;
    o.title = Loc::Get("bug_report_discord_required_title");
    o.primary = Loc::Get("bug_report_discord_connect_btn");
    o.close = Loc::Get("back");
    const std::string key = introKey;
    Dialogs::Show(owner, o, [key](Ui& ui, const Rect& area) {
        auto& g = ui.g;
        Column col(area);
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
    const auto paragraph = [&](const std::wstring& text, float size, Color color, bool bold, float marginBottom) {
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
    ButtonStyle s = Styles::Footer();
    s.minWidth = 100.f;
    s.height = 32.f;
    const float w = ButtonWidth(ui, back, Mono(11.f, true), s);
    if (TextButton(ui, ui.MakeId("discord-info-close"), { frame.footer.R() - 16.f - w, frame.footer.CY() - 16.f, w, 32.f }, back,
                   Mono(11.f, true).Center(), Palette::Dim, s))
        open = false;
}

} // namespace SpeedrunLauncher::Main
