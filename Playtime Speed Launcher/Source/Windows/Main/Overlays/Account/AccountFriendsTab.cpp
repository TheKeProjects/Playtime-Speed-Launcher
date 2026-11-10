#include <algorithm>
#include "App/Launcher.h"
#include "Core/Async.h"
#include "Core/Process.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Graphics/Animation.h"
#include "Seasonal/SeasonalTheme.h"
#include "Localization/Localization.h"
#include "Services/Account/AccountService.h"
#include "Services/Account/BadgesService.h"
#include "Services/Account/CommunityRanking.h"
#include "Services/Account/DuelsService.h"
#include "Services/Account/MessagesService.h"
#include "Services/Account/FriendsService.h"
#include "Services/Chapters/ChapterPlaytimeStore.h"
#include "UI/Layout.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "UI/Widgets/TextBox.h"
#include "Windows/Main/Common/ProfileDraw.h"
#include "Windows/Main/Overlays/Account/AccountKit.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/Account/AccountOverlay.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
using namespace AccountKit;
using Services::Account::Friend;
using Services::Account::FriendsService;
using Services::Account::Profile;

namespace {

constexpr float kNavW = 214.f, kDetailW = 300.f, kRowH = 62.f;
constexpr Color kOnline = Hex(0x3DDC84), kPlaying = Hex(0xFFB020), kOffline = Hex(0x55606A), kDanger = Hex(0xCC4422);

enum Page { PageAll, PageOnline, PagePending, PageAdd, PageEveryone, PageMessages };

Color PresenceColor(const Friend& f) { return f.Online() ? (f.playing ? kPlaying : kOnline) : kOffline; }
std::wstring NameOf(const Profile& p) { return p.ShownName(p.speedrunName.empty() ? Loc::Get("acc_default_name") : p.speedrunName); }
std::wstring Hours(const Profile& p) { return Services::ChapterPlaytimeStore::Format(ProfileDraw::TotalSeconds(p)); }

// Picture with a presence ring and dot; the avatar decoration goes over the dot (or, `deferDecoration`,
// the caller draws it last so it's over the row's texts too)
void AvatarWithPresence(Ui& ui, const Friend& f, const Rect& r, bool ring = true, bool deferDecoration = false) {
    auto& g = ui.g;
    ProfileDraw::Avatar(ui, f.profile.AvatarUrl(), r, f.profile, NameOf(f.profile), 0.f, Colors::Transparent, true, false);
    NoColorFilter keep(g);
    const Color c = PresenceColor(f);
    if (ring && f.Online()) g.StrokeEllipse(r.CX(), r.CY(), r.w * 0.5f + 2.5f, r.h * 0.5f + 2.5f, c.WithAlpha(0.8f), 2.f);
    const float d = std::max(10.f, r.w * 0.3f);
    g.FillEllipse(r.R() - d * 0.4f, r.B() - d * 0.4f, d * 0.5f + 2.5f, d * 0.5f + 2.5f, Hex(0x0A1420));
    g.FillEllipse(r.R() - d * 0.4f, r.B() - d * 0.4f, d * 0.5f, d * 0.5f, c);
    if (!deferDecoration) ProfileDraw::AvatarDecoration(ui, r, f.profile);
}

bool IconButton(Ui& ui, Id id, const Rect& r, wchar_t glyph, MenuKit::Kind kind, bool enabled, const std::wstring& tooltip, Color accent = kAccent) {
    const bool clicked = MenuKit::FooterButton(ui, id, r, L"", kind, enabled, glyph, accent);
    if (!tooltip.empty()) ui.Tooltip(id, tooltip);
    return clicked;
}

// Small rounded label; returns its width. On a nameplate (`onPlate`) it gets a solid dark body (the
// see-through one disappears on bright plates) with a soft shadow
float Chip(Ui& ui, float x, float cy, const std::wstring& text, Color color, wchar_t glyph = 0, bool onPlate = false) {
    auto& g = ui.g;
    const TextStyle st = Mono(9.f, true);
    const float tw = g.Measure(text, st).w;
    const float w = tw + 14.f + (glyph ? 15.f : 0.f);
    const Rect r{ x, cy - 9.f, w, 18.f };
    if (onPlate) {
        NoColorFilter keep(g);
        g.GlowRound(r, 9.f, Colors::Black, 6.f, 0.4f);
        g.Border(r, 9.f, Lerp(Hex(0x060C14), color, 0.12f).WithAlpha(0.92f), color.WithAlpha(0.75f), 1.f);
        color = Lerp(color, Colors::White, 0.15f);
    } else {
        g.Border(r, 9.f, color.WithAlpha(0.12f), color.WithAlpha(0.32f), 1.f);
    }
    float tx = r.x + 7.f;
    if (glyph) {
        UI::Glyph(ui, glyph, { tx - 2.f, r.y, 14.f, r.h }, 8.5f, color);
        tx += 15.f;
    }
    g.Text(text, { tx, r.y, tw + 2.f, r.h }, color, st);
    return w;
}

void HoursListClip(Ui& ui, const Rect& r, const Profile& profile) {
    if (r.h <= 0.f) return;
    ui.PushClip(r);
    ProfileDraw::HoursList(ui, r, profile, kAccent);
    ui.PopClip();
}

// Big empty-state: glyph, message and an optional button; returns true when the button is clicked
bool EmptyState(Ui& ui, const Rect& r, wchar_t glyph, const std::wstring& text, const std::wstring& button = L"") {
    auto& g = ui.g;
    const float bob = 3.f * std::sin(static_cast<float>(ui.Time()) * 1.6f);
    UI::Glyph(ui, glyph, { r.CX() - 30.f, r.CY() - 70.f + bob, 60.f, 50.f }, 34.f, kCardBorder);
    ui.KeepAnimatingThrottled();
    TextStyle st = Mono(11.f, true).Center();
    st.wrap = true;
    g.Text(text, { r.x + 30.f, r.CY() - 12.f, r.w - 60.f, 40.f }, kSub, st);
    if (button.empty()) return false;
    const float w = MenuKit::FooterButtonWidth(ui, button, wchar_t(0xE8FA), 150.f);
    return MenuKit::FooterButton(ui, ui.MakeId("fr-empty-btn"), { r.CX() - w * 0.5f, r.CY() + 40.f, w, 36.f }, button, MenuKit::Kind::Primary, true,
                                 wchar_t(0xE8FA), kAccent);
}

} // namespace

void AccountOverlay::FriendMessage(const std::wstring& message, bool ok) {
    status = message;
    statusOk = ok;
}

// ── Page ────────────────────────────────────────────────────────────────────
//   [ ALL / ONLINE / PENDING / ADD  ] [ the page ................................. ] [ selected ]
//   [ your friend code (bottom)     ]                                                [ friend   ]
void AccountOverlay::DrawFriends(Ui& ui, const Rect& body) {
    auto& friends = FriendsService::Get();
    const Rect nav = body.Left(kNavW);
    const Rect rest{ nav.R() + 18.f, body.y, body.w - kNavW - 18.f, body.h };
    {
        StaggerItem in(ui, tabChangedAt, 0, -20.f);
        DrawFriendsNav(ui, nav);
    }
    StaggerItem in(ui, friendsPageAt, 0, 18.f);
    if (friendsPage == PagePending) {
        DrawFriendsPending(ui, rest);
    } else if (friendsPage == PageAdd) {
        DrawFriendsAdd(ui, rest);
    } else if (friendsPage == PageEveryone) {
        DrawFriendsCommunity(ui, rest);
    } else if (friendsPage == PageMessages) {
        DrawMessages(ui, rest);
    } else {
        const bool detail = !friends.Friends().empty() && rest.w > kDetailW + 340.f;
        friendDetailShown = detail;
        const Rect list = detail ? rest.CutRight(kDetailW + 18.f) : rest;
        DrawFriendsList(ui, list, friendsPage == PageOnline);
        if (detail) DrawFriendCard(ui, rest.Right(kDetailW));
    }
}

void AccountOverlay::SetFriendsPage(int page) {
    if (friendsPage == page) return;
    friendsPage = page;
    friendsPageAt = Async::Now();
    confirmRemove.clear();
    if (page == PageEveryone) {
        LoadCommunity(false);
        Services::Account::DuelsService::Get().Refresh();   // who's online right now
    }
}

void AccountOverlay::DrawFriendsNav(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    auto& friends = FriendsService::Get();
    int online = 0;
    for (const auto& f : friends.Friends())
        if (f.Online()) ++online;
    const int pending = static_cast<int>(friends.Incoming().size() + friends.Outgoing().size());

    struct Item {
        int page;
        wchar_t glyph;
        const char* key;
        int count;
        bool alert;
    };
    const Item items[] = {
        { PageAll, wchar_t(0xE716), "fr_page_all", static_cast<int>(friends.Friends().size()), false },
        { PageOnline, wchar_t(0xE91F), "fr_page_online", online, false },
        { PageMessages, wchar_t(0xE8BD), "msg_page", Services::Account::MessagesService::Get().Unread(), Services::Account::MessagesService::Get().Unread() > 0 },
        { PagePending, wchar_t(0xE823), "fr_page_pending", pending, !friends.Incoming().empty() },
        { PageEveryone, wchar_t(0xE774), "fr_page_everyone", Services::Account::DuelsService::Get().OnlineCount(), false },
        { PageAdd, wchar_t(0xE8FA), "fr_page_add", 0, false },
    };
    float y = r.y;
    float selY = r.y;
    for (const auto& item : items) {
        const Rect row{ r.x, y, r.w, 44.f };
        const Id id = ui.MakeId("fr-nav", item.page);
        const Interaction it = ui.Interact(id, row);
        const bool sel = friendsPage == item.page;
        const float hover = ui.Animate(HashId(id, 1), it.hovered && !sel ? 1.f : 0.f, 16.f);
        const float on = ui.Animate(HashId(id, 2), sel ? 1.f : 0.f, 14.f);
        if (hover > 0.01f) g.FillRound(row, 10.f, kRowHover.WithAlpha(hover));
        if (sel) selY = row.y;
        const Color color = Lerp(Lerp(kSub, kText, hover), kAccent, on);
        UI::Glyph(ui, item.glyph, { row.x + 12.f, row.y, 20.f, row.h }, 13.f, item.page == PageOnline ? Lerp(color, kOnline, 0.6f) : color);
        if (item.page == PageEveryone && item.count > 0 && ui.MouseOver(row)) ui.Tooltip(id, Loc::Fmt("fr_everyone_online_tip", item.count));
        g.Text(Loc::Get(item.key), { row.x + 42.f, row.y, row.w - 90.f, row.h }, color, Mono(10.5f, true).Ellipsis());
        if (item.count > 0) {
            const std::wstring n = item.count > 99 ? L"99+" : std::to_wstring(item.count);
            const float cw = std::max(22.f, g.Measure(n, Mono(9.f, true)).w + 12.f);
            const Rect chip{ row.R() - 12.f - cw, row.CY() - 10.f, cw, 20.f };
            g.FillRound(chip, 10.f, item.alert ? Hex(0xCC2200) : kAccent.WithAlpha(0.18f));
            g.Text(n, chip, item.alert ? Colors::White : kAccent, Mono(9.f, true).Center());
        }
        if (it.hovered && !sel) ui.SetCursor(IDC_HAND);
        if (it.clicked) SetFriendsPage(item.page);
        y += 48.f;
        if (item.page == PagePending) {
            g.FillRect({ r.x + 10.f, y + 2.f, r.w - 20.f, 1.f }, Palette::Divider);
            y += 10.f;
        }
    }
    // Selection bar gliding to the selected page
    const float by = ui.Animate(ui.MakeId("fr-nav-bar"), selY, 16.f, selY);
    g.FillRound({ r.x, by, r.w, 44.f }, 10.f, Rgba(0, 204, 170, 22));
    g.FillRound({ r.x, by + 10.f, 3.f, 24.f }, 1.5f, kAccent);

    // Your friend code at the bottom
    const Rect box{ r.x, r.B() - 112.f, r.w, 112.f };
    Card(ui, box, kAccent.WithAlpha(0.3f));
    Caption(ui, box.x + 14.f, box.y + 12.f, box.w - 28.f, Loc::Get("fr_my_code"), kAccent);
    const std::wstring& code = friends.MyCode();
    g.Text(code.empty() ? L"\x2026" : code, { box.x + 14.f, box.y + 32.f, box.w - 28.f, 30.f }, kText, Mono(20.f, true));
    const bool copied = codeCopied && ui.Time() - codeCopiedAt < 2.0;
    const std::wstring label = Loc::Get(copied ? "fr_copied" : "fr_copy");
    if (MenuKit::FooterButton(ui, ui.MakeId("fr-copy"), { box.x + 14.f, box.B() - 44.f, box.w - 28.f, 32.f }, label, MenuKit::Kind::Secondary, !code.empty(),
                              copied ? Glyphs::CheckMark : Glyphs::Copy, kAccent)) {
        System::SetClipboardText(code);
        codeCopied = true;
        codeCopiedAt = ui.Time();
    }
    if (copied || code.empty()) ui.KeepAnimatingThrottled();
}

// ── All / online ────────────────────────────────────────────────────────────

void AccountOverlay::DrawFriendsList(Ui& ui, const Rect& r, bool onlineOnly) {
    auto& g = ui.g;
    auto& friends = FriendsService::Get();
    const auto& all = friends.Friends();
    int online = 0;
    for (const auto& f : all)
        if (f.Online()) ++online;

    // Header: title + count, filter on the right
    g.Text(Loc::Get(onlineOnly ? "fr_page_online" : "fr_page_all"), { r.x, r.y, r.w, 22.f }, kText, Mono(14.f, true));
    g.Text(Loc::Fmt("fr_count", all.size(), online), { r.x, r.y + 22.f, r.w * 0.5f, 18.f }, kSub, Mono(9.5f));
    {
        const float fw = std::min(240.f, r.w * 0.45f);
        const Rect field{ r.R() - fw, r.y + 4.f, fw, 32.f };
        TextBoxStyle style = Field(false, 32);
        style.placeholder = Loc::Get("fr_filter_placeholder");
        style.padding = { 30.f, 6.f, 10.f, 6.f };
        style.text = Mono(10.5f);
        TextBox(ui, ui.MakeId("fr-filter"), field, friendFilter, style);
        UI::Glyph(ui, wchar_t(0xE721), { field.x + 6.f, field.y, 20.f, field.h }, 11.f, kSub);
    }
    const Rect area{ r.x, r.y + 52.f, r.w, r.h - 52.f };
    Card(ui, area);

    if (all.empty()) {
        if (!friends.Loaded() && !friends.Failed()) {
            Form::Spinner(ui, area.CX(), area.CY(), 9.f, kAccent);
            ui.KeepAnimatingThrottled();
        } else if (EmptyState(ui, area, wchar_t(0xE716), Loc::Get(friends.Failed() ? "fr_load_failed" : "fr_empty"), Loc::Get("fr_page_add"))) {
            SetFriendsPage(PageAdd);
        }
        return;
    }

    // Groups: playing · online · offline (filtered by name)
    const std::wstring filter = Text::Lower(Text::Trim(friendFilter));
    std::vector<const Friend*> groups[3];
    for (const auto& f : all) {
        if (!filter.empty() && !Text::Contains(Text::Lower(NameOf(f.profile)), filter) && !Text::Contains(Text::Lower(f.profile.speedrunName), filter)) continue;
        const int group = f.Online() ? (f.playing ? 0 : 1) : 2;
        if (onlineOnly && group == 2) continue;
        groups[group].push_back(&f);
    }
    const size_t shown = groups[0].size() + groups[1].size() + groups[2].size();
    if (shown == 0) {
        EmptyState(ui, area, onlineOnly ? wchar_t(0xE91F) : wchar_t(0xE721), Loc::Get(!filter.empty() ? "fr_filter_none" : "fr_none_online"));
        return;
    }
    if (selectedFriend.empty() || std::none_of(all.begin(), all.end(), [&](const Friend& f) { return f.profile.userId == selectedFriend; }))
        selectedFriend = groups[0].empty() ? (groups[1].empty() ? groups[2].front()->profile.userId : groups[1].front()->profile.userId)
                                           : groups[0].front()->profile.userId;

    const Rect inner = area.Inset(12.f, 10.f);
    ScrollArea scroll(ui, ui.MakeId("fr-list", friendsPage), inner);
    float y = inner.y;
    int index = 0;
    const char* groupKeys[] = { "fr_group_playing", "fr_group_online", "fr_group_offline" };
    const Color groupColors[] = { kPlaying, kOnline, kSub };
    for (int gi = 0; gi < 3; ++gi) {
        if (groups[gi].empty()) continue;
        g.Text(Loc::Fmt(groupKeys[gi], groups[gi].size()), { inner.x + 4.f, y, inner.w, 22.f }, groupColors[gi], Mono(9.f, true));
        y += 26.f;
        for (const Friend* f : groups[gi]) {
            StaggerItem in(ui, friendsPageAt + 0.04, index++, 0.f, 8.f, 0.03, 0.28);
            const Rect row{ inner.x, y, inner.w - 10.f, kRowH };
            y += kRowH + 4.f;
            const int64_t key = static_cast<int64_t>(std::hash<std::wstring>{}(f->profile.userId));
            const Id id = ui.MakeId("fr-row", key);
            const Interaction it = ui.Interact(id, row);
            const bool sel = f->profile.userId == selectedFriend;
            const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 16.f);
            g.Border(row, 10.f, Lerp(sel ? Rgba(0, 204, 170, 20) : kRow.WithAlpha(0.f), kRowHover, hover), sel ? kAccent.WithAlpha(0.7f) : kCardBorder.WithAlpha(0.35f + 0.4f * hover), 1.f);
            ProfileDraw::Nameplate(ui, row, 10.f, f->profile);
            const Rect pic{ row.x + 12.f, row.CY() - 20.f, 40.f, 40.f };
            AvatarWithPresence(ui, *f, pic, true, true);
            const float right = row.R() - 12.f;
            // Chips on the right: hours, speedrun.com
            float cx = right;
            const std::wstring hours = Hours(f->profile);
            const float hw = g.Measure(hours, Mono(9.f, true)).w + 29.f;
            cx -= hw;
            const bool onPlate = ProfileDraw::HasNameplate(f->profile);
            Chip(ui, cx, row.CY(), hours, kAccent, wchar_t(0xE916), onPlate);
            if (f->profile.SpeedrunLinked() && cx - 70.f > pic.R() + 160.f) {
                const float sw = g.Measure(L"SRC", Mono(9.f, true)).w + 29.f;
                cx -= sw + 6.f;
                Chip(ui, cx, row.CY(), L"SRC", kSpeedrun, wchar_t(0xE734), onPlate);
            }
            const float textW = cx - pic.R() - 24.f;
            ui.PushClip({ pic.R() + 12.f, row.y, textW, row.h });
            ProfileDraw::Name(ui, NameOf(f->profile), { pic.R() + 12.f, row.y + 8.f, textW, 26.f }, f->profile, 15.f);
            ui.PopClip();
            {
                const bool plate = ProfileDraw::HasNameplate(f->profile);
                ProfileDraw::RowText(ui, FriendsService::StatusText(*f), { pic.R() + 12.f, row.y + 34.f, textW, 18.f },
                                     f->Online() ? PresenceColor(*f) : plate ? kText : kSub, Mono(9.5f, f->Online() || plate).Ellipsis(), plate);
            }
            ProfileDraw::AvatarDecoration(ui, pic, f->profile);   // over the dot and the texts
            if (it.hovered) ui.SetCursor(IDC_HAND);
            if (it.clicked) {
                selectedFriend = f->profile.userId;
                confirmRemove.clear();
                // Narrow menus have no side card: open the profile instead
                if (!friendDetailShown) OpenViewer(f->profile);
            }
        }
        y += 8.f;
    }
    scroll.SetContentSize(y - inner.y);
}

// ── Everyone: every launcher account, online and offline ───────────────────

void AccountOverlay::LoadCommunity(bool force) {
    if (communityLoading) return;
    if (!force && !community.empty() && Async::Now() - communityLoadedAt < 120.0) return;
    if (!force && communityFailed && Async::Now() - communityLoadedAt < 30.0) return;   // don't hammer a failing server
    communityLoading = true;
    communityFailed = false;
    auto token = alive;
    Async::Run([] { return Services::Account::Community::FetchMembers(); }, [this, token](std::optional<std::vector<Profile>> result) {
        if (!*token) return;
        communityLoading = false;
        communityLoadedAt = Async::Now();
        if (!result) {
            communityFailed = true;
            return;
        }
        community = std::move(*result);
    });
}

void AccountOverlay::DrawFriendsCommunity(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    auto& friends = FriendsService::Get();
    const std::wstring& myId = Services::Account::AccountService::Get().UserId();
    LoadCommunity(false);   // (re)loads every 2 minutes while the page is open

    // Presence: friends know when they were last seen; everyone else is online when the duels'
    // online list has them
    std::map<std::wstring, Friend> presence;
    for (const auto& u : Services::Account::DuelsService::Get().Online()) {
        Friend f;
        f.relation = Friend::Relation::None;
        f.profile = u.profile;
        f.secondsAgo = 0.0;
        f.playing = u.playing;
        presence[u.profile.userId] = f;
    }
    for (const auto& f : friends.Friends()) presence[f.profile.userId] = f;
    const auto statusOf = [&](const Profile& p) {
        Friend f;
        if (auto it = presence.find(p.userId); it != presence.end()) f = it->second;
        f.relation = Friend::Relation::None;
        f.profile = p;
        if (p.userId == myId) {   // you're here
            f.secondsAgo = 0.0;
            f.playing = 0;
        }
        return f;
    };

    // Groups: playing · online · offline (filtered by name)
    const std::wstring filter = Text::Lower(Text::Trim(communityFilter));
    std::vector<Friend> groups[3];
    for (const auto& p : community) {
        if (!filter.empty() && !Text::Contains(Text::Lower(NameOf(p)), filter) && !Text::Contains(Text::Lower(p.speedrunName), filter)) continue;
        Friend f = statusOf(p);
        groups[f.Online() ? (f.playing ? 0 : 1) : 2].push_back(std::move(f));
    }
    size_t online = 0;
    for (const auto& p : community)
        if (statusOf(p).Online()) ++online;

    // Header: title + counts; refresh and filter on the right
    g.Text(Loc::Get("fr_page_everyone"), { r.x, r.y, r.w, 22.f }, kText, Mono(14.f, true));
    g.Text(community.empty() ? Loc::Get("fr_everyone_sub") : Loc::Fmt("fr_everyone_count", community.size(), online), { r.x, r.y + 22.f, r.w * 0.5f, 18.f },
           kSub, Mono(9.5f).Ellipsis());
    {
        const float fw = std::min(240.f, r.w * 0.45f);
        const Rect field{ r.R() - fw, r.y + 4.f, fw, 32.f };
        TextBoxStyle style = Field(false, 32);
        style.placeholder = Loc::Get("fr_everyone_filter");
        style.padding = { 30.f, 6.f, 10.f, 6.f };
        style.text = Mono(10.5f);
        TextBox(ui, ui.MakeId("fr-everyone-filter"), field, communityFilter, style);
        UI::Glyph(ui, wchar_t(0xE721), { field.x + 6.f, field.y, 20.f, field.h }, 11.f, kSub);
        if (IconButton(ui, ui.MakeId("fr-everyone-refresh"), { field.x - 42.f, field.y, 34.f, 32.f }, Glyphs::Refresh, MenuKit::Kind::Secondary, !communityLoading,
                       Loc::Get("fr_everyone_refresh"))) {
            LoadCommunity(true);
            Services::Account::DuelsService::Get().Refresh();
        }
    }
    const Rect area{ r.x, r.y + 52.f, r.w, r.h - 52.f };
    Card(ui, area);

    if (community.empty()) {
        if (!communityFailed) {
            Form::Spinner(ui, area.CX(), area.CY(), 9.f, kAccent);
            ui.KeepAnimatingThrottled();
        } else {
            EmptyState(ui, area, wchar_t(0xE774), Loc::Get("fr_everyone_failed"));
        }
        return;
    }
    if (groups[0].empty() && groups[1].empty() && groups[2].empty()) {
        EmptyState(ui, area, wchar_t(0xE721), Loc::Get("fr_everyone_none"));
        return;
    }

    const Rect inner = area.Inset(12.f, 10.f);
    ScrollArea scroll(ui, ui.MakeId("fr-everyone-list"), inner);
    float y = inner.y;
    int index = 0;
    const char* groupKeys[] = { "fr_group_playing", "fr_group_online", "fr_group_offline" };
    const Color groupColors[] = { kPlaying, kOnline, kSub };
    for (int gi = 0; gi < 3; ++gi) {
        if (groups[gi].empty()) continue;
        g.Text(Loc::Fmt(groupKeys[gi], groups[gi].size()), { inner.x + 4.f, y, inner.w, 22.f }, groupColors[gi], Mono(9.f, true));
        y += 26.f;
        for (const Friend& f : groups[gi]) {
            const Rect row{ inner.x, y, inner.w - 10.f, kRowH };
            y += kRowH + 4.f;
            StaggerItem in(ui, friendsPageAt + 0.04, std::min(index++, 12), 0.f, 8.f, 0.03, 0.28);
            const Profile& p = f.profile;
            const bool mine = p.userId == myId;
            const int64_t key = static_cast<int64_t>(std::hash<std::wstring>{}(p.userId));
            const Id id = ui.MakeId("fr-every-row", key);
            const Interaction it = ui.Interact(id, row);
            const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 16.f);
            g.Border(row, 10.f, Lerp(mine ? Rgba(0, 204, 170, 16) : kRow.WithAlpha(0.f), kRowHover, hover),
                     mine ? kAccent.WithAlpha(0.6f) : kCardBorder.WithAlpha(0.35f + 0.4f * hover), 1.f);

            // Right: their hours, then who they are to you (or "add")
            float cx = row.R() - 12.f;
            const std::wstring hours = Hours(p);
            cx -= g.Measure(hours, Mono(9.f, true)).w + 29.f;
            const float hoursX = cx;
            const auto chipW = [&](const std::wstring& text, bool glyph) { return g.Measure(text, Mono(9.f, true)).w + 14.f + (glyph ? 15.f : 0.f); };
            std::wstring relation;
            Color relationColor = kAccent;
            wchar_t relationGlyph = 0;
            bool canAdd = false;
            if (mine) relation = Loc::Get("rk_you_chip");
            else if (friends.IsFriend(p.userId)) relation = Loc::Get("fr_is_friend"), relationGlyph = wchar_t(0xE716);
            else if (friends.HasRequest(p.userId)) relation = Loc::Get("pf_rel_pending"), relationColor = kSub, relationGlyph = wchar_t(0xE823);
            else canAdd = true;
            float relationX = 0.f;
            if (!relation.empty()) {
                cx -= chipW(relation, relationGlyph != 0) + 8.f;
                relationX = cx;
            } else if (canAdd) {
                cx -= 34.f + 8.f;
                relationX = cx;
            }

            // The nameplate stays behind the player, the chips keep the plain row
            ProfileDraw::Nameplate(ui, row, 10.f, p);
            const Rect pic{ row.x + 12.f, row.CY() - 20.f, 40.f, 40.f };
            AvatarWithPresence(ui, f, pic, true, true);
            const bool onPlate = ProfileDraw::HasNameplate(p);
            Chip(ui, hoursX, row.CY(), hours, kAccent, wchar_t(0xE916), onPlate);
            if (!relation.empty()) Chip(ui, relationX, row.CY(), relation, relationColor, relationGlyph, onPlate);
            if (canAdd && IconButton(ui, ui.MakeId("fr-every-add", key), { relationX, row.CY() - 16.f, 34.f, 32.f }, wchar_t(0xE8FA), MenuKit::Kind::Primary,
                                     !friends.Busy(), Loc::Get("fr_add_tooltip"))) {
                auto token = alive;
                friends.SendToUser(p.userId, [this, token](const std::wstring& message, bool ok) {
                    if (*token) FriendMessage(message, ok);
                });
            }

            const float textW = cx - pic.R() - 24.f;
            ui.PushClip({ pic.R() + 12.f, row.y, textW, row.h });
            ProfileDraw::Name(ui, NameOf(p), { pic.R() + 12.f, row.y + 8.f, textW, 26.f }, p, 15.f);
            ui.PopClip();
            std::wstring line = FriendsService::StatusText(f);
            if (p.SpeedrunLinked()) line += L"  \x00B7  speedrun.com/" + p.speedrunName;
            const bool plate = ProfileDraw::HasNameplate(p);
            ProfileDraw::RowText(ui, line, { pic.R() + 12.f, row.y + 34.f, textW, 18.f }, f.Online() ? PresenceColor(f) : plate ? kText : kSub,
                                 Mono(9.5f, f.Online() || plate).Ellipsis(), plate);
            ProfileDraw::AvatarDecoration(ui, pic, p);   // over the dot and the texts
            if (it.hovered) ui.Tooltip(id, Loc::Get("acc_rank_open_profile"));
            if (it.clicked) OpenViewer(p);
        }
        y += 8.f;
    }
    scroll.SetContentSize(y - inner.y);
}

// The selected friend: card, status, hours per chapter, actions
void AccountOverlay::DrawFriendCard(Ui& ui, const Rect& r) {
    auto& friends = FriendsService::Get();
    const Friend* f = nullptr;
    for (const auto& item : friends.Friends())
        if (item.profile.userId == selectedFriend) f = &item;
    if (!f) return;
    const std::wstring name = NameOf(f->profile);
    ProfileDraw::CardInfo info;
    info.name = name;
    info.avatarSource = f->profile.AvatarUrl();
    info.bannerSource = f->profile.BannerUrl();
    info.line1 = FriendsService::StatusText(*f);
    info.line1Color = f->Online() ? PresenceColor(*f) : kSub;
    info.line2 = Loc::Fmt("fr_card_hours", Hours(f->profile)) + (f->profile.SpeedrunLinked() ? L"  \x00B7  speedrun.com/" + f->profile.speedrunName : L"");
    const float cardH = ProfileDraw::CardHeight(r.w);
    info.badge = Services::Account::BadgesService::Get().Shown(f->profile.userId);
    ProfileDraw::Card(ui, r.Top(cardH), f->profile, info);

    const float top = r.y + cardH + 16.f;
    Caption(ui, r.x, top, r.w, Loc::Fmt("fr_hours_total", Hours(f->profile)));
    HoursListClip(ui, { r.x, top + 22.f, r.w, r.B() - 46.f - 42.f - top - 22.f }, f->profile);

    // Full profile + message + challenge to a duel
    const float duelW = 44.f;
    if (MenuKit::FooterButton(ui, ui.MakeId("fr-card-full"), { r.x, r.B() - 34.f - 42.f, r.w - (duelW + 8.f) * 2.f, 34.f }, Loc::Get("pf_open_full"),
                              MenuKit::Kind::Primary, true, wchar_t(0xE77B), kAccent))
        OpenFullProfile(f->profile, false);
    {
        const Id msgId = ui.MakeId("fr-card-msg");
        if (MenuKit::FooterButton(ui, msgId, { r.R() - duelW * 2.f - 8.f, r.B() - 34.f - 42.f, duelW, 34.f }, L"", MenuKit::Kind::Secondary, true, wchar_t(0xE8BD),
                                  Hex(0x3A9BFF))) {
            OpenChat(f->profile);
            return;
        }
        ui.Tooltip(msgId, Loc::Get("msg_button"));
    }
    {
        const Id duelId = ui.MakeId("fr-card-duel");
        if (MenuKit::FooterButton(ui, duelId, { r.R() - duelW, r.B() - 34.f - 42.f, duelW, 34.f }, L"", MenuKit::Kind::Secondary, true, wchar_t(0xE916),
                                  Hex(0xFF5A3C))) {
            const Profile who = f->profile;
            Close();
            main.ChallengeToDuel(who);
            return;
        }
        ui.Tooltip(duelId, Loc::Get("duel_challenge_btn"));
    }

    float x = r.x;
    const float y = r.B() - 34.f;
    if (f->profile.SpeedrunLinked() && !f->profile.speedrunUrl.empty()) {
        const std::wstring label = Loc::Get("acc_src_open");
        const float w = MenuKit::FooterButtonWidth(ui, label, wchar_t(0xE8A7), 100.f);
        if (MenuKit::FooterButton(ui, ui.MakeId("fr-card-src"), { x, y, w, 34.f }, label, MenuKit::Kind::Secondary, true, wchar_t(0xE8A7), kSpeedrun))
            Process::OpenUrl(f->profile.speedrunUrl);
        x += w + 8.f;
    }
    const bool confirming = confirmRemove == f->profile.userId;
    const std::wstring label = Loc::Get(confirming ? "fr_remove_confirm" : "fr_remove");
    const float w = std::min(r.R() - x, MenuKit::FooterButtonWidth(ui, label, Glyphs::Delete, 100.f));
    const Rect button{ x, y, w, 34.f };
    if (MenuKit::FooterButton(ui, ui.MakeId("fr-card-remove"), button, label, MenuKit::Kind::Danger, !friends.Busy(), Glyphs::Delete)) {
        if (!confirming) {
            confirmRemove = f->profile.userId;
        } else {
            confirmRemove.clear();
            auto token = alive;
            friends.Remove(f->profile.userId, [this, token, name](const std::wstring& message, bool ok) {
                if (!*token) return;
                FriendMessage(ok ? Loc::Fmt("fr_removed", name) : message, ok);
            });
        }
    }
    if (confirming && ui.In().pressed[0] && !ui.MouseOver(button)) confirmRemove.clear();
}

// ── Pending ─────────────────────────────────────────────────────────────────

void AccountOverlay::DrawFriendsPending(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    auto& friends = FriendsService::Get();
    g.Text(Loc::Get("fr_page_pending"), { r.x, r.y, r.w, 22.f }, kText, Mono(14.f, true));
    g.Text(Loc::Get("fr_pending_sub"), { r.x, r.y + 22.f, r.w, 18.f }, kSub, Mono(9.5f));
    const Rect area{ r.x, r.y + 52.f, r.w, r.h - 52.f };
    Card(ui, area);
    if (friends.Incoming().empty() && friends.Outgoing().empty()) {
        if (EmptyState(ui, area, wchar_t(0xE823), Loc::Get("fr_no_pending"), Loc::Get("fr_page_add"))) SetFriendsPage(PageAdd);
        return;
    }
    const Rect inner = area.Inset(14.f, 12.f);
    ScrollArea scroll(ui, ui.MakeId("fr-pending"), inner);
    float y = inner.y;
    int index = 0;
    auto token = alive;
    const auto section = [&](const std::vector<Friend>& list, bool received) {
        if (list.empty()) return;
        g.Text(Loc::Fmt(received ? "fr_group_received" : "fr_group_sent", list.size()), { inner.x + 4.f, y, inner.w, 22.f },
               received ? MenuKit::kAmber : kSub, Mono(9.f, true));
        y += 26.f;
        for (const Friend& f : list) {
            StaggerItem in(ui, friendsPageAt + 0.04, index++, 0.f, 8.f, 0.03, 0.28);
            const Rect row{ inner.x, y, inner.w - 10.f, 68.f };
            y += 72.f;
            const int64_t key = static_cast<int64_t>(std::hash<std::wstring>{}(f.profile.userId));
            const Id id = ui.MakeId("fr-pend-row", key);
            const Interaction it = ui.Interact(id, row);
            const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 16.f);
            g.Border(row, 10.f, Lerp(kRow, kRowHover, hover), received ? MenuKit::kAmber.WithAlpha(0.35f + 0.3f * hover) : kCardBorder.WithAlpha(0.5f + 0.4f * hover), 1.f);
            ProfileDraw::Nameplate(ui, row, 10.f, f.profile);
            const Rect pic{ row.x + 12.f, row.CY() - 21.f, 42.f, 42.f };
            AvatarWithPresence(ui, f, pic, false, true);
            const std::wstring name = NameOf(f.profile);
            const float buttonsW = received ? 236.f : 130.f;
            const float textW = row.w - (pic.R() - row.x) - 24.f - buttonsW;
            ui.PushClip({ pic.R() + 12.f, row.y, textW, row.h });
            ProfileDraw::Name(ui, name, { pic.R() + 12.f, row.y + 9.f, textW, 26.f }, f.profile, 15.f);
            ui.PopClip();
            float cx = pic.R() + 12.f;
            const bool onPlate = ProfileDraw::HasNameplate(f.profile);
            cx += Chip(ui, cx, row.y + 48.f, Loc::Get(received ? "fr_wants" : "fr_pending"), received ? MenuKit::kAmber : onPlate ? kText : kSub, 0, onPlate) + 6.f;
            if (ProfileDraw::TotalSeconds(f.profile) > 0.0 && cx + 80.f < row.R() - buttonsW)
                Chip(ui, cx, row.y + 48.f, Hours(f.profile), kAccent, wchar_t(0xE916), onPlate);
            ProfileDraw::AvatarDecoration(ui, pic, f.profile);   // over the dot and the texts

            const float by = row.CY() - 17.f;
            float bx = row.R() - 12.f;
            if (received) {
                const std::wstring accept = Loc::Get("fr_btn_accept");
                const float aw = MenuKit::FooterButtonWidth(ui, accept, Glyphs::CheckMark, 110.f);
                const float dw = 40.f;
                bx -= dw;
                if (IconButton(ui, ui.MakeId("fr-decline", key), { bx, by, dw, 34.f }, Glyphs::Cancel, MenuKit::Kind::Danger, !friends.Busy(), Loc::Get("fr_decline")))
                    friends.Respond(f.profile.userId, false, [this, token](const std::wstring& message, bool ok) {
                        if (*token && !ok) FriendMessage(message, false);
                    });
                bx -= aw + 8.f;
                if (MenuKit::FooterButton(ui, ui.MakeId("fr-accept", key), { bx, by, aw, 34.f }, accept, MenuKit::Kind::Primary, !friends.Busy(), Glyphs::CheckMark,
                                          MenuKit::kGreen))
                    friends.Respond(f.profile.userId, true, [this, token, name](const std::wstring& message, bool ok) {
                        if (*token) FriendMessage(ok ? Loc::Fmt("fr_accepted", name) : message, ok);
                    });
            } else {
                const std::wstring cancel = Loc::Get("fr_btn_cancel");
                const float cw = MenuKit::FooterButtonWidth(ui, cancel, Glyphs::Cancel, 110.f);
                bx -= cw;
                if (MenuKit::FooterButton(ui, ui.MakeId("fr-cancel", key), { bx, by, cw, 34.f }, cancel, MenuKit::Kind::Secondary, !friends.Busy(), Glyphs::Cancel,
                                          kAccent))
                    friends.Remove(f.profile.userId, [this, token](const std::wstring& message, bool ok) {
                        if (*token && !ok) FriendMessage(message, false);
                    });
            }
            if (it.hovered) ui.SetCursor(IDC_HAND);
            if (it.clicked) OpenViewer(f.profile);
        }
        y += 10.f;
    };
    section(friends.Incoming(), true);
    section(friends.Outgoing(), false);
    scroll.SetContentSize(y - inner.y);
}

// ── Add a friend ────────────────────────────────────────────────────────────

void AccountOverlay::DrawFriendsAdd(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    auto& friends = FriendsService::Get();
    // Hero: title, how to find people, the search field
    const Rect hero = r.Top(132.f);
    Card(ui, hero, kAccent.WithAlpha(0.3f));
    {
        NoColorFilter keep(g);
        g.FillLinear(hero, 12.f, { { 0.f, Seasonal::Filter(Rgba(0, 204, 170, 26)) }, { 1.f, Rgba(0, 0, 0, 0) } }, { 0.f, 0.f }, { 1.f, 1.f });
    }
    const Rect h = hero.Inset(20.f, 16.f);
    MenuKit::GlyphBadge(ui, { h.x, h.y, 40.f, 40.f }, wchar_t(0xE8FA), kAccent, 16.f);
    g.Text(Loc::Get("fr_add_title"), { h.x + 54.f, h.y, h.w - 54.f, 22.f }, kText, Mono(14.f, true));
    g.Text(Loc::Get("fr_add_sub"), { h.x + 54.f, h.y + 22.f, h.w - 54.f, 18.f }, kSub, Mono(9.5f).Ellipsis());
    const Rect field{ h.x, h.B() - 42.f, h.w, 42.f };
    TextBoxStyle style = Field(false, 40);
    style.placeholder = Loc::Get("fr_search_placeholder");
    style.padding = { 38.f, 10.f, 40.f, 10.f };
    style.text = Mono(12.f);
    const std::wstring before = friendCode;
    bool enter = false;
    Form::Field(ui, ui.MakeId("fr-code"), field, friendCode, style, kAccent);
    if (ui.Focus() == ui.MakeId("fr-code") && ui.KeyPressed(VK_RETURN, false)) enter = true;
    UI::Glyph(ui, wchar_t(0xE721), { field.x + 10.f, field.y, 22.f, field.h }, 13.f, kSub);
    if (friendCode != before) searchEditedAt = ui.Time();
    const std::wstring query = Text::Trim(friendCode);
    if (!query.empty() &&
        IconButton(ui, ui.MakeId("fr-search-clear"), { field.R() - 36.f, field.y + 6.f, 30.f, 30.f }, Glyphs::Cancel, MenuKit::Kind::Secondary, true, L"")) {
        friendCode.clear();
        searchResults.clear();
        searchedFor.clear();
        searching = searchFailed = false;
    }

    // Search a moment after the typing stops (2+ characters)
    if (query.size() < 2) {
        searchResults.clear();
        searchedFor.clear();
        searching = searchFailed = false;
    } else if (query != searchedFor && (ui.Time() - searchEditedAt > 0.35 || enter)) {
        searchedFor = query;
        searching = true;
        auto token = alive;
        friends.Search(query, [this, token](std::optional<std::vector<Friend>> results) {
            if (!*token) return;
            searching = false;
            searchFailed = !results;
            searchResults = results ? std::move(*results) : std::vector<Friend>{};
            searchShownAt = Async::Now();
        });
    } else if (query != searchedFor) {
        ui.KeepAnimatingThrottled();
    }

    const Rect area{ r.x, hero.B() + 14.f, r.w, r.B() - hero.B() - 14.f };
    if (query.size() < 2) {
        // Nothing typed: what can be searched
        Card(ui, area);
        const Rect c = area.Inset(24.f, 22.f);
        Caption(ui, c.x, c.y, c.w, Loc::Get("fr_add_how"));
        const std::pair<wchar_t, const char*> tips[] = {
            { wchar_t(0xE77B), "fr_tip_name" }, { wchar_t(0xE734), "fr_tip_src" }, { wchar_t(0xE8C8), "fr_tip_code" }, { wchar_t(0xE9F9), "fr_tip_ranking" } };
        float y = c.y + 30.f;
        for (const auto& [glyph, key] : tips) {
            UI::Glyph(ui, glyph, { c.x, y, 22.f, 26.f }, 12.f, kAccent);
            g.Text(Loc::Get(key), { c.x + 32.f, y, c.w - 32.f, 26.f }, kText, Mono(10.5f).Ellipsis());
            y += 34.f;
        }
        const std::wstring& code = friends.MyCode();
        if (!code.empty() && y + 70.f < c.B()) {
            g.FillRect({ c.x, y + 6.f, c.w, 1.f }, Palette::Divider);
            g.Text(Loc::Get("fr_share_code"), { c.x, y + 20.f, c.w, 18.f }, kSub, Mono(9.5f));
            g.Text(code, { c.x, y + 40.f, c.w, 30.f }, kAccent, Mono(20.f, true));
        }
        return;
    }
    if (searching) {
        Form::Spinner(ui, area.CX(), area.y + 60.f, 9.f, kAccent);
        g.Text(Loc::Get("fr_searching"), { area.x, area.y + 80.f, area.w, 20.f }, kSub, Mono(10.f).Center());
        ui.KeepAnimatingThrottled();
        return;
    }
    if (searchFailed || searchResults.empty()) {
        Card(ui, area);
        EmptyState(ui, area, wchar_t(0xE721), Loc::Get(searchFailed ? "fr_search_failed" : "fr_search_none"));
        return;
    }

    // Results as mini profile cards
    constexpr float kCardH = 176.f, kGap = 12.f;
    // Room round the cards for their frames (some hang over the edges)
    constexpr float kPadX = 10.f, kPadTop = 16.f;
    const int columns = std::max(1, static_cast<int>((area.w - kPadX * 2.f + kGap) / (250.f + kGap)));
    const float cardW = (area.w - 10.f - kPadX * 2.f - kGap * (columns - 1)) / columns;
    ScrollArea scroll(ui, ui.MakeId("fr-results"), area);
    auto token = alive;
    for (size_t i = 0; i < searchResults.size(); ++i) {
        const Friend& f = searchResults[i];
        StaggerItem in(ui, searchShownAt, static_cast<int>(i), 0.f, 10.f, 0.04, 0.3);
        const Rect card{ area.x + kPadX + (i % columns) * (cardW + kGap), area.y + kPadTop + (i / columns) * (kCardH + kGap + 8.f), cardW, kCardH };
        const int64_t key = static_cast<int64_t>(std::hash<std::wstring>{}(f.profile.userId));
        const Id id = ui.MakeId("fr-result", key);
        const Interaction it = ui.Interact(id, card.CutBottom(48.f));
        const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 16.f);
        const Rect lifted = card.Offset(0.f, -2.f * hover);
        {
            NoColorFilter keep(g);
            g.FillRound(lifted, 12.f, ProfileDraw::CardColor(f.profile));
        }
        g.PushRoundClip(lifted, 12.f);
        ProfileDraw::Banner(ui, f.profile.BannerUrl(), lifted.Top(56.f), f.profile, 0.f);
        g.PopLayer();
        g.StrokeRound(lifted, 12.f, Lerp(kCardBorder, kAccent, hover * 0.8f), 1.f);
        ProfileDraw::ProfileFrame(ui, lifted, 12.f, f.profile);
        const Rect pic{ lifted.x + 14.f, lifted.y + 30.f, 48.f, 48.f };
        AvatarWithPresence(ui, f, pic, false);
        const std::wstring name = NameOf(f.profile);
        ui.PushClip({ lifted.x + 14.f, pic.B() + 4.f, lifted.w - 28.f, 26.f });
        ProfileDraw::Name(ui, name, { lifted.x + 14.f, pic.B() + 4.f, lifted.w - 28.f, 26.f }, f.profile, 14.f);
        ui.PopClip();
        float cx = lifted.x + 14.f;
        cx += Chip(ui, cx, pic.B() + 40.f, Hours(f.profile), kAccent, wchar_t(0xE916)) + 6.f;
        if (f.profile.SpeedrunLinked() && cx + 60.f < lifted.R()) Chip(ui, cx, pic.B() + 40.f, L"SRC", kSpeedrun, wchar_t(0xE734));
        if (it.hovered) {
            ui.SetCursor(IDC_HAND);
            ui.Tooltip(id, Loc::Get("acc_rank_open_profile"));
        }
        if (it.clicked) OpenViewer(f.profile);

        // The action, full width at the bottom
        const Rect button{ lifted.x + 12.f, lifted.B() - 44.f, lifted.w - 24.f, 32.f };
        const std::wstring userId = f.profile.userId;
        const auto after = [this, token](const std::wstring& message, bool ok) {
            if (!*token) return;
            FriendMessage(message, ok);
            searchedFor.clear();   // look again: the card shows the new relation
        };
        switch (f.relation) {
        case Friend::Relation::None:
            if (MenuKit::FooterButton(ui, ui.MakeId("fr-res-add", key), button, Loc::Get("fr_btn_add"), MenuKit::Kind::Primary, !friends.Busy(), wchar_t(0xE8FA), kAccent))
                friends.SendToUser(userId, after);
            break;
        case Friend::Relation::Incoming:
            if (MenuKit::FooterButton(ui, ui.MakeId("fr-res-accept", key), button, Loc::Get("fr_btn_accept"), MenuKit::Kind::Primary, !friends.Busy(),
                                      Glyphs::CheckMark, MenuKit::kGreen))
                friends.Respond(userId, true, [this, token, name](const std::wstring& message, bool ok) {
                    if (!*token) return;
                    FriendMessage(ok ? Loc::Fmt("fr_accepted", name) : message, ok);
                    searchedFor.clear();
                });
            break;
        case Friend::Relation::Outgoing:
            MenuKit::FooterButton(ui, ui.MakeId("fr-res-sent", key), button, Loc::Get("fr_btn_sent"), MenuKit::Kind::Secondary, false, wchar_t(0xE823), kAccent);
            break;
        case Friend::Relation::Friend:
            MenuKit::FooterButton(ui, ui.MakeId("fr-res-friend", key), button, Loc::Get("fr_btn_friend"), MenuKit::Kind::Secondary, false, wchar_t(0xE716), kAccent);
            break;
        }
    }
    // Enter on a lone result that can be added: add it
    if (enter && searchResults.size() == 1 && searchResults[0].relation == Friend::Relation::None && !friends.Busy())
        friends.SendToUser(searchResults[0].profile.userId, [this, token](const std::wstring& message, bool ok) {
            if (!*token) return;
            FriendMessage(message, ok);
            searchedFor.clear();
        });
    const size_t rows = (searchResults.size() + columns - 1) / columns;
    scroll.SetContentSize(kPadTop + rows * (kCardH + kGap + 8.f) + 8.f);
}

// ── Someone's profile ───────────────────────────────────────────────────────

void AccountOverlay::OpenViewer(const Profile& profile, const std::wstring& runUrl, const std::wstring& runText) {
    standaloneProfile = false;   // opened from the account menu (ShowProfile sets it again)
    viewer.preview = false;
    viewer.avatarSource.clear();
    viewer.bannerSource.clear();
    viewer.open = true;
    viewer.profile = profile;
    viewer.runUrl = runUrl;
    viewer.runText = runText;
    viewer.openedAt = Async::Now();
    viewer.full = false;
}

// Their card, hours in total and per chapter, and what can be done (add as a friend, their run,
// their speedrun.com profile). Drawn over the whole menu; a click outside or Escape closes it.
void AccountOverlay::DrawViewer(Ui& ui, const Rect& screen) {
    if (viewer.full) {
        DrawFullProfile(ui, screen);
        return;
    }
    auto& g = ui.g;
    auto& friends = FriendsService::Get();
    const auto& p = viewer.profile;
    const bool mine = p.userId == Services::Account::AccountService::Get().UserId();
    const float t = Anim::EaseOutCubic(static_cast<float>((ui.Time() - viewer.openedAt) / 0.22));
    if (t < 1.f) ui.KeepAnimating();

    g.FillRect(screen, Rgba(0, 0, 0, static_cast<int>(150 * t)));
    constexpr float kW = 380.f;
    const float cardH = ProfileDraw::CardHeight(kW - 32.f);
    const float hoursH = ProfileDraw::HoursListHeight(p);
    const float h = std::min(screen.h - 40.f, 16.f + cardH + 16.f + (viewer.runText.empty() ? 0.f : 26.f) + 22.f + hoursH + 18.f + 42.f + 36.f + 16.f);
    Rect panel = screen.Center(kW, h);
    panel.y += (1.f - t) * 18.f;
    const bool outside = ui.In().pressed[0] && !ui.MouseOver(panel);
    ui.Block(screen);
    g.PushOpacity(t);
    g.Shadow(panel, 14.f, 24.f, Rgba(0, 0, 0, 160));
    g.Border(panel, 14.f, Palette::PanelBg, kCardBorder, 1.f);

    const Rect inner = panel.Inset(16.f);
    const std::wstring name = NameOf(p);
    ProfileDraw::CardInfo info;
    info.name = name;
    info.avatarSource = p.AvatarUrl();
    info.bannerSource = p.BannerUrl();
    info.line1 = p.SpeedrunLinked() ? L"speedrun.com/" + p.speedrunName : Loc::Get("acc_not_linked_short");
    info.line1Color = p.SpeedrunLinked() ? kSpeedrun : kSub;
    info.line2 = Loc::Fmt("fr_card_hours", Hours(p));
    info.badge = Services::Account::BadgesService::Get().Shown(p.userId);
    ProfileDraw::Card(ui, inner.Top(cardH), p, info);

    float y = inner.y + cardH + 14.f;
    if (!viewer.runText.empty()) {
        g.Text(L"\x23F1  " + viewer.runText, { inner.x, y, inner.w, 20.f }, kSpeedrun, Mono(11.f, true).Ellipsis());
        y += 26.f;
    }
    Caption(ui, inner.x, y, inner.w, Loc::Fmt("fr_hours_total", Hours(p)));
    y += 22.f;
    HoursListClip(ui, { inner.x, y, inner.w, inner.B() - 42.f - 36.f - 16.f - y }, p);

    // The full profile: records, runs, hours and info
    bool openFull = MenuKit::FooterButton(ui, ui.MakeId("viewer-full"), { inner.x, inner.B() - 34.f - 42.f, inner.w, 34.f }, Loc::Get("pf_open_full"),
                                          MenuKit::Kind::Primary, true, wchar_t(0xE77B), kAccent);
    float x = inner.x;
    const float by = inner.B() - 34.f;
    const auto button = [&](const char* id, const std::wstring& label, wchar_t glyph, MenuKit::Kind kind, bool enabled, Color accent) {
        const float w = std::min(inner.R() - x, MenuKit::FooterButtonWidth(ui, label, glyph, 60.f));
        const bool clicked = w > 40.f && MenuKit::FooterButton(ui, ui.MakeId(id), { x, by, w, 34.f }, label, kind, enabled, glyph, accent);
        x += w + 8.f;
        return clicked;
    };
    if (!mine) {
        if (friends.IsFriend(p.userId)) {
            if (button("viewer-msg", Loc::Get("msg_button"), wchar_t(0xE8BD), MenuKit::Kind::Primary, true, Hex(0x3A9BFF))) {
                g.PopLayer();   // (the panel's fade)
                MessageButton(p);
                return;
            }
        } else if (friends.HasRequest(p.userId)) {
            x += Chip(ui, x, by + 17.f, Loc::Get("fr_pending"), kSub, wchar_t(0xE823)) + 10.f;
        } else if (button("viewer-add", Loc::Get("fr_add_short"), wchar_t(0xE8FA), MenuKit::Kind::Primary, !friends.Busy(), kAccent)) {
            auto token = alive;
            friends.SendToUser(p.userId, [this, token](const std::wstring& message, bool ok) {
                if (*token) FriendMessage(message, ok);
            });
        }
    }
    if (!viewer.runUrl.empty() && button("viewer-run", Loc::Get("fr_view_run"), wchar_t(0xE768), MenuKit::Kind::Secondary, true, kSpeedrun))
        Process::OpenUrl(viewer.runUrl);
    else if (viewer.runUrl.empty() && p.SpeedrunLinked() && !p.speedrunUrl.empty() &&
             button("viewer-src", Loc::Get("acc_src_open"), wchar_t(0xE8A7), MenuKit::Kind::Secondary, true, kSpeedrun))
        Process::OpenUrl(p.speedrunUrl);
    const Rect close{ panel.R() - 40.f, panel.y + 10.f, 30.f, 30.f };
    const bool closeClicked = MenuKit::FooterButton(ui, ui.MakeId("viewer-close"), close, L"", MenuKit::Kind::Secondary, true, Glyphs::Cancel, kAccent);
    g.PopLayer();
    if (openFull) {
        const Profile copy = p;
        OpenFullProfile(copy, true);
        return;
    }
    if (closeClicked || outside || ui.KeyPressed(VK_ESCAPE)) viewer.open = false;
}

} // namespace SpeedrunLauncher::Main
