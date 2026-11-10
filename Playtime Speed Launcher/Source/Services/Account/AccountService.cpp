#include "Services/Account/AccountService.h"
#include "Services/Account/PictureCompressor.h"
#include <variant>
#include <vector>
#include "App/Config.h"
#include "App/Paths.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Graphics/AnimatedImages.h"
#include "Localization/Localization.h"
#include "Services/Discord/DiscordOAuthService.h"
#include "Settings/HeartbeatSettings.h"
#include "UI/Window.h"

namespace SpeedrunLauncher::Services::Account {

namespace {

std::wstring AccountFile() { return Paths::DataFile(L"account.dat"); }
std::wstring ProfileCacheFile() { return Paths::DataFile(L"account_profile.json"); }

std::wstring PictureContentType(const std::wstring& file) {
    const std::wstring ext = Text::Lower(Files::Extension(file));
    if (ext == L".png") return L"image/png";
    if (ext == L".jpg" || ext == L".jpeg") return L"image/jpeg";
    if (ext == L".gif") return L"image/gif";
    if (ext == L".webp") return L"image/webp";
    if (ext == L".bmp") return L"image/bmp";
    return L"";
}

std::wstring ObjectUrl(const std::wstring& userId, const std::wstring& file) {
    return Api::Url(L"/storage/v1/object/" + std::wstring(Config::Supabase::ProfilesBucket) + L"/" + Text::UrlEncode(userId) + L"/" +
                    Text::UrlEncode(file));
}

std::wstring ProfileUrl(const std::wstring& query) { return Api::Url(L"/rest/v1/" + std::wstring(Config::Supabase::ProfilesTable) + query); }

struct Failure {
    std::string key;
    std::wstring detail;
};

} // namespace

AccountService& AccountService::Get() {
    static AccountService* instance = new AccountService();   // never destroyed: workers may still post to it at exit
    return *instance;
}

void AccountService::Initialize() {
    if (initialized) return;
    initialized = true;
    LoadAccount();
    if (!SignedIn()) return;
    LoadProfileCache();
    ReloadProfile();
    if (onAccountChanged) onAccountChanged();
}

std::wstring AccountService::TakeNotice(bool* ok) {
    std::wstring out;
    out.swap(notice);
    if (ok) *ok = noticeOk;
    return out;
}

// ── Waiting for the email confirmation ──────────────────────────────────────

namespace {
constexpr double kWaitLimitSeconds = 60.0 * 60.0;   // stop trying after an hour
void Wipe(std::wstring& secret) {
    SecureZeroMemory(secret.data(), secret.size() * sizeof(wchar_t));
    secret.clear();
}
} // namespace

void AccountService::StartWaiting(const std::wstring& mail, const std::wstring& password) {
    waiting = true;
    waitEmail = Text::Trim(mail);
    Wipe(waitPassword);
    waitPassword = password;
    waitStartedAt = Async::Now();
    ++waitGeneration;
    ScheduleWaitPoll(8000);
    Changed();
}

void AccountService::CancelWaiting() {
    if (!waiting) return;
    waiting = false;
    ++waitGeneration;
    Wipe(waitPassword);
    Changed();
}

void AccountService::ScheduleWaitPoll(unsigned delayMs) {
    const unsigned gen = waitGeneration;
    Async::After(delayMs, [this, gen] {
        if (gen == waitGeneration && waiting) PollConfirmation();
    });
}

// Signing in only works once the email is confirmed: try every few seconds (gently: the server
// limits sign-in attempts per IP)
void AccountService::PollConfirmation() {
    if (SignedIn() || Async::Now() - waitStartedAt > kWaitLimitSeconds) {
        CancelWaiting();
        return;
    }
    const unsigned gen = waitGeneration;
    Async::Run([mail = waitEmail, password = waitPassword]() mutable {
        Api::AuthResult result = Api::SignIn(mail, password);
        Wipe(password);
        return result;
    }, [this, gen](Api::AuthResult result) {
        if (gen != waitGeneration || !waiting) return;
        if (result.ok) {
            waiting = false;
            Wipe(waitPassword);
            Activate(result.session);
            notice = Loc::Get("acc_confirmed_signed_in");
            noticeOk = true;
            Changed();
            return;
        }
        if (result.network || result.errorKey == "acc_err_not_confirmed") {
            ScheduleWaitPoll(10000);
        } else if (result.errorKey == "acc_err_rate_limit") {
            ScheduleWaitPoll(45000);
        } else {
            // Something else (e.g. the account was deleted): stop and say why
            waiting = false;
            Wipe(waitPassword);
            notice = Translate(result.errorKey, result.errorDetail);
            noticeOk = false;
            Changed();
        }
    });
}

void AccountService::ResendConfirmation(std::function<void(const std::wstring&)> done) {
    if (!waiting || authBusy) return;
    authBusy = true;
    Changed();
    Async::Run([mail = waitEmail] { return Api::ResendConfirmation(mail); }, [this, done = std::move(done)](Api::AuthResult result) {
        authBusy = false;
        Changed();
        if (done) done(result.ok ? L"" : Translate(result.errorKey, result.errorDetail));
    });
}

void AccountService::Changed() {
    for (UI::Window* window : UI::Window::All()) window->Invalidate();
}

std::wstring AccountService::Translate(const std::string& key, const std::wstring& detail) const {
    if (key.empty()) return L"";
    if (key == "acc_err_server" && !detail.empty()) return Loc::Get(key) + L": " + detail;
    return Loc::Get(key);
}

// ── Sign in / out ───────────────────────────────────────────────────────────

void AccountService::SignIn(const std::wstring& mail, const std::wstring& password, std::function<void(const std::wstring&)> done) {
    if (authBusy) return;
    authBusy = true;
    Changed();
    Async::Run([mail, password] { return Api::SignIn(mail, password); }, [this, done = std::move(done)](Api::AuthResult result) {
        authBusy = false;
        if (result.ok) Activate(result.session);
        Changed();
        if (done) done(result.ok ? L"" : Translate(result.errorKey, result.errorDetail));
    });
}

void AccountService::SignUp(const std::wstring& mail, const std::wstring& password, std::function<void(const std::wstring&, bool)> done) {
    if (authBusy) return;
    authBusy = true;
    Changed();
    Async::Run([mail, password] { return Api::SignUp(mail, password); }, [this, mail, password, done = std::move(done)](Api::AuthResult result) {
        authBusy = false;
        if (result.ok && !result.needsConfirmation) Activate(result.session);
        // Confirmation needed: wait for it and sign in by itself (no typing the password again)
        if (result.ok && result.needsConfirmation) StartWaiting(mail, password);
        Changed();
        if (done) done(result.ok ? L"" : Translate(result.errorKey, result.errorDetail), result.ok && result.needsConfirmation);
    });
}

void AccountService::SendPasswordReset(const std::wstring& mail, std::function<void(const std::wstring&)> done) {
    if (authBusy) return;
    authBusy = true;
    Changed();
    Async::Run([mail] { return Api::SendPasswordReset(mail); }, [this, done = std::move(done)](Api::AuthResult result) {
        authBusy = false;
        Changed();
        if (done) done(result.ok ? L"" : Translate(result.errorKey, result.errorDetail));
    });
}

void AccountService::Activate(const Api::Session& newSession) {
    if (waiting) {
        waiting = false;
        ++waitGeneration;
        Wipe(waitPassword);
    }
    {
        std::lock_guard lock(sessionMutex);
        session = newSession;
        SaveAccountLocked();
    }
    ++generation;
    sessionDead = false;
    email = newSession.email;
    if (userId != newSession.userId) {
        profile = {};
        profileLoaded = false;
    }
    userId = newSession.userId;
    profile.userId = userId;
    signedIn = true;
    ReloadProfile();
    if (onAccountChanged) onAccountChanged();
}

void AccountService::SignOut() {
    if (!SignedIn()) return;
    Api::Session old;
    {
        std::lock_guard lock(sessionMutex);
        old = session;
    }
    Async::Run([old] { Api::SignOut(old); });
    SignOutLocal(L"");
}

void AccountService::SignOutLocal(const std::wstring& why) {
    {
        std::lock_guard lock(sessionMutex);
        session = {};
    }
    ++generation;
    signedIn = false;
    email.clear();
    userId.clear();
    profile = {};
    profileLoaded = false;
    saveBusy = linkBusy = false;
    discordCancel.Request();
    discordBusy = discordWaiting = false;
    notice = why;
    Files::Delete(AccountFile());
    Files::Delete(ProfileCacheFile());
    if (onAccountChanged) onAccountChanged();
    Changed();
}

// ── Requests with the user's token (worker threads) ─────────────────────────

bool AccountService::RefreshLocked() {
    if (session.refreshToken.empty()) return false;
    const Api::AuthResult auth = Api::Refresh(session.refreshToken);
    if (!auth.ok) {
        if (!auth.network && !sessionDead.exchange(true)) {
            // The refresh token is no good any more: this PC is signed out
            const unsigned gen = generation;
            Async::Post([this, gen] {
                if (gen == generation && SignedIn()) SignOutLocal(Loc::Get("acc_session_expired"));
            });
        }
        return false;
    }
    session.accessToken = auth.session.accessToken;
    session.refreshToken = auth.session.refreshToken;
    session.expiresAt = auth.session.expiresAt;
    SaveAccountLocked();
    return true;
}

Http::Response AccountService::Send(Http::Request request) {
    const auto extra = request.headers;
    for (int attempt = 0; attempt < 2; ++attempt) {
        Api::Session s;
        {
            std::lock_guard lock(sessionMutex);
            if (session.refreshToken.empty()) {
                Http::Response none;
                none.status = 401;
                return none;
            }
            if (session.NeedsRefresh() && !RefreshLocked()) {
                Http::Response failed;
                failed.status = sessionDead ? 401 : 0;
                return failed;
            }
            s = session;
        }
        request.headers = Api::AuthHeaders(s);
        request.headers.insert(request.headers.end(), extra.begin(), extra.end());
        Http::Response response = Http::Send(request);
        if (response.status != 401 || attempt == 1) return response;
        std::lock_guard lock(sessionMutex);
        // Only refresh when nobody did it meanwhile
        if (session.accessToken == s.accessToken && !RefreshLocked()) return response;
    }
    return {};
}

Http::Response AccountService::Rpc(const std::wstring& function, const Json::Value& body) {
    Http::Request request;
    request.method = L"POST";
    request.url = Api::Url(L"/rest/v1/rpc/" + function);
    request.body = body.Serialize();
    request.contentType = L"application/json";
    request.timeoutMs = 25000;
    return Send(std::move(request));
}

// ── Profile ─────────────────────────────────────────────────────────────────

void AccountService::ReloadProfile() {
    if (!SignedIn()) return;
    const unsigned gen = generation;
    const std::wstring uid = userId;
    Async::Run([this, uid] {
        Http::Request request;
        request.url = ProfileUrl(L"?select=*&user_id=eq." + Text::UrlEncode(uid));
        request.timeoutMs = 20000;
        const Http::Response response = Send(request);
        std::optional<Profile> out;
        if (response.Ok()) {
            const Json::Value rows = Json::Parse(std::string_view(response.body));
            if (rows.IsArray()) {
                Profile p = rows.Size() > 0 ? Profile::FromJson(rows[0]) : Profile{};
                p.userId = uid;
                out = std::move(p);
            }
        }
        return out;
    }, [this, gen](std::optional<Profile> loaded) {
        if (gen != generation || !loaded) return;
        profile = std::move(*loaded);
        profileLoaded = true;
        SaveProfileCache();
        Changed();
    });
}

void AccountService::SaveProfile(Profile edited, std::optional<std::wstring> newAvatar, std::optional<std::wstring> newBanner,
                                 std::function<void(const std::wstring&)> done) {
    if (!SignedIn() || saveBusy) return;
    // Check the pictures here so the error is immediate (big ones are shrunk before uploading)
    for (const auto* picked : { &newAvatar, &newBanner }) {
        if (!*picked || (*picked)->empty()) continue;
        const auto size = Files::Size(**picked);
        if (!size || PictureContentType(**picked).empty()) {
            if (done) done(Loc::Get("acc_err_picture"));
            return;
        }
        if (*size > Config::Supabase::MaxSourcePictureBytes) {
            if (done) done(Loc::Get("acc_err_too_big"));
            return;
        }
    }
    saveBusy = true;
    Changed();
    edited.userId = userId;
    edited.avatar = profile.avatar;
    edited.banner = profile.banner;
    struct Upload {
        std::wstring local;
        bool banner = false;
    };
    std::vector<Upload> uploads;
    std::vector<std::wstring> removed;
    const auto plan = [&](const std::optional<std::wstring>& picked, std::wstring& field, bool banner) {
        if (!picked) return;
        if (!field.empty()) removed.push_back(field);
        field.clear();
        if (!picked->empty()) uploads.push_back({ *picked, banner });
    };
    plan(newAvatar, edited.avatar, false);
    plan(newBanner, edited.banner, true);

    struct Saved {
        Profile profile;
        std::vector<std::pair<std::wstring, std::wstring>> seeds;   // uploaded file name -> local copy
    };
    const unsigned gen = generation;
    const std::wstring uid = userId;
    Async::Run([this, uid, edited, uploads, removed]() mutable -> std::variant<Saved, Failure> {
        Saved out;
        std::vector<std::wstring> temporary;
        const auto cleanup = [&] {
            for (const auto& file : temporary) Files::Delete(file);
        };
        for (const auto& up : uploads) {
            // Shrink it first (big GIFs are scaled down and re-encoded)
            const PictureCompressor::Result picture = PictureCompressor::Prepare(up.local, up.banner);
            if (!picture.ok) {
                cleanup();
                return Failure{ picture.errorKey, L"" };
            }
            if (picture.temporary) temporary.push_back(picture.path);
            const auto bytes = Files::ReadBytes(picture.path);
            if (!bytes) {
                cleanup();
                return Failure{ "acc_err_picture", L"" };
            }
            // New pictures get a new name (the URL changes, so nobody keeps an old cached copy);
            // a new name also means no "upsert", which would need more storage permissions
            const std::wstring file = std::wstring(up.banner ? L"banner_" : L"avatar_") + Settings::NewGuid().substr(0, 13) +
                                      Text::Lower(Files::Extension(picture.path));
            Http::Request request;
            request.method = L"POST";
            request.url = ObjectUrl(uid, file);
            request.headers = { { L"cache-control", L"31536000" } };
            request.body = *bytes;
            request.contentType = PictureContentType(picture.path);
            request.timeoutMs = 120000;
            const Http::Response response = Send(request);
            if (!response.Ok()) {
                cleanup();
                std::wstring detail;
                const std::string key = Api::ErrorKey(response, &detail);
                return Failure{ key, detail };
            }
            (up.banner ? edited.banner : edited.avatar) = file;
            // Keep a copy of what went up, so this PC doesn't download it back
            const std::wstring keep = Files::UniqueTempFile(Files::Extension(picture.path));
            if (Files::Copy(picture.path, keep, true)) out.seeds.emplace_back(file, keep);
        }
        cleanup();
        Http::Request request;
        request.method = L"POST";
        request.url = ProfileUrl(L"?on_conflict=user_id&select=*");
        request.headers = { { L"Prefer", L"resolution=merge-duplicates,return=representation" } };
        request.body = edited.EditableJson().Serialize();
        request.contentType = L"application/json";
        request.timeoutMs = 25000;
        const Http::Response response = Send(request);
        if (!response.Ok()) {
            for (const auto& seed : out.seeds) Files::Delete(seed.second);
            std::wstring detail;
            const std::string key = Api::ErrorKey(response, &detail);
            return Failure{ key, detail };
        }
        const Json::Value rows = Json::Parse(std::string_view(response.body));
        out.profile = rows.IsArray() && rows.Size() > 0 ? Profile::FromJson(rows[0]) : edited;
        out.profile.userId = uid;
        // Old pictures nobody points at any more (best effort)
        for (const auto& file : removed) {
            Http::Request del;
            del.method = L"DELETE";
            del.url = ObjectUrl(uid, file);
            del.timeoutMs = 15000;
            Send(del);
        }
        return out;
    }, [this, gen, done = std::move(done)](std::variant<Saved, Failure> result) {
        if (gen != generation) return;
        saveBusy = false;
        if (auto* failure = std::get_if<Failure>(&result)) {
            Changed();
            if (done) done(Translate(failure->key, failure->detail));
            return;
        }
        Saved& saved = std::get<Saved>(result);
        profile = std::move(saved.profile);
        profileLoaded = true;
        for (const auto& seed : saved.seeds) {
            AnimatedImages::Seed(PictureUrl(userId, seed.first), seed.second);
            Files::Delete(seed.second);
        }
        SaveProfileCache();
        Changed();
        if (done) done(L"");
    });
}

void AccountService::LinkSpeedrun(const std::wstring& apiKey, std::function<void(const std::wstring&)> done) {
    if (!SignedIn() || linkBusy) return;
    const std::wstring key = Text::Trim(apiKey);
    if (key.empty()) {
        if (done) done(Loc::Get("acc_src_err_bad_key"));
        return;
    }
    linkBusy = true;
    Changed();
    const unsigned gen = generation;
    Async::Run([this, key] {
        Json::Value body = Json::Value::MakeObject();
        body.Set("p_api_key", key);
        return Rpc(L"link_speedrun", body);
    }, [this, gen, done = std::move(done)](Http::Response response) {
        if (gen != generation) return;
        linkBusy = false;
        std::wstring error;
        if (!response.Ok()) {
            std::wstring detail;
            error = Translate(Api::ErrorKey(response, &detail), detail);
        } else {
            const Json::Value json = Json::Parse(std::string_view(response.body));
            const std::string code = json["error"].Str();
            if (code == "bad_key") error = Loc::Get("acc_src_err_bad_key");
            else if (code == "taken") error = Loc::Get("acc_src_err_taken");
            else if (!code.empty()) error = Loc::Get("acc_src_err_unreachable");
            else {
                profile.speedrunId = json["id"].WStr();
                profile.speedrunName = json["name"].WStr();
                profile.speedrunUrl = json["url"].WStr();
                profile.speedrunAvatar = json["avatar"].WStr();
                SaveProfileCache();
            }
        }
        Changed();
        if (done) done(error);
    });
}

void AccountService::UnlinkSpeedrun(std::function<void(const std::wstring&)> done) {
    if (!SignedIn() || linkBusy) return;
    linkBusy = true;
    Changed();
    const unsigned gen = generation;
    Async::Run([this] { return Rpc(L"unlink_speedrun", Json::Value::MakeObject()); }, [this, gen, done = std::move(done)](Http::Response response) {
        if (gen != generation) return;
        linkBusy = false;
        std::wstring error;
        if (response.Ok()) {
            profile.speedrunId.clear();
            profile.speedrunName.clear();
            profile.speedrunUrl.clear();
            profile.speedrunAvatar.clear();
            SaveProfileCache();
        } else {
            std::wstring detail;
            error = Translate(Api::ErrorKey(response, &detail), detail);
        }
        Changed();
        if (done) done(error);
    });
}

namespace {
struct DiscordLinkResult {
    bool tokenArrived = false;
    Http::Response response;
};
} // namespace

void AccountService::LinkDiscord(std::function<void(const std::wstring&)> done) {
    if (!SignedIn() || discordBusy) return;
    discordCancel.Request();
    discordCancel = Async::Cancel();
    const Async::Cancel cancel = discordCancel;
    discordBusy = discordWaiting = true;
    Changed();
    const unsigned gen = generation;
    Async::Run([this, cancel] {
        DiscordLinkResult out;
        // The browser ends on the website's page, which says how it went (with the Discord name and picture)
        const std::wstring page = Config::Links::DiscordLinkedPage;
        const auto failed = [&](const wchar_t* reason) { return DiscordOAuth::TokenResult{ false, page + L"?status=error&reason=" + reason }; };
        out.tokenArrived = DiscordOAuth::Authorize(cancel, [&](const std::string& token) {
            Json::Value body = Json::Value::MakeObject();
            body.Set("p_token", Text::Widen(token));
            out.response = Rpc(L"link_discord", body);
            if (!out.response.Ok()) return failed(L"unreachable");
            const Json::Value json = Json::Parse(std::string_view(out.response.body));
            const std::string code = json["error"].Str();
            if (code == "taken") return failed(L"taken");
            if (code == "bad_token") return failed(L"denied");
            if (!code.empty()) return failed(L"unreachable");
            std::wstring url = page + L"?status=ok&name=" + Text::UrlEncode(json["name"].WStr());
            if (!json["avatar"].WStr().empty()) url += L"&avatar=" + Text::UrlEncode(json["avatar"].WStr());
            return DiscordOAuth::TokenResult{ true, url };
        }, page + L"?status=error&reason=denied");
        return out;
    }, [this, gen, cancel, done = std::move(done)](DiscordLinkResult out) {
        if (gen != generation || cancel.Requested()) return;
        discordBusy = discordWaiting = false;
        std::wstring error;
        if (!out.tokenArrived) {
            error = Loc::Get("acc_dc_err_not_authorized");
        } else if (!out.response.Ok()) {
            std::wstring detail;
            error = Translate(Api::ErrorKey(out.response, &detail), detail);
        } else {
            const Json::Value json = Json::Parse(std::string_view(out.response.body));
            const std::string code = json["error"].Str();
            if (code == "taken") error = Loc::Get("acc_dc_err_taken");
            else if (code == "bad_token") error = Loc::Get("acc_dc_err_not_authorized");
            else if (!code.empty()) error = Loc::Get("acc_dc_err_unreachable");
            else {
                profile.discordId = json["id"].WStr();
                profile.discordName = json["name"].WStr();
                profile.discordAvatar = json["avatar"].WStr();
                SaveProfileCache();
            }
        }
        Changed();
        if (done) done(error);
    });
}

void AccountService::CancelDiscordLink() {
    if (!discordWaiting) return;
    discordCancel.Request();
    discordBusy = discordWaiting = false;
    Changed();
}

void AccountService::UnlinkDiscord(std::function<void(const std::wstring&)> done) {
    if (!SignedIn() || discordBusy) return;
    discordBusy = true;
    Changed();
    const unsigned gen = generation;
    Async::Run([this] { return Rpc(L"unlink_discord", Json::Value::MakeObject()); }, [this, gen, done = std::move(done)](Http::Response response) {
        if (gen != generation) return;
        discordBusy = false;
        std::wstring error;
        if (response.Ok()) {
            profile.discordId.clear();
            profile.discordName.clear();
            profile.discordAvatar.clear();
            SaveProfileCache();
        } else {
            std::wstring detail;
            error = Translate(Api::ErrorKey(response, &detail), detail);
        }
        Changed();
        if (done) done(error);
    });
}

// ── Persistence ─────────────────────────────────────────────────────────────

void AccountService::LoadAccount() {
    const auto bytes = Files::ReadBytes(AccountFile());
    if (!bytes) return;
    const auto plain = System::Unprotect(*bytes);
    if (!plain) return;
    bool ok = false;
    const Json::Value json = Json::Parse(std::string_view(*plain), &ok);
    if (!ok || !json["refreshToken"].IsString() || json["refreshToken"].Str().empty()) return;
    std::lock_guard lock(sessionMutex);
    session.refreshToken = json["refreshToken"].Str();
    session.userId = json["userId"].WStr();
    session.email = json["email"].WStr();
    email = session.email;
    userId = session.userId;
    profile.userId = userId;
    signedIn = true;
}

void AccountService::SaveAccountLocked() const {
    if (session.refreshToken.empty()) return;
    Json::Value json = Json::Value::MakeObject();
    json.Set("email", session.email);
    json.Set("userId", session.userId);
    json.Set("refreshToken", session.refreshToken);
    // DPAPI: only this Windows user on this PC can read the token
    const auto protectedBytes = System::Protect(json.Serialize());
    if (!protectedBytes) return;
    const std::wstring temp = AccountFile() + L".tmp";
    if (Files::WriteBytes(temp, *protectedBytes)) Files::Move(temp, AccountFile(), true);
}

void AccountService::LoadProfileCache() {
    const auto text = Files::ReadText(ProfileCacheFile());
    if (!text) return;
    bool ok = false;
    const Json::Value json = Json::Parse(std::wstring_view(*text), &ok);
    if (!ok || json["user_id"].WStr() != userId) return;
    profile = Profile::FromJson(json);
    profile.userId = userId;
}

void AccountService::SaveProfileCache() const {
    if (!SignedIn()) return;
    Files::WriteText(ProfileCacheFile(), Text::Widen(profile.CacheJson().Serialize()));
}

} // namespace SpeedrunLauncher::Services::Account
