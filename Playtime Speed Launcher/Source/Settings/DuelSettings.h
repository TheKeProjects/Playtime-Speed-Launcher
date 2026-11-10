#pragma once

namespace SpeedrunLauncher::Settings {

// Duels → Settings: which duel events show a notification and / or play a sound.
// (duel_settings.json: {"Notifications":true,"Sounds":true,"Invites":true,"Joins":true,"Starts":true,"Results":true,
//  "BanTurn":true,"RivalFinished":true,"Leaves":true,"FriendInvitesOnly":false,"FlashTaskbar":true,"ToastLength":1})
struct Duels {
    bool notifications = true;   // the toasts at all
    bool sounds = true;          // the duel sounds at all
    // Per event (both its toast and its sound)
    bool invites = true;         // invited to a room / inviting someone
    bool joins = true;           // someone joined your room / you joined one
    bool starts = true;          // the race started / the category was rolled
    bool results = true;         // won / lost
    bool banTurn = true;         // your turn to ban a category
    bool rivalFinished = true;   // someone else in your race sent their time / forfeited
    bool leaves = true;          // someone left your room (or the host kicked them)
    bool roomChat = true;        // a message in your room's chat (while the duels menu is closed)
    bool directMessages = true;  // a private message from a friend (while you aren't reading that chat)
    // Filters and how they show
    bool friendInvitesOnly = false;   // only toast / ring invitations from friends
    bool flashTaskbar = true;         // flash the launcher in the taskbar when it isn't in front
    int toastLength = 1;              // 0 short (5 s), 1 normal (8 s), 2 long (14 s)
    double ToastSeconds() const { return toastLength <= 0 ? 5.0 : toastLength >= 2 ? 14.0 : 8.0; }

    void Save() const;
    static Duels& Current();
};

} // namespace SpeedrunLauncher::Settings
