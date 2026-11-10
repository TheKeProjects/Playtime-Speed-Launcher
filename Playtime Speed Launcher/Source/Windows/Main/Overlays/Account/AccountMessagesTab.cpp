#include <algorithm>
#include "Core/Async.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "Services/Account/AccountService.h"
#include "Services/Account/FriendsService.h"
#include "Services/Account/MessagesService.h"
#include "UI/Layout.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Form.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "Windows/Main/Common/ChatView.h"
#include "Windows/Main/Common/ProfileDraw.h"
#include "Windows/Main/Overlays/Account/AccountKit.h"
#include "Windows/Main/Overlays/Account/AccountOverlay.h"

// Friends → MESSAGES: private chats between friends.
//   [ conversations (newest first, unread in bold) ] [ the open one: who, the messages, the field ]
namespace SpeedrunLauncher::Main {

using namespace UI;
using namespace AccountKit;
using Services::Account::ChatThread;
using Services::Account::Friend;
using Services::Account::FriendsService;
using Services::Account::MessagesService;
using Services::Account::Profile;

namespace {

constexpr Color kChat = Hex(0x3A9BFF);
constexpr Color kOnline = Hex(0x3DDC84), kPlaying = Hex(0xFFB020), kOffline = Hex(0x55606A);
constexpr float kRowH = 60.f;

std::wstring NameOf(const Profile& p) { return p.ShownName(p.speedrunName.empty() ? Loc::Get("acc_default_name") : p.speedrunName); }

// Presence of someone: the friends list knows best, then the conversation's copy
Friend PresenceOf(const Profile& p, const ChatThread* thread) {
    for (const auto& f : FriendsService::Get().Friends())
        if (f.profile.userId == p.userId) return f;
    Friend f;
    f.relation = Friend::Relation::None;
    f.profile = p;
    if (thread) {
        f.secondsAgo = thread->secondsAgo;
        f.playing = thread->playing;
    }
    return f;
}

void AvatarDot(Ui& ui, const Rect& pic, const Friend& f) {
    auto& g = ui.g;
    ProfileDraw::Avatar(ui, f.profile.AvatarUrl(), pic, f.profile, NameOf(f.profile), 0.f, Colors::Transparent, true, false);
    {
        NoColorFilter keep(g);
        const Color c = f.Online() ? (f.playing ? kPlaying : kOnline) : kOffline;
        g.FillEllipse(pic.R() - 4.f, pic.B() - 4.f, 6.5f, 6.5f, kCard);
        g.FillEllipse(pic.R() - 4.f, pic.B() - 4.f, 4.5f, 4.5f, c);
    }
    ProfileDraw::AvatarDecoration(ui, pic, f.profile);   // over the dot
}

std::wstring ShortAgo(double seconds) {
    if (seconds < 60.0) return Loc::Get("chat_now");
    const long long minutes = static_cast<long long>(seconds / 60.0);
    if (minutes < 60) return Loc::Fmt("chat_min", minutes);
    if (minutes < 60 * 24) return Loc::Fmt("chat_h", minutes / 60);
    return Loc::Fmt("chat_d", minutes / (60 * 24));
}

} // namespace

void AccountOverlay::DrawMessages(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    auto& messages = MessagesService::Get();
    const auto& threads = messages.Threads();

    const float listW = std::min(290.f, std::max(220.f, r.w * 0.36f));
    const Rect left = r.Left(listW);
    const Rect right{ left.R() + 16.f, r.y, r.w - listW - 16.f, r.h };

    // ── The conversations ──
    g.Text(Loc::Get("msg_page"), { left.x, left.y, left.w, 22.f }, kText, Mono(14.f, true));
    g.Text(Loc::Get("msg_sub"), { left.x, left.y + 22.f, left.w, 18.f }, kSub, Mono(9.5f).Ellipsis());
    const Rect area{ left.x, left.y + 52.f, left.w, left.h - 52.f };
    Card(ui, area);

    // The open one first if it has no messages yet (a new chat)
    std::vector<const ChatThread*> rows;
    ChatThread fresh;
    const bool openListed = std::any_of(threads.begin(), threads.end(), [&](const ChatThread& t) { return t.profile.userId == chatProfile.userId; });
    if (!chatProfile.userId.empty() && !openListed) {
        fresh.profile = chatProfile;
        rows.push_back(&fresh);
    }
    for (const auto& t : threads) rows.push_back(&t);

    if (rows.empty()) {
        if (!messages.Loaded() && !messages.Failed()) {
            Form::Spinner(ui, area.CX(), area.CY(), 9.f, kChat);
            ui.KeepAnimatingThrottled();
        } else {
            TextStyle st = Mono(10.f, true).Center();
            st.wrap = true;
            UI::Glyph(ui, wchar_t(0xE8BD), { area.CX() - 30.f, area.CY() - 56.f, 60.f, 44.f }, 28.f, kCardBorder);
            g.Text(Loc::Get(messages.Failed() ? "msg_load_failed" : "msg_none"), { area.x + 16.f, area.CY(), area.w - 32.f, 60.f }, kSub, st);
        }
    } else {
        const Rect inner = area.Inset(8.f, 8.f);
        ScrollArea scroll(ui, ui.MakeId("msg-threads"), inner);
        float y = inner.y;
        const double now = Async::Now();
        int index = 0;
        for (const ChatThread* t : rows) {
            StaggerItem in(ui, friendsPageAt + 0.04, std::min(index++, 10), 0.f, 6.f, 0.03, 0.26);
            const Rect row{ inner.x, y, inner.w - 8.f, kRowH };
            y += kRowH + 4.f;
            const int64_t key = static_cast<int64_t>(std::hash<std::wstring>{}(t->profile.userId));
            const Id id = ui.MakeId("msg-thread", key);
            const Interaction it = ui.Interact(id, row);
            const bool sel = t->profile.userId == chatProfile.userId;
            const float hover = ui.Animate(HashId(id, 1), it.hovered ? 1.f : 0.f, 16.f);
            g.Border(row, 10.f, Lerp(sel ? kChat.WithAlpha(0.12f) : kRow.WithAlpha(0.f), kRowHover, hover),
                     sel ? kChat.WithAlpha(0.7f) : kCardBorder.WithAlpha(0.3f + 0.4f * hover), 1.f);
            const Rect pic{ row.x + 10.f, row.CY() - 18.f, 36.f, 36.f };
            AvatarDot(ui, pic, PresenceOf(t->profile, t));

            // Right: when, and the unread count
            float rx = row.R() - 10.f;
            if (t->lastId > 0) {
                const std::wstring when = ShortAgo(now - (t->fetchedAt - t->ago));
                const float ww = g.Measure(when, Mono(8.5f)).w + 2.f;
                g.Text(when, { rx - ww, row.y + 9.f, ww, 16.f }, t->unread > 0 ? kChat : kSub, Mono(8.5f, t->unread > 0));
                if (t->unread > 0) {
                    const std::wstring n = t->unread > 99 ? L"99+" : std::to_wstring(t->unread);
                    const float cw = std::max(20.f, g.Measure(n, Mono(8.5f, true)).w + 10.f);
                    const Rect chip{ rx - cw, row.y + 30.f, cw, 18.f };
                    g.FillRound(chip, 9.f, kChat);
                    g.Text(n, chip, Colors::White, Mono(8.5f, true).Center());
                }
                rx -= std::max(ww, 24.f) + 8.f;
            }
            const float textX = pic.R() + 10.f, textW = std::max(20.f, rx - textX);
            ui.PushClip({ textX, row.y, textW, row.h });
            ProfileDraw::Name(ui, NameOf(t->profile), { textX, row.y + 8.f, textW, 22.f }, t->profile, 12.5f);
            ui.PopClip();
            const std::wstring preview = t->lastId == 0 ? Loc::Get("msg_new_chat") : (t->lastMine ? Loc::Get("msg_you") + L" " : L"") + t->last;
            g.Text(preview, { textX, row.y + 32.f, textW, 18.f }, t->unread > 0 ? kText : kSub, Mono(9.5f, t->unread > 0).Ellipsis());

            if (it.clicked && !sel) {
                chatProfile = t->profile;
                dmText.clear();
                chatOpenedAt = Async::Now();
                messages.Open(chatProfile.userId);
            }
        }
        scroll.SetContentSize(y - inner.y);
    }

    // ── The open conversation ──
    if (chatProfile.userId.empty()) {
        Card(ui, right);
        TextStyle st = Mono(11.f, true).Center();
        st.wrap = true;
        UI::Glyph(ui, wchar_t(0xE8BD), { right.CX() - 30.f, right.CY() - 60.f, 60.f, 48.f }, 32.f, kCardBorder);
        g.Text(Loc::Get("msg_pick"), { right.x + 30.f, right.CY(), right.w - 60.f, 44.f }, kSub, st);
        return;
    }
    StaggerItem in(ui, chatOpenedAt, 0, 12.f);
    DrawConversation(ui, right);
}

void AccountOverlay::DrawConversation(Ui& ui, const Rect& r) {
    auto& g = ui.g;
    auto& messages = MessagesService::Get();
    auto& friends = FriendsService::Get();
    const Profile p = chatProfile;
    const ChatThread* thread = nullptr;
    for (const auto& t : messages.Threads())
        if (t.profile.userId == p.userId) thread = &t;
    const Friend presence = PresenceOf(p, thread);
    const bool canWrite = friends.IsFriend(p.userId);

    // Header: picture, name, status; their profile on the right
    const Rect header = r.Top(52.f);
    const Rect pic{ header.x + 4.f, header.CY() - 20.f, 40.f, 40.f };
    AvatarDot(ui, pic, presence);
    const float bw = 40.f;
    const float textX = pic.R() + 12.f, textW = header.R() - bw - 12.f - textX;
    ui.PushClip({ textX, header.y, textW, header.h });
    ProfileDraw::Name(ui, NameOf(p), { textX, header.y + 4.f, textW, 26.f }, p, 15.f);
    ui.PopClip();
    g.Text(FriendsService::StatusText(presence), { textX, header.y + 30.f, textW, 18.f },
           presence.Online() ? (presence.playing ? kPlaying : kOnline) : kSub, Mono(9.5f, presence.Online()).Ellipsis());
    {
        const Id id = ui.MakeId("msg-profile");
        if (MenuKit::FooterButton(ui, id, { header.R() - bw, header.CY() - 17.f, bw, 34.f }, L"", MenuKit::Kind::Secondary, true, wchar_t(0xE77B), kAccent))
            OpenViewer(p);
        ui.Tooltip(id, Loc::Get("acc_rank_open_profile"));
    }

    // The messages
    const Rect input = r.Bottom(38.f);
    const float noteH = canWrite ? 0.f : 22.f;
    const Rect area{ r.x, header.B() + 8.f, r.w, input.y - 10.f - noteH - header.B() - 8.f };
    Card(ui, area, kChat.WithAlpha(0.3f));
    std::vector<ChatView::Line> lines;
    const std::wstring name = NameOf(p);
    if (messages.OpenWith() == p.userId)
        for (const auto& m : messages.Messages()) {
            ChatView::Line line;
            line.mine = m.mine;
            line.pending = m.id == 0;
            line.read = m.read;
            line.author = m.mine ? L"" : name;
            line.profile = m.mine ? nullptr : &chatProfile;
            line.text = m.text;
            line.sentAt = line.pending ? m.fetchedAt : m.fetchedAt - m.ago;
            lines.push_back(std::move(line));
        }
    if (lines.empty() && messages.OpenWith() == p.userId && !messages.HistoryLoaded()) {
        Form::Spinner(ui, area.CX(), area.CY(), 9.f, kChat);
        ui.KeepAnimatingThrottled();
    } else {
        ChatView::Options options;
        options.accent = kChat;
        options.empty = Loc::Fmt("msg_empty_chat", name);
        options.showRead = true;
        ChatView::Messages(ui, ui.MakeId("msg-list"), area.Inset(6.f, 6.f), lines, options);
    }

    // Only friends can write to each other
    if (!canWrite)
        g.Text(Loc::Get("msg_friends_only"), { r.x + 4.f, input.y - 8.f - noteH, r.w - 8.f, noteH }, kSub, Mono(9.f).Ellipsis());
    if (ChatView::Input(ui, ui.MakeId("msg-input"), input, dmText, Loc::Fmt("msg_placeholder", name), kChat, 500, canWrite)) {
        auto token = alive;
        messages.Send(p.userId, dmText, [this, token](const std::wstring& message, bool ok) {
            if (*token && !ok) FriendMessage(message, false);
        });
        dmText.clear();
    }
}

} // namespace SpeedrunLauncher::Main
