#pragma once
#include <cstddef>
#include <functional>
#include <optional>
#include <string>

namespace SpeedrunLauncher::Services::ResourceExtractor {

// Unpacks the assets embedded in the exe to %TEMP%\SpeedrunLauncher (Saves.zip / "CH5 NG+.zip" are
// expanded into Assets\Saves). Unchanged files from a previous run are kept. `progress` gets
// (done, total) after every asset; any thread.
void Extract(const std::function<void(size_t done, size_t total)>& progress = {});
// Bytes of one embedded asset ("Assets", "Images\\Chapter 1.png"), without extracting anything.
std::optional<std::string> ReadEmbedded(const std::wstring& root, const std::wstring& path);
// Deletes the unpacked checkpoint saves (on exit).
void CleanupSaves();

} // namespace SpeedrunLauncher::Services::ResourceExtractor
