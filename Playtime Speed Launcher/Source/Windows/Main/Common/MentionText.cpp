#include "Windows/Main/Common/MentionText.h"
#include <windows.h>
#include <algorithm>
#include <map>
#include <vector>
#include "App/Config.h"
#include "Core/Process.h"
#include "UI/Style.h"

namespace SpeedrunLauncher::Main {

using namespace UI;

namespace {

// Contributors credited in the changelogs → Discord user ids
const std::map<std::wstring, std::wstring>& DiscordUsers() { return Config::Discord::ContributorIds(); }

constexpr Color kDiscord = Rgba(88, 101, 242);

bool IsWordChar(wchar_t c) { return c == L'_' || IsCharAlphaNumericW(c); }   // regex \w (Unicode letters, digits, _)

struct Token {
    std::wstring text;
    bool mention = false;
    bool newline = false;
    bool space = false;
};

std::vector<Token> Tokenize(const std::wstring& text) {
    std::vector<Token> tokens;
    size_t i = 0;
    while (i < text.size()) {
        const wchar_t c = text[i];
        if (c == L'\n') {
            tokens.push_back({ L"", false, true, false });
            ++i;
        } else if (c == L'\r') {
            ++i;
        } else if (c == L'@' && i + 1 < text.size() && IsWordChar(text[i + 1])) {
            size_t j = i + 1;
            while (j < text.size() && IsWordChar(text[j])) ++j;
            tokens.push_back({ text.substr(i, j - i), true, false, false });
            i = j;
        } else if (c == L' ' || c == L'\t') {
            size_t j = i;
            while (j < text.size() && (text[j] == L' ' || text[j] == L'\t')) ++j;
            tokens.push_back({ text.substr(i, j - i), false, false, true });
            i = j;
        } else {
            size_t j = i;
            while (j < text.size() && text[j] != L' ' && text[j] != L'\t' && text[j] != L'\n' && text[j] != L'\r' &&
                   !(text[j] == L'@' && j + 1 < text.size() && IsWordChar(text[j + 1])))
                ++j;
            tokens.push_back({ text.substr(i, j - i), false, false, false });
            i = j;
        }
    }
    return tokens;
}

} // namespace

float MentionText(Ui& ui, Id id, float x, float y, float width, const std::wstring& text, float fontSize, Color color, bool draw) {
    auto& g = ui.g;
    const TextStyle body = Mono(fontSize);
    TextStyle chipText = Mono(10.f, true);
    const float lineH = g.Measure(L"Ag", body).h;
    const float chipH = g.Measure(L"Ag", chipText).h + 2.f + 2.f;
    const float rowH = std::max(lineH, chipH);
    const bool hasMention = text.find(L'@') != std::wstring::npos;
    const float step = hasMention ? rowH : lineH;

    float cx = x, cy = y;
    int chipIndex = 0;
    bool wrapped = false;   // the current line started because the previous one was full
    const auto newLine = [&](bool byWrap) {
        cx = x;
        cy += step;
        wrapped = byWrap;
    };
    for (const auto& t : Tokenize(text)) {
        if (t.newline) {
            newLine(false);
            continue;
        }
        if (t.mention) {
            const float w = g.Measure(t.text, chipText).w + 10.f + 2.f;
            if (cx + 2.f + w > x + width && cx > x) newLine(true);
            const Rect chip{ cx + 2.f, cy + (step - chipH) * 0.5f, w, chipH };
            if (!draw) {
                cx += w + 4.f;
                continue;
            }
            const auto user = DiscordUsers().find(t.text.substr(1));
            const Interaction it = ui.Interact(HashId(chipIndex++, id), chip, user != DiscordUsers().end(), false);
            if (user != DiscordUsers().end() && it.hovered) ui.SetCursor(IDC_HAND);
            g.Border(chip, 3.f, kDiscord.WithAlpha((it.hovered ? 50.f : 30.f) / 255.f), kDiscord.WithAlpha(80.f / 255.f), 1.f);
            g.Text(t.text, chip, kDiscord, TextStyle(chipText).Center());
            if (it.held && ui.In().pressed[0] && user != DiscordUsers().end()) Process::OpenUrl(Config::Discord::UserProfileUrl + user->second);
            cx += w + 4.f;
            continue;
        }
        const float w = g.Measure(t.text, body).w;
        if (t.space) {
            if (cx > x || !wrapped) cx += w;   // spaces that would start a wrapped line are dropped
            continue;
        }
        if (cx + w > x + width && cx > x) newLine(true);
        if (w > width) {
            // A single word wider than the line: let DirectWrite break it
            TextStyle longWord = body;
            longWord.wrap = true;
            longWord.valign = VAlign::Top;
            const float h = g.Measure(t.text, longWord, width).h;
            if (draw) g.Text(t.text, { x, cy, width, h }, color, longWord);
            cy += h - step;
            newLine(true);
            continue;
        }
        if (draw) g.Text(t.text, { cx, cy + (step - lineH) * 0.5f, w + 1.f, lineH }, color, body);
        cx += w;
    }
    return cy + step - y;
}

} // namespace SpeedrunLauncher::Main
