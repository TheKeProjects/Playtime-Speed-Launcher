#pragma once
#include <windows.h>
#include <functional>
#include <memory>
#include <optional>
#include "Core/Async.h"
#include "Services/Discord/DiscordOAuthService.h"
#include "UI/Layout.h"
#include "UI/Ui.h"

namespace SpeedrunLauncher::Main {

// "Connect with Discord" block of the bug report / hand-mod submission forms: a connect button
// (+ "?" info button), the "waiting for the browser…" line with Cancel, or the connected
// username with a disconnect ✕. The OAuth flow runs on a worker thread.
// Colours of the block (purple on the bug report, blue-grey on the hand-mod submission).
struct DiscordIdentityStyle {
    Color buttonBackground = Hex(0x0E0818);
    Color border = Hex(0x2A1050);
    Color glyphText = Hex(0x7722AA);   // connect button glyph + text
    Color accent = Hex(0x5522AA);      // "?", waiting line, cancel
    float marginBottom = 14.f;
    static DiscordIdentityStyle BugReport() { return {}; }
    static DiscordIdentityStyle HandMods() { return { Hex(0x0A1825), Hex(0x0D2535), Hex(0x3A6A8A), Hex(0x3A6A8A), 16.f }; }
};

class DiscordIdentity {
public:
    ~DiscordIdentity() {
        *alive = false;
        CancelAuth();
    }
    void Reset();   // back to the cached login (discord_user.json), if any
    const std::optional<Services::DiscordOAuth::User>& User() const { return user; }
    bool Waiting() const { return waiting; }
    void Connect();
    void CancelAuth();
    void Disconnect();

    // Draws the current state into `col` (Margin bottom 14 included). `onInfo` = the "?" button.
    void Draw(UI::Ui& ui, UI::Column& col, const std::function<void()>& onInfo, const DiscordIdentityStyle& style = {});

private:
    std::optional<Services::DiscordOAuth::User> user;
    bool waiting = false;
    Async::Cancel cancel;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};

// "Discord required" explainer before sending (intro line + the identify permission + what it
// can't see). Calls onConnect when the user chooses to connect.
void ShowDiscordRequired(HWND owner, const char* introKey, std::function<void()> onConnect);

// The "why Discord?" popup of the "?" buttons, drawn over everything (sets `open` false on close).
void DrawDiscordInfoPopup(UI::Ui& ui, const Rect& screen, const char* whyKey, bool& open);

} // namespace SpeedrunLauncher::Main
