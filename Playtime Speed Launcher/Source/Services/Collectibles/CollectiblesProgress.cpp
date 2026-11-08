#include "Services/Collectibles/CollectiblesProgress.h"
#include <windows.h>
#include "Core/Async.h"
#include "Services/Collectibles/CollectiblesData.h"
#include "Services/Collectibles/CollectiblesMemory.h"
#include "Services/Collectibles/SaveGameService.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Services::Collectibles::Progress {

namespace {

constexpr ULONGLONG kEveryMs = 1500;

Snapshot latest;
bool reading = false;
ULONGLONG lastRead = 0;

ChapterProgress Read(int chapter) {
    ChapterProgress p;
    p.chapter = chapter;
    const auto info = SaveGames::Info(chapter);
    p.folderExists = info.folderExists;
    p.running = info.running;
    const auto& data = Data::Get(chapter);
    p.total = data.Total();
    const Memory::Result memory = Memory::Read(chapter);
    p.source = memory.live ? Source::Live : memory.any ? Source::Memory : Source::None;
    const std::string& text = memory.text;
    p.have = Data::Collected(data, text);
    const Data::PlayedCount played = Data::Played(data, text);
    p.played = played.played;
    p.playedTotal = played.total;
    for (const auto& cat : p.have)
        for (const bool b : cat) p.got += b ? 1 : 0;
    return p;
}

} // namespace

void Poll() {
    const ULONGLONG now = GetTickCount64();
    if (reading || (latest.ready && lastRead && now - lastRead < kEveryMs)) return;
    reading = true;
    lastRead = now;
    Async::Run(
        [] {
            Snapshot s;
            s.ready = true;
            s.autoChapter = SaveGames::AutoChapter();
            for (const int n : Data::kChapters) s.chapters[n - 1] = Read(n);
            return s;
        },
        [](Snapshot s) {
            latest = std::move(s);
            reading = false;
            lastRead = GetTickCount64();
            if (auto* main = Main::MainWindow::Instance()) main->Invalidate();
            // One more frame once the next read is due: if the progress is still on screen, it polls again
            Async::After(static_cast<unsigned>(kEveryMs) + 50, [] {
                if (auto* main = Main::MainWindow::Instance()) main->Invalidate();
            });
        });
}

void Refresh() { lastRead = 0; }

const Snapshot& Latest() { return latest; }

} // namespace SpeedrunLauncher::Services::Collectibles::Progress
