#include "Windows/Main/Overlays/Duels/DuelsOverlay.h"
#include <algorithm>
#include <cmath>
#include "App/Paths.h"
#include "Core/Async.h"
#include "Graphics/Images.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/Account/AccountService.h"
#include "Services/Account/FriendsService.h"
#include "UI/Layout.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Main/Common/ChatView.h"
#include "Windows/Main/Common/ProfileDraw.h"
#include "Windows/Main/Common/ReportKit.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Overlays/Duels/DuelsKit.h"

namespace SpeedrunLauncher::Main {

using namespace UI;
using namespace DuelsKit;
using Services::Account::AccountService;
using Services::Account::Duel;
using Services::Account::DuelsService;

// ── Shared look (DuelsKit.h) ────────────────────────────────────────────────

namespace DuelsKit {

std::wstring NameOf(const Services::Account::Profile& p) { return p.ShownName(p.speedrunName.empty() ? Loc::Get("acc_default_name") : p.speedrunName); }

Color RankColor(int mmr) { return Hex(Services::Account::DuelRankFor(mmr).color); }

void SpeedWatchIcon(Ui& ui, const Rect& r, float size, Color color, float motion) {
    auto& g = ui.g;
    // The watch leans forward a little and sits right of centre, leaving room for the lines
    const float shift = size * 0.22f;
    const Rect watch{ r.x + shift, r.y, r.w, r.h };
    UI::Glyph(ui, wchar_t(0xE916), watch, size, color);
    const float cx = r.CX() + shift - size * 0.62f;   // where the lines end (just behind the watch)
    const float t = static_cast<float>(ui.Time());
    const float lengths[] = { 0.42f, 0.62f, 0.34f };
    const float rows[] = { -0.26f, 0.04f, 0.32f };
    for (int i = 0; i < 3; ++i) {
        const float flicker = motion > 0.01f ? 0.12f * std::sin(t * 18.f + i * 2.1f) * motion : 0.f;
        const float len = size * (lengths[i] * (1.f + 0.45f * motion) + flicker);
        const float y = r.CY() + size * rows[i];
        g.Line(cx - len, y, cx, y, color.WithAlpha(0.55f + 0.35f * motion - 0.12f * i), std::max(1.5f, size * 0.11f), true);
    }
    if (motion > 0.01f) ui.KeepAnimating();
}

void RankIconOf(Ui& ui, const Rect& r, const Services::Account::DuelRank& rank, float opacity) {
    auto img = Images::Load(Paths::Asset(rank.icon), 64);
    if (!img) {
        ui.g.FillRound(r.Inset(r.w * 0.15f), 4.f, Hex(rank.color).WithAlpha(opacity));
        return;
    }
    NoColorFilter keep(ui.g);
    ui.g.DrawImage(*img, r, opacity);
}

void RankIcon(Ui& ui, const Rect& r, int mmr, float opacity) { RankIconOf(ui, r, Services::Account::DuelRankFor(mmr), opacity); }

// The chips' body: see-through, or solid dark on a nameplate (returns the text colour to use)
Color ChipBody(Ui& ui, const Rect& r, Color color, bool onPlate) {
    auto& g = ui.g;
    if (!onPlate) {
        g.Border(r, r.h * 0.5f, color.WithAlpha(0.12f), color.WithAlpha(0.32f), 1.f);
        return color;
    }
    NoColorFilter keep(g);
    g.GlowRound(r, r.h * 0.5f, Colors::Black, 6.f, 0.4f);
    g.Border(r, r.h * 0.5f, Lerp(Hex(0x060C14), color, 0.12f).WithAlpha(0.92f), color.WithAlpha(0.75f), 1.f);
    return Lerp(color, Colors::White, 0.15f);
}

float Chip(Ui& ui, float x, float cy, const std::wstring& text, Color color, wchar_t glyph, bool onPlate) {
    auto& g = ui.g;
    const TextStyle st = Mono(9.f, true);
    const float tw = g.Measure(text, st).w;
    const float w = tw + 14.f + (glyph ? 15.f : 0.f);
    const Rect r{ x, cy - 9.f, w, 18.f };
    color = ChipBody(ui, r, color, onPlate);
    float tx = r.x + 7.f;
    if (glyph) {
        UI::Glyph(ui, glyph, { tx - 2.f, r.y, 14.f, r.h }, 8.5f, color);
        tx += 15.f;
    }
    g.Text(text, { tx, r.y, tw + 2.f, r.h }, color, st);
    return w;
}

float RankChip(Ui& ui, float x, float cy, int mmr, bool withMmr, bool onPlate) {
    auto& g = ui.g;
    const auto& rank = Services::Account::DuelRankFor(mmr);
    Color color = Hex(rank.color);
    std::wstring text = Text::Widen(rank.name);
    if (withMmr) text += L" \x00B7 " + std::to_wstring(mmr);
    const TextStyle st = Mono(9.f, true);
    const float tw = g.Measure(text, st).w;
    const float w = tw + 14.f + 20.f;
    const Rect r{ x, cy - 10.f, w, 20.f };
    color = ChipBody(ui, r, color, onPlate);
    RankIconOf(ui, { r.x + 4.f, r.y + 2.f, 16.f, 16.f }, rank);
    g.Text(text, { r.x + 24.f, r.y, tw + 2.f, r.h }, color, st);
    return w;
}

int Pills(Ui& ui, const Rect& r, const std::vector<std::wstring>& labels, int selected, std::string_view idBase, Color accent, float minWidth) {
    auto& g = ui.g;
    const float gap = 8.f;
    const float w = std::max(minWidth, (r.w - gap * (labels.size() - 1)) / labels.size());
    int clicked = -1;
    for (size_t i = 0; i < labels.size(); ++i) {
        const Rect t{ r.x + i * (w + gap), r.y, w, r.h };
        const Id id = ui.MakeId(idBase, static_cast<int64_t>(i));
        const Interaction it = ui.Interact(id, t);
        const bool sel = static_cast<int>(i) == selected;
        const float hover = ui.Animate(HashId(id, 1), it.hovered && !sel ? 1.f : 0.f, 16.f);
        const float on = ui.Animate(HashId(id, 2), sel ? 1.f : 0.f, 12.f);
        if (on > 0.01f) g.GlowRound(t, r.h * 0.5f, accent, 12.f, 0.22f * on);
        g.Border(t, r.h * 0.5f, Lerp(Lerp(kCard, kRowHover, hover), accent.WithAlpha(0.16f), on), Lerp(Lerp(kCardBorder, kHoverBorder, hover), accent, on), 1.f);
        g.Text(labels[i], t, Lerp(Lerp(kSub, kText, hover), kText, on), Mono(10.5f, true).Center().Ellipsis());
        if (it.hovered && !sel) ui.SetCursor(IDC_HAND);
        if (it.clicked && !sel) clicked = static_cast<int>(i);
    }
    return clicked;
}

void EmptyState(Ui& ui, const Rect& r, wchar_t glyph, const std::wstring& text) {
    auto& g = ui.g;
    const float bob = 3.f * std::sin(static_cast<float>(ui.Time()) * 1.6f);
    UI::Glyph(ui, glyph, { r.CX() - 30.f, r.CY() - 60.f + bob, 60.f, 50.f }, 34.f, kCardBorder);
    ui.KeepAnimatingThrottled();
    TextStyle st = Mono(11.f, true).Center();
    st.wrap = true;
    g.Text(text, { r.x + 30.f, r.CY() - 2.f, r.w - 60.f, 44.f }, kSub, st);
}

bool IconButton(Ui& ui, Id id, const Rect& r, wchar_t glyph, MenuKit::Kind kind, bool enabled, const std::wstring& tooltip, Color accent) {
    const bool clicked = MenuKit::FooterButton(ui, id, r, L"", kind, enabled, glyph, accent);
    if (!tooltip.empty()) ui.Tooltip(id, tooltip);
    return clicked;
}

std::wstring CategoryText(const std::string& categoryId) {
    const auto* c = Services::Account::FindDuelCategory(categoryId);
    return c ? L"Ch" + std::to_wstring(c->chapter) + L" \x00B7 " + c->label : L"\x2014";
}

std::wstring DuelTypeText(const Duel& duel) {
    switch (duel.type) {
    case Duel::Type::Chapter:
        return Loc::Fmt("fr_chapter", duel.chapter) + L" \x00B7 " +
               (duel.category.empty() ? Loc::Get("duel_random_category") : std::wstring(Services::Account::FindDuelCategory(duel.category)
                                                                                            ? Services::Account::FindDuelCategory(duel.category)->label
                                                                                            : L"?"));
    case Duel::Type::Bans: return Loc::Fmt("duel_type_bans_n", duel.bans);
    default: return Loc::Get("duel_type_random_long");
    }
}

Color PlaceColor(int placement) {
    switch (placement) {
    case 1: return kGold;
    case 2: return Hex(0xC0CCD8);
    case 3: return Hex(0xD08A50);
    default: return kSub;
    }
}

} // namespace DuelsKit

namespace {

constexpr float kNavW = 232.f, kOnlineW = 268.f;
enum Page { PageLobbies, PageCreate, PageRanking, PageRules, PageMatch, PageSettings };

PanelStyle DuelsPanel() {
    PanelStyle s;
    s.title = Loc::Get("duel_title");
    s.titleColor = kDuel;
    return s;
}

} // namespace

// ── Opening / closing ───────────────────────────────────────────────────────

DuelsOverlay::~DuelsOverlay() {
    *alive = false;
    DuelsService::Get().SetWatching(false);
}

void DuelsOverlay::Open() {
    auto& duels = DuelsService::Get();
    status.clear();
    confirmForfeit = confirmLeave = false;
    page = duels.Active() || duels.Result() ? PageMatch : PageLobbies;
    pageAt = Async::Now() + 0.06;
    openedAt = pageAt;
    if (AccountService::Get().SignedIn()) {
        duels.SetWatching(true);
        duels.Refresh();
        Services::Account::FriendsService::Get().Refresh();
    }
    popup.Open();
}

void DuelsOverlay::Challenge(const Profile& who) {
    if (!popup.IsOpen()) Open();
    auto& duels = DuelsService::Get();
    // In a room already: invite them there; racing: nothing to do; else open a room with them invited
    if (duels.Active()) {
        if (duels.Active()->status == Duel::Status::Open)
            duels.Invite(duels.Active()->id, who.userId, Reply(Loc::Fmt("duel_invited", NameOf(who))));
        else
            Message(Loc::Get("duel_err_busy"), false);
        SetPage(PageMatch);
        return;
    }
    CreateRoom(&who);
}

void DuelsOverlay::CreateRoom(const Profile* invite) {
    auto& duels = DuelsService::Get();
    if (duels.Active() || duels.Busy()) return;
    Services::Account::DuelSetup s = setup;
    s.invite = invite ? invite->userId : L"";
    const std::wstring name = invite ? NameOf(*invite) : L"";
    auto token = alive;
    duels.Create(s, [this, token, name](const std::wstring& message, bool ok) {
        if (!*token) return;
        Message(ok ? (name.empty() ? Loc::Get("duel_created") : Loc::Fmt("duel_room_with", name)) : message, ok);
        if (ok) {
            roomTab = name.empty() ? 1 : 0;   // an empty room opens on "invite people"
            roomTabAt = Async::Now();
        }
    });
}

void DuelsOverlay::Close() {
    DuelsService::Get().SetWatching(false);
    popup.Close();
}

void DuelsOverlay::Message(const std::wstring& text, bool ok) {
    status = text;
    statusOk = ok;
}

DuelsService::Done DuelsOverlay::Reply(const std::wstring& success) {
    auto token = alive;
    return [this, token, success](const std::wstring& message, bool ok) {
        if (!*token) return;
        if (!ok) Message(message, false);
        else if (!success.empty()) Message(success, true);
    };
}

void DuelsOverlay::SetPage(int p) {
    if (page == p) return;
    page = p;
    pageAt = Async::Now();
    confirmForfeit = confirmLeave = false;
    if (p == PageRanking) DuelsService::Get().LoadLeaderboard();
}

// ── Frame ───────────────────────────────────────────────────────────────────

void DuelsOverlay::Draw(Ui& ui, const Rect& screen) {
    auto& duels = DuelsService::Get();
    const bool signedIn = AccountService::Get().SignedIn();
    if (popup.IsOpen()) duels.SetWatching(signedIn);
    const float width = signedIn ? std::min(1200.f, screen.w - 60.f) : 560.f;
    const float height = signedIn ? std::min(780.f, screen.h - 60.f) : 0.f;

    // A duel that starts (or finishes) while the menu is open takes over the middle
    const Duel* current = duels.Active() ? &*duels.Active() : duels.Result() ? &*duels.Result() : nullptr;
    const std::wstring key = current ? current->id + L":" + std::to_wstring(static_cast<int>(current->status)) : L"";
    if (key != shownDuel) {
        if (current && (shownDuel.empty() || shownDuel.substr(0, current->id.size()) != current->id)) {
            SetPage(PageMatch);
            sideTab = 1;   // a new duel: its chat on the right
            chatText.clear();
        }
        shownDuel = key;
        matchAt = Async::Now();
        confirmForfeit = confirmLeave = false;
        if (!current || current->status != Duel::Status::Running) timeText.clear();
    }
    if (page == PageMatch && !current) SetPage(PageLobbies);

    {
        OverlayFrame frame(ui, ui.MakeId("duels"), screen, width, height, DuelsPanel(), popup.Opacity(), popup.Scale());
        if (!signedIn) {
            DrawSignedOut(ui, frame.body);
            frame.SetContentHeight(330.f);
        } else {
            const Rect inner = frame.body.Inset(20.f, 16.f);
            const bool showOnline = inner.w > kNavW + kOnlineW + 520.f;
            const Rect nav = inner.Left(kNavW);
            const Rect online = showOnline ? inner.Right(kOnlineW) : Rect{ inner.R(), inner.y, 0.f, inner.h };
            const Rect middle{ nav.R() + 18.f, inner.y, online.x - nav.R() - 18.f - (showOnline ? 18.f : 0.f), inner.h };
            {
                StaggerItem in(ui, openedAt, 0, -20.f);
                DrawNav(ui, nav);
            }
            if (showOnline) {
                StaggerItem in(ui, openedAt, 1, 20.f);
                DrawSide(ui, online, current);
            }
            StaggerItem in(ui, page == PageMatch ? matchAt : pageAt, 0, 0.f, 12.f);
            switch (page) {
            case PageMatch:
                if (current) DrawMatch(ui, middle, *current);
                break;
            case PageRanking: DrawRanking(ui, middle); break;
            case PageRules: DrawRules(ui, middle); break;
            case PageSettings: DrawSettings(ui, middle); break;
            default: DrawLobbies(ui, middle); break;
            }
        }

        // Footer: status on the left, BACK on the right
        const Rect row = frame.footer.Inset(14.f, 0.f);
        const std::wstring back = Loc::Get("back");
        const float bw = MenuKit::FooterButtonWidth(ui, back, Glyphs::Back, 120.f);
        const float right = row.R() - bw;
        const bool escape = popup.IsOpen() && !main.ProfileShown() && ui.Focus() == 0 && ui.KeyPressed(VK_ESCAPE);
        if (MenuKit::FooterButton(ui, ui.MakeId("duel-close"), { right, row.CY() - 18.f, bw, 36.f }, back, MenuKit::Kind::Secondary, true, Glyphs::Back, kDuel) ||
            escape)
            Close();
        ReportKit::StatusPill(ui, ui.MakeId("duel-status"), { row.x, row.CY() - 18.f, std::max(0.f, right - row.x - 14.f), 36.f }, status,
                              statusOk ? Form::Status::Success : Form::Status::Error);
    }
}

void DuelsOverlay::DrawSignedOut(Ui& ui, const Rect& bodyRect) {
    auto& g = ui.g;
    const Rect body = bodyRect.Inset(30.f, 22.f);
    Column col(body.x, body.y, body.w);
    {
        StaggerItem in(ui, openedAt, 0, 0.f, 10.f);
        MenuKit::Hero(ui, col.Next(MenuKit::kHeroH, 18.f), kDuelGlyph, Loc::Get("duel_signin_title"), Loc::Get("duel_signin_sub"), kDuel);
    }
    {
        StaggerItem in(ui, openedAt, 1, 0.f, 10.f);
        const char* perks[] = { "duel_perk_race", "duel_perk_friends", "duel_perk_mmr" };
        const wchar_t glyphs[] = { kDuelGlyph, wchar_t(0xE716), wchar_t(0xE9F9) };
        const Rect box = col.Next(3 * 26.f + 20.f, 18.f);
        Card(ui, box);
        for (int i = 0; i < 3; ++i) {
            const Rect line{ box.x + 14.f, box.y + 10.f + i * 26.f, box.w - 28.f, 26.f };
            UI::Glyph(ui, glyphs[i], { line.x, line.y, 20.f, line.h }, 13.f, kDuel);
            g.Text(Loc::Get(perks[i]), { line.x + 30.f, line.y, line.w - 30.f, line.h }, kText, Mono(10.5f).Ellipsis());
        }
    }
    {
        StaggerItem in(ui, openedAt, 2, 0.f, 10.f);
        if (MenuKit::FooterButton(ui, ui.MakeId("duel-signin"), col.Next(42.f, 10.f), Loc::Get("duel_signin_btn"), MenuKit::Kind::Primary, true, wchar_t(0xE77B),
                                  kDuel)) {
            Close();
            main.OpenAccount();
        }
    }
}

// ── Left: your rank + pages ─────────────────────────────────────────────────

void DuelsOverlay::DrawNav(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    auto& duels = DuelsService::Get();
    const auto& me = AccountService::Get().Me();

    // Rank card
    const Rect card = r.Top(176.f);
    const int mmr = duels.MyMmr();
    const auto& rank = Services::Account::DuelRankFor(mmr);
    const Color rc = Hex(rank.color);
    Card(ui, card, rc.WithAlpha(0.45f));
    {
        NoColorFilter keep(g);
        g.FillLinear(card, 12.f, { { 0.f, rc.WithAlpha(0.16f) }, { 1.f, Rgba(0, 0, 0, 0) } }, { 0.f, 0.f }, { 0.f, 1.f });
    }
    const Rect c = card.Inset(14.f, 12.f);
    const auto* next = Services::Account::NextDuelRank(mmr);
    const Rect pic{ c.x, c.y, 40.f, 40.f };
    const std::wstring myName = me.ShownName(Loc::Get("acc_default_name"));
    ProfileDraw::Avatar(ui, me.AvatarUrl(), pic, me, myName, 1.5f, rc);
    if (const Id meId = ui.MakeId("duel-me-profile"); ui.Interact(meId, { pic.x, pic.y, c.w, pic.h }).clicked) main.ShowProfile(me);
    ui.PushClip({ pic.R() + 10.f, c.y, c.w - 50.f, 22.f });
    ProfileDraw::Name(ui, myName, { pic.R() + 10.f, c.y, c.w - 50.f, 22.f }, me, 13.f);
    ui.PopClip();
    g.Text(Loc::Get("duel_your_rank"), { pic.R() + 10.f, c.y + 22.f, c.w - 50.f, 16.f }, kSub, Mono(9.f, true));
    // The rank's icon (the bot's emoji), with a soft glow in its colour
    const Rect icon{ c.x, pic.B() + 8.f, 44.f, 44.f };
    g.GlowRound(icon.Inset(6.f), 10.f, rc, 14.f, 0.35f);
    RankIconOf(ui, icon, rank);
    g.Text(Text::Widen(rank.name), { icon.R() + 10.f, pic.B() + 8.f, c.w - 54.f, 26.f }, rc, Mono(16.f, true).Ellipsis());
    g.Text(std::to_wstring(mmr) + L" MMR", { icon.R() + 10.f, pic.B() + 34.f, c.w - 54.f, 18.f }, kText, Mono(11.f, true));
    if (next) RankIconOf(ui, { c.R() - 18.f, pic.B() + 36.f, 18.f, 18.f }, *next, 0.45f);   // what's next, faded
    // Progress to the next rank
    const Rect bar{ c.x, pic.B() + 58.f, c.w, 8.f };
    g.FillRound(bar, 4.f, kRow);
    const float progress = next ? std::clamp(static_cast<float>(mmr - rank.min) / static_cast<float>(next->min - rank.min), 0.f, 1.f) : 1.f;
    const float shown = ui.Animate(ui.MakeId("duel-rank-bar"), progress, 6.f, 0.f);
    if (shown > 0.01f) {
        g.GlowRound({ bar.x, bar.y, bar.w * shown, bar.h }, 4.f, rc, 8.f, 0.35f);
        g.FillRound({ bar.x, bar.y, bar.w * shown, bar.h }, 4.f, rc);
    }
    const std::wstring nextText = next ? Loc::Fmt("duel_next_rank", next->min - mmr, Text::Widen(next->name)) : Loc::Get("duel_max_rank");
    g.Text(nextText, { c.x, bar.B() + 4.f, c.w, 16.f }, kSub, Mono(8.5f).Ellipsis());
    const int played = duels.MyWins() + duels.MyLosses();
    const std::wstring record = Loc::Fmt("duel_record", duels.MyWins(), duels.MyLosses()) +
                                (played > 0 ? L"  \x00B7  " + std::to_wstring(duels.MyWins() * 100 / played) + L"%" : L"");
    g.Text(record, { c.x, c.B() - 16.f, c.w, 16.f }, kText, Mono(9.5f, true).Ellipsis());
    if (played < Services::Account::kDuelPlacementMatches)
        Chip(ui, c.R() - g.Measure(Loc::Get("duel_placement"), Mono(9.f, true)).w - 14.f, c.B() - 8.f, Loc::Get("duel_placement"), kGold);

    // Pages
    const Duel* current = duels.Active() ? &*duels.Active() : duels.Result() ? &*duels.Result() : nullptr;
    struct Item {
        int page;
        wchar_t glyph;
        std::wstring label;
        int count;
        bool alert;
    };
    std::vector<Item> items;
    if (current) {
        const char* key = duels.Result() && !duels.Active() ? "duel_page_result"
                          : current->status == Duel::Status::Open ? "duel_page_waiting"
                          : current->status == Duel::Status::Banning ? "duel_page_bans"
                                                                     : "duel_page_race";
        items.push_back({ PageMatch, kDuelGlyph, Loc::Get(key), 0, duels.MyTurnToBan() || duels.NeedsMyTime() || (duels.Result() && !duels.Active()) });
    }
    items.push_back({ PageLobbies, wchar_t(0xE8F2), Loc::Get("duel_page_lobbies"), static_cast<int>(duels.Lobbies().size() + duels.Invites().size()),
                      !duels.Invites().empty() });
    if (!duels.Active()) items.push_back({ PageCreate, wchar_t(0xE710), Loc::Get("duel_page_create"), 0, false });   // an action: opens a room
    items.push_back({ PageRanking, wchar_t(0xE9F9), Loc::Get("duel_page_ranking"), 0, false });
    items.push_back({ PageRules, wchar_t(0xE946), Loc::Get("duel_page_rules"), 0, false });
    items.push_back({ PageSettings, Glyphs::Settings, Loc::Get("duel_page_settings"), 0, false });

    float y = card.B() + 16.f;
    float selY = -1.f;
    for (const auto& item : items) {
        const Rect row{ r.x, y, r.w, 42.f };
        const Id id = ui.MakeId("duel-nav", item.page);
        const Interaction it = ui.Interact(id, row);
        const bool sel = page == item.page;
        const float hover = ui.Animate(HashId(id, 1), it.hovered && !sel ? 1.f : 0.f, 16.f);
        const float on = ui.Animate(HashId(id, 2), sel ? 1.f : 0.f, 14.f);
        if (hover > 0.01f) g.FillRound(row, 10.f, kRowHover.WithAlpha(hover));
        if (sel) selY = row.y;
        const Color color = Lerp(Lerp(kSub, kText, hover), kDuel, on);
        UI::Glyph(ui, item.glyph, { row.x + 12.f, row.y, 20.f, row.h }, 13.f, color);
        g.Text(item.label, { row.x + 42.f, row.y, row.w - 90.f, row.h }, Lerp(color, kText, on * 0.5f), Mono(10.5f, true).Ellipsis());
        if (item.count > 0 || item.alert) {
            const std::wstring n = item.count > 0 ? (item.count > 99 ? L"99+" : std::to_wstring(item.count)) : L"!";
            const float cw = std::max(22.f, g.Measure(n, Mono(9.f, true)).w + 12.f);
            const Rect chip{ row.R() - 12.f - cw, row.CY() - 10.f, cw, 20.f };
            const float pulse = item.alert ? 0.75f + 0.25f * std::sin(static_cast<float>(ui.Time()) * 5.f) : 1.f;
            g.FillRound(chip, 10.f, item.alert ? Hex(0xCC2200).WithAlpha(pulse) : kDuel.WithAlpha(0.18f));
            g.Text(n, chip, item.alert ? Colors::White : kDuel, Mono(9.f, true).Center());
            if (item.alert) ui.KeepAnimatingThrottled();
        }
        if (it.hovered && !sel) ui.SetCursor(IDC_HAND);
        if (it.clicked) {
            if (item.page == PageCreate) CreateRoom();   // straight into a new room
            else SetPage(item.page);
        }
        y += 46.f;
        if (item.page == PageMatch) {
            g.FillRect({ r.x + 10.f, y + 1.f, r.w - 20.f, 1.f }, Palette::Divider);
            y += 8.f;
        }
    }
    if (selY >= 0.f) {
        const float by = ui.Animate(ui.MakeId("duel-nav-bar"), selY, 16.f, selY);
        g.FillRound({ r.x, by, r.w, 42.f }, 10.f, kDuel.WithAlpha(0.09f));
        g.FillRound({ r.x, by + 9.f, 3.f, 24.f }, 1.5f, kDuel);
    }

    // Online right now (bottom)
    const Rect foot{ r.x, r.B() - 34.f, r.w, 34.f };
    g.FillEllipse(foot.x + 10.f, foot.CY(), 4.f, 4.f, kOnline);
    g.Text(Loc::Fmt("duel_online_count", std::max(duels.OnlineCount(), 1)), { foot.x + 22.f, foot.y, foot.w - 22.f, foot.h }, kSub, Mono(9.5f, true));
}

// ── Right: who's online ─────────────────────────────────────────────────────

void DuelsOverlay::DrawSide(Ui& ui, const Rect& r, const Duel* current) {
    if (!current) {
        DrawOnline(ui, r);
        return;
    }
    // Unread: messages from the others since the chat tab was last on screen
    const std::wstring& me = AccountService::Get().UserId();
    long long newest = 0;
    int unread = 0;
    for (const auto& m : current->chat) {
        newest = std::max(newest, m.id);
        if (m.id > chatSeen && m.userId != me) ++unread;
    }
    if (sideTab == 1) {
        chatSeen = std::max(chatSeen, newest);
        unread = 0;
    }
    const std::wstring chatLabel = Loc::Get("duel_chat_tab") + (unread > 0 ? L" (" + std::to_wstring(unread) + L")" : L"");
    const int clicked = Pills(ui, r.Top(32.f), { chatLabel, Loc::Get("duel_online_tab") }, sideTab == 1 ? 0 : 1, "duel-side-tab");
    if (clicked >= 0) sideTab = clicked == 0 ? 1 : 0;
    const Rect rest = r.CutTop(44.f);
    if (sideTab == 1) DrawRoomChat(ui, rest, *current);
    else DrawOnline(ui, rest);
}

void DuelsOverlay::DrawRoomChat(Ui& ui, const Rect& r, const Duel& duel) {
    auto& duels = DuelsService::Get();
    const std::wstring& me = AccountService::Get().UserId();
    // Closed an hour after the duel finished (the server says so too)
    const bool open = duel.status != Duel::Status::Expired && duel.status != Duel::Status::Cancelled;
    const Rect input = r.Bottom(38.f);
    const Rect area{ r.x, r.y, r.w, r.h - 38.f - 10.f };
    Card(ui, area);

    std::vector<ChatView::Line> lines;
    lines.reserve(duel.chat.size());
    for (const auto& m : duel.chat) {
        ChatView::Line line;
        line.mine = m.userId == me;
        line.pending = m.id == 0;
        const auto* player = duel.Player(m.userId);
        line.profile = player ? &player->profile : nullptr;
        line.author = player ? NameOf(player->profile) : (m.name.empty() ? Loc::Get("acc_default_name") : m.name);
        line.text = m.text;
        line.sentAt = line.pending ? Async::Now() : duel.fetchedAt - m.ago;
        lines.push_back(std::move(line));
    }
    ChatView::Options options;
    options.accent = kDuel;
    options.empty = Loc::Get("duel_chat_empty");
    ChatView::Messages(ui, ui.MakeId("duel-chat-list"), area.Inset(6.f, 6.f), lines, options);

    if (ChatView::Input(ui, ui.MakeId("duel-chat-input"), input, chatText, Loc::Get("duel_chat_placeholder"), kDuel, 200, open)) {
        duels.Say(duel.id, chatText, Reply());
        chatText.clear();
    }
}

void DuelsOverlay::DrawOnline(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    auto& duels = DuelsService::Get();
    g.Text(Loc::Get("duel_online_title"), { r.x, r.y, r.w, 22.f }, kText, Mono(13.f, true));
    g.Text(Loc::Get("duel_online_sub"), { r.x, r.y + 22.f, r.w, 18.f }, kSub, Mono(9.f).Ellipsis());
    const Rect area{ r.x, r.y + 48.f, r.w, r.h - 48.f };
    Card(ui, area);
    const auto& list = duels.Online();
    if (list.empty()) {
        if (!duels.Loaded() && !duels.Failed()) {
            Form::Spinner(ui, area.CX(), area.CY(), 9.f, kDuel);
            ui.KeepAnimatingThrottled();
        } else {
            EmptyState(ui, area, wchar_t(0xE91F), Loc::Get(duels.Failed() ? "duel_load_failed" : "duel_online_none"));
        }
        return;
    }
    const auto& active = duels.Active();
    const bool inviting = active && active->status == Duel::Status::Open;
    const bool racing = active && active->status != Duel::Status::Open;

    const Rect inner = area.Inset(10.f, 10.f);
    ScrollArea scroll(ui, ui.MakeId("duel-online"), inner);
    float y = inner.y;
    bool friendsHeader = false, othersHeader = false;
    int index = 0;
    for (const auto& u : list) {
        if (u.isFriend && !friendsHeader) {
            friendsHeader = true;
            g.Text(Loc::Get("duel_online_friends"), { inner.x + 4.f, y, inner.w, 20.f }, kOnline, Mono(8.5f, true));
            y += 22.f;
        } else if (!u.isFriend && !othersHeader) {
            othersHeader = true;
            if (friendsHeader) y += 6.f;
            g.Text(Loc::Get("duel_online_others"), { inner.x + 4.f, y, inner.w, 20.f }, kSub, Mono(8.5f, true));
            y += 22.f;
        }
        StaggerItem in(ui, openedAt + 0.05, index++, 0.f, 6.f, 0.03, 0.26);
        const Rect row{ inner.x, y, inner.w - 8.f, 58.f };
        y += 62.f;
        const int64_t key = static_cast<int64_t>(std::hash<std::wstring>{}(u.profile.userId));
        const Id id = ui.MakeId("duel-on-row", key);
        const Interaction it = ui.Interact(id, row.CutRight(44.f));
        const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 16.f);
        g.Border(row, 10.f, Lerp(kRow, kRowHover, hover), kCardBorder.WithAlpha(0.4f + 0.4f * hover), 1.f);
        ProfileDraw::Nameplate(ui, row, 10.f, u.profile);
        const Rect pic{ row.x + 10.f, row.CY() - 17.f, 34.f, 34.f };
        const std::wstring name = NameOf(u.profile);
        ProfileDraw::Avatar(ui, u.profile.AvatarUrl(), pic, u.profile, name, 0.f, Colors::Transparent, true, false);
        {
            NoColorFilter keep(g);
            const Color dot = u.inDuel ? kDuel : u.playing ? kGold : kOnline;
            g.FillEllipse(pic.R() - 4.f, pic.B() - 4.f, 6.5f, 6.5f, kCard);
            g.FillEllipse(pic.R() - 4.f, pic.B() - 4.f, 4.5f, 4.5f, dot);
        }
        const float textX = pic.R() + 10.f, textW = row.R() - 52.f - textX;
        ui.PushClip({ textX, row.y, textW, row.h });
        ProfileDraw::Name(ui, name, { textX, row.y + 7.f, textW, 22.f }, u.profile, 12.5f);
        ui.PopClip();
        const std::wstring line = u.inDuel ? Loc::Get("duel_status_in_duel")
                                  : u.playing ? Services::Account::FriendsService::StatusText({ Services::Account::Friend::Relation::Friend, u.profile, L"", 0.0, u.playing })
                                              : Text::Widen(Services::Account::DuelRankFor(u.mmr).name) + L" \x00B7 " + std::to_wstring(u.mmr);
        const bool plate = ProfileDraw::HasNameplate(u.profile);
        if (plate) ProfileDraw::PlateScrim(ui, { textX, row.y + 31.f, 16.f, 16.f });
        RankIcon(ui, { textX, row.y + 31.f, 16.f, 16.f }, u.mmr);
        ProfileDraw::RowText(ui, line, { textX + 20.f, row.y + 30.f, textW - 20.f, 18.f }, u.inDuel ? kDuel : u.playing ? kGold : RankColor(u.mmr),
                             Mono(9.f, true).Ellipsis(), plate);
        ProfileDraw::AvatarDecoration(ui, pic, u.profile);   // over the dot and the texts
        if (it.hovered) ui.Tooltip(id, Loc::Fmt("duel_record_tooltip", u.wins, u.losses) + L"  \x00B7  " + Loc::Get("acc_rank_open_profile"));
        if (it.clicked) main.ShowProfile(u.profile);

        // Challenge / invite into our lobby
        const Rect button{ row.R() - 42.f, row.CY() - 16.f, 32.f, 32.f };
        bool challenged = duels.Challenged(u.profile.userId);
        if (active)
            for (const auto& p : active->invited)
                if (p.userId == u.profile.userId) challenged = true;
        if (racing) {
            // nothing to do while racing
        } else if (inviting) {
            const bool inLobby = active->Player(u.profile.userId) != nullptr;
            if (!inLobby &&
                IconButton(ui, ui.MakeId("duel-on-invite", key), button, challenged ? Glyphs::CheckMark : wchar_t(0xE8FA), MenuKit::Kind::Secondary,
                           !challenged && !u.inDuel && !duels.Busy(), Loc::Get(challenged ? "duel_challenged" : u.inDuel ? "duel_status_in_duel" : "duel_invite_btn")))
                duels.Invite(active->id, u.profile.userId, Reply(Loc::Fmt("duel_invited", name)));
        } else if (!active) {
            if (IconButton(ui, ui.MakeId("duel-on-challenge", key), button, kDuelGlyph, MenuKit::Kind::Primary, !u.inDuel,
                           Loc::Get(u.inDuel ? "duel_status_in_duel" : "duel_challenge_btn")))
                Challenge(u.profile);
        }
    }
    scroll.SetContentSize(y - inner.y);
}

} // namespace SpeedrunLauncher::Main
