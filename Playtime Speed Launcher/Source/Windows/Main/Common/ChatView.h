#pragma once
#include <string>
#include <vector>
#include "Graphics/Rect.h"
#include "Services/Account/Profile.h"
#include "UI/Ui.h"

// A chat: speech bubbles (ours on the right, theirs on the left with who wrote it) that scroll to
// the newest message, and the field to write in. Used by the duel rooms and the private messages.
namespace SpeedrunLauncher::Main::ChatView {

struct Line {
    bool mine = false;
    bool pending = false;        // ours, still being sent
    bool read = false;           // ours, they've read it (private messages)
    std::wstring author;         // shown above their messages (empty = no author line)
    const Services::Account::Profile* profile = nullptr;   // styles the author's name (optional)
    std::wstring text;
    double sentAt = 0.0;         // Async::Now() when it was sent (for "2 min"), < 0 = unknown
};

struct Options {
    Color accent;
    std::wstring empty;          // what an empty chat says
    wchar_t emptyGlyph = 0xE8BD;
    bool showRead = false;       // "Seen" under our last message
};

// The messages in `r` (scrolled to the newest when new ones arrive)
void Messages(UI::Ui& ui, UI::Id id, const Rect& r, const std::vector<Line>& lines, const Options& options);
// The field and the send button; returns true when Enter / send was pressed with something written
// (the caller sends `text` and clears it)
bool Input(UI::Ui& ui, UI::Id id, const Rect& r, std::wstring& text, const std::wstring& placeholder, Color accent, size_t maxLength, bool enabled = true);

} // namespace SpeedrunLauncher::Main::ChatView
