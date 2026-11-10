#include <algorithm>
#include <cmath>
#include "Graphics/AnimatedImages.h"
#include "App/Config.h"
#include "App/Launcher.h"
#include "Core/Async.h"
#include "Core/Process.h"
#include "Core/System.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Services/Account/AccountService.h"
#include "Services/Account/BadgesService.h"
#include "Services/Account/DuelsService.h"
#include "Services/Account/FriendsService.h"
#include "Services/Chapters/ChapterPlaytimeStore.h"
#include "UI/Layout.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Main/Common/BadgeDraw.h"
#include "Windows/Main/Common/ProfileDraw.h"
#include "Windows/Main/Overlays/Account/AccountKit.h"
#include "Windows/Main/Overlays/Account/AccountOverlay.h"

// Someone's full profile, over the account menu: a wide header (banner, picture, name, actions)
// and four sections: OVERVIEW (stat tiles, top records, hours), RECORDS (every personal best on
// the chapters' speedrun.com leaderboards, with the verified runs per chapter), HOURS (per chapter
// and multiplayer game) and INFO.
namespace SpeedrunLauncher::Main {

using namespace UI;
using namespace AccountKit;
using Services::Account::Friend;
using Services::Account::FriendsService;
using Services::Account::Profile;
namespace Community = Services::Account::Community;
namespace SC = Services::SpeedrunCom;

namespace {

constexpr float kTabsH = 38.f;
constexpr Color kGold = Hex(0xFFC83D), kOnline = Hex(0x3DDC84), kPlaying = Hex(0xFFB020);

enum Section { SecOverview, SecRecords, SecHours, SecInfo, SecBadges };

std::wstring NameOf(const Profile& p) { return p.ShownName(p.speedrunName.empty() ? Loc::Get("acc_default_name") : p.speedrunName); }
std::wstring Hours(double seconds) { return Services::ChapterPlaytimeStore::Format(seconds); }

int ChapterOfGame(const std::wstring& gameId) {
    for (const auto& [chapter, id] : Config::SpeedrunCom::ChapterGameIds())
        if (id == gameId) return chapter;
    return 0;
}

bool IsMultiplayer(int id) {
    for (const auto& game : App::L().multiplayers)
        if (game.playtimeId == id) return true;
    return false;
}

std::wstring PlaytimeLabel(int id) {
    for (const auto& game : App::L().multiplayers)
        if (game.playtimeId == id) return Loc::Get("mp_" + game.key + "_title");
    return Loc::Fmt("fr_chapter", id);
}

// "Any% · Glitchless": the run type of the chapter's leaderboard the run belongs to, with its
// subcategory values ("…" while the chapter's categories load)
std::wstring RecordLabel(const Community::PersonalBest& pb, const std::map<int, std::vector<SC::RunType>>& runTypes) {
    if (!pb.label.empty()) return pb.label;   // named from speedrun.com when the runner was fetched
    auto types = runTypes.find(ChapterOfGame(pb.gameId));
    if (types == runTypes.end()) return L"\x2026";
    const auto value = [&](const std::wstring& var) {
        auto it = pb.values.find(var);
        return it == pb.values.end() ? std::wstring() : it->second;
    };
    for (const auto& rt : types->second) {
        if (rt.categoryId != pb.categoryId) continue;
        if (!rt.primaryVarId.empty() && value(rt.primaryVarId) != rt.primaryValueId) continue;
        std::wstring label = rt.label;
        for (const auto& v : rt.secondaryVariables) {
            const std::wstring got = value(v.id);
            for (const auto& option : v.values)
                if (option.id == got) label += L"  \x00B7  " + option.label;
        }
        return label;
    }
    return Loc::Get("pf_other_category");
}

// Small rounded label; returns its width
float Chip(Ui& ui, float x, float cy, const std::wstring& text, Color color, wchar_t glyph = 0) {
    auto& g = ui.g;
    const TextStyle st = Mono(9.5f, true);
    const float tw = g.Measure(text, st).w;
    const float w = tw + 16.f + (glyph ? 16.f : 0.f);
    const Rect r{ x, cy - 10.f, w, 20.f };
    g.Border(r, 10.f, color.WithAlpha(0.12f), color.WithAlpha(0.35f), 1.f);
    float tx = r.x + 8.f;
    if (glyph) {
        UI::Glyph(ui, glyph, { tx - 2.f, r.y, 14.f, r.h }, 9.f, color);
        tx += 16.f;
    }
    g.Text(text, { tx, r.y, tw + 2.f, r.h }, color, st);
    return w;
}

// A centred message (and a spinner while loading)
void Message(Ui& ui, const Rect& r, wchar_t glyph, const std::wstring& text, bool spinner = false) {
    auto& g = ui.g;
    if (spinner) {
        Form::Spinner(ui, r.CX(), r.CY() - 22.f, 9.f, kAccent);
        ui.KeepAnimatingThrottled();
    } else {
        UI::Glyph(ui, glyph, { r.CX() - 30.f, r.CY() - 56.f, 60.f, 44.f }, 28.f, kCardBorder);
    }
    TextStyle st = Mono(10.5f, true).Center();
    st.wrap = true;
    g.Text(text, { r.x + 24.f, r.CY() + 2.f, r.w - 48.f, 40.f }, kSub, st);
}

const Friend* FindFriend(const std::wstring& userId) {
    for (const auto& f : FriendsService::Get().Friends())
        if (f.profile.userId == userId) return &f;
    return nullptr;
}

} // namespace

// ── Data ────────────────────────────────────────────────────────────────────

void AccountOverlay::RequestRunner(const std::wstring& speedrunId) {
    if (speedrunId.empty() || runners.count(speedrunId) || runnersLoading.count(speedrunId)) return;
    runnersLoading.insert(speedrunId);
    auto token = alive;
    Async::Run([speedrunId] { return Community::FetchRunner(speedrunId); }, [this, token, speedrunId](Community::RunnerData data) {
        if (!*token) return;
        runnersLoading.erase(speedrunId);
        runners[speedrunId] = std::move(data);
    });
}

void AccountOverlay::OpenFullProfile(const Profile& profile, bool fromCard) {
    if (!fromCard) {
        OpenViewer(profile);
    } else {
        viewer.profile = profile;
    }
    viewer.open = true;
    viewer.full = true;
    viewer.fullFromCard = fromCard;
    viewer.fullAt = Async::Now();
    viewer.section = SecOverview;
    viewer.sectionAt = viewer.fullAt;
    RequestRunner(profile.speedrunId);
}

void AccountOverlay::PreviewMyProfile() {
    Profile me = draft;
    me.userId = Services::Account::AccountService::Get().UserId();
    if (me.playtime.empty()) me.playtime = Services::Account::AccountService::Get().Me().playtime;
    OpenFullProfile(me, false);
    viewer.preview = true;
    viewer.avatarSource = AvatarSource();
    viewer.bannerSource = BannerSource();
}

// ── Frame ───────────────────────────────────────────────────────────────────

void AccountOverlay::DrawFullProfile(Ui& ui, const Rect& screen) {
    auto& g = ui.g;
    auto& friends = FriendsService::Get();
    const Profile p = viewer.profile;   // a copy: the buttons below may replace the viewer's profile
    const bool preview = viewer.preview;
    const bool mine = p.userId == Services::Account::AccountService::Get().UserId();
    // The pictures: the uploaded ones, or the editor's in a preview
    const std::wstring avatarSource = preview ? viewer.avatarSource : p.AvatarUrl();
    const std::wstring bannerSource = preview ? viewer.bannerSource : p.BannerUrl();
    const float t = Anim::EaseOutCubic(static_cast<float>((ui.Time() - viewer.fullAt) / 0.25));
    if (t < 1.f) ui.KeepAnimating();
    RequestRunner(p.speedrunId);

    g.FillRect(screen, Rgba(0, 0, 0, static_cast<int>(170 * t)));
    Rect panel = screen.Center(std::min(screen.w - 48.f, 1060.f), screen.h - 48.f);
    panel.y += (1.f - t) * 22.f;
    const bool outside = ui.In().pressed[0] && !ui.MouseOver(panel);
    ui.Block(screen);
    g.PushOpacity(t);
    g.Shadow(panel, 14.f, 28.f, Rgba(0, 0, 0, 170));
    g.Border(panel, 14.f, Palette::PanelBg, kCardBorder, 1.f);

    // ── Header: banner, picture, name, who they are to you, actions ──
    // The banner as the profile card shows it (the same part of the picture, card proportions) stretched
    // over the whole header: taller on big windows
    const float bannerH = std::clamp(panel.w * 0.22f, 150.f, std::max(150.f, panel.h * 0.3f));
    const float headerH = bannerH + 76.f;
    const std::wstring name = NameOf(p);
    {
        NoColorFilter keep(g);
        g.PushRoundClip(panel, 14.f);
        const Rect header{ panel.x, panel.y, panel.w, bannerH };
        ProfileDraw::Banner(ui, L"", header, p, 0.f);   // the banner colour (behind / while loading)
        if (const std::wstring url = bannerSource; !url.empty())
            if (auto animation = AnimatedImages::Get(url, ProfileDraw::kBannerSide)) {
                if (auto frame = animation->At(ui.Time()); frame && frame->bitmap && frame->w > 0.f && frame->h > 0.f) {
                    // The card's crop: the centred part of the picture at 1 : 0.36
                    // (in the bitmap's own units: DIPs, which differ from pixels on scaled screens)
                    constexpr float kCardRatio = 0.36f;
                    const D2D1_SIZE_F size = frame->bitmap->GetSize();
                    D2D1_RECT_F source{ 0.f, 0.f, size.width, size.height };
                    if (size.height / size.width > kCardRatio) {
                        const float h = size.width * kCardRatio;
                        source.top = (size.height - h) * 0.5f;
                        source.bottom = source.top + h;
                    } else {
                        const float w = size.height / kCardRatio;
                        source.left = (size.width - w) * 0.5f;
                        source.right = source.left + w;
                    }
                    g.DrawBitmap(frame->bitmap.Get(), header, 1.f, &source);
                }
                if (animation->Animated()) ui.KeepAnimatingThrottled();
            }
        g.FillRect({ panel.x, panel.y + bannerH - 1.f, panel.w, 1.f }, Rgba(0, 0, 0, 90));
        g.PopLayer();
    }
    const Rect avatar{ panel.x + 28.f, panel.y + bannerH - 64.f, 108.f, 108.f };
    ProfileDraw::Avatar(ui, avatarSource, avatar, p, name, 5.f, Palette::PanelBg);
    if (preview) {
        // "Preview: this is how the others see your profile" on the banner's corner
        const std::wstring text = Loc::Get(Dirty() ? "pf_preview_unsaved" : "pf_preview_banner");
        const float tw = g.Measure(text, Mono(10.f, true)).w;
        const Rect pill{ panel.x + 16.f, panel.y + 14.f, tw + 44.f, 28.f };
        NoColorFilter keep(g);
        g.GlowRound(pill, 14.f, Colors::Black, 8.f, 0.4f);
        g.Border(pill, 14.f, Rgba(6, 12, 20, 225), kAccent.WithAlpha(0.8f), 1.f);
        UI::Glyph(ui, wchar_t(0xE7B3), { pill.x + 10.f, pill.y, 18.f, pill.h }, 12.f, kAccent);
        g.Text(text, { pill.x + 32.f, pill.y, tw + 4.f, pill.h }, kText, Mono(10.f, true));
    }
    const float textX = avatar.R() + 20.f;

    // Actions on the right of the name
    float bx = panel.R() - 24.f;
    const float by = panel.y + bannerH + 10.f;
    const auto button = [&](const char* id, const std::wstring& label, wchar_t glyph, MenuKit::Kind kind, bool enabled, Color accent) {
        const float w = MenuKit::FooterButtonWidth(ui, label, glyph, 60.f);
        bx -= w;
        const bool clicked = MenuKit::FooterButton(ui, ui.MakeId(id), { bx, by, w, 34.f }, label, kind, enabled, glyph, accent);
        bx -= 8.f;
        return clicked;
    };
    bool back = false;
    if (viewer.fullFromCard && button("pf-back", Loc::Get("back"), Glyphs::Back, MenuKit::Kind::Secondary, true, kAccent)) back = true;
    if (p.SpeedrunLinked() && !p.speedrunUrl.empty() && button("pf-src", Loc::Get("acc_src_open"), wchar_t(0xE8A7), MenuKit::Kind::Secondary, true, kSpeedrun))
        Process::OpenUrl(p.speedrunUrl);
    if (!mine && friends.IsFriend(p.userId) && button("pf-msg", Loc::Get("msg_button"), wchar_t(0xE8BD), MenuKit::Kind::Primary, true, Hex(0x3A9BFF))) {
        g.PopLayer();
        MessageButton(p);
        return;
    }
    if (p.DiscordLinked() && button("pf-dc", L"Discord", wchar_t(0xE8BD), MenuKit::Kind::Secondary, true, kBlurple))
        Process::OpenUrl(std::wstring(Config::Discord::UserProfileUrl) + p.discordId);
    if (!mine && !friends.IsFriend(p.userId) && !friends.HasRequest(p.userId) &&
        button("pf-add", Loc::Get("fr_add_short"), wchar_t(0xE8FA), MenuKit::Kind::Primary, !friends.Busy(), kAccent)) {
        auto token = alive;
        friends.SendToUser(p.userId, [this, token](const std::wstring& message, bool ok) {
            if (*token) FriendMessage(message, ok);
        });
    }

    const float nameW = std::max(60.f, bx - textX - 12.f);
    ui.PushClip({ textX, by - 6.f, nameW, 44.f });
    ProfileDraw::Name(ui, name, { textX, by - 4.f, nameW, 40.f }, p, 26.f);
    ui.PopClip();
    {
        float cx = textX;
        const float cy = by + 50.f;
        if (preview) cx += Chip(ui, cx, cy, Loc::Get("pf_preview_chip"), kAccent, wchar_t(0xE7B3)) + 6.f;
        else if (mine) cx += Chip(ui, cx, cy, Loc::Get("rk_you_chip"), kAccent) + 6.f;
        else if (friends.IsFriend(p.userId)) cx += Chip(ui, cx, cy, Loc::Get("fr_is_friend"), kAccent, wchar_t(0xE716)) + 6.f;
        else if (friends.HasRequest(p.userId)) cx += Chip(ui, cx, cy, Loc::Get("pf_rel_pending"), kSub, wchar_t(0xE823)) + 6.f;
        if (const Friend* f = FindFriend(p.userId); f && f->Online())
            cx += Chip(ui, cx, cy, FriendsService::StatusText(*f), f->playing ? kPlaying : kOnline, wchar_t(0xE91F)) + 6.f;
        // Their badge: the one they picked, or the newest (all of them in BADGES)
        if (const auto* badge = Services::Account::BadgesService::Get().Shown(p.userId, preview ? &p.featuredBadge : nullptr))
            cx += BadgeDraw::Pill(ui, cx, cy, *badge) + 6.f;
        g.Text(p.SpeedrunLinked() ? L"speedrun.com/" + p.speedrunName : Loc::Get("acc_not_linked_short"),
               { cx + 4.f, cy - 9.f, std::max(0.f, panel.R() - 24.f - cx), 18.f }, p.SpeedrunLinked() ? kSpeedrun : kSub, Mono(10.f, true).Ellipsis());
    }
    const Rect close{ panel.R() - 44.f, panel.y + 12.f, 32.f, 32.f };
    const bool closeClicked = MenuKit::FooterButton(ui, ui.MakeId("pf-close"), close, L"", MenuKit::Kind::Secondary, true, Glyphs::Cancel, kAccent);
    // Copy their user id (to give badges by hand in Supabase, reports...)
    {
        const Id id = ui.MakeId("pf-copy-id");
        double& copiedAt = ui.Data<double>(HashId(id, 7));
        const bool copied = ui.Time() - copiedAt < 2.0 && copiedAt > 0.0;
        const std::wstring label = Loc::Get(copied ? "pf_id_copied" : "pf_copy_id");
        const float w = MenuKit::FooterButtonWidth(ui, label, copied ? Glyphs::CheckMark : Glyphs::Copy, 60.f);
        if (MenuKit::FooterButton(ui, id, { close.x - 8.f - w, close.y, w, close.h }, label, MenuKit::Kind::Secondary, !p.userId.empty(),
                                  copied ? Glyphs::CheckMark : Glyphs::Copy, kAccent)) {
            System::SetClipboardText(p.userId);
            copiedAt = ui.Time();
        }
        if (copied) ui.KeepAnimatingThrottled();
        ui.Tooltip(id, Loc::Fmt("pf_copy_id_tip", p.userId));
    }

    // ── Section tabs ──
    const Community::RunnerData* data = nullptr;
    if (auto it = runners.find(p.speedrunId); p.SpeedrunLinked() && it != runners.end()) data = &it->second;
    const size_t records = data && data->ok ? data->pbs.size() : 0;
    const Rect tabs{ panel.x + 24.f, panel.y + headerH, panel.w - 48.f, kTabsH };
    {
        struct Tab {
            wchar_t glyph;
            const char* key;
            std::wstring count;
        };
        const auto* badgeList = Services::Account::BadgesService::Get().Of(p.userId);
        const size_t badges = badgeList ? badgeList->size() : 0;
        const Tab items[] = {
            { wchar_t(0xE80F), "pf_tab_overview", L"" },
            { wchar_t(0xE734), "pf_tab_records", records ? std::to_wstring(records) : L"" },
            { wchar_t(0xE823), "pf_tab_hours", L"" },
            { Glyphs::Info, "pf_tab_info", L"" },
            { wchar_t(0xE735), "pf_tab_badges", badges ? std::to_wstring(badges) : L"" },
        };
        float x = tabs.x;
        float selX = tabs.x, selW = 0.f;
        for (int i = 0; i < 5; ++i) {
            const std::wstring label = Loc::Get(items[i].key);
            const float lw = g.Measure(label, Mono(11.f, true)).w;
            const float cw = items[i].count.empty() ? 0.f : g.Measure(items[i].count, Mono(9.f, true)).w + 14.f;
            const float w = 18.f + 22.f + lw + (cw > 0.f ? cw + 8.f : 0.f) + 18.f;
            const Rect cell{ x, tabs.y, w, tabs.h };
            const Id id = ui.MakeId("pf-cell", i);
            const Interaction it = ui.Interact(id, cell);
            const bool sel = viewer.section == i;
            const float hover = ui.Animate(HashId(id, 1), it.hovered && !sel ? 1.f : 0.f, 16.f);
            if (hover > 0.01f) g.FillRound(cell, 9.f, kRowHover.WithAlpha(hover));
            const Color color = sel ? kAccent : Lerp(kSub, kText, hover);
            UI::Glyph(ui, items[i].glyph, { cell.x + 16.f, cell.y, 18.f, cell.h }, 12.f, color);
            g.Text(label, { cell.x + 40.f, cell.y, lw + 4.f, cell.h }, color, Mono(11.f, true));
            if (cw > 0.f) {
                const Rect chip{ cell.x + 40.f + lw + 8.f, cell.CY() - 9.f, cw, 18.f };
                g.FillRound(chip, 9.f, kAccent.WithAlpha(0.18f));
                g.Text(items[i].count, chip, kAccent, Mono(9.f, true).Center());
            }
            if (sel) {
                selX = cell.x;
                selW = cell.w;
            }
            if (it.hovered && !sel) ui.SetCursor(IDC_HAND);
            if (it.clicked && !sel) {
                viewer.section = i;
                viewer.sectionAt = Async::Now();
            }
            x += w + 6.f;
        }
        const float ux = ui.Animate(ui.MakeId("pf-tab-x"), selX, 16.f, selX);
        const float uw = ui.Animate(ui.MakeId("pf-tab-w"), selW, 16.f, selW);
        g.FillRound({ ux, tabs.y, uw, tabs.h }, 9.f, Rgba(0, 204, 170, 22));
        g.FillRound({ ux + 14.f, tabs.B() + 4.f, std::max(0.f, uw - 28.f), 3.f }, 1.5f, kAccent);
        g.FillRect({ panel.x + 16.f, tabs.B() + 9.f, panel.w - 32.f, 1.f }, Palette::Divider);
    }

    // ── The section ──
    const Rect content{ panel.x + 24.f, tabs.B() + 24.f, panel.w - 48.f, panel.B() - 22.f - (tabs.B() + 24.f) };
    if (content.h > 40.f) {
        StaggerItem in(ui, viewer.sectionAt, 0, 0.f, 10.f);
        switch (viewer.section) {
            case SecRecords: DrawFullRecords(ui, content); break;
            case SecHours: DrawFullHours(ui, content); break;
            case SecInfo: DrawFullInfo(ui, content); break;
            case SecBadges: DrawFullBadges(ui, content); break;
            default: DrawFullOverview(ui, content); break;
        }
    }
    // Their profile frame around the whole panel, like on their card
    ProfileDraw::ProfileFrame(ui, panel, 14.f, p);
    g.PopLayer();

    const bool escape = ui.KeyPressed(VK_ESCAPE);
    if (back || (escape && viewer.fullFromCard)) {
        viewer.full = false;
        viewer.openedAt = Async::Now();
    } else if (closeClicked || outside || escape) {
        viewer.full = false;
        viewer.open = false;
        viewer.preview = false;
    }
}

// ── Badges: everything they've won ──────────────────────────────────────────

void AccountOverlay::DrawFullBadges(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    const Profile& p = viewer.profile;
    auto& service = Services::Account::BadgesService::Get();
    const auto* list = service.Of(p.userId);
    if (!list) {
        Message(ui, r, wchar_t(0xE735), L"", true);
        return;
    }
    const bool mine = viewer.preview || p.userId == Services::Account::AccountService::Get().UserId();
    if (list->empty()) {
        Message(ui, r, wchar_t(0xE735), Loc::Get(mine ? "pf_badges_none_you" : "pf_badges_none"));
        return;
    }
    const Services::Account::Badge* shown = service.Shown(p.userId, viewer.preview ? &p.featuredBadge : nullptr);
    Caption(ui, r.x, r.y, r.w, Loc::Fmt("pf_badges_count", list->size()), kGold);
    if (mine) g.Text(Loc::Get("pf_badges_pick_hint"), { r.x, r.y, r.w, 16.f }, kSub, Mono(9.f).Right().Ellipsis());
    const Rect area{ r.x, r.y + 24.f, r.w, r.h - 24.f };
    ScrollArea scroll(ui, ui.MakeId("pf-badges"), area);
    constexpr float kCardH = 104.f, kGap = 12.f;
    const int cols = std::max(1, static_cast<int>((area.w - 10.f + kGap) / (300.f + kGap)));
    const float cw = (area.w - 10.f - kGap * (cols - 1)) / cols;
    for (size_t i = 0; i < list->size(); ++i) {
        const auto& b = (*list)[i];
        const auto look = BadgeDraw::LookOf(b);
        StaggerItem in(ui, viewer.sectionAt, static_cast<int>(std::min<size_t>(i, 12)), 0.f, 8.f, 0.04, 0.3);
        const int col = static_cast<int>(i) % cols, row = static_cast<int>(i) / cols;
        const Rect card{ area.x + col * (cw + kGap), area.y + row * (kCardH + kGap), cw, kCardH };
        {
            NoColorFilter keep(g);
            g.GlowRound(card, 12.f, look.color, 12.f, 0.12f);
            g.FillLinear(card, 12.f, { { 0.f, Lerp(Hex(0x0A1018), look.color, 0.14f) }, { 1.f, Hex(0x0A1018) } }, { 0.f, 0.f }, { 1.f, 1.f });
            g.StrokeRound(card, 12.f, look.color.WithAlpha(0.55f), 1.f);
        }
        const float mr = 28.f;
        BadgeDraw::Medal(ui, card.x + 18.f + mr, card.CY(), mr, b, static_cast<int>(i));
        const float textX = card.x + 18.f + mr * 2.f + 16.f, textW = card.R() - 14.f - textX;
        g.Text(look.title, { textX, card.y + 16.f, textW, 22.f }, look.color, Mono(13.f, true).Ellipsis());
        TextStyle desc = Mono(9.5f);
        desc.wrap = true;
        desc.valign = VAlign::Top;
        g.Text(look.description, { textX, card.y + 40.f, textW, 30.f }, kText, desc);
        if (b.kind == "champion")
            g.Text(Loc::Fmt("pf_badge_stats", b.mmr, b.wins, b.losses), { textX, card.B() - 24.f, textW, 16.f }, kSub, Mono(9.f).Ellipsis());
        if (&b == shown) {
            const std::wstring label = Loc::Get("pf_badge_shown");
            const float lw = g.Measure(label, Mono(8.5f, true)).w + 14.f;
            const Rect tag{ card.R() - 12.f - lw, card.B() - 12.f - 20.f, lw, 20.f };
            g.FillRound(tag, 10.f, look.color);
            g.Text(label, tag, Hex(0x10141A), Mono(8.5f, true).Center());
        }
    }
    scroll.SetContentSize(((list->size() + cols - 1) / cols) * (kCardH + kGap));
}

// ── Overview: 4 stat tiles, the best records and the hours ──────────────────

void AccountOverlay::DrawFullOverview(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    const Profile& p = viewer.profile;
    const Community::RunnerData* data = nullptr;
    if (auto it = runners.find(p.speedrunId); p.SpeedrunLinked() && it != runners.end() && it->second.ok) data = &it->second;
    const bool loading = p.SpeedrunLinked() && runnersLoading.count(p.speedrunId);

    int runs = 0, bestPlace = 0;
    if (data) {
        for (const auto& [game, count] : data->verifiedRuns) runs += count;
        for (const auto& pb : data->pbs)
            if (pb.place > 0 && (bestPlace == 0 || pb.place < bestPlace)) bestPlace = pb.place;
    }
    const std::wstring unknown = loading ? L"\x2026" : L"\x2014";
    struct Tile {
        wchar_t glyph;
        const char* key;
        std::wstring value;
        Color color;
    };
    const Tile tiles[] = {
        { wchar_t(0xE823), "pf_stat_hours", Hours(ProfileDraw::TotalSeconds(p)), kAccent },
        { Glyphs::Play, "pf_stat_runs", data ? std::to_wstring(runs) : unknown, kSpeedrun },
        { wchar_t(0xE734), "pf_stat_records", data ? std::to_wstring(data->pbs.size()) : unknown, kGold },
        { wchar_t(0xE9F9), "pf_stat_best", data && bestPlace > 0 ? L"#" + std::to_wstring(bestPlace) : unknown, kSpeedrun },
    };
    constexpr float kTileH = 96.f, kGap = 14.f;
    const float tileW = (r.w - kGap * 3.f) / 4.f;
    for (int i = 0; i < 4; ++i) {
        StaggerItem in(ui, viewer.sectionAt, i, 0.f, 8.f, 0.04, 0.3);
        const Rect tile{ r.x + i * (tileW + kGap), r.y, tileW, kTileH };
        Card(ui, tile, tiles[i].color.WithAlpha(0.3f));
        MenuKit::GlyphBadge(ui, { tile.x + 16.f, tile.y + 16.f, 34.f, 34.f }, tiles[i].glyph, tiles[i].color, 14.f);
        g.Text(tiles[i].value, { tile.x + 62.f, tile.y + 14.f, tile.w - 74.f, 38.f }, kText, Mono(20.f, true).Ellipsis());
        g.Text(Loc::Get(tiles[i].key), { tile.x + 16.f, tile.B() - 32.f, tile.w - 32.f, 18.f }, kSub, Mono(9.5f, true).Ellipsis());
    }

    // Best records (by speedrun.com place) on the left, hours on the right
    const Rect below{ r.x, r.y + kTileH + 18.f, r.w, r.h - kTileH - 18.f };
    if (below.h < 80.f) return;
    const float leftW = std::floor((below.w - kGap) * 0.58f);
    const Rect left = below.Left(leftW);
    const Rect right{ left.R() + kGap, below.y, below.w - leftW - kGap, below.h };
    {
        StaggerItem in(ui, viewer.sectionAt, 4, 0.f, 8.f, 0.04, 0.3);
        Card(ui, left);
        const Rect c = left.Inset(18.f, 16.f);
        Caption(ui, c.x, c.y, c.w, Loc::Get("pf_best_records"), kGold);
        const Rect list{ c.x, c.y + 28.f, c.w, c.h - 28.f };
        if (!p.SpeedrunLinked()) Message(ui, list, wchar_t(0xE734), Loc::Get("pf_not_linked"));
        else if (!data && loading) Message(ui, list, 0, Loc::Get("pf_loading"), true);
        else if (!data) Message(ui, list, Glyphs::Warning, Loc::Get("pf_failed"));
        else if (data->pbs.empty()) Message(ui, list, wchar_t(0xE734), Loc::Get("pf_no_records"));
        else {
            std::vector<const Community::PersonalBest*> best;
            for (const auto& pb : data->pbs) {
                best.push_back(&pb);
                RequestRunTypes(ChapterOfGame(pb.gameId));
            }
            std::sort(best.begin(), best.end(), [](const auto* a, const auto* b) {
                const int pa = a->place > 0 ? a->place : 1 << 30, pb = b->place > 0 ? b->place : 1 << 30;
                return pa != pb ? pa < pb : a->time < b->time;
            });
            float y = list.y;
            for (size_t i = 0; i < best.size() && y + 54.f <= list.B() + 0.5f; ++i) {
                const auto& pb = *best[i];
                const Rect row{ list.x, y, list.w, 50.f };
                const Id id = ui.MakeId("pf-ov-rec", static_cast<int64_t>(i));
                const Interaction it = ui.Interact(id, row);
                const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 16.f);
                g.Border(row, 9.f, Lerp(kRow, kRowHover, hover), Lerp(kCardBorder.WithAlpha(0.5f), kHoverBorder, hover), 1.f);
                const Color medal = pb.place == 1 ? kGold : pb.place == 2 ? Hex(0xC8D2DC) : pb.place == 3 ? Hex(0xD08A4A) : kSpeedrun;
                g.Text(pb.place > 0 ? L"#" + std::to_wstring(pb.place) : L"\x2014", { row.x + 8.f, row.y, 54.f, row.h }, medal, Mono(14.f, true).Center());
                const float tx = row.x + 70.f, timeW = 110.f;
                g.Text(Loc::Fmt("rk_chapter_short", ChapterOfGame(pb.gameId)), { tx, row.y + 8.f, row.w - 80.f - timeW, 16.f }, kSub, Mono(9.f, true));
                g.Text(RecordLabel(pb, runTypes), { tx, row.y + 24.f, row.R() - tx - timeW - 12.f, 20.f }, kText, Mono(11.f, true).Ellipsis());
                g.Text(SC::FormatTime(pb.time), { row.R() - timeW - 14.f, row.y, timeW, row.h }, kText, Mono(13.f, true).Right());
                if (it.hovered) {
                    ui.SetCursor(IDC_HAND);
                    ui.Tooltip(id, Loc::Get("pf_open_run"));
                }
                if (it.clicked && !pb.weblink.empty()) Process::OpenUrl(pb.weblink);
                y += 56.f;
            }
        }
    }
    {
        StaggerItem in(ui, viewer.sectionAt, 5, 0.f, 8.f, 0.04, 0.3);
        Card(ui, right);
        const Rect c = right.Inset(18.f, 16.f);
        Caption(ui, c.x, c.y, c.w, Loc::Fmt("fr_hours_total", Hours(ProfileDraw::TotalSeconds(p))), kAccent);
        const Rect list{ c.x, c.y + 28.f, c.w, c.h - 28.f };
        ui.PushClip(list);
        ProfileDraw::HoursList(ui, list, p, kAccent);
        ui.PopClip();
    }
}

// ── Records: per chapter, every personal best and the verified runs ─────────

void AccountOverlay::DrawFullRecords(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    const Profile& p = viewer.profile;
    Card(ui, r);
    if (!p.SpeedrunLinked()) {
        Message(ui, r, wchar_t(0xE734), Loc::Get("pf_not_linked"));
        return;
    }
    auto found = runners.find(p.speedrunId);
    if (found == runners.end()) {
        Message(ui, r, 0, Loc::Get("pf_loading"), true);
        return;
    }
    if (!found->second.ok) {
        Message(ui, r, Glyphs::Warning, Loc::Get("pf_failed"));
        const std::wstring label = Loc::Get("pf_retry");
        const float w = MenuKit::FooterButtonWidth(ui, label, Glyphs::Refresh, 130.f);
        if (MenuKit::FooterButton(ui, ui.MakeId("pf-retry"), { r.CX() - w * 0.5f, r.CY() + 50.f, w, 34.f }, label, MenuKit::Kind::Secondary, true, Glyphs::Refresh,
                                  kAccent)) {
            const std::wstring id = p.speedrunId;
            runners.erase(id);
            RequestRunner(id);
        }
        return;
    }
    const Community::RunnerData& data = found->second;
    if (data.pbs.empty() && data.verifiedRuns.empty()) {
        Message(ui, r, wchar_t(0xE734), Loc::Get("pf_no_records"));
        return;
    }

    const Rect inner = r.Inset(16.f, 14.f);
    ScrollArea scroll(ui, ui.MakeId("pf-records"), inner);
    float y = inner.y;
    int index = 0;
    for (const auto& [chapter, gameId] : Config::SpeedrunCom::ChapterGameIds()) {
        std::vector<const Community::PersonalBest*> pbs;
        for (const auto& pb : data.pbs)
            if (pb.gameId == gameId) pbs.push_back(&pb);
        auto runs = data.verifiedRuns.find(gameId);
        const int runCount = runs == data.verifiedRuns.end() ? 0 : runs->second;
        if (pbs.empty() && runCount == 0) continue;
        RequestRunTypes(chapter);
        // Grouped by category (full game first, then individual levels), fastest first
        std::sort(pbs.begin(), pbs.end(), [](const auto* a, const auto* b) {
            if (a->levelId.empty() != b->levelId.empty()) return a->levelId.empty();
            if (a->label != b->label) return a->label < b->label;
            return a->time < b->time;
        });

        // Chapter header: number, title, verified runs
        {
            StaggerItem in(ui, viewer.sectionAt, index++, 0.f, 8.f, 0.03, 0.28);
            const Rect head{ inner.x, y, inner.w - 10.f, 34.f };
            g.Text(Loc::Fmt("rk_chapter_short", chapter), { head.x + 4.f, head.y, 110.f, head.h }, kAccent, Mono(10.f, true));
            g.Text(Loc::Get("ch" + std::to_string(chapter) + "_title"), { head.x + 116.f, head.y, head.w - 300.f, head.h }, kText, Mono(11.f, true).Ellipsis());
            const std::wstring runsText = Loc::Fmt("pf_chapter_runs", runCount);
            const float cw = g.Measure(runsText, Mono(9.5f, true)).w + 32.f;
            Chip(ui, head.R() - cw, head.CY(), runsText, kSpeedrun, Glyphs::Play);
            g.FillRect({ head.x, head.B(), head.w, 1.f }, kCardBorder);
            y += 42.f;
        }
        for (const auto* pbPtr : pbs) {
            const auto& pb = *pbPtr;
            StaggerItem in(ui, viewer.sectionAt, index++, 0.f, 8.f, 0.03, 0.28);
            const Rect row{ inner.x, y, inner.w - 10.f, 54.f };
            y += 60.f;
            const int64_t key = static_cast<int64_t>(std::hash<std::wstring>{}(pb.weblink + pb.categoryId));
            const Id id = ui.MakeId("pf-rec", key);
            const Rect buttons{ row.R() - 88.f, row.CY() - 16.f, 80.f, 32.f };
            const Interaction it = ui.Interact(id, row.CutRight(96.f));
            const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 16.f);
            g.Border(row, 9.f, Lerp(kRow, kRowHover, hover), Lerp(kCardBorder.WithAlpha(0.5f), kHoverBorder, hover), 1.f);
            if (pb.place >= 1 && pb.place <= 3) {
                const Color medal = pb.place == 1 ? kGold : pb.place == 2 ? Hex(0xC8D2DC) : Hex(0xD08A4A);
                g.FillRound({ row.x, row.y + 9.f, 3.f, row.h - 18.f }, 1.5f, medal);
            }
            const float timeW = 120.f, placeW = 110.f;
            const float textW = row.w - 16.f - timeW - placeW - buttons.w - 36.f;
            g.Text(RecordLabel(pb, runTypes), { row.x + 16.f, row.y + 8.f, textW, 20.f }, kText, Mono(11.5f, true).Ellipsis());
            g.Text(pb.date.empty() ? L"\x2014" : pb.date, { row.x + 16.f, row.y + 29.f, textW, 16.f }, kSub, Mono(9.5f));
            const float placeX = row.x + 16.f + textW + 12.f;
            if (pb.place > 0) Chip(ui, placeX, row.CY(), Loc::Fmt("rk_global", pb.place), pb.place <= 3 ? kGold : kSpeedrun);
            g.Text(SC::FormatTime(pb.time), { placeX + placeW, row.y, timeW, row.h }, pb.place == 1 ? kGold : kText, Mono(14.f, true).Right());
            // Video and run buttons
            const bool video = !pb.videoUrl.empty();
            if (MenuKit::FooterButton(ui, ui.MakeId("pf-rec-video", key), { buttons.x, buttons.y, 36.f, 32.f }, L"", MenuKit::Kind::Secondary, video, Glyphs::Video,
                                      kAccent))
                Process::OpenUrl(pb.videoUrl);
            if (video) ui.Tooltip(ui.MakeId("pf-rec-video", key), Loc::Get("pf_watch_video"));
            if (MenuKit::FooterButton(ui, ui.MakeId("pf-rec-run", key), { buttons.x + 44.f, buttons.y, 36.f, 32.f }, L"", MenuKit::Kind::Secondary,
                                      !pb.weblink.empty(), wchar_t(0xE8A7), kSpeedrun))
                Process::OpenUrl(pb.weblink);
            ui.Tooltip(ui.MakeId("pf-rec-run", key), Loc::Get("pf_open_run"));
            if (it.hovered) ui.SetCursor(IDC_HAND);
            if (it.clicked && !pb.weblink.empty()) Process::OpenUrl(pb.weblink);
        }
        if (pbs.empty()) {
            g.Text(Loc::Get("pf_no_records_chapter"), { inner.x + 4.f, y, inner.w - 14.f, 24.f }, kSub, Mono(10.f));
            y += 30.f;
        }
        y += 14.f;
    }
    scroll.SetContentSize(y - inner.y);
}

// ── Hours: total, then every chapter / multiplayer game with its share ──────

void AccountOverlay::DrawFullHours(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    const Profile& p = viewer.profile;
    const double total = ProfileDraw::TotalSeconds(p);
    int chapters = 0, games = 0;
    double most = 0.0;
    std::vector<std::pair<int, double>> rows;
    for (const auto& [id, seconds] : p.playtime) {
        if (seconds < 60.0 || !App::L().CountsPlaytime(id)) continue;
        rows.push_back({ id, seconds });
        (IsMultiplayer(id) ? games : chapters)++;
        most = std::max(most, seconds);
    }

    // Total on top
    const Rect hero = r.Top(92.f);
    Card(ui, hero, kAccent.WithAlpha(0.35f));
    {
        NoColorFilter keep(g);
        g.FillLinear(hero, 12.f, { { 0.f, Rgba(0, 204, 170, 24) }, { 1.f, Rgba(0, 0, 0, 0) } }, { 0.f, 0.f }, { 1.f, 1.f });
    }
    MenuKit::GlyphBadge(ui, { hero.x + 20.f, hero.CY() - 22.f, 44.f, 44.f }, wchar_t(0xE823), kAccent, 18.f);
    g.Text(Hours(total), { hero.x + 80.f, hero.y + 14.f, hero.w - 100.f, 40.f }, kText, Mono(26.f, true));
    g.Text(Loc::Fmt("pf_played_summary", chapters, games), { hero.x + 80.f, hero.y + 54.f, hero.w - 100.f, 20.f }, kSub, Mono(10.f, true).Ellipsis());

    const Rect area{ r.x, hero.B() + 16.f, r.w, r.h - hero.h - 16.f };
    Card(ui, area);
    if (rows.empty()) {
        Message(ui, area, wchar_t(0xE823), Loc::Get("pf_no_hours"));
        return;
    }
    const Rect inner = area.Inset(18.f, 14.f);
    ScrollArea scroll(ui, ui.MakeId("pf-hours"), inner);
    float y = inner.y;
    bool shownGames = false;
    int index = 0;
    for (const auto& [id, seconds] : rows) {
        if (IsMultiplayer(id) && !shownGames) {
            shownGames = true;
            y += 6.f;
            Caption(ui, inner.x + 4.f, y, inner.w, Loc::Get("pf_multiplayer"), kSub);
            y += 24.f;
        } else if (index == 0) {
            Caption(ui, inner.x + 4.f, y, inner.w, Loc::Get("pf_chapters"), kSub);
            y += 24.f;
        }
        StaggerItem in(ui, viewer.sectionAt, index++, 0.f, 6.f, 0.03, 0.28);
        const Rect row{ inner.x, y, inner.w - 10.f, 44.f };
        y += 50.f;
        g.Border(row, 9.f, kRow, kCardBorder.WithAlpha(0.5f), 1.f);
        const float labelW = std::min(220.f, row.w * 0.3f), timeW = 90.f, shareW = 60.f;
        g.Text(PlaytimeLabel(id), { row.x + 16.f, row.y, labelW, row.h }, kText, Mono(11.f, true).Ellipsis());
        const Rect track{ row.x + 16.f + labelW + 12.f, row.CY() - 4.f, std::max(10.f, row.w - labelW - timeW - shareW - 56.f), 8.f };
        g.FillRound(track, 4.f, Palette::ButtonBorder);
        const float fill = most > 0.0 ? static_cast<float>(seconds / most) : 0.f;
        const float shown = ui.Animate(ui.MakeId("pf-hours-bar", id), fill, 8.f, 0.f);
        if (std::abs(shown - fill) > 0.002f) ui.KeepAnimating();
        g.FillRound({ track.x, track.y, std::max(8.f, track.w * shown), track.h }, 4.f, IsMultiplayer(id) ? kSpeedrun : kAccent);
        const int share = total > 0.0 ? static_cast<int>(std::lround(seconds * 100.0 / total)) : 0;
        g.Text(std::to_wstring(share) + L"%", { track.R() + 10.f, row.y, shareW, row.h }, kSub, Mono(10.f, true).Center());
        g.Text(Hours(seconds), { row.R() - timeW - 14.f, row.y, timeW, row.h }, kText, Mono(12.f, true).Right());
    }
    scroll.SetContentSize(y - inner.y);
}

// ── Info: who they are ──────────────────────────────────────────────────────

void AccountOverlay::DrawFullInfo(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    auto& friends = FriendsService::Get();
    const Profile& p = viewer.profile;
    const bool mine = p.userId == Services::Account::AccountService::Get().UserId();
    const Community::RunnerData* data = nullptr;
    if (auto it = runners.find(p.speedrunId); p.SpeedrunLinked() && it != runners.end() && it->second.ok) data = &it->second;

    // Most played, chapters and games played
    int favourite = 0, chapters = 0, games = 0;
    double most = 0.0;
    for (const auto& [id, seconds] : p.playtime) {
        if (seconds < 60.0 || !App::L().CountsPlaytime(id)) continue;
        (IsMultiplayer(id) ? games : chapters)++;
        if (seconds > most) {
            most = seconds;
            favourite = id;
        }
    }
    // First and latest record
    std::wstring first, latest;
    if (data)
        for (const auto& pb : data->pbs) {
            if (pb.date.empty()) continue;
            if (first.empty() || pb.date < first) first = pb.date;
            if (latest.empty() || pb.date > latest) latest = pb.date;
        }

    std::wstring relation = Loc::Get(mine ? "pf_rel_you" : friends.IsFriend(p.userId) ? "pf_rel_friend" : friends.HasRequest(p.userId) ? "pf_rel_pending" : "pf_rel_none");
    std::wstring presence = L"\x2014";
    if (const Friend* f = FindFriend(p.userId)) presence = FriendsService::StatusText(*f);

    struct Row {
        wchar_t glyph;
        const char* key;
        std::wstring value;
        Color color;
    };
    const Row rows[] = {
        { wchar_t(0xE77B), "pf_info_name", NameOf(p), kText },
        { wchar_t(0xE734), "pf_info_src", p.SpeedrunLinked() ? L"speedrun.com/" + p.speedrunName : Loc::Get("acc_not_linked_short"), p.SpeedrunLinked() ? kSpeedrun : kSub },
        { wchar_t(0xE8BD), "pf_info_discord", p.DiscordLinked() ? L"@" + p.discordName : Loc::Get("acc_dc_not_linked_short"), p.DiscordLinked() ? kBlurple : kSub },
        { wchar_t(0xE716), "pf_info_relation", relation, kText },
        { wchar_t(0xE91F), "pf_info_status", presence, kText },
        { wchar_t(0xE823), "pf_info_favorite", favourite ? PlaytimeLabel(favourite) + L"  \x00B7  " + Hours(most) : L"\x2014", kText },
        { wchar_t(0xE8F1), "pf_info_chapters", std::to_wstring(chapters), kText },
        { Glyphs::Gamepad, "pf_info_multiplayer", std::to_wstring(games), kText },
        { wchar_t(0xE787), "pf_info_first_record", first.empty() ? L"\x2014" : first, kText },
        { wchar_t(0xE787), "pf_info_last_record", latest.empty() ? L"\x2014" : latest, kText },
        { wchar_t(0xE771), "pf_info_effect", Loc::Get(p.Effect().locKey), kText },
    };
    Card(ui, r);
    const Rect inner = r.Inset(18.f, 12.f);
    ScrollArea scroll(ui, ui.MakeId("pf-info"), inner);
    float y = inner.y;
    int index = 0;
    for (const auto& row : rows) {
        StaggerItem in(ui, viewer.sectionAt, index++, 0.f, 6.f, 0.03, 0.28);
        const Rect line{ inner.x, y, inner.w - 10.f, 42.f };
        UI::Glyph(ui, row.glyph, { line.x + 4.f, line.y, 20.f, line.h }, 12.f, kAccent);
        g.Text(Loc::Get(row.key), { line.x + 36.f, line.y, 220.f, line.h }, kSub, Mono(10.f, true).Ellipsis());
        float valueRight = line.R();
        if (row.key == std::string_view("pf_info_effect")) {
            // The name's colours as swatches
            NoColorFilter keep(g);
            const auto swatch = [&](unsigned rgb) {
                valueRight -= 18.f;
                g.FillEllipse(valueRight + 9.f, line.CY(), 7.f, 7.f, Hex(rgb));
                g.StrokeEllipse(valueRight + 9.f, line.CY(), 7.f, 7.f, Rgba(255, 255, 255, 40), 1.f);
                valueRight -= 6.f;
            };
            if (p.Effect().twoColors) swatch(p.color2);
            swatch(p.color1);
        }
        g.Text(row.value, { line.x + 260.f, line.y, std::max(0.f, valueRight - line.x - 260.f), line.h }, row.color, Mono(11.f, true).Right().Ellipsis());
        g.FillRect({ line.x, line.B(), line.w, 1.f }, Palette::Divider);
        y += 44.f;
    }
    scroll.SetContentSize(y - inner.y);
}

} // namespace SpeedrunLauncher::Main
