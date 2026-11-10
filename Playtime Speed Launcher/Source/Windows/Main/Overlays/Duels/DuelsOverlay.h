#pragma once
#include <memory>
#include <string>
#include "Services/Account/DuelsService.h"
#include "Services/Account/Profile.h"
#include "UI/Widgets/Panel.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// "[ DUELS ]", opened from the side rail (the tile above the account): speedrun races between
// launcher accounts, like the Discord bot's matchmaking.
//   Left:   your rank (MMR, wins / losses, progress to the next rank) and the pages:
//           YOUR ROOM · LOBBIES (invitations + open rooms) · CREATE ROOM · RANKING · HOW IT WORKS.
//   Middle: the page, or the duel you're in (room → ban phase → race → results). A room is where
//           the host changes the settings, invites and kicks people and starts the race.
//   Right:  who's online right now (friends first): CHALLENGE opens a room with them invited,
//           INVITE brings them into the room you're in.
class DuelsOverlay : public Overlay {
public:
    using Overlay::Overlay;
    ~DuelsOverlay() override;
    bool Visible() const override { return popup.Visible(); }
    void Draw(UI::Ui& ui, const Rect& screen) override;
    void Open();
    // Opens a room with them invited (or invites them to the room you're in)
    void Challenge(const Services::Account::Profile& who);

private:
    using Duel = Services::Account::Duel;
    using Profile = Services::Account::Profile;

    void Close();
    void Message(const std::wstring& text, bool ok);
    Services::Account::DuelsService::Done Reply(const std::wstring& success = L"");
    void SetPage(int page);
    // Opens a room with the last settings used, optionally inviting someone
    void CreateRoom(const Profile* invite = nullptr);

    // ── DuelsOverlay.cpp ──
    void DrawSignedOut(UI::Ui& ui, const Rect& body);
    void DrawNav(UI::Ui& ui, const Rect& r);
    void DrawOnline(UI::Ui& ui, const Rect& r);

    // ── DuelsPages.cpp ──
    void DrawLobbies(UI::Ui& ui, const Rect& r);
    void DrawLobbyRow(UI::Ui& ui, const Rect& row, const Duel& duel, bool invite);
    // The room: its settings (the host changes them) and inviting people
    void DrawRoomSettings(UI::Ui& ui, const Rect& r, const Duel& duel);
    void DrawRoomInvite(UI::Ui& ui, const Rect& r, const Duel& duel);
    void DrawRanking(UI::Ui& ui, const Rect& r);
    void DrawRules(UI::Ui& ui, const Rect& r);
    void DrawSettings(UI::Ui& ui, const Rect& r);   // notifications and sounds

    // ── DuelsMatch.cpp: the duel you're in ──
    void DrawMatch(UI::Ui& ui, const Rect& r, const Duel& duel);
    void DrawWaiting(UI::Ui& ui, const Rect& r, const Duel& duel);
    void DrawBans(UI::Ui& ui, const Rect& r, const Duel& duel);
    void DrawRace(UI::Ui& ui, const Rect& r, const Duel& duel);
    void DrawResult(UI::Ui& ui, const Rect& r, const Duel& duel);
    void DrawMatchHeader(UI::Ui& ui, const Rect& r, const Duel& duel, const std::wstring& title, const std::wstring& subtitle);

    UI::Popup popup;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    double openedAt = -10.0;
    int page = 0;                  // 0 lobbies, 2 ranking, 3 how it works, 4 the duel you're in, 5 settings
    double pageAt = -10.0;
    std::wstring status;           // footer message
    bool statusOk = false;

    // The settings new rooms start with (the last ones used)
    Services::Account::DuelSetup setup;
    int roomTab = 0;               // 0 settings, 1 invite people
    double roomTabAt = -10.0;
    std::wstring inviteFilter;

    // The duel you're in
    std::wstring shownDuel;        // id + status the match view last drew (its entrance replays on change)
    double matchAt = -10.0;
    std::wstring timeText;         // the race's time field
    bool confirmForfeit = false;
    bool confirmLeave = false;
    std::wstring confirmKick;      // the player waiting for the second click on "kick"
};

} // namespace SpeedrunLauncher::Main
