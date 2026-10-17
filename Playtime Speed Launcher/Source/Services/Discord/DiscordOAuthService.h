#pragma once
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

std::optional<User> LoadCached();   // discord_user.json
void SaveCached(const User& user);
void ClearCached();

} // namespace SpeedrunLauncher::Services::DiscordOAuth
