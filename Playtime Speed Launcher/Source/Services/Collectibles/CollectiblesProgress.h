#pragma once
#include <string>
#include <vector>

// 100%: what Settings → 100% shows for each chapter: where the progress comes from and which
// collectibles are in it. Read on a worker thread, like the overlay's /api/save: the launcher's memory
// of the run (CollectiblesMemory), updated from the running game's inventory.
namespace SpeedrunLauncher::Services::Collectibles::Progress {

enum class Source { None, Live, Memory };   // Memory: the game is closed, what the launcher remembers

struct ChapterProgress {
    int chapter = 0;
    bool folderExists = false;
    bool running = false;
    Source source = Source::None;
    std::vector<std::vector<bool>> have;      // per category, per item (Data::Collected)
    int got = 0, total = 0;
    int played = 0, playedTotal = 0;          // tapes put in their VHS player (playedTotal 0: not tracked)
};

struct Snapshot {
    bool ready = false;                       // read at least once
    int autoChapter = 5;                      // the one "Automatic" shows
    ChapterProgress chapters[5];              // by chapter number - 1
    const ChapterProgress& Of(int chapter) const { return chapters[chapter >= 1 && chapter <= 5 ? chapter - 1 : 4]; }
};

// Starts a new read when the last one is older than a moment and none is running (UI thread;
// call it every frame the progress is on screen). The window is redrawn when it lands.
void Poll();
// The next Poll reads again at once (after a reset)
void Refresh();
const Snapshot& Latest();   // UI thread

} // namespace SpeedrunLauncher::Services::Collectibles::Progress
