#pragma once
#include <optional>
#include <string>

// The install / remove flows behind the UE4SS, LOAD MANIP and FULLBRIGHT card buttons. UE4SS,
// Load Manip and FullBright share files (the same pak, UE4SS builds overwriting each other), so
// every flow knows how to leave the other mods in a consistent state.
// All functions block: run them on a worker thread.
namespace SpeedrunLauncher::Services::ModInstaller {

struct Target {
    int chapter = 0;
    std::optional<std::wstring> win64;   // .../Binaries/Win64 of the active install
    std::optional<std::wstring> paks;    // .../Content/Paks
};
Target Resolve(int chapter);   // UI thread (reads the launcher's install selection)

struct Result {
    bool ok = false;
    std::wstring message;
};

// State checks (cheap file probes)
bool Ue4ssPresent(const Target& t);                 // any UE4SS build in Win64
bool LoadManipMarker(const Target& t);              // launcher.playtime
bool FullBrightInstalled(const Target& t);
bool LoadManipInstalled(const Target& t);
bool StandaloneUe4ssInstalled(const Target& t);     // UE4SS not owned by Load Manip / FullBright
bool ForeignUe4ssBlocksLoadManip(const Target& t);  // a standalone UE4SS that must be deleted first
bool ForeignUe4ssBlocksFullBright(const Target& t);

std::wstring GenericUe4ssZip(int chapter);          // Tools\Chapter 1 - 4 | Chapter 5\Ue4ss.zip

// UE4SS card
Result InstallUe4ss(const Target& t);               // plain extract (also the popups' INSTALL UE4SS)
Result RemoveUe4ss(const Target& t);
Result ReplaceModUe4ssWithStandalone(const Target& t);   // removes FullBright + Load Manip first

// LOAD MANIP card
Result InstallLoadManip(const Target& t, bool deleteForeignUe4ss);
Result RemoveLoadManip(const Target& t, bool alsoFullBright);

// FULLBRIGHT card
Result InstallFullBright(const Target& t, bool withLoadManip, bool deleteForeignUe4ss);
Result RemoveFullBright(const Target& t, bool alsoLoadManip);

} // namespace SpeedrunLauncher::Services::ModInstaller
