#pragma once
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Services/Account/CommunityRanking.h"
#include "Services/Account/FriendsService.h"
#include "Services/Account/Profile.h"
#include "Services/Community/SpeedrunComService.h"
#include "UI/Layout.h"
#include "UI/Widgets/Panel.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// "[ ACCOUNT ]", opened from the account badge in the top bar.
//   Signed out: sign in / create an account (what an account is for, email + password).
//   Signed in, PROFILE: a live preview card on the left (banner, picture, styled name) with the
//   account details under it (email hidden until "show", the speedrun.com link, sign out); on the
//   right the editor: name, picture and banner (GIFs move), the name's font, effect and colours.
//   Signed in, RANKING: the launcher users with a linked speedrun.com account, ranked per chapter
//   and category by their personal bests on speedrun.com, with their runs and hours.
class AccountOverlay : public Overlay {
public:
    using Overlay::Overlay;
    ~AccountOverlay() override { *alive = false; }
    bool Visible() const override { return popup.Visible(); }
    void Draw(UI::Ui& ui, const Rect& screen) override;
    void Open(bool ranking = false);
    // Someone's full profile on its own, over whatever menu is open (e.g. the duels): drawn by
    // ProfileOnTop, above every other overlay. Closing it leaves that menu as it was.
    void ShowProfile(const Services::Account::Profile& profile);
    bool ProfileShown() const { return standaloneProfile && viewer.open; }
    void DrawProfileOnTop(UI::Ui& ui, const Rect& screen);
    // Friends → MESSAGES with this person's conversation open
    void OpenChat(const Services::Account::Profile& profile);

private:
    using Profile = Services::Account::Profile;
    using RunType = Services::SpeedrunCom::RunType;

    // ── Signed out (AccountOverlay.cpp) ──
    float DrawSignIn(UI::Ui& ui, const Rect& body);   // returns the content height
    float DrawWaiting(UI::Ui& ui, const Rect& body);  // account created: waiting for the email confirmation
    void SubmitSignIn();
    void Close();

    // ── Profile (AccountProfileTab.cpp) ──
    void DrawProfile(UI::Ui& ui, const Rect& body);
    float DrawEditor(UI::Ui& ui, const Rect& r);   // returns the content height
    void DrawAccountBox(UI::Ui& ui, const Rect& r);
    void DrawSpeedrunBox(UI::Ui& ui, UI::Column& col);
    void DrawDiscordBox(UI::Ui& ui, UI::Column& col);
    bool Dirty() const;
    void Save();
    void Discard();
    std::wstring AvatarSource() const;
    std::wstring BannerSource() const;
    void PickPicture(bool banner);

    // ── Friends (AccountFriendsTab.cpp): pages ALL · ONLINE · PENDING · ADD ──
    void DrawFriends(UI::Ui& ui, const Rect& body);
    void DrawFriendsNav(UI::Ui& ui, const Rect& r);
    void DrawFriendsList(UI::Ui& ui, const Rect& r, bool onlineOnly);
    void DrawFriendsPending(UI::Ui& ui, const Rect& r);
    void DrawFriendsAdd(UI::Ui& ui, const Rect& r);
    // EVERYONE: every launcher account, online first (presence from the duels' online list + friends)
    void DrawFriendsCommunity(UI::Ui& ui, const Rect& r);
    void LoadCommunity(bool force);
    // MESSAGES (AccountMessagesTab.cpp): the conversations on the left, the open one on the right
    void DrawMessages(UI::Ui& ui, const Rect& r);
    void DrawConversation(UI::Ui& ui, const Rect& r);
    // A "message" button's action: from the account menu or from a profile over another menu
    void MessageButton(const Profile& profile);
    void DrawFriendCard(UI::Ui& ui, const Rect& r);
    void SetFriendsPage(int page);
    void FriendMessage(const std::wstring& message, bool ok);

    // ── Someone's profile over the menu (AccountFriendsTab.cpp) ──
    struct Viewer {
        bool open = false;
        Profile profile;
        std::wstring runUrl, runText;   // from the ranking: their run there
        double openedAt = 0.0;
        // The full profile (AccountFullProfile.cpp): over the card, or opened straight from the friends list
        bool full = false;
        bool fullFromCard = false;   // "back" returns to the card instead of closing
        double fullAt = 0.0;
        int section = 0;             // 0 overview, 1 records, 2 hours, 3 info, 4 badges
        double sectionAt = 0.0;
        // Your own profile as the others see it (PROFILE → PREVIEW): the editor's pictures (local
        // files until saved) instead of the uploaded ones
        bool preview = false;
        std::wstring avatarSource, bannerSource;
    } viewer;
    void OpenViewer(const Profile& profile, const std::wstring& runUrl = L"", const std::wstring& runText = L"");
    void DrawViewer(UI::Ui& ui, const Rect& screen);
    void OpenFullProfile(const Profile& profile, bool fromCard);
    void DrawFullProfile(UI::Ui& ui, const Rect& screen);
    void DrawFullOverview(UI::Ui& ui, const Rect& r);
    void DrawFullRecords(UI::Ui& ui, const Rect& r);
    void DrawFullHours(UI::Ui& ui, const Rect& r);
    void DrawFullInfo(UI::Ui& ui, const Rect& r);
    void DrawFullBadges(UI::Ui& ui, const Rect& r);
    // PROFILE → PREVIEW: the full profile with the editor's (maybe unsaved) changes
    void PreviewMyProfile();
    // One runner's speedrun.com personal bests and runs (shared with the ranking's `runners`)
    void RequestRunner(const std::wstring& speedrunId);
    std::set<std::wstring> runnersLoading;

    // ── Ranking (AccountRankingTab.cpp) ──
    void DrawRanking(UI::Ui& ui, const Rect& body);
    void LoadRanking(bool force);
    void RequestRunTypes(int chapter);
    void DrawRankTabs(UI::Ui& ui, const Rect& r);
    void DrawRankCategories(UI::Ui& ui, const Rect& r, std::vector<RunType>& types);
    void DrawRankTable(UI::Ui& ui, const Rect& r, const RunType& runType);

    bool standaloneProfile = false;   // the viewer was opened by ShowProfile (no account menu behind it)

    UI::Popup popup;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    double openedAt = -10.0;
    int tab = 0;                   // 0 profile, 1 ranking
    double tabChangedAt = -10.0;
    std::wstring status;           // footer message
    bool statusOk = false;

    // Sign in / up
    bool signUpMode = false;
    std::wstring email, password, password2;

    // Profile editor
    Profile draft;
    Profile draftBase;   // the account's profile the draft started from (edits = differences from it)
    std::optional<std::wstring> pendingAvatar, pendingBanner;   // nullopt = keep, empty = remove
    bool showEmail = false;
    std::wstring apiKey;
    bool confirmSignOut = false;
    std::wstring customColor1, customColor2, customBanner, customCard;   // the hex fields

    // Friends
    int friendsPage = 0;           // 0 all, 1 online, 2 pending, 3 add, 4 everyone, 5 messages
    double friendsPageAt = -10.0;
    bool friendDetailShown = false;   // the selected friend's card fits next to the list
    std::wstring friendFilter;     // filters the lists by name
    double searchShownAt = -10.0;
    std::wstring friendCode;       // the "add a friend" field (a name or a friend code)
    std::wstring searchedFor;      // what the results below the field are for
    double searchEditedAt = -10.0;
    bool searching = false, searchFailed = false;
    std::vector<Services::Account::Friend> searchResults;
    std::wstring selectedFriend;   // user id shown in the card
    std::wstring confirmRemove;    // user id waiting for the second click on "remove"
    bool codeCopied = false;
    // Everyone (the friends' EVERYONE page)
    bool communityLoading = false, communityFailed = false;
    double communityLoadedAt = -1000.0;
    std::vector<Profile> community;
    std::wstring communityFilter;
    // Messages
    Profile chatProfile;           // the conversation on screen (its person)
    std::wstring dmText;           // what's being written
    double chatOpenedAt = -10.0;
    double codeCopiedAt = -10.0;

    // Ranking
    bool membersLoading = false, membersFailed = false, membersLoaded = false;
    std::vector<Profile> members;
    std::map<std::wstring, Services::Account::Community::RunnerData> runners;   // by speedrun.com id
    int runnersPending = 0;
    unsigned rankingGeneration = 0;
    int rankChapter = 1;
    std::map<int, std::vector<RunType>> runTypes;
    std::set<int> loadingTypes;
    std::map<int, int> rankCategory;
    double rankChangedAt = -10.0;
};

// Draws AccountOverlay::ShowProfile's profile at the top of the overlay stack (the account menu
// itself sits below the duels menu)
class ProfileOnTop : public Overlay {
public:
    ProfileOnTop(MainWindow& window, AccountOverlay& accountOverlay) : Overlay(window), account(accountOverlay) {}
    bool Visible() const override { return account.ProfileShown(); }
    void Draw(UI::Ui& ui, const Rect& screen) override { account.DrawProfileOnTop(ui, screen); }

private:
    AccountOverlay& account;
};

} // namespace SpeedrunLauncher::Main
