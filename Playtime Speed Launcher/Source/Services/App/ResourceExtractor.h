#pragma once

namespace SpeedrunLauncher::Services::ResourceExtractor {

// Unpacks the assets embedded in the exe to %TEMP%\SpeedrunLauncher (Saves.zip / "CH5 NG+.zip" are
// expanded into Assets\Saves). Unchanged files from a previous run are kept.
void Extract();
// Deletes the unpacked checkpoint saves (on exit).
void CleanupSaves();

} // namespace SpeedrunLauncher::Services::ResourceExtractor
