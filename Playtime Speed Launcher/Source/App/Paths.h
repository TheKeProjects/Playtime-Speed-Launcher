#pragma once
#include <string>

// Well-known folders (same locations as previous launcher versions, so existing settings carry over).
namespace SpeedrunLauncher::Paths {

// %LOCALAPPDATA%\SpeedrunLauncher — every settings / cfg file
const std::wstring& DataDir();
std::wstring DataFile(const std::wstring& name);

// %TEMP%\SpeedrunLauncher — embedded assets are unpacked here on startup
const std::wstring& TempDir();
std::wstring Asset(const std::wstring& relative);          // TempDir\Assets\<relative>
std::wstring Translation(const std::wstring& fileName);    // TempDir\Translations\<file>
std::wstring SavesDir();                                    // TempDir\Assets\Saves
std::wstring Chapter5NgPlusDir();                           // TempDir\Assets\Saves\Chapter 5 NG+

} // namespace SpeedrunLauncher::Paths
