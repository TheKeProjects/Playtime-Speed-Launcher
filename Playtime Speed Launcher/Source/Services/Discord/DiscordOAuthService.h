#pragma once
#include <functional>
#include <optional>
#include <string>
#include "Core/Async.h"

// Discord OAuth2 implicit grant (scope: identify) through the system browser and a local
// listener on http://localhost:45678/callback.
namespace SpeedrunLauncher::Services::DiscordOAuth {

struct User {
    std::wstring id, username;
};

// Opens the browser and waits (up to 5 minutes, or until cancelled) for the user to authorize.
// Blocking: call on a worker thread.
std::optional<User> Authenticate(const Async::Cancel& cancel);
// Same browser flow, but hands the access token to `onToken` (on this worker thread) instead of
// reading the user here. The browser then shows "Connected!" / "Not connected", or goes to
// `redirectUrl` when it's set. `deniedUrl`: where it goes when the user cancels on Discord (empty =
// the built-in page). Returns whether a token arrived (false = cancelled, timed out, denied or the
// browser couldn't be opened).
struct TokenResult {
    bool ok = false;
    std::wstring redirectUrl;
};
bool Authorize(const Async::Cancel& cancel, const std::function<TokenResult(const std::string& accessToken)>& onToken,
               const std::wstring& deniedUrl = L"");

std::optional<User> LoadCached();   // discord_user.json
void SaveCached(const User& user);
void ClearCached();

} // namespace SpeedrunLauncher::Services::DiscordOAuth
