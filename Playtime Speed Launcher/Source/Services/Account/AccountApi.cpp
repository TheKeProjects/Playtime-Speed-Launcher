#include "Services/Account/AccountApi.h"
#include <ctime>
#include "App/Config.h"
#include "Core/Text.h"

namespace SpeedrunLauncher::Services::Account::Api {

namespace {

void ReadUser(const Json::Value& user, Session& session) {
    session.userId = user["id"].WStr();
    session.email = user["email"].WStr();
}

AuthResult PostAuth(const std::wstring& path, const Json::Value& body, bool expectSession) {
    Http::Request request;
    request.method = L"POST";
    request.url = Url(path);
    request.headers = BaseHeaders();
    request.body = body.Serialize();
    request.contentType = L"application/json";
    request.timeoutMs = 20000;
    const Http::Response response = Http::Send(request);

    AuthResult out;
    if (!response.Ok()) {
        out.network = response.status == 0;
        out.errorKey = ErrorKey(response, &out.errorDetail);
        return out;
    }
    const Json::Value json = Json::Parse(std::string_view(response.body));
    if (json["access_token"].IsString()) {
        out.ok = true;
        out.session.accessToken = json["access_token"].Str();
        out.session.refreshToken = json["refresh_token"].Str();
        out.session.expiresAt = json["expires_at"].IsNumber() ? json["expires_at"].Int() : UnixNow() + json["expires_in"].Int(3600);
        ReadUser(json["user"], out.session);
        return out;
    }
    if (expectSession) {
        out.errorKey = "acc_err_unexpected";
        return out;
    }
    // Sign up with email confirmation on: only the user comes back. An empty identity list means
    // the email already has an account (Supabase hides that on purpose).
    if (json["identities"].IsArray() && json["identities"].Size() == 0) {
        out.errorKey = "acc_err_exists";
        return out;
    }
    out.ok = true;
    out.needsConfirmation = true;
    return out;
}

Json::Value Credentials(const std::wstring& email, const std::wstring& password) {
    Json::Value body = Json::Value::MakeObject();
    body.Set("email", Text::Trim(email));
    body.Set("password", password);
    return body;
}

} // namespace

long long UnixNow() { return static_cast<long long>(std::time(nullptr)); }

bool Session::NeedsRefresh() const { return accessToken.empty() || expiresAt - UnixNow() < 120; }

std::wstring Url(const std::wstring& path) { return std::wstring(Config::Supabase::Url) + path; }

Headers BaseHeaders() { return { { L"apikey", Config::Supabase::AnonKey } }; }

Headers AuthHeaders(const Session& session) {
    Headers headers = BaseHeaders();
    headers.emplace_back(L"Authorization", L"Bearer " + Text::Widen(session.accessToken));
    return headers;
}

std::string ErrorKey(const Http::Response& response, std::wstring* detail) {
    if (response.status == 0) return "acc_err_network";
    const Json::Value json = Json::Parse(std::string_view(response.body));
    std::string code = json["error_code"].Str();
    if (code.empty() && json["code"].IsString()) code = json["code"].Str();
    std::wstring message;
    for (const char* key : { "msg", "error_description", "message", "error" })
        if (message.empty() && json[key].IsString()) message = json[key].WStr();
    const std::wstring lower = Text::Lower(message);
    if (detail) *detail = message;

    if (code == "invalid_credentials" || Text::Contains(lower, L"invalid login credentials")) return "acc_err_credentials";
    if (code == "email_not_confirmed" || Text::Contains(lower, L"email not confirmed")) return "acc_err_not_confirmed";
    if (code == "user_already_exists" || code == "email_exists" || Text::Contains(lower, L"already registered")) return "acc_err_exists";
    if (code == "weak_password" || Text::Contains(lower, L"password should be")) return "acc_err_weak_password";
    if (code == "email_address_invalid" || code == "validation_failed" || Text::Contains(lower, L"invalid format")) return "acc_err_bad_email";
    if (code.find("rate_limit") != std::string::npos || response.status == 429) return "acc_err_rate_limit";
    if (code == "signup_disabled") return "acc_err_signup_disabled";
    if (code == "email_provider_disabled") return "acc_err_signup_disabled";
    if (response.status == 404 || code == "PGRST202" || code == "PGRST205" || code == "42P01" || code == "42883") return "acc_err_not_setup";
    if (response.status == 413) return "acc_err_too_big";
    return "acc_err_server";
}

AuthResult SignIn(const std::wstring& email, const std::wstring& password) {
    return PostAuth(L"/auth/v1/token?grant_type=password", Credentials(email, password), true);
}

AuthResult SignUp(const std::wstring& email, const std::wstring& password) {
    return PostAuth(L"/auth/v1/signup?redirect_to=" + Text::UrlEncode(std::wstring(Config::Links::AccountEmailPage)), Credentials(email, password), false);
}

AuthResult Refresh(const std::string& refreshToken) {
    Json::Value body = Json::Value::MakeObject();
    body.Set("refresh_token", refreshToken);
    return PostAuth(L"/auth/v1/token?grant_type=refresh_token", body, true);
}

AuthResult SendPasswordReset(const std::wstring& email) {
    Json::Value body = Json::Value::MakeObject();
    body.Set("email", Text::Trim(email));
    Http::Request request;
    request.method = L"POST";
    request.url = Url(L"/auth/v1/recover?redirect_to=" + Text::UrlEncode(std::wstring(Config::Links::AccountEmailPage)));
    request.headers = BaseHeaders();
    request.body = body.Serialize();
    request.contentType = L"application/json";
    request.timeoutMs = 20000;
    const Http::Response response = Http::Send(request);
    AuthResult out;
    out.ok = response.Ok();
    if (!out.ok) {
        out.network = response.status == 0;
        out.errorKey = ErrorKey(response, &out.errorDetail);
    }
    return out;
}

AuthResult ResendConfirmation(const std::wstring& email) {
    Json::Value body = Json::Value::MakeObject();
    body.Set("type", "signup");
    body.Set("email", Text::Trim(email));
    Http::Request request;
    request.method = L"POST";
    request.url = Url(L"/auth/v1/resend?redirect_to=" + Text::UrlEncode(std::wstring(Config::Links::AccountEmailPage)));
    request.headers = BaseHeaders();
    request.body = body.Serialize();
    request.contentType = L"application/json";
    request.timeoutMs = 20000;
    const Http::Response response = Http::Send(request);
    AuthResult out;
    out.ok = response.Ok();
    if (!out.ok) {
        out.network = response.status == 0;
        out.errorKey = ErrorKey(response, &out.errorDetail);
    }
    return out;
}

void SignOut(const Session& session) {
    if (session.accessToken.empty()) return;
    Http::Request request;
    request.method = L"POST";
    request.url = Url(L"/auth/v1/logout");
    request.headers = AuthHeaders(session);
    request.timeoutMs = 10000;
    Http::Send(request);
}

} // namespace SpeedrunLauncher::Services::Account::Api
