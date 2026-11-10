#include "Windows/Main/Overlays/Account/AccountOverlay.h"
#include <algorithm>
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/Account/AccountService.h"
#include "Services/Account/MessagesService.h"
#include "Services/Account/FriendsService.h"
#include "UI/Layout.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/Stagger.h"
#include "UI/Widgets/TextBox.h"
#include "Windows/Main/Common/ReportKit.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/Account/AccountKit.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
using namespace AccountKit;
using Services::Account::AccountService;

namespace {

PanelStyle AccountPanel() {
    PanelStyle s;
    s.title = Loc::Get("acc_title");
    s.titleColor = kAccent;
    return s;
}

// Two pill tabs; returns the clicked index or -1
int PillTabs(Ui& ui, const Rect& r, const std::vector<std::wstring>& labels, int selected, std::string_view idBase, wchar_t glyphs[] = nullptr) {
    auto& g = ui.g;
    const float gap = 8.f;
    const float w = (r.w - gap * (labels.size() - 1)) / labels.size();
    int clicked = -1;
    float selX = r.x;
    for (size_t i = 0; i < labels.size(); ++i) {
        const Rect t{ r.x + i * (w + gap), r.y, w, r.h };
        const Id id = ui.MakeId(idBase, static_cast<int64_t>(i));
        const Interaction it = ui.Interact(id, t);
        const bool sel = static_cast<int>(i) == selected;
        const float hover = ui.Animate(HashId(id, 1), it.hovered && !sel ? 1.f : 0.f, 16.f);
        const float on = ui.Animate(HashId(id, 2), sel ? 1.f : 0.f, 12.f);
        g.Border(t, r.h * 0.5f, Lerp(kCard, kRowHover, hover), Lerp(kCardBorder, kHoverBorder, hover), 1.f);
        const Color color = Lerp(kSub, kText, std::max(on, hover));
        if (glyphs && glyphs[i]) {
            const TextStyle text = Mono(11.f, true);
            const float tw = g.Measure(labels[i], text).w;
            const float total = 18.f + 8.f + tw;
            UI::Glyph(ui, glyphs[i], { t.CX() - total * 0.5f, t.y, 18.f, t.h }, 12.f, Lerp(kSub, kAccent, on));
            g.Text(labels[i], { t.CX() - total * 0.5f + 26.f, t.y, tw + 2.f, t.h }, color, text);
        } else {
            g.Text(labels[i], t, color, Mono(11.f, true).Center());
        }
        if (sel) selX = t.x;
        if (it.hovered && !sel) ui.SetCursor(IDC_HAND);
        if (it.clicked && !sel) clicked = static_cast<int>(i);
    }
    const float x = ui.Animate(ui.MakeId(idBase, 99), selX, 14.f, selX);
    const Rect cap{ x, r.y, w, r.h };
    g.GlowRound(cap, r.h * 0.5f, kAccent, 14.f, 0.25f);
    g.Border(cap, r.h * 0.5f, Rgba(0, 204, 170, 22), kAccent, 1.f);
    return clicked;
}

} // namespace

void AccountOverlay::Open(bool ranking) {
    auto& account = AccountService::Get();
    // Always on the profile (pending friend requests show their count on the FRIENDS tab);
    // the friends tab itself opens on its pending page while requests are waiting
    const bool requests = Services::Account::FriendsService::Get().PendingCount() > 0;
    tab = account.SignedIn() && ranking ? 1 : 0;
    if (requests) friendsPage = 2;
    friendsPageAt = Async::Now();
    friendFilter.clear();
    confirmRemove.clear();
    friendCode.clear();
    if (account.SignedIn()) Services::Account::FriendsService::Get().Refresh();
    status.clear();
    password.clear();
    password2.clear();
    apiKey.clear();
    showEmail = false;
    confirmSignOut = false;
    Discard();
    bool noticeOk = false;
    if (std::wstring notice = account.TakeNotice(&noticeOk); !notice.empty()) {
        status = notice;
        statusOk = noticeOk;
    }
    if (account.SignedIn()) account.ReloadProfile();
    openedAt = Async::Now() + 0.06;
    tabChangedAt = openedAt;
    popup.Open();
}

void AccountOverlay::Close() {
    viewer.open = false;
    Services::Account::MessagesService::Get().Open(L"");
    Services::Account::MessagesService::Get().SetWatching(false);
    if (Dirty() && AccountService::Get().SignedIn() && !AccountService::Get().SaveBusy()) Discard();
    popup.Close();
}

void AccountOverlay::Draw(Ui& ui, const Rect& screen) {
    auto& account = AccountService::Get();
    // Messages from the account while the menu is open (e.g. signed in after confirming the email)
    {
        bool noticeOk = false;
        if (std::wstring notice = account.TakeNotice(&noticeOk); !notice.empty()) {
            status = notice;
            statusOk = noticeOk;
            if (noticeOk) {
                openedAt = Async::Now();
                tabChangedAt = openedAt;
                Discard();
            }
        }
    }
    const bool signedIn = account.SignedIn();
    {
        // The open conversation is only "being read" while MESSAGES is on screen
        auto& messages = Services::Account::MessagesService::Get();
        const bool reading = signedIn && popup.IsOpen() && tab == 2 && friendsPage == 5;
        messages.SetWatching(reading);
        if (!reading && !messages.OpenWith().empty()) messages.Open(L"");
        if (reading && messages.OpenWith().empty() && !chatProfile.userId.empty()) messages.Open(chatProfile.userId);
    }
    const float width = signedIn ? std::min(1120.f, screen.w - 60.f) : 560.f;
    const float height = signedIn ? std::min(760.f, screen.h - 60.f) : 0.f;
    if (!signedIn) viewer.open = false;
    {
    OverlayFrame frame(ui, ui.MakeId("account"), screen, width, height, AccountPanel(), popup.Opacity(), popup.Scale());

    if (!signedIn) {
        frame.SetContentHeight(DrawSignIn(ui, frame.body));
    } else {
        const Rect inner = frame.body.Inset(22.f, 16.f);
        wchar_t glyphs[] = { wchar_t(0xE77B), wchar_t(0xE9F9), wchar_t(0xE716) };   // contact · chart · people
        {
            StaggerItem in(ui, openedAt, 0, 0.f, -8.f);
            const int pending = Services::Account::FriendsService::Get().PendingCount() + Services::Account::MessagesService::Get().Unread();
            const std::wstring friendsLabel = Loc::Get("fr_tab") + (pending > 0 ? L" (" + std::to_wstring(pending) + L")" : L"");
            const int clicked = PillTabs(ui, inner.Top(36.f).CenterH(600.f), { Loc::Get("acc_tab_profile"), Loc::Get("acc_tab_ranking"), friendsLabel },
                                         tab, "acc-tab", glyphs);
            if (clicked >= 0) {
                tab = clicked;
                tabChangedAt = Async::Now();
                status.clear();
                if (tab == 2) Services::Account::FriendsService::Get().Refresh();
            }
        }
        const Rect body = inner.CutTop(36.f + 16.f);
        if (tab == 0) DrawProfile(ui, body);
        else if (tab == 1) DrawRanking(ui, body);
        else DrawFriends(ui, body);
    }

    // Footer: status on the left, [SAVE] and BACK on the right
    const Rect row = frame.footer.Inset(14.f, 0.f);
    const std::wstring back = Loc::Get("back");
    const float bw = MenuKit::FooterButtonWidth(ui, back, Glyphs::Back, 120.f);
    float right = row.R() - bw;
    const bool escape = popup.IsOpen() && !viewer.open && ui.Focus() == 0 && ui.KeyPressed(VK_ESCAPE);
    if (MenuKit::FooterButton(ui, ui.MakeId("acc-close"), { right, row.CY() - 18.f, bw, 36.f }, back, MenuKit::Kind::Secondary, true, Glyphs::Back, kAccent) ||
        escape)
        Close();
    if (signedIn && tab == 0) {
        // Your profile as the others see it (with the changes not saved yet)
        {
            const std::wstring label = Loc::Get("pf_preview");
            const float pw = MenuKit::FooterButtonWidth(ui, label, wchar_t(0xE7B3), 130.f);
            right -= pw + 10.f;
            if (MenuKit::FooterButton(ui, ui.MakeId("acc-preview"), { right, row.CY() - 18.f, pw, 36.f }, label, MenuKit::Kind::Secondary, true, wchar_t(0xE7B3),
                                      kAccent))
                PreviewMyProfile();
        }
        const bool busy = account.SaveBusy();
        const std::wstring save = Loc::Get(busy ? "acc_saving" : "acc_save");
        const float sw = MenuKit::FooterButtonWidth(ui, save, Glyphs::Save, 140.f);
        right -= sw + 10.f;
        if (MenuKit::FooterButton(ui, ui.MakeId("acc-save"), { right, row.CY() - 18.f, sw, 36.f }, save, MenuKit::Kind::Primary, Dirty() && !busy,
                                  Glyphs::Save, kAccent))
            Save();
        if (Dirty() && !busy) {
            const std::wstring undo = Loc::Get("acc_discard");
            const float uw = MenuKit::FooterButtonWidth(ui, undo, Glyphs::Refresh, 110.f);
            right -= uw + 10.f;
            if (MenuKit::FooterButton(ui, ui.MakeId("acc-discard"), { right, row.CY() - 18.f, uw, 36.f }, undo, MenuKit::Kind::Secondary, true,
                                      Glyphs::Refresh, kAccent))
                Discard();
        }
    }
    ReportKit::StatusPill(ui, ui.MakeId("acc-status"), { row.x, row.CY() - 18.f, std::max(0.f, right - row.x - 14.f), 36.f }, status,
                          statusOk ? Form::Status::Success : Form::Status::Error);
    }
    if (viewer.open && popup.IsOpen() && !standaloneProfile) DrawViewer(ui, screen);
}

void AccountOverlay::OpenChat(const Profile& profile) {
    if (!popup.IsOpen()) Open();
    viewer.open = false;
    standaloneProfile = false;
    tab = 2;
    tabChangedAt = Async::Now();
    status.clear();
    chatProfile = profile;
    dmText.clear();
    chatOpenedAt = Async::Now();
    SetFriendsPage(5);
    Services::Account::MessagesService::Get().Open(profile.userId);
}

void AccountOverlay::MessageButton(const Profile& profile) {
    const Profile who = profile;   // (the viewer's profile goes with it)
    viewer.open = false;
    viewer.full = false;
    standaloneProfile = false;
    main.OpenChat(who);
}

void AccountOverlay::ShowProfile(const Profile& profile) {
    // The mini profile first; its "full profile" button opens the big one (and "back" returns to it)
    OpenViewer(profile);
    RequestRunner(profile.speedrunId);
    standaloneProfile = true;
}

void AccountOverlay::DrawProfileOnTop(Ui& ui, const Rect& screen) {
    if (!AccountService::Get().SignedIn()) viewer.open = false;
    if (viewer.open) DrawViewer(ui, screen);
    if (!viewer.open) standaloneProfile = false;
}

// ── Signed out ──────────────────────────────────────────────────────────────

void AccountOverlay::SubmitSignIn() {
    auto& account = AccountService::Get();
    if (account.AuthBusy()) return;
    const std::wstring mail = Text::Trim(email);
    if (mail.empty() || mail.find(L'@') == std::wstring::npos) {
        status = Loc::Get("acc_err_bad_email");
        statusOk = false;
        return;
    }
    if (password.size() < 6) {
        status = Loc::Get("acc_err_weak_password");
        statusOk = false;
        return;
    }
    status.clear();
    auto token = alive;
    if (signUpMode) {
        if (password != password2) {
            status = Loc::Get("acc_err_mismatch");
            statusOk = false;
            return;
        }
        account.SignUp(mail, password, [this, token](const std::wstring& error, bool confirm) {
            if (!*token) return;
            if (!error.empty()) {
                status = error;
                statusOk = false;
                return;
            }
            password.clear();
            password2.clear();
            if (confirm) {
                signUpMode = false;   // the waiting screen takes over (signs in once confirmed)
                status.clear();
            } else {
                status = Loc::Get("acc_welcome");
                statusOk = true;
                openedAt = Async::Now();
            }
        });
    } else {
        account.SignIn(mail, password, [this, token](const std::wstring& error) {
            if (!*token) return;
            if (!error.empty()) {
                status = error;
                statusOk = false;
                return;
            }
            password.clear();
            status = Loc::Get("acc_welcome");
            statusOk = true;
            Discard();
            openedAt = Async::Now();
        });
    }
}

// Account created, email not confirmed yet: wait here; the launcher signs in by itself
float AccountOverlay::DrawWaiting(Ui& ui, const Rect& bodyRect) {
    auto& g = ui.g;
    auto& account = AccountService::Get();
    const Rect body = bodyRect.Inset(30.f, 22.f);
    Column col(body.x, body.y, body.w);
    {
        StaggerItem in(ui, openedAt, 0, 0.f, 10.f);
        MenuKit::Hero(ui, col.Next(MenuKit::kHeroH, 18.f), wchar_t(0xE715), Loc::Get("acc_wait_title"), Loc::Get("acc_wait_sub"), kAccent);   // mail
    }
    {
        StaggerItem in(ui, openedAt, 1, 0.f, 10.f);
        const Rect box = col.Next(150.f, 18.f);
        Card(ui, box, kAccent.WithAlpha(0.35f));
        g.Text(Loc::Get("acc_wait_sent_to"), { box.x, box.y + 18.f, box.w, 16.f }, kSub, Mono(10.f).Center());
        g.Text(account.WaitingEmail(), { box.x + 12.f, box.y + 38.f, box.w - 24.f, 24.f }, kText, Mono(13.f, true).Center().Ellipsis());
        Form::Spinner(ui, box.CX(), box.y + 88.f, 10.f, kAccent);
        ui.KeepAnimatingThrottled();
        TextStyle hint = Mono(10.f).Center();
        hint.wrap = true;
        g.Text(Loc::Get("acc_wait_hint"), { box.x + 18.f, box.y + 108.f, box.w - 36.f, 36.f }, kSub, hint);
    }
    {
        StaggerItem in(ui, openedAt, 2, 0.f, 10.f);
        const Rect row = col.Next(40.f, 10.f);
        const float half = (row.w - 10.f) * 0.5f;
        const bool busy = account.AuthBusy();
        if (MenuKit::FooterButton(ui, ui.MakeId("acc-wait-resend"), { row.x, row.y, half, row.h }, Loc::Get(busy ? "acc_working" : "acc_wait_resend"),
                                  MenuKit::Kind::Secondary, !busy, Glyphs::Refresh, kAccent)) {
            auto token = alive;
            account.ResendConfirmation([this, token](const std::wstring& error) {
                if (!*token) return;
                status = error.empty() ? Loc::Get("acc_wait_resent") : error;
                statusOk = error.empty();
            });
        }
        if (MenuKit::FooterButton(ui, ui.MakeId("acc-wait-cancel"), { row.x + half + 10.f, row.y, half, row.h }, Loc::Get("acc_wait_cancel"),
                                  MenuKit::Kind::Danger, true, Glyphs::Cancel)) {
            account.CancelWaiting();
            status.clear();
        }
    }
    return col.Height() + 44.f;
}

float AccountOverlay::DrawSignIn(Ui& ui, const Rect& bodyRect) {
    auto& g = ui.g;
    auto& account = AccountService::Get();
    if (account.WaitingConfirmation()) return DrawWaiting(ui, bodyRect);
    const Rect body = bodyRect.Inset(30.f, 22.f);
    Column col(body.x, body.y, body.w);
    int reveal = 0;
    {
        StaggerItem in(ui, openedAt, reveal++, 0.f, 10.f);
        MenuKit::Hero(ui, col.Next(MenuKit::kHeroH, 14.f), wchar_t(0xE77B), Loc::Get("acc_signin_title"), Loc::Get("acc_signin_sub"), kAccent);
    }
    {
        // What an account is for
        StaggerItem in(ui, openedAt, reveal++, 0.f, 10.f);
        const char* perks[] = { "acc_perk_hours", "acc_perk_profile", "acc_perk_ranking" };
        const wchar_t perkGlyphs[] = { wchar_t(0xE823), wchar_t(0xE771), wchar_t(0xE9F9) };   // clock · personalize · chart
        const Rect box = col.Next(3 * 26.f + 20.f, 16.f);
        Card(ui, box);
        for (int i = 0; i < 3; ++i) {
            const Rect line{ box.x + 14.f, box.y + 10.f + i * 26.f, box.w - 28.f, 26.f };
            UI::Glyph(ui, perkGlyphs[i], { line.x, line.y, 20.f, line.h }, 13.f, kAccent);
            g.Text(Loc::Get(perks[i]), { line.x + 30.f, line.y, line.w - 30.f, line.h }, kText, Mono(10.5f).Ellipsis());
        }
    }
    {
        StaggerItem in(ui, openedAt, reveal++, 0.f, 10.f);
        const int clicked = PillTabs(ui, col.Next(34.f, 16.f), { Loc::Get("acc_mode_signin"), Loc::Get("acc_mode_signup") }, signUpMode ? 1 : 0,
                                     "acc-mode");
        if (clicked >= 0) {
            signUpMode = clicked == 1;
            status.clear();
        }
    }
    bool enter = false;
    {
        StaggerItem in(ui, openedAt, reveal++, 0.f, 10.f);
        Caption(ui, col.x, col.y, col.w, Loc::Get("acc_email"));
        col.Gap(20.f);
        TextBoxStyle style = Field(false, 120);
        style.placeholder = L"tu@email.com";
        const float h = TextBoxHeight(ui, style) + 4.f;
        Form::Field(ui, ui.MakeId("acc-email"), col.Next(h, 12.f), email, style, kAccent);
    }
    {
        StaggerItem in(ui, openedAt, reveal++, 0.f, 10.f);
        Caption(ui, col.x, col.y, col.w, Loc::Get("acc_password"));
        col.Gap(20.f);
        TextBoxStyle style = Field(true, 72);
        const float h = TextBoxHeight(ui, style) + 4.f;
        const Rect r = col.Next(h, 12.f);
        Form::Field(ui, ui.MakeId("acc-password"), r, password, style, kAccent);
        if (ui.Focus() == ui.MakeId("acc-password") && ui.KeyPressed(VK_RETURN)) enter = true;
        if (signUpMode) {
            Caption(ui, col.x, col.y, col.w, Loc::Get("acc_password_repeat"));
            col.Gap(20.f);
            Form::Field(ui, ui.MakeId("acc-password2"), col.Next(h, 12.f), password2, style, kAccent);
            if (ui.Focus() == ui.MakeId("acc-password2") && ui.KeyPressed(VK_RETURN)) enter = true;
        }
    }
    {
        StaggerItem in(ui, openedAt, reveal++, 0.f, 10.f);
        col.Gap(4.f);
        const Rect r = col.Next(42.f, 10.f);
        const bool busy = account.AuthBusy();
        const std::wstring label = Loc::Get(busy ? "acc_working" : signUpMode ? "acc_create_btn" : "acc_signin_btn");
        if (MenuKit::FooterButton(ui, ui.MakeId("acc-submit"), r, label, MenuKit::Kind::Primary, !busy, signUpMode ? wchar_t(0xE8FA) : wchar_t(0xE77B),
                                  kAccent) ||
            (enter && !busy))
            SubmitSignIn();
        if (busy) Form::Spinner(ui, r.R() - 22.f, r.CY(), 7.f, Colors::White);
        if (!signUpMode) {
            const std::wstring forgot = Loc::Get("acc_forgot");
            const float fw = g.Measure(forgot, Mono(10.f)).w + 4.f;
            if (LinkButton(ui, ui.MakeId("acc-forgot"), { col.x + (col.w - fw) * 0.5f, col.y, fw, 18.f }, forgot, Mono(10.f), kSub) && !busy) {
                if (Text::Trim(email).find(L'@') == std::wstring::npos) {
                    status = Loc::Get("acc_forgot_need_email");
                    statusOk = false;
                } else {
                    auto token = alive;
                    account.SendPasswordReset(email, [this, token](const std::wstring& error) {
                        if (!*token) return;
                        status = error.empty() ? Loc::Get("acc_forgot_sent") : error;
                        statusOk = error.empty();
                    });
                }
            }
            col.Gap(22.f);
        }
    }
    return col.Height() + 44.f;
}

} // namespace SpeedrunLauncher::Main
