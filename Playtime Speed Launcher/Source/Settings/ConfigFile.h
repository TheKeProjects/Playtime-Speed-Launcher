#pragma once
#include <optional>
#include <string>
#include "Core/Json.h"

// Small settings files under %LOCALAPPDATA%\SpeedrunLauncher. Every read/write swallows errors,
// like the try { } catch { } wrappers in the old code.
namespace SpeedrunLauncher::Config {

// Plain text .cfg files (trimmed contents)
std::optional<std::wstring> ReadText(const std::wstring& fileName);
void WriteText(const std::wstring& fileName, const std::wstring& value);
bool ReadFlag(const std::wstring& fileName, bool fallback);          // "1" = true
void WriteFlag(const std::wstring& fileName, bool value);

// JSON files (System.Text.Json compatible: PascalCase property names)
Json::Value ReadJson(const std::wstring& fileName, bool* exists = nullptr);
void WriteJson(const std::wstring& fileName, const Json::Value& value, bool indented = false);

} // namespace SpeedrunLauncher::Config
