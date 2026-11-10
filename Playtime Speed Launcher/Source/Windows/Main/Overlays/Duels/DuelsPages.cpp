#include <algorithm>
#include <cmath>
#include "Core/Async.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/Account/AccountService.h"
#include "Services/Account/BadgesService.h"
#include "Services/Account/FriendsService.h"
#include "Services/Audio/SoundEffects.h"
#include "Settings/DuelSettings.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "UI/Widgets/TextBox.h"
#include "Windows/Main/Common/BadgeDraw.h"
#include "Windows/Main/Common/ProfileDraw.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/Duels/DuelsKit.h"
#include "Windows/Main/Overlays/Duels/DuelsOverlay.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
using namespace DuelsKit;
using Services::Account::AccountService;
using Services::Account::Duel;
using Services::Account::DuelsService;

namespace {

std::wstring Clock(double seconds) {
    const int s = static_cast<int>(std::ceil(std::max(0.0, seconds)));
    wchar_t buffer[16];
    swprintf_s(buffer, L"%d:%02d", s / 60, s % 60);
    return buffer;
}

// Page title + subtitle; returns the y under them
float Header(Ui& ui, const Rect& r, const std::wstring& title, const std::wstring& subtitle) {
    ui.g.Text(title, { r.x, r.y, r.w, 22.f }, kText, Mono(14.f, true));
    ui.g.Text(subtitle, { r.x, r.y + 22.f, r.w, 18.f }, kSub, Mono(9.5f).Ellipsis());
    return r.y + 52.f;
}

// "MODE" style caption over a group of options
void Section(Ui& ui, float x, float y, float w, const std::wstring& text) { Caption(ui, x, y, w, text, kSub); }

} // namespace

// ── Lobbies: challenges for you + the open lobbies ──────────────────────────

void DuelsOverlay::DrawLobbyRow(Ui& ui, const Rect& row, const Duel& duel, bool invite) {
    auto& g = ui.g;
    auto& duels = DuelsService::Get();
    const int64_t key = static_cast<int64_t>(std::hash<std::wstring>{}(duel.id)) + (invite ? 1 : 0);
    const Id id = ui.MakeId("duel-lobby", key);
    const Interaction it = ui.Interact(id, row);
    const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 16.f);
    const Color edge = invite ? kGold : kCardBorder;
    g.Border(row, 10.f, Lerp(kRow, kRowHover, hover), edge.WithAlpha((invite ? 0.45f : 0.4f) + 0.35f * hover), 1.f);
    if (const Profile& shown = invite && !duel.from.userId.empty() ? duel.from : duel.Host() ? duel.Host()->profile : duel.from; !shown.userId.empty())
        ProfileDraw::Nameplate(ui, row, 10.f, shown);

    const Services::Account::DuelPlayer* host = duel.Host();
    const Profile& who = invite && !duel.from.userId.empty() ? duel.from : host ? host->profile : duel.from;
    const Rect pic{ row.x + 12.f, row.y + 12.f, 42.f, 42.f };
    const std::wstring name = NameOf(who);
    ProfileDraw::Avatar(ui, who.AvatarUrl(), pic, who, name, 1.5f, invite ? kGold : RankColor(host ? host->mmr : 0));

    // Buttons on the right
    const float by = row.CY() - 17.f;
    float bx = row.R() - 12.f;
    const bool inDuel = duels.Active().has_value();
    if (invite) {
        bx -= 40.f;
        if (IconButton(ui, ui.MakeId("duel-decline", key), { bx, by, 40.f, 34.f }, Glyphs::Cancel, MenuKit::Kind::Danger, true, Loc::Get("duel_decline")))
            duels.Decline(duel.id, Reply());
        const std::wstring accept = Loc::Get("duel_accept");
        const float aw = MenuKit::FooterButtonWidth(ui, accept, kDuelGlyph, 110.f);
        bx -= aw + 8.f;
        if (MenuKit::FooterButton(ui, ui.MakeId("duel-accept", key), { bx, by, aw, 34.f }, accept, MenuKit::Kind::Primary, !inDuel && !duels.Busy() && !duel.Full(),
                                  kDuelGlyph, kGold))
            duels.Join(duel.id, Reply());
    } else {
        const std::wstring join = Loc::Get(duel.Full() ? "duel_full" : "duel_join");
        const float jw = MenuKit::FooterButtonWidth(ui, join, kDuelGlyph, 110.f);
        bx -= jw;
        if (MenuKit::FooterButton(ui, ui.MakeId("duel-join", key), { bx, by, jw, 34.f }, join, MenuKit::Kind::Primary, !inDuel && !duels.Busy() && !duel.Full(),
                                  kDuelGlyph, kDuel))
            duels.Join(duel.id, Reply());
        if (inDuel && it.hovered) ui.Tooltip(id, Loc::Get("duel_err_busy"));
    }
    if (it.hovered && !(inDuel && !invite)) ui.Tooltip(id, Loc::Get("acc_rank_open_profile"));
    if (it.clicked && !who.userId.empty()) main.ShowProfile(who);

    // Who + what
    const float textX = pic.R() + 12.f, textW = bx - textX - 14.f;
    ui.PushClip({ textX, row.y, textW, row.h });
    const float nameW = std::min(textW * 0.6f, ProfileDraw::NameWidth(ui, name, who, 14.f) + 4.f);
    ProfileDraw::Name(ui, name, { textX, row.y + 8.f, nameW, 24.f }, who, 14.f);
    const bool plate = ProfileDraw::HasNameplate(who);
    if (invite)
        ProfileDraw::RowText(ui, Loc::Get("duel_challenges_you"), { textX + nameW + 8.f, row.y + 8.f, textW - nameW - 8.f, 24.f }, kGold, Mono(10.f, true).Ellipsis(),
                             plate);
    else if (host) RankChip(ui, textX + nameW + 10.f, row.y + 20.f, host->mmr, true, plate);
    float cx = textX;
    const float cy = row.y + 46.f;
    cx += Chip(ui, cx, cy, DuelModeLabel(duel.slots), kDuel, kDuelGlyph, plate) + 6.f;
    cx += Chip(ui, cx, cy, DuelTypeText(duel), plate ? kText : kSub, 0, plate) + 6.f;
    if (duel.friendsOnly) cx += Chip(ui, cx, cy, Loc::Get("duel_vis_friends_short"), kOnline, wchar_t(0xE716), plate) + 6.f;
    ui.PopClip();

    // Seats + time left (under the button)
    const float dotsW = duel.slots * 12.f;
    const float dx = std::max(textX, bx - dotsW - 64.f);
    for (int i = 0; i < duel.slots; ++i) {
        const bool filled = i < static_cast<int>(duel.players.size());
        g.FillEllipse(dx + 5.f + i * 12.f, row.B() - 12.f, 4.f, 4.f, filled ? kDuel : kCardBorder);
    }
    ProfileDraw::RowText(ui, Clock(duel.LobbySecondsLeft()), { dx + dotsW + 6.f, row.B() - 20.f, 54.f, 16.f }, plate ? kText : kSub, Mono(9.f, true), plate);
    ui.KeepAnimatingThrottled();
}

void DuelsOverlay::DrawLobbies(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    auto& duels = DuelsService::Get();
    const float top = Header(ui, r, Loc::Get("duel_page_lobbies"), Loc::Get("duel_lobbies_sub"));
    {
        const std::wstring create = Loc::Get("duel_create_btn");
        const float w = MenuKit::FooterButtonWidth(ui, create, wchar_t(0xE710), 130.f);
        if (MenuKit::FooterButton(ui, ui.MakeId("duel-lobbies-create"), { r.R() - w, r.y + 2.f, w, 34.f }, create, MenuKit::Kind::Primary, !duels.Active(),
                                  wchar_t(0xE710), kDuel))
            CreateRoom();
    }
    const Rect area{ r.x, top, r.w, r.B() - top };
    Card(ui, area);
    const auto& invites = duels.Invites();
    const auto& lobbies = duels.Lobbies();
    if (invites.empty() && lobbies.empty()) {
        if (!duels.Loaded() && !duels.Failed()) {
            Form::Spinner(ui, area.CX(), area.CY(), 9.f, kDuel);
            ui.KeepAnimatingThrottled();
        } else {
            EmptyState(ui, area, kDuelGlyph, Loc::Get(duels.Failed() ? "duel_load_failed" : "duel_lobbies_none"));
        }
        return;
    }
    const Rect inner = area.Inset(12.f, 10.f);
    ScrollArea scroll(ui, ui.MakeId("duel-lobbies"), inner);
    float y = inner.y;
    int index = 0;
    constexpr float kRowH = 66.f;
    if (!invites.empty()) {
        g.Text(Loc::Fmt("duel_group_invites", invites.size()), { inner.x + 4.f, y, inner.w, 22.f }, kGold, Mono(9.f, true));
        y += 26.f;
        for (const auto& d : invites) {
            StaggerItem in(ui, pageAt + 0.04, index++, 0.f, 8.f, 0.03, 0.28);
            DrawLobbyRow(ui, { inner.x, y, inner.w - 10.f, kRowH }, d, true);
            y += kRowH + 6.f;
        }
        y += 8.f;
    }
    // Lobbies we were invited to already show above
    std::vector<const Duel*> open;
    for (const auto& d : lobbies)
        if (std::none_of(invites.begin(), invites.end(), [&](const Duel& i) { return i.id == d.id; })) open.push_back(&d);
    if (!open.empty()) {
        g.Text(Loc::Fmt("duel_group_open", open.size()), { inner.x + 4.f, y, inner.w, 22.f }, kDuel, Mono(9.f, true));
        y += 26.f;
        for (const Duel* d : open) {
            StaggerItem in(ui, pageAt + 0.04, index++, 0.f, 8.f, 0.03, 0.28);
            DrawLobbyRow(ui, { inner.x, y, inner.w - 10.f, kRowH }, *d, false);
            y += kRowH + 6.f;
        }
    }
    scroll.SetContentSize(y - inner.y);
}

// ── The room: settings ──────────────────────────────────────────────────────
// Everyone sees them; only the host can change them (each change is sent right away).

void DuelsOverlay::DrawRoomSettings(Ui& ui, const Rect& r, const Duel& duel) {
    auto& g = ui.g;
    auto& duels = DuelsService::Get();
    const bool host = duel.hostId == AccountService::Get().UserId();
    const Services::Account::DuelSetup current = DuelsService::SetupOf(duel);
    // A change from the host: sent to the server and remembered for the next room
    const auto apply = [&](Services::Account::DuelSetup changed) {
        if (!host || duels.Busy()) return;
        if (changed.slots < static_cast<int>(duel.players.size())) {
            Message(Loc::Get("duel_err_too_small"), false);
            return;
        }
        setup = changed;
        setup.invite.clear();
        duels.Update(duel.id, changed, Reply());
    };

    ScrollArea scroll(ui, ui.MakeId("duel-room-settings"), r);
    const float w = r.w - 12.f;
    float y = r.y;
    if (!host) {
        MenuKit::Note(ui, { r.x, y, w, 34.f }, Glyphs::Info, Loc::Get("duel_room_host_only"), kSub);
        y += 34.f + 12.f;
    }

    // Mode
    Caption(ui, r.x, y, w, Loc::Get("duel_mode"));
    y += 20.f;
    {
        std::vector<std::wstring> labels;
        for (int s = Services::Account::kDuelMinSlots; s <= Services::Account::kDuelMaxSlots; ++s) labels.push_back(DuelModeLabel(s));
        const int clicked = Pills(ui, { r.x, y, w, 32.f }, labels, current.slots - Services::Account::kDuelMinSlots, "duel-mode");
        if (clicked >= 0) {
            auto changed = current;
            changed.slots = clicked + Services::Account::kDuelMinSlots;
            apply(changed);
        }
        y += 32.f + 6.f;
        g.Text(Loc::Fmt("duel_mode_hint", current.slots), { r.x, y, w, 16.f }, kSub, Mono(9.f).Ellipsis());
        y += 16.f + 14.f;
    }

    // How the category is decided
    Caption(ui, r.x, y, w, Loc::Get("duel_type"));
    y += 20.f;
    {
        const int clicked = Pills(ui, { r.x, y, w, 32.f }, { Loc::Get("duel_type_random"), Loc::Get("duel_type_chapter"), Loc::Get("duel_type_bans") },
                                  static_cast<int>(current.type), "duel-type");
        if (clicked >= 0) {
            auto changed = current;
            changed.type = static_cast<Duel::Type>(clicked);
            apply(changed);
        }
        y += 32.f + 6.f;
        const char* hints[] = { "duel_type_random_hint", "duel_type_chapter_hint", "duel_type_bans_hint" };
        TextStyle hint = Mono(9.f);
        hint.wrap = true;
        g.Text(Loc::Get(hints[static_cast<int>(current.type)]), { r.x, y, w, 28.f }, kSub, hint.Top());
        y += 28.f + 10.f;
    }

    if (current.type == Duel::Type::Chapter) {
        Caption(ui, r.x, y, w, Loc::Get("duel_chapter"));
        y += 20.f;
        std::vector<std::wstring> chapters;
        for (int c = 1; c <= 5; ++c) chapters.push_back(Loc::Fmt("fr_chapter", c));
        const int clicked = Pills(ui, { r.x, y, w, 32.f }, chapters, current.chapter - 1, "duel-chapter");
        if (clicked >= 0) {
            auto changed = current;
            changed.chapter = clicked + 1;
            changed.category.clear();
            apply(changed);
        }
        y += 32.f + 12.f;
        Caption(ui, r.x, y, w, Loc::Get("duel_category"));
        y += 20.f;
        std::vector<std::pair<std::string, std::wstring>> options = { { "", Loc::Get("duel_random_category") } };
        for (const auto& c : Services::Account::DuelCategories())
            if (c.chapter == current.chapter) options.push_back({ c.id, c.label });
        constexpr int kPerRow = 4;
        const float gap = 8.f, cw = (w - gap * (kPerRow - 1)) / kPerRow;
        for (size_t i = 0; i < options.size(); ++i) {
            const Rect cell{ r.x + (i % kPerRow) * (cw + gap), y + (i / kPerRow) * (32.f + gap), cw, 32.f };
            if (Pills(ui, cell, { options[i].second }, options[i].first == current.category ? 0 : -1, "duel-cat-" + std::to_string(i)) >= 0) {
                auto changed = current;
                changed.category = options[i].first;
                apply(changed);
            }
        }
        y += ((options.size() + kPerRow - 1) / kPerRow) * (32.f + gap) + 8.f;
    } else if (current.type == Duel::Type::Bans) {
        Caption(ui, r.x, y, w, Loc::Get("duel_ban_count"));
        y += 20.f;
        const auto& counts = Services::Account::DuelBanCounts();
        std::vector<std::wstring> labels;
        int selected = -1;
        for (size_t i = 0; i < counts.size(); ++i) {
            labels.push_back(std::to_wstring(counts[i]));
            if (counts[i] == current.bans) selected = static_cast<int>(i);
        }
        const int clicked = Pills(ui, { r.x, y, w, 32.f }, labels, selected, "duel-bans");
        if (clicked >= 0) {
            auto changed = current;
            changed.bans = counts[clicked];
            apply(changed);
        }
        y += 32.f + 6.f;
        g.Text(Loc::Fmt("duel_bans_hint", Services::Account::DuelCategories().size() - current.bans), { r.x, y, w, 16.f }, kSub, Mono(9.f).Ellipsis());
        y += 16.f + 14.f;
    }

    // Who can join
    Caption(ui, r.x, y, w, Loc::Get("duel_visibility"));
    y += 20.f;
    {
        const int clicked = Pills(ui, { r.x, y, w, 32.f }, { Loc::Get("duel_vis_public"), Loc::Get("duel_vis_friends") }, current.friendsOnly ? 1 : 0, "duel-vis");
        if (clicked >= 0) {
            auto changed = current;
            changed.friendsOnly = clicked == 1;
            apply(changed);
        }
        y += 32.f + 6.f;
        g.Text(Loc::Get(current.friendsOnly ? "duel_vis_friends_hint" : "duel_vis_public_hint"), { r.x, y, w, 16.f }, kSub, Mono(9.f).Ellipsis());
        y += 16.f + 8.f;
    }
    scroll.SetContentSize(y - r.y);
}

// ── The room: inviting people ───────────────────────────────────────────────
// Friends (online first) and everyone else who's online, filtered by name; anyone in the room can
// invite, the host can also withdraw an invite.

void DuelsOverlay::DrawRoomInvite(Ui& ui, const Rect& r, const Duel& duel) {
    auto& g = ui.g;
    auto& duels = DuelsService::Get();
    auto& friends = Services::Account::FriendsService::Get();
    const bool host = duel.hostId == AccountService::Get().UserId();

    // Filter
    {
        const Rect field{ r.x, r.y, r.w, 34.f };
        TextBoxStyle style = Field(false, 32);
        style.placeholder = Loc::Get("duel_invite_filter");
        style.padding = { 30.f, 7.f, 10.f, 7.f };
        style.text = Mono(10.5f);
        style.focusBorder = kDuel;
        style.caret = kDuel;
        TextBox(ui, ui.MakeId("duel-invite-filter"), field, inviteFilter, style);
        UI::Glyph(ui, wchar_t(0xE721), { field.x + 6.f, field.y, 20.f, field.h }, 11.f, kSub);
    }

    // Who can be invited: friends, then the rest of the online list
    struct Person {
        const Profile* profile;
        bool isFriend;
        bool online;
        int mmr;          // -1 = unknown (offline friend)
        bool busy;        // in another duel
    };
    std::vector<Person> people;
    const auto onlineEntry = [&](const std::wstring& id) -> const Services::Account::DuelOnlineUser* {
        for (const auto& u : duels.Online())
            if (u.profile.userId == id) return &u;
        return nullptr;
    };
    for (const auto& f : friends.Friends()) {
        const auto* on = onlineEntry(f.profile.userId);
        people.push_back({ &f.profile, true, f.Online(), on ? on->mmr : -1, on && on->inDuel && !duel.Player(f.profile.userId) });
    }
    for (const auto& u : duels.Online())
        if (!u.isFriend) people.push_back({ &u.profile, false, true, u.mmr, u.inDuel && !duel.Player(u.profile.userId) });
    std::stable_sort(people.begin(), people.end(), [](const Person& a, const Person& b) {
        if (a.isFriend != b.isFriend) return a.isFriend;
        return a.online && !b.online;
    });
    const std::wstring filter = Text::Lower(Text::Trim(inviteFilter));
    if (!filter.empty())
        people.erase(std::remove_if(people.begin(), people.end(),
                                    [&](const Person& p) {
                                        return !Text::Contains(Text::Lower(NameOf(*p.profile)), filter) && !Text::Contains(Text::Lower(p.profile->speedrunName), filter);
                                    }),
                     people.end());

    const Rect area{ r.x, r.y + 44.f, r.w, r.h - 44.f };
    if (people.empty()) {
        EmptyState(ui, area, wchar_t(0xE8FA), Loc::Get(filter.empty() ? "duel_invite_none" : "fr_filter_none"));
        return;
    }
    ScrollArea scroll(ui, ui.MakeId("duel-invite-list"), area);
    float y = area.y;
    int index = 0;
    for (const auto& p : people) {
        StaggerItem in(ui, roomTabAt + 0.03, index++, 0.f, 6.f, 0.025, 0.24);
        const Rect row{ area.x, y, area.w - 10.f, 50.f };
        y += 54.f;
        const int64_t key = static_cast<int64_t>(std::hash<std::wstring>{}(p.profile->userId));
        const Id rowId = ui.MakeId("duel-invite-row", key);
        const Interaction rowIt = ui.Interact(rowId, row);   // the button on the right sits above it
        const float rowHover = ui.Animate(HashId(rowId, 1), rowIt.hovered ? 1.f : 0.f, 16.f);
        g.Border(row, 10.f, Lerp(kRow, kRowHover, rowHover), kCardBorder.WithAlpha(0.4f + 0.3f * rowHover), 1.f);
        ProfileDraw::Nameplate(ui, row, 10.f, *p.profile);
        if (rowIt.hovered) ui.Tooltip(rowId, Loc::Get("acc_rank_open_profile"));
        if (rowIt.clicked) main.ShowProfile(*p.profile);
        const Rect pic{ row.x + 10.f, row.CY() - 16.f, 32.f, 32.f };
        const std::wstring name = NameOf(*p.profile);
        ProfileDraw::Avatar(ui, p.profile->AvatarUrl(), pic, *p.profile, name, 0.f, Colors::Transparent);
        {
            NoColorFilter keep(g);
            g.FillEllipse(pic.R() - 4.f, pic.B() - 4.f, 6.f, 6.f, kCard);
            g.FillEllipse(pic.R() - 4.f, pic.B() - 4.f, 4.f, 4.f, p.busy ? kDuel : p.online ? kOnline : Hex(0x55606A));
        }
        const float textX = pic.R() + 10.f, textW = row.R() - 150.f - textX;
        ui.PushClip({ textX, row.y, textW, row.h });
        ProfileDraw::Name(ui, name, { textX, row.y + 5.f, textW, 22.f }, *p.profile, 12.5f);
        ui.PopClip();
        float sx = textX;
        if (p.mmr >= 0) {
            RankIcon(ui, { sx, row.y + 29.f, 15.f, 15.f }, p.mmr);
            sx += 19.f;
        }
        const std::wstring line = p.busy ? Loc::Get("duel_status_in_duel")
                                  : (p.isFriend ? Loc::Get("duel_invite_friend") + L" \x00B7 " : L"") + Loc::Get(p.online ? "fr_online" : "fr_offline");
        const bool plate = ProfileDraw::HasNameplate(*p.profile);
        ProfileDraw::RowText(ui, line, { sx, row.y + 28.f, textW - (sx - textX), 16.f }, p.busy ? kDuel : p.online ? kOnline : plate ? kText : kSub,
                             Mono(9.f, true).Ellipsis(), plate);

        // The action on the right
        const Rect button{ row.R() - 130.f, row.CY() - 15.f, 120.f, 30.f };
        bool invited = duels.Challenged(p.profile->userId);
        for (const auto& i : duel.invited)
            if (i.userId == p.profile->userId) invited = true;
        if (duel.Player(p.profile->userId)) {
            MenuKit::FooterButton(ui, ui.MakeId("duel-inv-in", key), button, Loc::Get("duel_in_room"), MenuKit::Kind::Secondary, false, Glyphs::CheckMark, kOnline);
        } else if (invited) {
            if (MenuKit::FooterButton(ui, ui.MakeId("duel-inv-cancel", key), button, Loc::Get(host ? "duel_invite_cancel" : "duel_invited_short"),
                                      host ? MenuKit::Kind::Danger : MenuKit::Kind::Secondary, host && !duels.Busy(), host ? Glyphs::Cancel : Glyphs::CheckMark, kDuel) &&
                host)
                duels.Kick(duel.id, p.profile->userId, Reply());
        } else if (MenuKit::FooterButton(ui, ui.MakeId("duel-inv-send", key), button, Loc::Get("duel_invite_short"), MenuKit::Kind::Primary,
                                         !duels.Busy() && !p.busy, wchar_t(0xE8FA), kDuel)) {
            duels.Invite(duel.id, p.profile->userId, Reply(Loc::Fmt("duel_invited", name)));
        }
    }
    scroll.SetContentSize(y - area.y);
}

// ── Ranking ─────────────────────────────────────────────────────────────────

void DuelsOverlay::DrawRanking(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    auto& duels = DuelsService::Get();
    // The ranking is this month's: it starts again on the 1st, and its top player becomes champion
    const int daysLeft = Services::Account::DuelMonthDaysLeft();
    float top = Header(ui, r, Loc::Fmt("duel_ranking_month", Text::Upper(Services::Account::DuelMonthName())),
                       Loc::Fmt(daysLeft == 1 ? "duel_ranking_month_sub_1" : "duel_ranking_month_sub", daysLeft,
                                Services::Account::DuelChampionTitle("")));
    if (IconButton(ui, ui.MakeId("duel-rank-refresh"), { r.R() - 36.f, r.y + 2.f, 36.f, 34.f }, Glyphs::Refresh, MenuKit::Kind::Secondary,
                   !duels.LeaderboardLoading(), Loc::Get("duel_refresh")))
        duels.LoadLeaderboard();

    // The champions of the months before (newest first), for everyone to see
    if (const auto& champions = duels.Champions(); !champions.empty()) {
        Caption(ui, r.x, top - 6.f, r.w, Loc::Get("duel_champions"), kGold);
        const Rect strip{ r.x, top + 14.f, r.w, 58.f };
        const float cardW = std::min(250.f, std::max(190.f, (strip.w - 8.f * 2.f) / 3.f));
        const int fit = std::max(1, static_cast<int>((strip.w + 8.f) / (cardW + 8.f)));
        for (int i = 0; i < std::min(fit, static_cast<int>(champions.size())); ++i) {
            const auto& c = champions[i];
            const Rect card{ strip.x + i * (cardW + 8.f), strip.y, cardW, strip.h };
            const Id id = ui.MakeId("duel-champion", i);
            const Interaction it = ui.Interact(id, card);
            const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 16.f);
            {
                NoColorFilter keep(g);
                g.GlowRound(card, 10.f, kGold, 10.f, 0.12f + 0.12f * hover);
                g.Border(card, 10.f, Lerp(Hex(0x1A1608), Hex(0x2A220A), hover), kGold.WithAlpha(0.55f + 0.3f * hover), 1.f);
            }
            ProfileDraw::Nameplate(ui, card, 10.f, c.profile);
            const bool plate = ProfileDraw::HasNameplate(c.profile);
            const Rect pic{ card.x + 10.f, card.CY() - 17.f, 34.f, 34.f };
            const std::wstring name = NameOf(c.profile);
            ProfileDraw::Avatar(ui, c.profile.AvatarUrl(), pic, c.profile, name, 1.5f, kGold);
            UI::Glyph(ui, wchar_t(0xE734), { pic.R() - 10.f, pic.y - 6.f, 16.f, 16.f }, 10.f, kGold);   // the trophy
            const float textX = pic.R() + 10.f, textW = card.R() - 10.f - textX;
            ui.PushClip({ textX, card.y, textW, card.h });
            ProfileDraw::Name(ui, name, { textX, card.y + 7.f, textW, 22.f }, c.profile, 12.5f);
            ui.PopClip();
            ProfileDraw::RowText(ui, Services::Account::DuelChampionTitle(c.month), { textX, card.y + 30.f, textW, 16.f }, kGold, Mono(9.f, true).Ellipsis(), plate);
            if (it.hovered) ui.Tooltip(id, Loc::Fmt("duel_champion_tip", c.mmr, c.wins, c.losses));
            if (it.clicked) main.ShowProfile(c.profile);
        }
        top = strip.B() + 14.f;
    }
    const Rect area{ r.x, top, r.w, r.B() - top };
    Card(ui, area);
    const auto& list = duels.Leaderboard();
    if (list.empty()) {
        if (duels.LeaderboardLoading()) {
            Form::Spinner(ui, area.CX(), area.CY(), 9.f, kDuel);
            ui.KeepAnimatingThrottled();
        } else {
            EmptyState(ui, area, wchar_t(0xE9F9), Loc::Get(duels.LeaderboardFailed() ? "duel_load_failed" : "duel_ranking_none"));
        }
        return;
    }
    const Rect inner = area.Inset(12.f, 10.f);
    // Column headers
    const float colPos = inner.x + 8.f, colName = inner.x + 56.f;
    const float colRecord = inner.R() - 130.f, colMmr = inner.R() - 220.f;
    g.Text(L"#", { colPos, inner.y, 40.f, 18.f }, kSub, Mono(9.f, true));
    g.Text(Loc::Get("duel_col_player"), { colName, inner.y, 200.f, 18.f }, kSub, Mono(9.f, true));
    g.Text(L"MMR", { colMmr, inner.y, 80.f, 18.f }, kSub, Mono(9.f, true));
    g.Text(Loc::Get("duel_col_record"), { colRecord, inner.y, 120.f, 18.f }, kSub, Mono(9.f, true));
    const Rect listRect = inner.CutTop(24.f);
    ScrollArea scroll(ui, ui.MakeId("duel-rank-list"), listRect);
    const std::wstring& me = AccountService::Get().UserId();
    float y = listRect.y;
    for (size_t i = 0; i < list.size(); ++i) {
        const auto& s = list[i];
        StaggerItem in(ui, pageAt + 0.04, static_cast<int>(i), 0.f, 6.f, 0.025, 0.26);
        const Rect row{ listRect.x, y, listRect.w - 10.f, 48.f };
        y += 52.f;
        const bool mine = s.profile.userId == me;
        const int place = static_cast<int>(i) + 1;
        const Id rowId = ui.MakeId("duel-rank-row", static_cast<int64_t>(i));
        const Interaction rowIt = ui.Interact(rowId, row);
        const float rowHover = ui.Animate(HashId(rowId, 1), rowIt.hovered ? 1.f : 0.f, 16.f);
        if (rowIt.hovered) ui.Tooltip(rowId, Loc::Get("acc_rank_open_profile"));
        if (rowIt.clicked) main.ShowProfile(s.profile);
        g.Border(row, 10.f, Lerp(mine ? kDuel.WithAlpha(0.09f) : kRow, kRowHover, rowHover), mine ? kDuel.WithAlpha(0.6f) : place <= 3 ? PlaceColor(place).WithAlpha(0.35f) : kCardBorder.WithAlpha(0.35f),
                 1.f);
        ProfileDraw::Nameplate(ui, row, 10.f, s.profile);
        const bool plate = ProfileDraw::HasNameplate(s.profile);
        ProfileDraw::RowText(ui, std::to_wstring(place), { row.x + 8.f, row.y, 36.f, row.h }, place <= 3 ? PlaceColor(place) : plate ? kText : kSub,
                             Mono(place <= 3 ? 14.f : 11.f, true), plate);
        const Rect pic{ colName, row.CY() - 15.f, 30.f, 30.f };
        const std::wstring name = NameOf(s.profile);
        ProfileDraw::Avatar(ui, s.profile.AvatarUrl(), pic, s.profile, name, 1.5f, RankColor(s.mmr));
        const float nameW = colMmr - pic.R() - 20.f;
        ui.PushClip({ pic.R() + 10.f, row.y, nameW, row.h });
        ProfileDraw::Name(ui, name, { pic.R() + 10.f, row.y + 4.f, nameW, 22.f }, s.profile, 12.5f);
        RankIcon(ui, { pic.R() + 10.f, row.y + 26.f, 15.f, 15.f }, s.mmr);
        const std::wstring rankName = Text::Widen(Services::Account::DuelRankFor(s.mmr).name);
        ProfileDraw::RowText(ui, rankName, { pic.R() + 29.f, row.y + 26.f, nameW - 19.f, 16.f }, RankColor(s.mmr), Mono(9.f, true), plate);
        // Their badge (the one they picked, or the newest)
        if (const auto* badge = Services::Account::BadgesService::Get().Shown(s.profile.userId)) {
            const float bx = pic.R() + 29.f + g.Measure(rankName, Mono(9.f, true)).w + 10.f;
            BadgeDraw::Pill(ui, bx, row.y + 34.f, *badge, std::max(0.f, pic.R() + 10.f + nameW - bx), 16.f);
        }
        ui.PopClip();
        ProfileDraw::RowText(ui, std::to_wstring(s.mmr), { colMmr, row.y, 80.f, row.h }, kText, Mono(12.f, true), plate);
        const int played = s.wins + s.losses;
        ProfileDraw::RowText(ui, Loc::Fmt("duel_record", s.wins, s.losses) + (played > 0 ? L"  " + std::to_wstring(s.wins * 100 / played) + L"%" : L""),
                             { colRecord, row.y, 128.f, row.h }, plate ? kText : kSub, Mono(9.5f, true).Ellipsis(), plate);
    }
    scroll.SetContentSize(y - listRect.y);
}

// ── How it works ────────────────────────────────────────────────────────────

void DuelsOverlay::DrawRules(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    const float top = Header(ui, r, Loc::Get("duel_page_rules"), Loc::Get("duel_rules_sub"));
    const Rect area{ r.x, top, r.w, r.B() - top };
    Card(ui, area);
    const Rect inner = area.Inset(18.f, 14.f);
    ScrollArea scroll(ui, ui.MakeId("duel-rules"), inner);
    const float w = inner.w - 12.f;
    float y = inner.y;
    TextStyle body = Mono(10.f);
    body.wrap = true;

    // How a duel goes
    Caption(ui, inner.x, y, w, Loc::Get("duel_rules_flow"), kDuel);
    y += 22.f;
    const char* steps[] = { "duel_rules_step1", "duel_rules_step2", "duel_rules_step3", "duel_rules_step4" };
    for (int i = 0; i < 4; ++i) {
        const Rect badge{ inner.x, y + 1.f, 20.f, 20.f };
        g.FillEllipse(badge.CX(), badge.CY(), 10.f, 10.f, kDuel.WithAlpha(0.18f));
        g.Text(std::to_wstring(i + 1), badge, kDuel, Mono(9.f, true).Center());
        const std::wstring text = Loc::Get(steps[i]);
        const float h = std::max(22.f, g.Measure(text, body, w - 32.f).h + 4.f);
        g.Text(text, { inner.x + 32.f, y, w - 32.f, h }, kText, TextStyle(body).Top());
        y += h + 8.f;
    }
    y += 10.f;

    // MMR
    Caption(ui, inner.x, y, w, Loc::Get("duel_rules_mmr"), kDuel);
    y += 22.f;
    const char* points[] = { "duel_rules_mmr1", "duel_rules_mmr2", "duel_rules_mmr3", "duel_rules_mmr4" };
    for (const char* key : points) {
        const std::wstring text = key == std::string("duel_rules_mmr4")
                                      ? Loc::Fmt(key, Services::Account::kDuelPlacementMatches, Services::Account::kDuelKPlacement, Services::Account::kDuelKBase)
                                      : Loc::Get(key);
        const float h = std::max(20.f, g.Measure(text, body, w - 20.f).h + 4.f);
        g.FillEllipse(inner.x + 5.f, y + 9.f, 2.5f, 2.5f, kDuel);
        g.Text(text, { inner.x + 18.f, y, w - 18.f, h }, kText, TextStyle(body).Top());
        y += h + 6.f;
    }
    y += 12.f;

    // 1v1 examples
    Caption(ui, inner.x, y, w, Loc::Get("duel_rules_example"), kDuel);
    y += 22.f;
    const float c1 = inner.x, c2 = inner.x + w * 0.34f, c3 = inner.x + w * 0.67f;
    g.Text(Loc::Get("duel_rules_gap"), { c1, y, w * 0.3f, 18.f }, kSub, Mono(9.f, true));
    g.Text(Loc::Get("duel_rules_fav_wins"), { c2, y, w * 0.32f, 18.f }, kSub, Mono(9.f, true));
    g.Text(Loc::Get("duel_rules_dog_wins"), { c3, y, w * 0.32f, 18.f }, kSub, Mono(9.f, true));
    y += 22.f;
    for (int gap : { 0, 100, 200, 300, 400 }) {
        const double expected = Services::Account::DuelExpected(gap, 0);
        const int fav = static_cast<int>(std::lround(Services::Account::kDuelKBase * (1.0 - expected)));
        const int dog = static_cast<int>(std::lround(Services::Account::kDuelKBase * expected));
        const Rect row{ inner.x, y, w, 28.f };
        g.FillRound(row, 8.f, kRow);
        g.Text(gap == 0 ? Loc::Get("duel_rules_even") : Loc::Fmt("duel_rules_gap_n", gap), { c1 + 10.f, y, w * 0.3f, 28.f }, kText, Mono(10.f, true));
        g.Text(L"+" + std::to_wstring(fav) + L" / -" + std::to_wstring(fav), { c2, y, w * 0.3f, 28.f }, kOnline, Mono(10.f, true));
        g.Text(L"+" + std::to_wstring(dog) + L" / -" + std::to_wstring(dog), { c3, y, w * 0.3f, 28.f }, kGold, Mono(10.f, true));
        y += 32.f;
    }
    y += 14.f;

    // Ranks
    Caption(ui, inner.x, y, w, Loc::Get("duel_rules_ranks"), kDuel);
    y += 22.f;
    const auto& ranks = Services::Account::DuelRanks();
    const int myMmr = DuelsService::Get().MyMmr();
    const auto& mine = Services::Account::DuelRankFor(myMmr);
    constexpr int kCols = 2;
    const float gap = 10.f, cw = (w - gap) / kCols;
    for (size_t i = 0; i < ranks.size(); ++i) {
        const auto& rank = ranks[i];
        const Rect cell{ inner.x + (i % kCols) * (cw + gap), y + (i / kCols) * 40.f, cw, 34.f };
        const Color c = Hex(rank.color);
        const bool current = &rank == &mine;
        g.Border(cell, 9.f, current ? c.WithAlpha(0.14f) : kRow, c.WithAlpha(current ? 0.8f : 0.3f), 1.f);
        RankIconOf(ui, { cell.x + 8.f, cell.CY() - 11.f, 22.f, 22.f }, rank);
        g.Text(Text::Widen(rank.name), { cell.x + 34.f, cell.y, cell.w - 120.f, cell.h }, current ? c : kText, Mono(10.5f, true).Ellipsis());
        g.Text(std::to_wstring(rank.min) + L"+ MMR", { cell.R() - 96.f, cell.y, 86.f, cell.h }, kSub, Mono(9.5f, true).Right());
    }
    y += ((ranks.size() + kCols - 1) / kCols) * 40.f + 6.f;
    scroll.SetContentSize(y - inner.y);
}

// ── Settings: notifications and sounds ─────────────────────────────────────

void DuelsOverlay::DrawSettings(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    auto& s = Settings::Duels::Current();
    const float top = Header(ui, r, Loc::Get("duel_page_settings"), Loc::Get("duel_settings_sub"));
    const Rect area{ r.x, top, r.w, r.B() - top };
    ScrollArea scroll(ui, ui.MakeId("duel-settings"), area);
    const float w = area.w - 12.f;
    float y = area.y;
    int reveal = 0;
    bool changed = false;

    // One row: glyph, label + hint, an optional ▶ (hear the sound) and the switch
    const auto row = [&](const char* id, wchar_t glyph, const std::wstring& label, const std::wstring& hint, bool& value, bool enabled,
                         const wchar_t* preview) {
        StaggerItem in(ui, pageAt, reveal++, 0.f, 8.f);
        const Rect box{ area.x, y, w, 58.f };
        y += 58.f + 8.f;
        g.Border(box, 10.f, kRow, kCardBorder.WithAlpha(0.45f), 1.f);
        const float dim = enabled ? 1.f : 0.45f;
        UI::Glyph(ui, glyph, { box.x + 14.f, box.y, 22.f, box.h }, 14.f, (value && enabled ? kDuel : kSub).WithAlpha(dim));
        const float right = box.R() - 14.f - ToggleWidth(ui, value) - (preview ? 44.f : 0.f);
        g.Text(label, { box.x + 48.f, box.y + 10.f, right - box.x - 56.f, 20.f }, kText.WithAlpha(dim), Mono(11.f, true).Ellipsis());
        g.Text(hint, { box.x + 48.f, box.y + 30.f, right - box.x - 56.f, 18.f }, kSub.WithAlpha(dim), Mono(9.f).Ellipsis());
        if (preview && IconButton(ui, ui.MakeId(id, 1), { right + 4.f, box.CY() - 15.f, 32.f, 30.f }, Glyphs::Play, MenuKit::Kind::Secondary, true,
                                  Loc::Get("duel_settings_preview")))
            Services::Sfx::Play(preview);
        if (Toggle(ui, ui.MakeId(id), { box.R() - 14.f - ToggleWidth(ui, value), box.CY() - 13.f, ToggleWidth(ui, value), 26.f }, value)) {
            value = !value;
            changed = true;
        }
    };

    Caption(ui, area.x, y, w, Loc::Get("duel_settings_general"), kDuel);
    y += 22.f;
    row("duel-set-toasts", wchar_t(0xEA8F), Loc::Get("duel_settings_toasts"), Loc::Get("duel_settings_toasts_hint"), s.notifications, true, nullptr);
    row("duel-set-sounds", wchar_t(0xE767), Loc::Get("duel_settings_sounds"), Loc::Get("duel_settings_sounds_hint"), s.sounds, true, nullptr);
    y += 10.f;

    Caption(ui, area.x, y, w, Loc::Get("duel_settings_events"), kDuel);
    y += 22.f;
    const bool any = s.notifications || s.sounds;
    row("duel-set-invites", wchar_t(0xE8FA), Loc::Get("duel_settings_invites"), Loc::Get("duel_settings_invites_hint"), s.invites, any,
        Services::Sfx::DuelInvite);
    row("duel-set-joins", wchar_t(0xE716), Loc::Get("duel_settings_joins"), Loc::Get("duel_settings_joins_hint"), s.joins, any, Services::Sfx::DuelJoin);
    row("duel-set-starts", Glyphs::Play, Loc::Get("duel_settings_starts"), Loc::Get("duel_settings_starts_hint"), s.starts, any, Services::Sfx::Chapter);
    row("duel-set-results", wchar_t(0xE734), Loc::Get("duel_settings_results"), Loc::Get("duel_settings_results_hint"), s.results, any,
        Services::Sfx::DuelWin);
    row("duel-set-ban", wchar_t(0xE8F8), Loc::Get("duel_settings_ban"), Loc::Get("duel_settings_ban_hint"), s.banTurn, any, Services::Sfx::Click);
    row("duel-set-rival", kDuelGlyph, Loc::Get("duel_settings_rival"), Loc::Get("duel_settings_rival_hint"), s.rivalFinished, any, Services::Sfx::Click);
    row("duel-set-leaves", wchar_t(0xE7E8), Loc::Get("duel_settings_leaves"), Loc::Get("duel_settings_leaves_hint"), s.leaves, any, nullptr);
    row("duel-set-room-chat", wchar_t(0xE8BD), Loc::Get("duel_settings_room_chat"), Loc::Get("duel_settings_room_chat_hint"), s.roomChat, any, Services::Sfx::Click);
    row("duel-set-dms", wchar_t(0xE715), Loc::Get("duel_settings_dms"), Loc::Get("duel_settings_dms_hint"), s.directMessages, any, Services::Sfx::Click);
    // Hear the defeat sound too (results covers both)
    {
        const std::wstring lose = Loc::Get("duel_settings_preview_lose");
        const float lw = MenuKit::FooterButtonWidth(ui, lose, Glyphs::Play, 120.f);
        if (MenuKit::FooterButton(ui, ui.MakeId("duel-set-lose"), { area.x + w - lw, y, lw, 30.f }, lose, MenuKit::Kind::Secondary, true, Glyphs::Play, kDuel))
            Services::Sfx::Play(Services::Sfx::DuelLose);
        y += 30.f + 8.f;
    }
    y += 10.f;

    // Who can ring, and how the notifications show
    Caption(ui, area.x, y, w, Loc::Get("duel_settings_display"), kDuel);
    y += 22.f;
    row("duel-set-friends-only", wchar_t(0xE716), Loc::Get("duel_settings_friends_only"), Loc::Get("duel_settings_friends_only_hint"), s.friendInvitesOnly,
        any && s.invites, nullptr);
    row("duel-set-flash", wchar_t(0xE7C4), Loc::Get("duel_settings_flash"), Loc::Get("duel_settings_flash_hint"), s.flashTaskbar, s.notifications, nullptr);
    {
        // How long a notification stays: short · normal · long
        StaggerItem in(ui, pageAt, reveal++, 0.f, 8.f);
        const Rect box{ area.x, y, w, 58.f };
        y += 58.f + 8.f;
        const float dim = s.notifications ? 1.f : 0.45f;
        g.Border(box, 10.f, kRow, kCardBorder.WithAlpha(0.45f), 1.f);
        UI::Glyph(ui, wchar_t(0xE916), { box.x + 14.f, box.y, 22.f, box.h }, 14.f, (s.notifications ? kDuel : kSub).WithAlpha(dim));
        const float pillsW = std::min(300.f, box.w * 0.5f);
        const float textW = box.w - pillsW - 48.f - 24.f;
        g.Text(Loc::Get("duel_settings_length"), { box.x + 48.f, box.y + 10.f, textW, 20.f }, kText.WithAlpha(dim), Mono(11.f, true).Ellipsis());
        g.Text(Loc::Get("duel_settings_length_hint"), { box.x + 48.f, box.y + 30.f, textW, 18.f }, kSub.WithAlpha(dim), Mono(9.f).Ellipsis());
        const int clicked = Pills(ui, { box.R() - 14.f - pillsW, box.CY() - 15.f, pillsW, 30.f },
                                  { Loc::Get("duel_settings_length_short"), Loc::Get("duel_settings_length_normal"), Loc::Get("duel_settings_length_long") },
                                  s.toastLength, "duel-set-length");
        if (clicked >= 0 && clicked != s.toastLength) {
            s.toastLength = clicked;
            changed = true;
        }
    }
    // Try it: a sample notification with the current settings
    {
        const std::wstring test = Loc::Get("duel_settings_test");
        const float tw = MenuKit::FooterButtonWidth(ui, test, wchar_t(0xEA8F), 160.f);
        if (MenuKit::FooterButton(ui, ui.MakeId("duel-set-test"), { area.x + w - tw, y, tw, 30.f }, test, MenuKit::Kind::Secondary, s.notifications,
                                  wchar_t(0xEA8F), kDuel)) {
            if (auto& notify = DuelsService::Get().onNotify) notify(Loc::Get("duel_settings_test_title"), Loc::Get("duel_settings_test_text"), 0xFF5A3C);
            if (s.sounds) Services::Sfx::Play(Services::Sfx::DuelInvite);
        }
        y += 30.f + 8.f;
    }
    if (changed) s.Save();
    scroll.SetContentSize(y - area.y);
}

} // namespace SpeedrunLauncher::Main

