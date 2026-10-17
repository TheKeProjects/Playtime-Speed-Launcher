#pragma once
#include <string>
#include "Core/Text.h"
#include "App/Config.h"
#include "Version.h"

namespace SpeedrunLauncher::AppVersion {

inline const std::wstring CurrentVersion = Text::Widen(APP_VERSION_STRING);

// Repository and GameBanana id live in App/Config.h
inline constexpr const wchar_t* GitHubOwner = Config::GitHub::Owner;
inline constexpr const wchar_t* GitHubRepo = Config::GitHub::Repo;
inline constexpr const wchar_t* GitHubBranch = Config::GitHub::Branch;

// GameBanana tool id (0 = disabled)
inline int& GameBananaToolId = Config::GameBanana::ToolId;

#if defined(LGBTQ_MODE)
inline constexpr bool LgbtqMode = true;
#else
inline constexpr bool LgbtqMode = false;
#endif
#if defined(SUMMER_MODE)
inline constexpr bool SummerMode = true;
#else
inline constexpr bool SummerMode = false;
#endif
#if defined(HALLOWEEN_MODE)
inline constexpr bool HalloweenMode = true;
#else
inline constexpr bool HalloweenMode = false;
#endif
#if defined(CHRISTMAS_MODE)
inline constexpr bool ChristmasMode = true;
#else
inline constexpr bool ChristmasMode = false;
#endif

inline std::wstring DisplayVersion() { return L"v" + CurrentVersion; }
inline std::wstring GitHubRepoUrl() { return std::wstring(L"https://github.com/") + GitHubOwner + L"/" + GitHubRepo; }

// The icon theme baked into the build (used when the icon theme setting is "default").
inline std::wstring BuildDefaultTheme() {
    if (SummerMode) return L"summer";
    if (HalloweenMode) return L"halloween";
    if (ChristmasMode) return L"christmas";
    if (LgbtqMode) return L"lgbtq";
    return L"default";
}

} // namespace SpeedrunLauncher::AppVersion
