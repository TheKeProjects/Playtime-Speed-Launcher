#pragma once
#include <string>

// 100%: the local web server OBS reads the collectibles overlay from (Chapters 4 and 5). Listens on
// 127.0.0.1 only, and stays up while the launcher is open:
//   /, /overlay        the page (setup page without ?view=, the overlay with it)
//   /icons/X.png       category icons (Assets\Collectibles\Icons)
//   /chapters/X.jpg    chapter art (Assets\Collectibles\Chapters)
//   /api/test          JSON {"n": count} of test events; ?fire adds one (the setup's "test in OBS" button)
//   /api/status        JSON: launcher version, port, both chapters' save folders, newest saves, running game
//   /api/save?ch=4|5|auto   the running game's inventory (read from its memory) or else the newest save
//                      of that chapter, decompressed (ETag / If-None-Match: 304 while unchanged, 204 if
//                      there is none). Headers X-Chapter, X-Save-File and X-Save-State.
//
// The OBS link carries the port, so it's kept in collectibles_port.cfg. Once the server has been used
// (Settings → 100%, or the overlay over the game), collectibles_server.cfg makes the launcher start it on
// every launch, so OBS keeps working without opening the settings again.
namespace SpeedrunLauncher::Services::Collectibles::Server {

// Starts the server if it isn't running. Returns its port, or 0 if no port could be opened.
int Ensure();
// Port of the running server, 0 if stopped
int Port();
// Starts it on launch if it was used before
void StartIfEnabled();
void Stop();
// A fake pickup in every open overlay (OBS included); nothing is counted
void FireTest();
// The OBS link with the given query ("view=panel&..."), empty if the server isn't running
std::wstring Link(const std::string& query);

} // namespace SpeedrunLauncher::Services::Collectibles::Server
