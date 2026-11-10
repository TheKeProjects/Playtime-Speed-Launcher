#include <algorithm>
#include <cmath>
#include "App/Launcher.h"
#include "Core/Async.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/Account/AccountService.h"
#include "Services/Account/FriendsService.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "UI/Widgets/TextBox.h"
#include "Windows/Main/Common/ProfileDraw.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/Duels/DuelsKit.h"
#include "Windows/Main/Overlays/Duels/DuelsOverlay.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
using namespace DuelsKit;
using Services::Account::AccountService;
using Services::Account::Duel;
using Services::Account::DuelPlayer;
using Services::Account::DuelsService;

namespace {

std::wstring Clock(double seconds, bool hours = false) {
    const long long s = static_cast<long long>(std::max(0.0, seconds));
    wchar_t buffer[24];
    if (hours || s >= 3600) swprintf_s(buffer, L"%lld:%02lld:%02lld", s / 3600, (s / 60) % 60, s % 60);
    else swprintf_s(buffer, L"%lld:%02lld", s / 60, s % 60);
    return buffer;
}

std::wstring Ordinal(int placement) { return Loc::Fmt("duel_place_n", placement); }

// The chapter's name from the launcher's translations ("A TIGHT SQUEEZE")
std::wstring ChapterName(int chapter) { return chapter > 0 ? Loc::Get("ch" + std::to_string(chapter) + "_title") : L"\x2014"; }

// The forfeit button's label: "forfeit" / "sure?", or how long until we can (2 forfeits in a row =
// no forfeiting for an hour)
std::wstring ForfeitLabel(bool confirm, double blockedFor) {
    if (blockedFor > 0.0) return Loc::Fmt("duel_forfeit_wait", std::max(1, static_cast<int>(std::ceil(blockedFor / 60.0))));
    return Loc::Get(confirm ? "duel_sure" : "duel_forfeit");
}

// One player in a list: picture, name, rank; `right` is drawn by the caller
void PlayerLine(Ui& ui, const Rect& row, const DuelPlayer& p, bool host, Color ring) {
    const Rect pic{ row.x + 12.f, row.CY() - 18.f, 36.f, 36.f };
    const std::wstring name = NameOf(p.profile);
    ProfileDraw::Avatar(ui, p.profile.AvatarUrl(), pic, p.profile, name, 1.5f, ring);
    const float textW = row.w * 0.5f;
    ui.PushClip({ pic.R() + 10.f, row.y, textW, row.h });
    ProfileDraw::Name(ui, name, { pic.R() + 10.f, row.y + 6.f, textW, 22.f }, p.profile, 13.f);
    ui.PopClip();
    float cx = pic.R() + 10.f;
    const bool plate = ProfileDraw::HasNameplate(p.profile);   // (race / results rows have it)
    cx += RankChip(ui, cx, row.y + row.h - 15.f, p.mmr, true, plate) + 6.f;
    if (host) Chip(ui, cx, row.y + row.h - 15.f, Loc::Get("duel_host"), kGold, wchar_t(0xE734), plate);
}

} // namespace

void DuelsOverlay::DrawMatchHeader(Ui& ui, const Rect& r, const Duel& duel, const std::wstring& title, const std::wstring& subtitle) {
    auto& g = ui.g;
    Card(ui, r, kDuel.WithAlpha(0.4f));
    {
        NoColorFilter keep(g);
        g.FillLinear(r, 12.f, { { 0.f, Rgba(255, 90, 60, 30) }, { 1.f, Rgba(0, 0, 0, 0) } }, { 0.f, 0.f }, { 1.f, 1.f });
    }
    const Rect h = r.Inset(18.f, 14.f);
    MenuKit::GlyphBadge(ui, { h.x, h.y, 44.f, 44.f }, L' ', kDuel, 18.f);   // just the badge: the icon goes on top
    SpeedWatchIcon(ui, { h.x, h.y, 44.f, 44.f }, 17.f, kDuel, 0.35f);
    g.Text(title, { h.x + 58.f, h.y, h.w - 58.f, 24.f }, kText, Mono(15.f, true).Ellipsis());
    g.Text(subtitle, { h.x + 58.f, h.y + 24.f, h.w - 58.f, 18.f }, kSub, Mono(9.5f).Ellipsis());
    float cx = h.x + 58.f;
    const float cy = h.B() - 8.f;
    cx += Chip(ui, cx, cy, DuelModeLabel(duel.slots), kDuel, kDuelGlyph) + 6.f;
    cx += Chip(ui, cx, cy, DuelTypeText(duel), kSub) + 6.f;
    if (duel.friendsOnly) cx += Chip(ui, cx, cy, Loc::Get("duel_vis_friends_short"), kOnline, wchar_t(0xE716)) + 6.f;
    Chip(ui, cx, cy, L"#" + duel.code, kSub);
}

void DuelsOverlay::DrawMatch(Ui& ui, const Rect& r, const Duel& duel) {
    switch (duel.status) {
    case Duel::Status::Open: DrawWaiting(ui, r, duel); break;
    case Duel::Status::Banning: DrawBans(ui, r, duel); break;
    case Duel::Status::Running: DrawRace(ui, r, duel); break;
    default: DrawResult(ui, r, duel); break;
    }
}

// ── The room ────────────────────────────────────────────────────────────────
//   [ header: ROOM n/slots · closes in m:ss · mode / category chips                          ]
//   [ seat ][ seat ][ invited… ][ + ]   (the host can kick people / withdraw invites)
//   [ SETTINGS | INVITE PEOPLE ]
//   [ the tab ........................................................................ ]
//   [ hint                                               START (n/slots) ] [ LEAVE / CLOSE ]

void DuelsOverlay::DrawWaiting(Ui& ui, const Rect& r, const Duel& duel) {
    auto& g = ui.g;
    auto& duels = DuelsService::Get();
    const std::wstring& me = AccountService::Get().UserId();
    const bool host = duel.hostId == me;
    DrawMatchHeader(ui, r.Top(104.f), duel, Loc::Fmt("duel_room_title", duel.players.size(), duel.slots),
                    Loc::Fmt("duel_waiting_sub", Clock(duel.LobbySecondsLeft())));
    ui.KeepAnimatingThrottled();

    // Seats: the players (by seat), then the invites still pending, then the free ones
    std::vector<const DuelPlayer*> seated;
    for (const auto& p : duel.players) seated.push_back(&p);
    std::sort(seated.begin(), seated.end(), [](const DuelPlayer* a, const DuelPlayer* b) { return a->seat < b->seat; });
    const int cells = duel.slots;
    const float gap = 10.f;
    const float cw = (r.w - gap * (cells - 1)) / cells;
    // The banner keeps the profile card's proportions (width × 0.36) so it shows whole
    const float bannerH = std::max(30.f, cw * 0.36f);
    const float side = std::min(46.f, cw - 24.f);
    const Rect strip{ r.x, r.y + 116.f, r.w, bannerH + side * 0.5f + 60.f };
    size_t pending = 0;
    for (int i = 0; i < cells; ++i) {
        StaggerItem in(ui, matchAt + 0.05, i, 0.f, 10.f, 0.05, 0.3);
        const Rect cell{ strip.x + i * (cw + gap), strip.y, cw, strip.h };
        if (i < static_cast<int>(seated.size())) {
            const DuelPlayer& p = *seated[i];
            const int64_t key = static_cast<int64_t>(std::hash<std::wstring>{}(p.profile.userId));
            const Color rc = RankColor(p.mmr);
            const Id seatId = ui.MakeId("duel-seat-profile", key);
            if (const Interaction seat = ui.Interact(seatId, cell); seat.clicked) main.ShowProfile(p.profile);
            else if (seat.hovered) ui.Tooltip(seatId, Loc::Get("acc_rank_open_profile"));
            {
                NoColorFilter keep(g);
                g.FillRound(cell, 12.f, ProfileDraw::CardColor(p.profile));
            }
            g.PushRoundClip(cell, 12.f);
            ProfileDraw::Banner(ui, p.profile.BannerUrl(), cell.Top(bannerH), p.profile, 0.f);
            g.PopLayer();
            g.StrokeRound(cell, 12.f, p.profile.userId == me ? kDuel : rc.WithAlpha(0.55f), 1.f);
            ProfileDraw::ProfileFrame(ui, cell, 12.f, p.profile);
            const Rect pic{ cell.CX() - side * 0.5f, cell.y + bannerH - side * 0.5f, side, side };
            const std::wstring name = NameOf(p.profile);
            ProfileDraw::Avatar(ui, p.profile.AvatarUrl(), pic, p.profile, name, 2.f, rc);
            ui.PushClip({ cell.x + 6.f, pic.B() + 4.f, cell.w - 12.f, 22.f });
            ProfileDraw::Name(ui, name, { cell.x + 6.f, pic.B() + 4.f, cell.w - 12.f, 22.f }, p.profile, 12.f, HAlign::Center);
            ui.PopClip();
            // Rank icon + MMR
            const std::wstring mmr = std::to_wstring(p.mmr) + L" MMR";
            const float tw = g.Measure(mmr, Mono(9.f, true)).w;
            const float rx = cell.CX() - (tw + 22.f) * 0.5f;
            RankIcon(ui, { rx, pic.B() + 29.f, 18.f, 18.f }, p.mmr);
            g.Text(mmr, { rx + 22.f, pic.B() + 28.f, tw + 2.f, 20.f }, rc, Mono(9.f, true));
            if (p.profile.userId == duel.hostId) {
                g.FillEllipse(cell.x + 16.f, cell.y + 16.f, 11.f, 11.f, Rgba(0, 0, 0, 140));
                UI::Glyph(ui, wchar_t(0xE734), { cell.x + 5.f, cell.y + 5.f, 22.f, 22.f }, 11.f, kGold);
            }
            // Kick (host, someone else): click twice
            if (host && p.profile.userId != me) {
                const bool confirming = confirmKick == p.profile.userId;
                const Rect kick{ cell.R() - 30.f, cell.y + 6.f, 24.f, 24.f };
                if (IconButton(ui, ui.MakeId("duel-kick", key), kick, confirming ? Glyphs::CheckMark : Glyphs::Cancel, MenuKit::Kind::Danger, !duels.Busy(),
                               Loc::Fmt(confirming ? "duel_kick_confirm" : "duel_kick", name))) {
                    if (!confirming) {
                        confirmKick = p.profile.userId;
                    } else {
                        confirmKick.clear();
                        duels.Kick(duel.id, p.profile.userId, Reply(Loc::Fmt("duel_kicked", name)));
                    }
                }
                if (confirming && ui.In().pressed[0] && !ui.MouseOver(kick)) confirmKick.clear();
            }
        } else if (pending < duel.invited.size()) {
            // Invited, not in yet
            const Profile& p = duel.invited[pending++];
            const int64_t key = static_cast<int64_t>(std::hash<std::wstring>{}(p.userId));
            const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(ui.Time()) * 2.4f + i);
            g.Border(cell, 12.f, kCard.WithAlpha(0.6f), kGold.WithAlpha(0.25f + 0.35f * pulse), 1.f);
            const Rect pic{ cell.CX() - side * 0.5f, cell.y + bannerH - side * 0.5f, side, side };
            const std::wstring name = NameOf(p);
            g.PushOpacity(0.55f);
            ProfileDraw::Avatar(ui, p.AvatarUrl(), pic, p, name);
            ui.PushClip({ cell.x + 6.f, pic.B() + 4.f, cell.w - 12.f, 22.f });
            ProfileDraw::Name(ui, name, { cell.x + 6.f, pic.B() + 4.f, cell.w - 12.f, 22.f }, p, 12.f, HAlign::Center);
            ui.PopClip();
            g.PopLayer();
            g.Text(Loc::Get("duel_seat_invited"), { cell.x + 4.f, pic.B() + 28.f, cell.w - 8.f, 20.f }, kGold, Mono(9.f, true).Center().Ellipsis());
            if (host && IconButton(ui, ui.MakeId("duel-uninvite", key), { cell.R() - 30.f, cell.y + 6.f, 24.f, 24.f }, Glyphs::Cancel, MenuKit::Kind::Secondary,
                                   !duels.Busy(), Loc::Get("duel_invite_cancel")))
                duels.Kick(duel.id, p.userId, Reply());
        } else {
            // Free seat: invite someone
            const Id id = ui.MakeId("duel-seat-free", i);
            const Interaction it = ui.Interact(id, cell);
            const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 16.f);
            const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(ui.Time()) * 2.4f + i);
            g.Border(cell, 12.f, Lerp(kCard.WithAlpha(0.5f), kRowHover, hover), Lerp(kCardBorder.WithAlpha(0.35f + 0.35f * pulse), kDuel, hover), 1.f);
            UI::Glyph(ui, wchar_t(0xE8FA), { cell.CX() - 15.f, cell.CY() - 26.f, 30.f, 30.f }, 16.f, Lerp(kSub, kDuel, hover));
            g.Text(Loc::Get("duel_seat_empty"), { cell.x + 4.f, cell.CY() + 8.f, cell.w - 8.f, 18.f }, kSub, Mono(9.f, true).Center().Ellipsis());
            if (it.hovered) {
                ui.SetCursor(IDC_HAND);
                ui.Tooltip(id, Loc::Get("duel_invite_btn"));
            }
            if (it.clicked && roomTab != 1) {
                roomTab = 1;
                roomTabAt = Async::Now();
            }
        }
    }

    // Tabs: settings · invite people
    const Rect tabs{ r.x, strip.B() + 14.f, std::min(r.w, 360.f), 32.f };
    {
        std::wstring inviteLabel = Loc::Get("duel_tab_invite");
        if (!duel.invited.empty()) inviteLabel += L" (" + std::to_wstring(duel.invited.size()) + L")";
        const int clicked = Pills(ui, tabs, { Loc::Get("duel_tab_settings"), inviteLabel }, roomTab, "duel-room-tab");
        if (clicked >= 0) {
            roomTab = clicked;
            roomTabAt = Async::Now();
        }
    }
    const Rect panel{ r.x, tabs.B() + 12.f, r.w, r.B() - 56.f - tabs.B() - 12.f };
    {
        StaggerItem in(ui, roomTabAt, 0, 0.f, 8.f);
        if (roomTab == 1) DrawRoomInvite(ui, panel, duel);
        else DrawRoomSettings(ui, panel, duel);
    }

    // Bottom: hint · START (host) · LEAVE / CLOSE
    const Rect bottom{ r.x, r.B() - 44.f, r.w, 44.f };
    const std::wstring label = Loc::Get(confirmLeave ? "duel_sure" : host ? "duel_close_lobby" : "duel_leave");
    const float bw = MenuKit::FooterButtonWidth(ui, label, Glyphs::Cancel, 140.f);
    const Rect leave{ bottom.R() - bw, bottom.y, bw, bottom.h };
    float hintRight = leave.x - 14.f;
    if (host) {
        const int n = static_cast<int>(duel.players.size());
        const std::wstring start = Loc::Fmt("duel_start_now", n, duel.slots);
        const float sw = MenuKit::FooterButtonWidth(ui, start, Glyphs::Play, 170.f);
        const Rect go{ leave.x - 10.f - sw, bottom.y, sw, bottom.h };
        hintRight = go.x - 14.f;
        const Id startId = ui.MakeId("duel-start-now");
        if (MenuKit::FooterButton(ui, startId, go, start, MenuKit::Kind::Primary, n >= 2 && !duels.Busy(), Glyphs::Play, kDuel))
            duels.StartNow(duel.id, Reply());
        if (n < 2) ui.Tooltip(startId, Loc::Get("duel_err_need_players"));
    }
    TextStyle hint = Mono(9.f);
    hint.wrap = true;
    g.Text(Loc::Get(host ? "duel_room_hint_host" : "duel_room_hint_guest"), { bottom.x, bottom.y, std::max(0.f, hintRight - bottom.x), bottom.h }, kSub, hint);
    if (MenuKit::FooterButton(ui, ui.MakeId("duel-leave"), leave, label, MenuKit::Kind::Danger, !duels.Busy(), Glyphs::Cancel)) {
        if (!confirmLeave) {
            confirmLeave = true;
        } else {
            confirmLeave = false;
            duels.Leave(duel.id, Reply(Loc::Get(host ? "duel_lobby_closed" : "duel_left")));
        }
    }
    if (confirmLeave && ui.In().pressed[0] && !ui.MouseOver(leave)) confirmLeave = false;
}

// ── Ban phase ───────────────────────────────────────────────────────────────

void DuelsOverlay::DrawBans(Ui& ui, const Rect& r, const Duel& duel) {
    auto& g = ui.g;
    auto& duels = DuelsService::Get();
    const DuelPlayer* turn = duel.BanTurn();
    const bool mine = duels.MyTurnToBan();
    const size_t step = std::min(duel.banned.size() + 1, duel.banOrder.size());
    DrawMatchHeader(ui, r.Top(104.f), duel, Loc::Fmt("duel_bans_title", step, duel.banOrder.size()),
                    mine ? Loc::Get("duel_bans_your_turn") : turn ? Loc::Fmt("duel_bans_their_turn", NameOf(turn->profile)) : L"");

    // Whose turn: the players in order, the current one lit
    float y = r.y + 116.f;
    {
        float x = r.x;
        for (const auto& p : duel.players) {
            const bool now = turn && turn->profile.userId == p.profile.userId;
            const std::wstring name = NameOf(p.profile);
            const float w = std::min(200.f, ProfileDraw::NameWidth(ui, name, p.profile, 11.f) + 48.f);
            if (x + w > r.R()) break;
            const Rect pill{ x, y, w, 30.f };
            const float glow = now ? 0.6f + 0.4f * std::sin(static_cast<float>(ui.Time()) * 4.f) : 0.f;
            if (now) {
                g.GlowRound(pill, 15.f, kDuel, 10.f, 0.3f * glow);
                ui.KeepAnimatingThrottled();
            }
            g.Border(pill, 15.f, now ? kDuel.WithAlpha(0.16f) : kRow, now ? kDuel : p.forfeited ? kDanger.WithAlpha(0.4f) : kCardBorder, 1.f);
            const Rect pic{ pill.x + 4.f, pill.y + 3.f, 24.f, 24.f };
            ProfileDraw::Avatar(ui, p.profile.AvatarUrl(), pic, p.profile, name);
            ui.PushClip({ pic.R() + 8.f, pill.y, w - 40.f, pill.h });
            ProfileDraw::Name(ui, name, { pic.R() + 8.f, pill.y, w - 40.f, pill.h }, p.profile, 11.f);
            ui.PopClip();
            x += w + 8.f;
        }
        y += 44.f;
    }

    // The categories by chapter (5 columns); the remaining ones can be banned on your turn
    const Rect area{ r.x, y, r.w, r.B() - y - 56.f };
    const float gap = 10.f;
    const float cw = (area.w - gap * 4) / 5;
    for (int chapter = 1; chapter <= 5; ++chapter) {
        const float x = area.x + (chapter - 1) * (cw + gap);
        g.Text(Loc::Fmt("fr_chapter", chapter), { x, area.y, cw, 18.f }, kText, Mono(10.f, true).Center());
        float ty = area.y + 24.f;
        int index = 0;
        for (const auto& c : Services::Account::DuelCategories()) {
            if (c.chapter != chapter) continue;
            StaggerItem in(ui, matchAt + 0.03, (chapter - 1) * 2 + index++, 0.f, 6.f, 0.02, 0.24);
            const Rect tile{ x, ty, cw, 44.f };
            ty += 50.f;
            const bool banned = duel.IsBanned(c.id);
            const Id id = ui.MakeId("duel-ban", static_cast<int64_t>(std::hash<std::string>{}(c.id)));
            const bool can = mine && !banned && !duels.Busy();
            const Interaction it = ui.Interact(id, tile, can);
            const float hover = ui.Animate(HashId(id, 1), it.hovered && can ? 1.f : 0.f, 16.f);
            const float gone = ui.Animate(HashId(id, 2), banned ? 1.f : 0.f, 10.f);
            g.Border(tile, 9.f, Lerp(Lerp(kRow, kDanger.WithAlpha(0.18f), hover), kCard.WithAlpha(0.5f), gone),
                     Lerp(Lerp(mine ? kCardBorder : kCardBorder.WithAlpha(0.5f), kDanger, hover), kDanger.WithAlpha(0.35f), gone), 1.f);
            TextStyle st = Mono(9.5f, true).Center();
            st.wrap = true;
            const Color tc = Lerp(Lerp(mine ? kText : kSub, Colors::White, hover), kSub.WithAlpha(0.5f), gone);
            g.Text(c.label, tile.Inset(6.f, 2.f), tc, st);
            if (gone > 0.01f) {
                g.Line(tile.x + 10.f, tile.CY(), tile.x + 10.f + (tile.w - 20.f) * gone, tile.CY(), kDanger.WithAlpha(0.8f), 2.f, true);
                if (gone > 0.9f) UI::Glyph(ui, Glyphs::Cancel, { tile.R() - 18.f, tile.y + 2.f, 14.f, 14.f }, 7.f, kDanger);
            }
            if (it.hovered && can) {
                ui.SetCursor(IDC_HAND);
                ui.Tooltip(id, Loc::Get("duel_ban_tooltip"));
            }
            if (it.clicked && can) duels.Ban(duel.id, c.id, Reply());
        }
    }

    // Bottom: what's left + forfeit
    const Rect bottom{ r.x, r.B() - 44.f, r.w, 44.f };
    const double blocked = duels.ForfeitBlockedFor();
    if (blocked > 0.0) ui.KeepAnimatingThrottled();
    const std::wstring label = ForfeitLabel(confirmForfeit, blocked);
    const float bw = MenuKit::FooterButtonWidth(ui, label, wchar_t(0xE7C1), 140.f);
    const Rect forfeit{ bottom.R() - bw, bottom.y, bw, bottom.h };
    g.Text(Loc::Fmt("duel_bans_left", Services::Account::DuelCategories().size() - duel.banned.size(), duel.banOrder.size() - duel.banned.size()),
           { bottom.x, bottom.y, forfeit.x - bottom.x - 14.f, bottom.h }, kSub, Mono(9.5f).Ellipsis());
    if (blocked > 0.0 && ui.MouseOver(forfeit)) ui.Tooltip(ui.MakeId("duel-ban-forfeit"), Loc::Get("duel_forfeit_blocked"));
    if (MenuKit::FooterButton(ui, ui.MakeId("duel-ban-forfeit"), forfeit, label, MenuKit::Kind::Danger, !duels.Busy() && blocked <= 0.0, wchar_t(0xE7C1))) {
        if (!confirmForfeit) {
            confirmForfeit = true;
        } else {
            confirmForfeit = false;
            duels.Forfeit(duel.id, Reply(Loc::Get("duel_forfeited")));
        }
    }
    if (confirmForfeit && ui.In().pressed[0] && !ui.MouseOver(forfeit)) confirmForfeit = false;
}

// ── The race ────────────────────────────────────────────────────────────────

void DuelsOverlay::DrawRace(Ui& ui, const Rect& r, const Duel& duel) {
    auto& g = ui.g;
    auto& duels = DuelsService::Get();
    const std::wstring& meId = AccountService::Get().UserId();
    const DuelPlayer* me = duel.Player(meId);
    const auto* category = Services::Account::FindDuelCategory(duel.category);

    // What to run, big, with the clock since the start
    const Rect hero = r.Top(150.f);
    Card(ui, hero, kDuel.WithAlpha(0.5f));
    {
        NoColorFilter keep(g);
        g.FillLinear(hero, 12.f, { { 0.f, Rgba(255, 90, 60, 40) }, { 1.f, Rgba(0, 0, 0, 0) } }, { 0.f, 0.f }, { 1.f, 1.f });
    }
    const Rect h = hero.Inset(22.f, 16.f);
    g.Text(Loc::Get("duel_race_title"), { h.x, h.y, h.w * 0.6f, 16.f }, kDuel, Mono(9.5f, true));
    g.Text(Loc::Fmt("fr_chapter", duel.chapter) + L" \x00B7 " + ChapterName(duel.chapter), { h.x, h.y + 20.f, h.w * 0.62f, 22.f }, kText,
           Mono(13.f, true).Ellipsis());
    g.Text(category ? category->label : L"\x2014", { h.x, h.y + 44.f, h.w * 0.62f, 40.f }, kGold, Mono(24.f, true).Ellipsis());
    const std::wstring clock = Clock(duel.RunningSeconds(), true);
    g.Text(Loc::Get("duel_race_elapsed"), { h.R() - 220.f, h.y, 220.f, 16.f }, kSub, Mono(9.f, true).Right());
    g.Text(clock, { h.R() - 260.f, h.y + 16.f, 260.f, 44.f }, kText, Mono(26.f, true).Right());
    ui.KeepAnimatingThrottled();
    // Play the chapter from here
    int chapterIndex = -1;
    const auto& chapters = App::L().chapters;
    for (size_t i = 0; i < chapters.size(); ++i)
        if (chapters[i].number == duel.chapter) chapterIndex = static_cast<int>(i);
    {
        const std::wstring play = Loc::Fmt("duel_play_chapter", duel.chapter);
        const float pw = MenuKit::FooterButtonWidth(ui, play, Glyphs::Play, 160.f);
        if (chapterIndex >= 0 && MenuKit::FooterButton(ui, ui.MakeId("duel-play"), { h.x, h.B() - 34.f, pw, 34.f }, play, MenuKit::Kind::Secondary, true, Glyphs::Play,
                                                       kDuel)) {
            Close();
            main.SelectChapter(chapterIndex);
            main.PlaySelected();
        }
    }

    // Everyone's status
    const float listTop = hero.B() + 14.f;
    const Rect submit{ r.x, r.B() - 96.f, r.w, 96.f };
    const Rect area{ r.x, listTop, r.w, submit.y - listTop - 12.f };
    Card(ui, area);
    const Rect inner = area.Inset(12.f, 10.f);
    {   // the scroll's clip must end before the time box below
    ScrollArea scroll(ui, ui.MakeId("duel-race-list"), inner);
    float y = inner.y;
    int index = 0;
    for (const DuelPlayer* p : duel.Standings()) {
        StaggerItem in(ui, matchAt + 0.05, index++, 0.f, 6.f, 0.04, 0.28);
        const Rect row{ inner.x, y, inner.w - 10.f, 52.f };
        y += 56.f;
        const bool mine = p->profile.userId == meId;
        g.Border(row, 10.f, mine ? kDuel.WithAlpha(0.08f) : kRow, mine ? kDuel.WithAlpha(0.5f) : kCardBorder.WithAlpha(0.4f), 1.f);
        if (const Interaction it = ui.Interact(ui.MakeId("duel-race-row", static_cast<int64_t>(index)), row); it.clicked) main.ShowProfile(p->profile);
        ProfileDraw::Nameplate(ui, row, 10.f, p->profile);
        PlayerLine(ui, row, *p, p->profile.userId == duel.hostId, RankColor(p->mmr));
        std::wstring right;
        Color color = kSub;
        if (p->forfeited) {
            right = Loc::Get("duel_status_forfeit");
            color = kDanger;
        } else if (p->done) {
            right = FormatDuelTime(p->seconds);
            color = kOnline;
        } else if (p->Online() && p->playing == duel.chapter) {
            right = Loc::Get("duel_status_running");
            color = kGold;
        } else {
            right = Loc::Get("duel_status_waiting");
        }
        const bool plate = ProfileDraw::HasNameplate(p->profile);
        ProfileDraw::RowText(ui, right, { row.R() - 220.f, row.y, 206.f, row.h }, plate && right == Loc::Get("duel_status_waiting") ? kText : color,
                             Mono(p->done && !p->forfeited ? 14.f : 10.f, true).Right(), plate);
    }
    scroll.SetContentSize(y - inner.y);
    }

    // Send your time / forfeit
    Card(ui, submit, kDuel.WithAlpha(0.3f));
    const Rect s = submit.Inset(16.f, 12.f);
    if (!me) return;
    if (me->done) {
        const std::wstring text = me->forfeited ? Loc::Get("duel_you_forfeited") : Loc::Fmt("duel_your_time", FormatDuelTime(me->seconds));
        g.Text(text, { s.x, s.y, s.w, 30.f }, me->forfeited ? kDanger : kOnline, Mono(13.f, true));
        g.Text(Loc::Get("duel_waiting_others"), { s.x, s.y + 32.f, s.w, 20.f }, kSub, Mono(9.5f));
        Form::Spinner(ui, s.R() - 20.f, s.CY(), 8.f, kDuel);
        return;
    }
    Caption(ui, s.x, s.y, s.w, Loc::Get("duel_time_caption"), kDuel);
    const double blocked = duels.ForfeitBlockedFor();
    if (blocked > 0.0) ui.KeepAnimatingThrottled();
    const std::wstring forfeitLabel = ForfeitLabel(confirmForfeit, blocked);
    const float fw = MenuKit::FooterButtonWidth(ui, forfeitLabel, wchar_t(0xE7C1), 130.f);
    const std::wstring sendLabel = Loc::Get("duel_send_time");
    const float sw = MenuKit::FooterButtonWidth(ui, sendLabel, Glyphs::CheckMark, 150.f);
    const float rowY = s.y + 22.f;
    const Rect forfeit{ s.R() - fw, rowY, fw, 40.f };
    const Rect send{ forfeit.x - 10.f - sw, rowY, sw, 40.f };
    const Rect field{ s.x, rowY, std::max(140.f, send.x - 12.f - s.x), 40.f };
    TextBoxStyle style = Field(false, 14);
    style.placeholder = L"1:23.456";
    style.text = Mono(14.f, true);
    style.focusBorder = kDuel;
    style.caret = kDuel;
    bool enter = false;
    Form::Field(ui, ui.MakeId("duel-time"), field, timeText, style, kDuel);
    if (ui.Focus() == ui.MakeId("duel-time") && ui.KeyPressed(VK_RETURN)) enter = true;
    const auto parsed = Services::Account::ParseDuelTime(timeText);
    const bool typed = !Text::Trim(timeText).empty();
    if (typed && !parsed) g.Text(Loc::Get("duel_time_invalid"), { field.x, field.B() + 2.f, field.w, 14.f }, kDanger, Mono(8.5f));
    else if (parsed) g.Text(L"= " + FormatDuelTime(*parsed), { field.x, field.B() + 2.f, field.w, 14.f }, kSub, Mono(8.5f));
    if ((MenuKit::FooterButton(ui, ui.MakeId("duel-send"), send, sendLabel, MenuKit::Kind::Primary, parsed.has_value() && !duels.Busy(), Glyphs::CheckMark, kDuel) ||
         (enter && parsed && !duels.Busy()))) {
        const std::wstring shown = FormatDuelTime(*parsed);
        duels.Submit(duel.id, *parsed, Reply(Loc::Fmt("duel_time_sent", shown)));
    }
    if (blocked > 0.0 && ui.MouseOver(forfeit)) ui.Tooltip(ui.MakeId("duel-forfeit"), Loc::Get("duel_forfeit_blocked"));
    if (MenuKit::FooterButton(ui, ui.MakeId("duel-forfeit"), forfeit, forfeitLabel, MenuKit::Kind::Danger, !duels.Busy() && blocked <= 0.0, wchar_t(0xE7C1))) {
        if (!confirmForfeit) {
            confirmForfeit = true;
        } else {
            confirmForfeit = false;
            duels.Forfeit(duel.id, Reply(Loc::Get("duel_forfeited")));
        }
    }
    if (confirmForfeit && ui.In().pressed[0] && !ui.MouseOver(forfeit)) confirmForfeit = false;
}

// ── Results ─────────────────────────────────────────────────────────────────

void DuelsOverlay::DrawResult(Ui& ui, const Rect& r, const Duel& duel) {
    auto& g = ui.g;
    auto& duels = DuelsService::Get();
    const std::wstring& meId = AccountService::Get().UserId();
    const DuelPlayer* me = duel.Player(meId);
    const bool won = me && me->placement == 1;
    const auto* category = Services::Account::FindDuelCategory(duel.category);

    // Banner: won / placement + what was run
    const Rect hero = r.Top(130.f);
    const Color accent = won ? kGold : kDuel;
    Card(ui, hero, accent.WithAlpha(0.55f));
    {
        NoColorFilter keep(g);
        g.FillLinear(hero, 12.f, { { 0.f, accent.WithAlpha(0.2f) }, { 1.f, Rgba(0, 0, 0, 0) } }, { 0.f, 0.f }, { 1.f, 1.f });
    }
    const Rect h = hero.Inset(22.f, 16.f);
    MenuKit::GlyphBadge(ui, { h.x, h.y + 6.f, 56.f, 56.f }, won ? wchar_t(0xE734) : kDuelGlyph, accent, 22.f);
    const std::wstring title = !me ? Loc::Get("duel_result_title") : won ? Loc::Get("duel_result_won") : Loc::Fmt("duel_result_place", Ordinal(me->placement));
    g.Text(title, { h.x + 72.f, h.y, h.w - 72.f, 34.f }, won ? kGold : kText, Mono(22.f, true).Ellipsis());
    g.Text(Loc::Fmt("fr_chapter", duel.chapter) + L" \x00B7 " + ChapterName(duel.chapter) + L"  \x00B7  " + (category ? category->label : L"\x2014"),
           { h.x + 72.f, h.y + 36.f, h.w - 72.f, 18.f }, kSub, Mono(10.f, true).Ellipsis());
    if (me) {
        const int after = me->mmrBefore + me->mmrChange;
        const std::wstring change = (me->mmrChange >= 0 ? L"+" : L"") + std::to_wstring(me->mmrChange) + L" MMR";
        float cx = h.x + 72.f;
        cx += Chip(ui, cx, h.y + 72.f, change, me->mmrChange >= 0 ? kOnline : kDanger) + 8.f;
        cx += RankChip(ui, cx, h.y + 72.f, after) + 8.f;
        const auto& before = Services::Account::DuelRankFor(me->mmrBefore);
        const auto& now = Services::Account::DuelRankFor(after);
        if (&before != &now)
            Chip(ui, cx, h.y + 72.f, Loc::Get(after > me->mmrBefore ? "duel_rank_up" : "duel_rank_down"), after > me->mmrBefore ? kGold : kDanger,
                 after > me->mmrBefore ? wchar_t(0xE70E) : wchar_t(0xE70D));
    }

    // Standings
    const Rect area{ r.x, hero.B() + 14.f, r.w, r.h - hero.h - 14.f - 56.f };
    Card(ui, area);
    const Rect inner = area.Inset(12.f, 10.f);
    {   // the scroll's clip must end before the buttons below
    ScrollArea scroll(ui, ui.MakeId("duel-result-list"), inner);
    float y = inner.y;
    int index = 0;
    for (const DuelPlayer* p : duel.Standings()) {
        StaggerItem in(ui, matchAt + 0.1, index++, -16.f, 0.f, 0.08, 0.34);
        const Rect row{ inner.x, y, inner.w - 10.f, 56.f };
        y += 60.f;
        const bool mine = p->profile.userId == meId;
        const Color pc = PlaceColor(p->placement);
        g.Border(row, 10.f, mine ? kDuel.WithAlpha(0.08f) : kRow, p->placement <= 3 ? pc.WithAlpha(0.5f) : kCardBorder.WithAlpha(0.4f), 1.f);
        if (const Interaction it = ui.Interact(ui.MakeId("duel-result-row", static_cast<int64_t>(index)), row); it.clicked) main.ShowProfile(p->profile);
        ProfileDraw::Nameplate(ui, row, 10.f, p->profile);
        const Rect medal{ row.x + 10.f, row.CY() - 15.f, 30.f, 30.f };
        g.FillEllipse(medal.CX(), medal.CY(), 15.f, 15.f, pc.WithAlpha(0.18f));
        g.Text(std::to_wstring(p->placement), medal, pc, Mono(12.f, true).Center());
        PlayerLine(ui, row.CutLeft(38.f), *p, false, RankColor(p->mmrBefore + p->mmrChange));
        const std::wstring time = p->forfeited ? Loc::Get("duel_status_forfeit") : p->seconds == 0.0 ? Loc::Get("duel_last_standing") : FormatDuelTime(p->seconds);
        const bool plate = ProfileDraw::HasNameplate(p->profile);
        ProfileDraw::RowText(ui, time, { row.R() - 300.f, row.y, 170.f, row.h }, p->forfeited ? kDanger : kText,
                             Mono(p->forfeited || p->seconds == 0.0 ? 10.f : 13.f, true).Right(), plate);
        const std::wstring change = (p->mmrChange >= 0 ? L"+" : L"") + std::to_wstring(p->mmrChange);
        ProfileDraw::RowText(ui, change, { row.R() - 120.f, row.y + 6.f, 106.f, 24.f }, p->mmrChange >= 0 ? kOnline : kDanger, Mono(13.f, true).Right(), plate);
        ProfileDraw::RowText(ui, std::to_wstring(p->mmrBefore + p->mmrChange) + L" MMR", { row.R() - 120.f, row.y + 28.f, 106.f, 18.f }, plate ? kText : kSub,
                             Mono(9.f, true).Right(), plate);
    }
    scroll.SetContentSize(y - inner.y);
    }

    // Bottom: close / another one
    const Rect bottom{ r.x, r.B() - 44.f, r.w, 44.f };
    const std::wstring again = Loc::Get("duel_again");
    const float aw = MenuKit::FooterButtonWidth(ui, again, wchar_t(0xE710), 150.f);
    const std::wstring close = Loc::Get("duel_result_close");
    const float cw = MenuKit::FooterButtonWidth(ui, close, Glyphs::CheckMark, 120.f);
    if (!duels.Active() && MenuKit::FooterButton(ui, ui.MakeId("duel-again"), { bottom.R() - aw, bottom.y, aw, bottom.h }, again, MenuKit::Kind::Primary, true,
                                                 wchar_t(0xE710), kDuel)) {
        duels.Dismiss(duel.id);
        CreateRoom();
    }
    if (MenuKit::FooterButton(ui, ui.MakeId("duel-result-close"), { bottom.R() - aw - 10.f - cw, bottom.y, cw, bottom.h }, close, MenuKit::Kind::Secondary, true,
                              Glyphs::CheckMark, kDuel)) {
        duels.Dismiss(duel.id);
        SetPage(0);
    }
}

} // namespace SpeedrunLauncher::Main
