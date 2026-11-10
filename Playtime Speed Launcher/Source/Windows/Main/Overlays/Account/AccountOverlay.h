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
        int section = 0;             // 0 overview, 1 records, 2 hours, 3 info
        double sectionAt = 0.0;
    } viewer;
    void OpenViewer(const Profile& profile, const std::wstring& runUrl = L"", const std::wstring& runText = L"");
    void DrawViewer(UI::Ui& ui, const Rect& screen);
    void OpenFullProfile(const Profile& profile, bool fromCard);
    void DrawFullProfile(UI::Ui& ui, const Rect& screen);
    void DrawFullOverview(UI::Ui& ui, const Rect& r);
    void DrawFullRecords(UI::Ui& ui, const Rect& r);
    void DrawFullHours(UI::Ui& ui, const Rect& r);
    void DrawFullInfo(UI::Ui& ui, const Rect& r);
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
    int friendsPage = 0;           // 0 all, 1 online, 2 pending, 3 add
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

} // namespace SpeedrunLauncher::Main
