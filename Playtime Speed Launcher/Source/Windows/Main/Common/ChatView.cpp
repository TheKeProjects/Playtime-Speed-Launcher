#include "Windows/Main/Common/ChatView.h"
#include <algorithm>
#include <cmath>
#include "Core/Async.h"
#include "Core/Text.h"
#include "Localization/Localization.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/MenuKit.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/TextBox.h"
#include "Windows/Main/Common/ProfileDraw.h"
#include "Windows/Main/Overlays/Account/AccountKit.h"

namespace SpeedrunLauncher::Main::ChatView {

using namespace UI;
using namespace AccountKit;

namespace {

constexpr float kPadX = 10.f, kPadY = 7.f, kAuthorH = 18.f, kGap = 4.f, kGroupGap = 10.f;

TextStyle BodyStyle() {
    TextStyle st = Mono(10.5f);
    st.wrap = true;
    st.valign = VAlign::Top;
    return st;
}

std::wstring Ago(double seconds) {
    if (seconds < 60.0) return Loc::Get("chat_now");
    const long long minutes = static_cast<long long>(seconds / 60.0);
    if (minutes < 60) return Loc::Fmt("chat_min", minutes);
    if (minutes < 60 * 24) return Loc::Fmt("chat_h", minutes / 60);
    return Loc::Fmt("chat_d", minutes / (60 * 24));
}

} // namespace

void Messages(Ui& ui, Id id, const Rect& r, const std::vector<Line>& lines, const Options& options) {
    auto& g = ui.g;
    if (lines.empty()) {
        const float bob = 3.f * std::sin(static_cast<float>(ui.Time()) * 1.6f);
        UI::Glyph(ui, options.emptyGlyph, { r.CX() - 30.f, r.CY() - 52.f + bob, 60.f, 44.f }, 28.f, kCardBorder);
        ui.KeepAnimatingThrottled();
        TextStyle st = Mono(10.f, true).Center();
        st.wrap = true;
        g.Text(options.empty, { r.x + 16.f, r.CY(), r.w - 32.f, 44.f }, kSub, st);
        return;
    }

    // New messages: jump to the newest (for a few frames: the scroll is clamped to the height the
    // content had in the previous frame, before the new bubbles were measured)
    size_t& shown = ui.Data<size_t>(HashId(id, 0xC4A7));
    double& stickUntil = ui.Data<double>(HashId(id, 0x57C1));
    if (shown != lines.size()) {
        shown = lines.size();
        stickUntil = ui.Time() + 0.25;
    }
    if (ui.Time() < stickUntil) {
        ScrollArea::ScrollTo(ui, id, 1e9f);
        ui.KeepAnimating();
    }

    const TextStyle body = BodyStyle();
    const float maxBubble = std::max(80.f, r.w * 0.78f - 10.f);
    ScrollArea scroll(ui, id, r);
    float y = r.y + 6.f;
    const double now = Async::Now();
    int lastRead = -1;
    if (options.showRead)
        for (int i = static_cast<int>(lines.size()) - 1; i >= 0; --i)
            if (lines[i].mine) {
                if (lines[i].read) lastRead = i;
                break;
            }
    for (size_t i = 0; i < lines.size(); ++i) {
        const Line& line = lines[i];
        const bool first = i == 0 || lines[i - 1].mine != line.mine || lines[i - 1].author != line.author;
        if (first && i > 0) y += kGroupGap - kGap;
        // Who wrote it (theirs), with when
        if (first && !line.mine && !line.author.empty()) {
            const float nameW = std::min(r.w * 0.6f, line.profile ? ProfileDraw::NameWidth(ui, line.author, *line.profile, 10.f) + 4.f
                                                                     : g.Measure(line.author, Mono(9.5f, true)).w + 4.f);
            ui.PushClip({ r.x + 8.f, y, nameW, kAuthorH });
            if (line.profile) ProfileDraw::Name(ui, line.author, { r.x + 8.f, y, nameW, kAuthorH }, *line.profile, 10.f);
            else g.Text(line.author, { r.x + 8.f, y, nameW, kAuthorH }, options.accent, Mono(9.5f, true));
            ui.PopClip();
            if (line.sentAt >= 0.0)
                g.Text(Ago(now - line.sentAt), { r.x + 14.f + nameW, y, 80.f, kAuthorH }, kSub.WithAlpha(0.7f), Mono(8.5f));
            y += kAuthorH;
        }
        const TextSize size = g.Measure(line.text, body, maxBubble - kPadX * 2.f);
        const float bw = std::min(maxBubble, size.w + kPadX * 2.f + 2.f), bh = size.h + kPadY * 2.f;
        const float bx = line.mine ? r.R() - 14.f - bw : r.x + 6.f;
        const Rect bubble{ bx, y, bw, bh };
        if (bubble.B() >= r.y + scroll.Offset() - 40.f && bubble.y <= r.B() + scroll.Offset() + 40.f) {
            const Color fill = line.mine ? options.accent.WithAlpha(line.pending ? 0.12f : 0.22f) : kRow;
            const Color edge = line.mine ? options.accent.WithAlpha(0.5f) : kCardBorder.WithAlpha(0.7f);
            g.Border(bubble, 10.f, fill, edge, 1.f);
            g.Text(line.text, bubble.Inset(kPadX, kPadY), line.pending ? kText.WithAlpha(0.6f) : kText, body);
        }
        y += bh + kGap;
        // Under our messages: "sending…" / "seen"
        if (line.mine && (line.pending || static_cast<int>(i) == lastRead)) {
            TextStyle st = Mono(8.5f).Right();
            g.Text(Loc::Get(line.pending ? "chat_sending" : "chat_seen"), { r.x, y - 1.f, r.w - 16.f, 14.f }, kSub.WithAlpha(0.8f), st);
            y += 14.f;
        }
    }
    scroll.SetContentSize(y - r.y + 6.f);
}

bool Input(Ui& ui, Id id, const Rect& r, std::wstring& text, const std::wstring& placeholder, Color accent, size_t maxLength, bool enabled) {
    TextBoxStyle style = Field(false, maxLength);
    style.placeholder = placeholder;
    style.focusBorder = accent;
    style.caret = accent;
    style.text = Mono(10.5f);
    style.padding = { 12.f, 8.f, 12.f, 8.f };
    const float bw = r.h;
    bool enter = false;
    if (enabled) TextBox(ui, id, { r.x, r.y, r.w - bw - 8.f, r.h }, text, style, &enter);
    else ui.g.Border({ r.x, r.y, r.w - bw - 8.f, r.h }, 8.f, style.background, style.border, 1.f);
    const bool has = !Text::Trim(text).empty();
    const bool clicked = MenuKit::FooterButton(ui, HashId(id, 1), { r.R() - bw, r.y, bw, r.h }, L"", MenuKit::Kind::Primary, enabled && has, wchar_t(0xE724), accent);
    if (has && enabled && (clicked || enter)) return true;
    return false;
}

} // namespace SpeedrunLauncher::Main::ChatView
