#include <algorithm>
#include <functional>
#include "App/Config.h"
#include "App/Launcher.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/Account/AccountService.h"
#include "Services/Chapters/ChapterPlaytimeStore.h"
#include "UI/Layout.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "UI/Widgets/TextBox.h"
#include "Seasonal/SeasonalTheme.h"
#include "Windows/Main/Common/ProfileDraw.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/Account/AccountKit.h"
#include "Windows/Main/Overlays/Account/AccountOverlay.h"
#include "Windows/Standalone/Common/RemoteImages.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
using namespace AccountKit;
using Services::Account::AccountService;
namespace Acc = Services::Account;

namespace {

constexpr float kLeftW = 380.f;
constexpr float kSwatch = 24.f, kSwatchGap = 8.f;
constexpr float kRing = 5.f;   // room for the selected colour's ring (radius + 3 + its stroke)

double TotalSeconds() {
    auto& l = App::L();
    double total = 0.0;
    for (const auto& chapter : l.chapters)
        if (chapter.isAvailable) total += l.playtime.Seconds(chapter.number);
    for (const auto& game : l.multiplayers) total += l.playtime.Seconds(game.playtimeId);
    return total;
}

// Small outlined button (height 32); returns true when clicked
bool SmallButton(Ui& ui, Id id, float x, float y, const std::wstring& text, wchar_t glyph, float& width, bool enabled = true,
                 MenuKit::Kind kind = MenuKit::Kind::Secondary, Color accent = kAccent) {
    width = MenuKit::FooterButtonWidth(ui, text, glyph, 90.f);
    return MenuKit::FooterButton(ui, id, { x, y, width, 32.f }, text, kind, enabled, glyph, accent);
}

// A row of colour circles + a "#RRGGBB" field. Returns the height used.
// kAutoColor in `colors` is the "follow the theme" swatch (an "A" over the theme's panel colour)
float ColorPicker(Ui& ui, std::string_view idBase, float x, float y, float w, const std::vector<unsigned>& colors, unsigned& value,
                  std::wstring& hexBuffer) {
    auto& g = ui.g;
    const int perRow = std::max(1, static_cast<int>((w - 110.f - kRing * 2.f + kSwatchGap) / (kSwatch + kSwatchGap)));
    float cy = y;
    for (size_t i = 0; i < colors.size(); ++i) {
        const int col = static_cast<int>(i) % perRow, row = static_cast<int>(i) / perRow;
        // Inset by the selection ring so it isn't cut by the scroll area's edge
        const Rect r{ x + kRing + col * (kSwatch + kSwatchGap), y + kRing + row * (kSwatch + kSwatchGap), kSwatch, kSwatch };
        cy = r.B() + kRing;
        const Id id = ui.MakeId(idBase, static_cast<int64_t>(i));
        const Interaction it = ui.Interact(id, r);
        const bool selected = colors[i] == value;
        const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 18.f);
        NoColorFilter keep(g);
        const float grow = 1.5f * hover;
        if (selected) g.StrokeEllipse(r.CX(), r.CY(), kSwatch * 0.5f + 3.f, kSwatch * 0.5f + 3.f, Colors::White, 2.f);
        if (colors[i] == Acc::kAutoColor) {
            g.FillEllipse(r.CX(), r.CY(), kSwatch * 0.5f + grow, kSwatch * 0.5f + grow, Hex(Acc::kDefaultCardColor));
            UI::Glyph(ui, wchar_t(0xE80F), r, 10.f, Hex(0x8AAABB));   // home: the default
            if (it.hovered) ui.Tooltip(id, Loc::Get("acc_color_auto"));
        } else {
            g.FillEllipse(r.CX(), r.CY(), kSwatch * 0.5f + grow, kSwatch * 0.5f + grow, Hex(colors[i]));
        }
        g.StrokeEllipse(r.CX(), r.CY(), kSwatch * 0.5f + grow - 0.5f, kSwatch * 0.5f + grow - 0.5f, Rgba(255, 255, 255, 40), 1.f);
        if (it.hovered) ui.SetCursor(IDC_HAND);
        if (it.clicked) value = colors[i];
    }
    // Custom colour
    const Id fieldId = ui.MakeId(idBase, 1000);
    if (ui.Focus() != fieldId) hexBuffer = Acc::ColorText(value);
    TextBoxStyle style = Field(false, 7);
    style.text = Mono(10.5f);
    style.padding = { 8.f, 5.f };
    style.placeholder = Loc::Get("acc_color_auto_short");
    const Rect field{ x + w - 96.f, y + kRing, 96.f, 28.f };
    {
        NoColorFilter keep(g);
        g.FillEllipse(field.x - 14.f, field.CY(), 6.f, 6.f, Hex(value == Acc::kAutoColor ? Acc::kDefaultCardColor : value));
    }
    if (TextBox(ui, fieldId, field, hexBuffer, style)) {
        std::wstring text = Text::Trim(hexBuffer);
        if (!text.empty() && text[0] != L'#') text = L"#" + text;
        if (text.size() == 7) value = Acc::ParseColor(Text::Narrow(text), value);
        else if (text.empty() && std::find(colors.begin(), colors.end(), Acc::kAutoColor) != colors.end()) value = Acc::kAutoColor;
    }
    return std::max(cy, field.B()) - y;
}

} // namespace

// ── Draft ───────────────────────────────────────────────────────────────────

bool AccountOverlay::Dirty() const {
    return pendingAvatar.has_value() || pendingBanner.has_value() || !draft.SameStyle(draftBase);
}

void AccountOverlay::Discard() {
    draft = draftBase = AccountService::Get().Me();
    pendingAvatar.reset();
    pendingBanner.reset();
}

std::wstring AccountOverlay::AvatarSource() const { return pendingAvatar ? *pendingAvatar : draft.AvatarUrl(); }
std::wstring AccountOverlay::BannerSource() const { return pendingBanner ? *pendingBanner : draft.BannerUrl(); }

void AccountOverlay::PickPicture(bool banner) {
    const std::wstring file = System::PickFile(main.Hwnd(), Loc::Get(banner ? "acc_pick_banner" : "acc_pick_avatar"),
                                               { { Loc::Get("acc_pictures"), L"*.png;*.jpg;*.jpeg;*.gif;*.webp;*.bmp" } });
    if (file.empty()) return;
    const auto size = Files::Size(file);
    if (size && *size > Config::Supabase::MaxSourcePictureBytes) {
        status = Loc::Get("acc_err_too_big");
        statusOk = false;
        return;
    }
    status.clear();
    (banner ? pendingBanner : pendingAvatar) = file;
}

void AccountOverlay::Save() {
    auto& account = AccountService::Get();
    if (!Dirty() || account.SaveBusy()) return;
    status.clear();
    auto token = alive;
    account.SaveProfile(draft, pendingAvatar, pendingBanner, [this, token](const std::wstring& error) {
        if (!*token) return;
        if (!error.empty()) {
            status = error;
            statusOk = false;
            return;
        }
        status = Loc::Get("acc_saved");
        statusOk = true;
        Discard();
    });
}

// ── Page ────────────────────────────────────────────────────────────────────

void AccountOverlay::DrawProfile(Ui& ui, const Rect& body) {
    auto& account = AccountService::Get();
    // While nothing is being edited, follow the account (the profile may arrive after opening)
    // (also when the profile arrives after signing in: nothing was edited yet, so nothing is lost)
    if (!Dirty() && !account.SaveBusy()) draft = draftBase = account.Me();

    const Rect left = body.Left(kLeftW);
    const Rect right{ left.R() + 22.f, body.y, body.w - kLeftW - 22.f, body.h };

    // Preview card
    const float cardH = ProfileDraw::CardHeight(left.w);
    {
        StaggerItem in(ui, tabChangedAt, 0, -20.f);
        ProfileDraw::CardInfo info;
        info.name = draft.ShownName(Loc::Get("acc_default_name"));
        info.avatarSource = AvatarSource();
        info.bannerSource = BannerSource();
        const Profile& me = account.Me();
        if (me.SpeedrunLinked()) {
            info.line1 = L"\x2713 speedrun.com/" + me.speedrunName;
            info.line1Color = kSpeedrun;
        } else {
            info.line1 = Loc::Get("acc_not_linked_short");
            info.line1Color = kSub;
        }
        info.line2 = Loc::Fmt("acc_card_hours", Services::ChapterPlaytimeStore::Format(TotalSeconds()));
        ProfileDraw::Card(ui, left.Top(cardH), draft, info);
        if (Dirty()) ui.g.Text(Loc::Get("acc_preview_unsaved"), { left.x, left.y + cardH + 4.f, left.w, 14.f }, MenuKit::kAmber, Mono(9.f, true).Center());
    }
    {
        StaggerItem in(ui, tabChangedAt, 1, -20.f);
        DrawAccountBox(ui, { left.x, left.y + cardH + 22.f, left.w, left.B() - left.y - cardH - 22.f });
    }

    // Editor (scrolls)
    StaggerItem in(ui, tabChangedAt, 0, 24.f);
    ScrollArea scroll(ui, ui.MakeId("acc-editor"), right);
    const float h = DrawEditor(ui, { right.x, right.y, right.w - 12.f, right.h });
    scroll.SetContentSize(h);
}

// Email (hidden), speedrun.com, Discord and sign out (scrolls when the window is short)
void AccountOverlay::DrawAccountBox(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    auto& account = AccountService::Get();
    Card(ui, r);
    const Rect view = r.Inset(1.f, 1.f);
    ScrollArea scroll(ui, ui.MakeId("acc-box"), view);
    Column col(r.x + 16.f, r.y + 14.f, r.w - 32.f);

    // Email: ******@gmail.com unless "show" is pressed
    Caption(ui, col.x, col.y, col.w, Loc::Get("acc_email"));
    col.Gap(20.f);
    {
        const Rect row = col.Next(30.f, 12.f);
        const std::wstring shown = showEmail ? account.Email() : Services::Account::MaskEmail(account.Email());
        const std::wstring label = Loc::Get(showEmail ? "acc_hide" : "acc_show");
        const wchar_t glyph = showEmail ? wchar_t(0xED1A) : wchar_t(0xE890);   // hide · view
        const float bw = MenuKit::FooterButtonWidth(ui, label, glyph, 80.f);
        g.Text(shown, { row.x, row.y, row.w - bw - 10.f, row.h }, kText, Mono(11.f, true).Ellipsis());
        if (MenuKit::FooterButton(ui, ui.MakeId("acc-show-email"), { row.R() - bw, row.y, bw, row.h }, label, MenuKit::Kind::Secondary, true, glyph,
                                  kAccent))
            showEmail = !showEmail;
    }

    DrawSpeedrunBox(ui, col);
    col.Gap(4.f);
    g.FillRect({ col.x, col.y, col.w, 1.f }, Palette::Divider);
    col.Gap(14.f);
    DrawDiscordBox(ui, col);

    // Sign out (asks once more)
    const std::wstring label = Loc::Get(confirmSignOut ? "acc_signout_confirm" : "acc_signout");
    const float bw = MenuKit::FooterButtonWidth(ui, label, Glyphs::Power, 120.f);
    const Rect button{ r.x + 16.f, std::max(col.y + 4.f, r.B() - 14.f - 34.f), bw, 34.f };
    scroll.SetContentSize(button.B() + 14.f - view.y);
    if (MenuKit::FooterButton(ui, ui.MakeId("acc-signout"), button, label, MenuKit::Kind::Danger, !account.SaveBusy(), Glyphs::Power)) {
        if (!confirmSignOut) {
            confirmSignOut = true;
        } else {
            confirmSignOut = false;
            account.SignOut();
            status.clear();
            email.clear();
            Discard();
        }
    }
    if (confirmSignOut && !ui.MouseOver(button) && ui.In().pressed[0]) confirmSignOut = false;
}

void AccountOverlay::DrawSpeedrunBox(Ui& ui, Column& col) {
    auto& g = ui.g;
    auto& account = AccountService::Get();
    const Profile& me = account.Me();
    Caption(ui, col.x, col.y, col.w, L"SPEEDRUN.COM", kSpeedrun);
    col.Gap(22.f);

    if (me.SpeedrunLinked()) {
        const Rect row = col.Next(40.f, 10.f);
        const Rect pic{ row.x, row.CY() - 18.f, 36.f, 36.f };
        if (auto image = me.speedrunAvatar.empty() ? nullptr : Windows::RemoteImages::Get(me.speedrunAvatar, 96))
            g.DrawImageRounded(*image, pic, 18.f);
        else
            g.FillEllipse(pic.CX(), pic.CY(), 18.f, 18.f, kSpeedrun.WithAlpha(0.2f));
        g.StrokeEllipse(pic.CX(), pic.CY(), 18.f, 18.f, kSpeedrun, 1.5f);
        g.Text(me.speedrunName, { pic.R() + 10.f, row.y + 2.f, row.w - 50.f, 20.f }, kSpeedrun, Mono(12.f, true).Ellipsis());
        g.Text(L"\x2713 " + Loc::Get("acc_src_verified"), { pic.R() + 10.f, row.y + 21.f, row.w - 50.f, 16.f }, MenuKit::kGreen, Mono(9.5f, true));
        const Rect buttons = col.Next(32.f, 14.f);
        float w = 0.f;
        if (SmallButton(ui, ui.MakeId("acc-src-open"), buttons.x, buttons.y, Loc::Get("acc_src_open"), wchar_t(0xE8A7), w, !me.speedrunUrl.empty(),
                        MenuKit::Kind::Secondary, kSpeedrun))
            Process::OpenUrl(me.speedrunUrl);
        float w2 = 0.f;
        if (SmallButton(ui, ui.MakeId("acc-src-unlink"), buttons.x + w + 8.f, buttons.y, Loc::Get(account.LinkBusy() ? "acc_working" : "acc_src_unlink"),
                        Glyphs::Cancel, w2, !account.LinkBusy(), MenuKit::Kind::Danger)) {
            auto token = alive;
            account.UnlinkSpeedrun([this, token](const std::wstring& error) {
                if (!*token) return;
                status = error.empty() ? Loc::Get("acc_src_unlinked") : error;
                statusOk = error.empty();
            });
        }
        return;
    }

    // Not linked: how to prove it's yours
    TextStyle text = Mono(9.5f);
    text.wrap = true;
    text.valign = VAlign::Top;
    const std::wstring& why = Loc::Get("acc_src_why");
    const float whyH = g.Measure(why, text, col.w).h;
    g.Text(why, col.Next(whyH, 6.f), kSub, text);
    {
        const std::wstring link = Loc::Get("acc_src_get_key");
        const float lw = std::min(col.w, g.Measure(link, Mono(10.f, true)).w + 4.f);
        if (LinkButton(ui, ui.MakeId("acc-src-keypage"), col.Next(18.f, 8.f).Left(lw), link, Mono(10.f, true), kSpeedrun))
            Process::OpenUrl(Config::SpeedrunCom::ApiKeyPage);
    }
    const Rect row = col.Next(32.f, 10.f);
    const std::wstring label = Loc::Get(account.LinkBusy() ? "acc_working" : "acc_src_link");
    const float bw = MenuKit::FooterButtonWidth(ui, label, wchar_t(0xE71B), 96.f);
    TextBoxStyle style = Field(true, 64);
    style.placeholder = Loc::Get("acc_src_key_placeholder");
    style.padding = { 10.f, 6.f };
    bool enter = false;
    TextBox(ui, ui.MakeId("acc-src-key"), { row.x, row.y, row.w - bw - 8.f, row.h }, apiKey, style, &enter);
    if (MenuKit::FooterButton(ui, ui.MakeId("acc-src-link"), { row.R() - bw, row.y, bw, row.h }, label, MenuKit::Kind::Primary,
                              !account.LinkBusy() && !Text::Trim(apiKey).empty(), wchar_t(0xE71B), kSpeedrun) ||
        (enter && !account.LinkBusy())) {
        auto token = alive;
        account.LinkSpeedrun(apiKey, [this, token](const std::wstring& error) {
            if (!*token) return;
            if (error.empty()) apiKey.clear();
            status = error.empty() ? Loc::Get("acc_src_linked") : error;
            statusOk = error.empty();
        });
    }
}

// Discord: the linked account (name + picture, open / unlink), the "waiting for the browser" line
// with Cancel, or the Connect button
void AccountOverlay::DrawDiscordBox(Ui& ui, Column& col) {
    auto& g = ui.g;
    auto& account = AccountService::Get();
    const Profile& me = account.Me();
    Caption(ui, col.x, col.y, col.w, L"DISCORD", kBlurple);
    col.Gap(22.f);

    if (me.DiscordLinked()) {
        const Rect row = col.Next(40.f, 10.f);
        const Rect pic{ row.x, row.CY() - 18.f, 36.f, 36.f };
        if (auto image = me.discordAvatar.empty() ? nullptr : Windows::RemoteImages::Get(me.discordAvatar, 96))
            g.DrawImageRounded(*image, pic, 18.f);
        else {
            g.FillEllipse(pic.CX(), pic.CY(), 18.f, 18.f, kBlurple.WithAlpha(0.25f));
            g.Text(me.discordName.empty() ? L"?" : Text::Upper(me.discordName.substr(0, 1)), pic, kText, Mono(13.f, true).Center());
        }
        g.StrokeEllipse(pic.CX(), pic.CY(), 18.f, 18.f, kBlurple, 1.5f);
        g.Text(L"@" + me.discordName, { pic.R() + 10.f, row.y + 2.f, row.w - 50.f, 20.f }, kBlurple, Mono(12.f, true).Ellipsis());
        g.Text(L"\x2713 " + Loc::Get("acc_src_verified"), { pic.R() + 10.f, row.y + 21.f, row.w - 50.f, 16.f }, MenuKit::kGreen, Mono(9.5f, true));
        const Rect buttons = col.Next(32.f, 14.f);
        float w = 0.f;
        if (SmallButton(ui, ui.MakeId("acc-dc-open"), buttons.x, buttons.y, Loc::Get("acc_src_open"), wchar_t(0xE8A7), w, true, MenuKit::Kind::Secondary,
                        kBlurple))
            Process::OpenUrl(std::wstring(Config::Discord::UserProfileUrl) + me.discordId);
        float w2 = 0.f;
        if (SmallButton(ui, ui.MakeId("acc-dc-unlink"), buttons.x + w + 8.f, buttons.y, Loc::Get(account.DiscordBusy() ? "acc_working" : "acc_src_unlink"),
                        Glyphs::Cancel, w2, !account.DiscordBusy(), MenuKit::Kind::Danger)) {
            auto token = alive;
            account.UnlinkDiscord([this, token](const std::wstring& error) {
                if (!*token) return;
                status = error.empty() ? Loc::Get("acc_dc_unlinked") : error;
                statusOk = error.empty();
            });
        }
        return;
    }

    TextStyle text = Mono(9.5f);
    text.wrap = true;
    text.valign = VAlign::Top;
    const std::wstring& why = Loc::Get(account.DiscordWaiting() ? "acc_dc_waiting" : "acc_dc_why");
    const float whyH = g.Measure(why, text, col.w).h;
    g.Text(why, col.Next(whyH, 8.f), account.DiscordWaiting() ? kBlurple : kSub, text);
    const Rect row = col.Next(32.f, 10.f);
    if (account.DiscordWaiting()) {
        Form::Spinner(ui, row.x + 9.f, row.CY(), 7.f, kBlurple);
        ui.KeepAnimatingThrottled();
        float w = 0.f;
        if (SmallButton(ui, ui.MakeId("acc-dc-cancel"), row.x + 28.f, row.y, Loc::Get("cancel"), Glyphs::Cancel, w)) account.CancelDiscordLink();
        return;
    }
    const std::wstring label = Loc::Get(account.DiscordBusy() ? "acc_working" : "acc_dc_link");
    const float bw = MenuKit::FooterButtonWidth(ui, label, wchar_t(0xE71B), 140.f);
    if (MenuKit::FooterButton(ui, ui.MakeId("acc-dc-link"), { row.x, row.y, bw, row.h }, label, MenuKit::Kind::Primary, !account.DiscordBusy(), wchar_t(0xE71B),
                              kBlurple)) {
        auto token = alive;
        account.LinkDiscord([this, token](const std::wstring& error) {
            if (!*token) return;
            status = error.empty() ? Loc::Get("acc_dc_linked") : error;
            statusOk = error.empty();
        });
    }
}

// Name, pictures, font, effect and colours
float AccountOverlay::DrawEditor(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    Column col(r.x, r.y, r.w);
    int reveal = 0;
    const auto section = [&](const char* key) {
        Caption(ui, col.x, col.y, col.w, Loc::Get(key));
        col.Gap(22.f);
    };

    {
        StaggerItem in(ui, tabChangedAt, reveal++, 24.f);
        section("acc_display_name");
        TextBoxStyle style = Field(false, 32);
        style.placeholder = Loc::Get("acc_display_name_placeholder");
        const float h = TextBoxHeight(ui, style) + 4.f;
        Form::Field(ui, ui.MakeId("acc-name"), col.Next(h, 20.f), draft.displayName, style, kAccent);
        if (draft.displayName.size() > 32) draft.displayName.resize(32);
    }

    // Pictures
    {
        StaggerItem in(ui, tabChangedAt, reveal++, 24.f);
        const float half = (col.w - 20.f) * 0.5f;
        const float top = col.y;
        // Profile picture
        Caption(ui, col.x, top, half, Loc::Get("acc_avatar"));
        const Rect avatar{ col.x, top + 24.f, 64.f, 64.f };
        ProfileDraw::Avatar(ui, AvatarSource(), avatar, draft, draft.ShownName(Loc::Get("acc_default_name")));
        float w = 0.f;
        if (SmallButton(ui, ui.MakeId("acc-avatar-pick"), avatar.R() + 12.f, avatar.y, Loc::Get("acc_change"), Glyphs::Image, w)) PickPicture(false);
        if (!AvatarSource().empty() && SmallButton(ui, ui.MakeId("acc-avatar-remove"), avatar.R() + 12.f, avatar.y + 36.f, Loc::Get("acc_remove"), Glyphs::Delete, w,
                                                   true, MenuKit::Kind::Danger))
            pendingAvatar = std::wstring();
        // Banner
        const float bx = col.x + half + 20.f;
        Caption(ui, bx, top, half, Loc::Get("acc_banner"));
        const Rect banner{ bx, top + 24.f, std::min(150.f, half - 110.f), 64.f };
        ProfileDraw::Banner(ui, BannerSource(), banner, draft, 8.f);
        if (SmallButton(ui, ui.MakeId("acc-banner-pick"), banner.R() + 12.f, banner.y, Loc::Get("acc_change"), Glyphs::Image, w)) PickPicture(true);
        if (!BannerSource().empty() && SmallButton(ui, ui.MakeId("acc-banner-remove"), banner.R() + 12.f, banner.y + 36.f, Loc::Get("acc_remove"), Glyphs::Delete, w,
                                                   true, MenuKit::Kind::Danger))
            pendingBanner = std::wstring();
        col.y = top + 24.f + 64.f + 8.f;
        g.Text(Loc::Get("acc_pictures_hint"), col.Next(16.f, 16.f), kSub, Mono(9.5f).Ellipsis());
    }
    {
        StaggerItem in(ui, tabChangedAt, reveal++, 24.f);
        section("acc_banner_color");
        col.Gap(ColorPicker(ui, "acc-banner-color", col.x, col.y, col.w, Acc::BannerColors(), draft.bannerColor, customBanner) + 22.f);
    }
    {
        StaggerItem in(ui, tabChangedAt, reveal++, 24.f);
        section("acc_card_color");
        col.Gap(ColorPicker(ui, "acc-card-color", col.x, col.y, col.w, Acc::CardColors(), draft.cardColor, customCard) + 22.f);
    }

    // Fonts: "Gg" in each one
    {
        StaggerItem in(ui, tabChangedAt, reveal++, 24.f);
        section("acc_pick_font");
        const auto& fonts = Acc::NameFonts();
        constexpr int kColumns = 4;
        constexpr float kTileH = 58.f, kGap = 8.f;
        const float tileW = (col.w - kGap * (kColumns - 1)) / kColumns;
        for (size_t i = 0; i < fonts.size(); ++i) {
            const Rect t{ col.x + (i % kColumns) * (tileW + kGap), col.y + (i / kColumns) * (kTileH + kGap), tileW, kTileH };
            const Id id = ui.MakeId("acc-font", static_cast<int64_t>(i));
            const Interaction it = ui.Interact(id, t);
            const bool sel = draft.font == fonts[i].id;
            const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 16.f);
            const float on = ui.Animate(HashId(id, 2), sel ? 1.f : 0.f, 14.f);
            if (on > 0.01f) g.GlowRound(t, 8.f, kAccent, 12.f, 0.3f * on);
            g.Border(t, 8.f, Lerp(kRow, kRowHover, hover), Lerp(Lerp(kCardBorder, kHoverBorder, hover), kAccent, on), on > 0.5f ? 1.5f : 1.f);
            Profile sample;
            sample.font = fonts[i].id;
            TextStyle st = ProfileDraw::NameStyle(sample, 22.f);
            st.align = HAlign::Center;
            g.Text(L"Gg", t, Lerp(Rgba(200, 210, 220), Colors::White, std::max(on, hover)), st);
            if (it.hovered) ui.SetCursor(IDC_HAND);
            if (it.clicked) draft.font = fonts[i].id;
        }
        col.Gap(((fonts.size() + kColumns - 1) / kColumns) * (kTileH + kGap) + 14.f);
    }

    // Effects: the name drawn with each one
    {
        StaggerItem in(ui, tabChangedAt, reveal++, 24.f);
        section("acc_pick_effect");
        const auto& effects = Acc::NameEffects();
        constexpr int kColumns = 4;
        constexpr float kTileH = 54.f, kGap = 8.f;
        const float tileW = (col.w - kGap * (kColumns - 1)) / kColumns;
        for (size_t i = 0; i < effects.size(); ++i) {
            const Rect t{ col.x + (i % kColumns) * (tileW + kGap), col.y + (i / kColumns) * (kTileH + kGap), tileW, kTileH };
            const Id id = ui.MakeId("acc-effect", static_cast<int64_t>(i));
            const Interaction it = ui.Interact(id, t);
            const bool sel = draft.effect == effects[i].id;
            const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 16.f);
            const float on = ui.Animate(HashId(id, 2), sel ? 1.f : 0.f, 14.f);
            if (on > 0.01f) g.GlowRound(t, 8.f, kAccent, 12.f, 0.3f * on);
            g.Border(t, 8.f, Lerp(kRow, kRowHover, hover), Lerp(Lerp(kCardBorder, kHoverBorder, hover), kAccent, on), on > 0.5f ? 1.5f : 1.f);
            ui.PushClip(t.Inset(4.f));
            ProfileDraw::NameWithEffect(ui, Loc::Get(effects[i].locKey), t.Inset(8.f, 0.f), draft, effects[i].id, 13.f);
            ui.PopClip();
            if (it.hovered) ui.SetCursor(IDC_HAND);
            if (it.clicked) draft.effect = effects[i].id;
        }
        col.Gap(((effects.size() + kColumns - 1) / kColumns) * (kTileH + kGap) + 14.f);
    }

    // Colours of the name
    {
        StaggerItem in(ui, tabChangedAt, reveal++, 24.f);
        const auto& effect = draft.Effect();
        if (effect.animated) {
            g.Text(Loc::Get("acc_prism_colors"), col.Next(16.f, 20.f), kSub, Mono(9.5f).Ellipsis());
        } else {
            section("acc_color1");
            col.Gap(ColorPicker(ui, "acc-color1", col.x, col.y, col.w, Acc::NameColors(), draft.color1, customColor1) + 18.f);
            if (effect.twoColors) {
                section("acc_color2");
                col.Gap(ColorPicker(ui, "acc-color2", col.x, col.y, col.w, Acc::NameColors(), draft.color2, customColor2) + 18.f);
            }
        }
    }
    // Cosmetics: each tile shows your profile wearing it
    const std::wstring shownName = draft.ShownName(Loc::Get("acc_default_name"));
    // A tile of a picker; returns true when clicked
    const auto tile = [&](std::string_view idBase, size_t i, const Rect& t, bool sel, const char* locKey, const std::function<void(const Rect&)>& preview) {
        const Id id = ui.MakeId(idBase, static_cast<int64_t>(i));
        const Interaction it = ui.Interact(id, t);
        const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 16.f);
        const float on = ui.Animate(HashId(id, 2), sel ? 1.f : 0.f, 14.f);
        if (on > 0.01f) g.GlowRound(t, 8.f, kAccent, 12.f, 0.3f * on);
        g.Border(t, 8.f, Lerp(kRow, kRowHover, hover), Lerp(Lerp(kCardBorder, kHoverBorder, hover), kAccent, on), on > 0.5f ? 1.5f : 1.f);
        const Rect area{ t.x + 4.f, t.y + 4.f, t.w - 8.f, t.h - 24.f };
        ui.PushClip(t.Inset(1.f));
        preview(area);
        ui.PopClip();
        g.Text(Loc::Get(locKey), { t.x + 4.f, t.B() - 20.f, t.w - 8.f, 16.f }, Lerp(kSub, kText, std::max(on, hover)), Mono(8.5f, true).Center().Ellipsis());
        if (it.hovered) ui.SetCursor(IDC_HAND);
        return it.clicked;
    };

    // Avatar decorations
    {
        StaggerItem in(ui, tabChangedAt, reveal++, 24.f);
        section("cos_avatar_title");
        const auto& list = Acc::AvatarDecorations();
        constexpr int kColumns = 5;
        constexpr float kTileH = 96.f, kGap = 8.f;
        const float tileW = (col.w - kGap * (kColumns - 1)) / kColumns;
        for (size_t i = 0; i < list.size(); ++i) {
            const Rect t{ col.x + (i % kColumns) * (tileW + kGap), col.y + (i / kColumns) * (kTileH + kGap), tileW, kTileH };
            const bool none = std::string(list[i].id) == "none";
            if (tile("cos-deco", i, t, draft.avatarDeco == list[i].id, list[i].locKey, [&](const Rect& a) {
                    Profile sample = draft;
                    sample.avatarDeco = list[i].id;
                    const float side = std::min(a.h, a.w) * 0.58f;
                    ProfileDraw::Avatar(ui, AvatarSource(), a.Center(side, side), sample, shownName);
                    if (none) {
                        NoColorFilter keep(g);
                        g.FillEllipse(a.CX() + side * 0.38f, a.CY() + side * 0.38f, 9.f, 9.f, Hex(0x0A1420));
                        UI::Glyph(ui, wchar_t(0xE711), { a.CX() + side * 0.38f - 9.f, a.CY() + side * 0.38f - 9.f, 18.f, 18.f }, 9.f, kSub);
                    }
                }))
                draft.avatarDeco = list[i].id;
        }
        col.Gap(((list.size() + kColumns - 1) / kColumns) * (kTileH + kGap) + 14.f);
    }

    // Profile frames: a small card in each one
    {
        StaggerItem in(ui, tabChangedAt, reveal++, 24.f);
        section("cos_frame_title");
        const auto& list = Acc::ProfileFrames();
        constexpr int kColumns = 3;
        constexpr float kTileH = 120.f, kGap = 8.f;
        const float tileW = (col.w - kGap * (kColumns - 1)) / kColumns;
        for (size_t i = 0; i < list.size(); ++i) {
            const Rect t{ col.x + (i % kColumns) * (tileW + kGap), col.y + (i / kColumns) * (kTileH + kGap), tileW, kTileH };
            if (tile("cos-frame", i, t, draft.frame == list[i].id, list[i].locKey, [&](const Rect& a) {
                    Profile sample = draft;
                    sample.frame = list[i].id;
                    sample.avatarDeco = "none";
                    const Rect card = a.Inset(14.f, 10.f);
                    {
                        NoColorFilter keep(g);
                        g.FillRound(card, 8.f, ProfileDraw::CardColor(sample));
                    }
                    g.PushRoundClip(card, 8.f);
                    ProfileDraw::Banner(ui, BannerSource(), card.Top(card.h * 0.38f), sample, 0.f);
                    g.PopLayer();
                    const float side = card.h * 0.34f;
                    ProfileDraw::Avatar(ui, AvatarSource(), { card.x + 8.f, card.y + card.h * 0.38f - side * 0.5f, side, side }, sample, shownName, 2.f,
                                        ProfileDraw::CardColor(sample));
                    ui.PushClip(card);
                    ProfileDraw::Name(ui, shownName, { card.x + 8.f, card.B() - 24.f, card.w - 16.f, 18.f }, sample, 11.f);
                    ui.PopClip();
                    ProfileDraw::ProfileFrame(ui, card, 8.f, sample);
                }))
                draft.frame = list[i].id;
        }
        col.Gap(((list.size() + kColumns - 1) / kColumns) * (kTileH + kGap) + 14.f);
    }

    // Nameplates: how your row looks in the lists
    {
        StaggerItem in(ui, tabChangedAt, reveal++, 24.f);
        section("cos_plate_title");
        const auto& list = Acc::Nameplates();
        constexpr int kColumns = 2;
        constexpr float kTileH = 70.f, kGap = 8.f;
        const float tileW = (col.w - kGap * (kColumns - 1)) / kColumns;
        for (size_t i = 0; i < list.size(); ++i) {
            const Rect t{ col.x + (i % kColumns) * (tileW + kGap), col.y + (i / kColumns) * (kTileH + kGap), tileW, kTileH };
            if (tile("cos-plate", i, t, draft.nameplate == list[i].id, list[i].locKey, [&](const Rect& a) {
                    Profile sample = draft;
                    sample.nameplate = list[i].id;
                    sample.avatarDeco = "none";
                    const Rect row = a.Inset(4.f, 2.f);
                    g.Border(row, 8.f, kRow, kCardBorder.WithAlpha(0.5f), 1.f);
                    ProfileDraw::Nameplate(ui, row, 8.f, sample);
                    const float side = row.h - 12.f;
                    ProfileDraw::Avatar(ui, AvatarSource(), { row.x + 8.f, row.CY() - side * 0.5f, side, side }, sample, shownName);
                    ui.PushClip(row);
                    ProfileDraw::Name(ui, shownName, { row.x + side + 16.f, row.y, row.w - side - 24.f, row.h }, sample, 12.f);
                    ui.PopClip();
                }))
                draft.nameplate = list[i].id;
        }
        col.Gap(((list.size() + kColumns - 1) / kColumns) * (kTileH + kGap) + 14.f);
    }

    col.Gap(10.f);
    return col.Height();
}

} // namespace SpeedrunLauncher::Main
