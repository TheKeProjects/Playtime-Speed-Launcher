#pragma once
#include <string>
#include <utility>
#include <vector>
#include "Core/Http.h"
#include "Core/Json.h"

// Blocking calls to the launcher's Supabase Auth (email + password accounts). Worker threads only.
// Errors come back as translation keys ("acc_err_*") so the UI thread translates them.
namespace SpeedrunLauncher::Services::Account::Api {

using Headers = std::vector<std::pair<std::wstring, std::wstring>>;

struct Session {
    std::string accessToken;     // JWT, about an hour
    std::string refreshToken;    // single use: every refresh hands out a new one
    long long expiresAt = 0;     // unix seconds
    std::wstring userId;
    std::wstring email;

    bool NeedsRefresh() const;
};

struct AuthResult {
    bool ok = false;
    bool network = false;            // no answer from the server
    bool needsConfirmation = false;  // sign up: the email must be confirmed before signing in
    Session session;
    std::string errorKey;            // "acc_err_*"
    std::wstring errorDetail;        // the server's own message when there's no better key
};

AuthResult SignIn(const std::wstring& email, const std::wstring& password);
AuthResult SignUp(const std::wstring& email, const std::wstring& password);
AuthResult Refresh(const std::string& refreshToken);
AuthResult SendPasswordReset(const std::wstring& email);
AuthResult ResendConfirmation(const std::wstring& email);   // the "confirm your email" email again
void SignOut(const Session& session);   // revokes the refresh token (best effort)

std::wstring Url(const std::wstring& path);
Headers BaseHeaders();
Headers AuthHeaders(const Session& session);
// The error key for a failed response (+ the server's text in `detail`)
std::string ErrorKey(const Http::Response& response, std::wstring* detail = nullptr);
long long UnixNow();

} // namespace SpeedrunLauncher::Services::Account::Api
