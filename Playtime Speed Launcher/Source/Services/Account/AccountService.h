#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include "Core/Async.h"
#include "Core/Http.h"
#include "Core/Json.h"
#include "Services/Account/AccountApi.h"
#include "Services/Account/Profile.h"

// The launcher account (UI-thread object; the network runs on workers).
//
// Email + password accounts on the launcher's Supabase project (Tools\SupabaseAccounts.sql). The
// session is kept on this PC with DPAPI (account.dat), the profile in launcher_profiles (public, so
// the community ranking can show it) and its pictures in the public launcher-profiles bucket.
// While signed in, the playtime of this PC is added to the account (ChapterPlaytimeStore), so the
// hours survive formatting the PC or moving to another one.
namespace SpeedrunLauncher::Services::Account {

class AccountService {
public:
    static AccountService& Get();
    // Loads the saved session and refreshes the profile (call once at startup)
    void Initialize();

    bool SignedIn() const { return signedIn.load(); }
    const std::wstring& Email() const { return email; }
    const std::wstring& UserId() const { return userId; }
    const Profile& Me() const { return profile; }
    bool ProfileLoaded() const { return profileLoaded; }
    bool AuthBusy() const { return authBusy; }
    bool SaveBusy() const { return saveBusy; }
    bool LinkBusy() const { return linkBusy; }
    bool DiscordBusy() const { return discordBusy; }
    bool DiscordWaiting() const { return discordWaiting; }   // the browser is open, waiting for the user
    // Shown once by the account menu (e.g. "the session expired"); empty when there's nothing
    std::wstring TakeNotice(bool* ok = nullptr);

    // After creating an account that needs its email confirmed: the launcher keeps trying to sign
    // in (the password stays in memory only) and signs in by itself once the link is opened.
    bool WaitingConfirmation() const { return waiting; }
    const std::wstring& WaitingEmail() const { return waitEmail; }
    void CancelWaiting();
    void ResendConfirmation(std::function<void(const std::wstring& error)> done);

    // Callbacks run on the UI thread with a translated error (empty = it worked)
    void SignIn(const std::wstring& email, const std::wstring& password, std::function<void(const std::wstring& error)> done);
    // `confirm` = the account was created but its email must be confirmed before signing in
    void SignUp(const std::wstring& email, const std::wstring& password, std::function<void(const std::wstring& error, bool confirm)> done);
    void SendPasswordReset(const std::wstring& email, std::function<void(const std::wstring& error)> done);
    void SignOut();
    // Saves the profile. Pictures: nullopt = keep, empty = remove, else a local file to upload.
    void SaveProfile(Profile edited, std::optional<std::wstring> newAvatar, std::optional<std::wstring> newBanner,
                     std::function<void(const std::wstring& error)> done);
    // Proves the speedrun.com account with its API key (checked by the server, never stored)
    void LinkSpeedrun(const std::wstring& apiKey, std::function<void(const std::wstring& error)> done);
    void UnlinkSpeedrun(std::function<void(const std::wstring& error)> done);
    // Signs in with Discord in the browser; the server checks the token with Discord (never stored).
    // Cancelling (or closing the browser) ends without calling `done`.
    void LinkDiscord(std::function<void(const std::wstring& error)> done);
    void CancelDiscordLink();
    void UnlinkDiscord(std::function<void(const std::wstring& error)> done);
    void ReloadProfile();

    // ── Worker threads ──
    // Sends with the user's token, refreshing it when needed. Status 401 when signed out.
    Http::Response Send(Http::Request request);
    Http::Response Rpc(const std::wstring& function, const Json::Value& body);

    // Signed in / out (the playtime store and the screens listen)
    std::function<void()> onAccountChanged;

private:
    AccountService() = default;
    void Activate(const Api::Session& session);
    void SignOutLocal(const std::wstring& notice);
    void Changed();
    bool RefreshLocked();   // sessionMutex held
    std::wstring Translate(const std::string& key, const std::wstring& detail) const;

    void StartWaiting(const std::wstring& email, const std::wstring& password);
    void ScheduleWaitPoll(unsigned delayMs);
    void PollConfirmation();

    void LoadAccount();
    void SaveAccountLocked() const;   // sessionMutex held
    void LoadProfileCache();
    void SaveProfileCache() const;

    // Tokens: shared with workers
    mutable std::mutex sessionMutex;
    Api::Session session;
    std::atomic_bool signedIn = false;
    std::atomic_bool sessionDead = false;

    // UI thread only
    std::wstring email, userId;
    Profile profile;
    bool profileLoaded = false;
    bool initialized = false;
    bool authBusy = false, saveBusy = false, linkBusy = false;
    bool discordBusy = false, discordWaiting = false;
    Async::Cancel discordCancel;
    std::atomic<unsigned> generation = 0;   // bumped on sign in / out (late results are dropped)
    std::wstring notice;
    bool noticeOk = false;
    // Waiting for the email to be confirmed
    bool waiting = false;
    std::wstring waitEmail, waitPassword;
    double waitStartedAt = 0.0;
    unsigned waitGeneration = 0;
};

} // namespace SpeedrunLauncher::Services::Account
