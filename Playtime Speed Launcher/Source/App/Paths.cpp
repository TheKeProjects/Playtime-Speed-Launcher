#include "App/Paths.h"
#include "Core/Files.h"

namespace SpeedrunLauncher::Paths {

const std::wstring& DataDir() {
    static const std::wstring dir = Files::Combine(Files::LocalAppData(), L"SpeedrunLauncher");
    return dir;
}

std::wstring DataFile(const std::wstring& name) { return Files::Combine(DataDir(), name); }

const std::wstring& TempDir() {
    static const std::wstring dir = Files::Combine(Files::TempDirectory(), L"SpeedrunLauncher");
    return dir;
}

std::wstring Asset(const std::wstring& relative) { return Files::Combine(TempDir(), L"Assets", relative); }

std::wstring Translation(const std::wstring& fileName) { return Files::Combine(TempDir(), L"Translations", fileName); }

std::wstring SavesDir() { return Files::Combine(TempDir(), L"Assets", L"Saves"); }

std::wstring Chapter5NgPlusDir() { return Files::Combine(SavesDir(), L"Chapter 5 NG+"); }

} // namespace SpeedrunLauncher::Paths
