#pragma once

namespace SpeedrunLauncher::Settings {

// Duels → Settings: which duel events show a notification and / or play a sound.
// (duel_settings.json: {"Notifications":true,"Sounds":true,"Invites":true,"Joins":true,"Starts":true,"Results":true})
struct Duels {
    bool notifications = true;   // the toasts at all
    bool sounds = true;          // the duel sounds at all
    // Per event (both its toast and its sound)
    bool invites = true;         // invited to a room / inviting someone
    bool joins = true;           // someone joined your room / you joined one
    bool starts = true;          // the race started / the category was rolled
    bool results = true;         // won / lost

    void Save() const;
    static Duels& Current();
};

} // namespace SpeedrunLauncher::Settings
