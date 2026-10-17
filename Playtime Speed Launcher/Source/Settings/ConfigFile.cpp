#include "Settings/ConfigFile.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Text.h"

namespace SpeedrunLauncher::Config {

std::optional<std::wstring> ReadText(const std::wstring& fileName) {
    auto text = Files::ReadText(Paths::DataFile(fileName));
    if (!text) return std::nullopt;
    return Text::Trim(*text);
}

void WriteText(const std::wstring& fileName, const std::wstring& value) {
    Files::CreateDirectories(Paths::DataDir());
    Files::WriteText(Paths::DataFile(fileName), value);
}

bool ReadFlag(const std::wstring& fileName, bool fallback) {
    auto text = ReadText(fileName);
    return text ? *text == L"1" : fallback;
}

void WriteFlag(const std::wstring& fileName, bool value) { WriteText(fileName, value ? L"1" : L"0"); }

Json::Value ReadJson(const std::wstring& fileName, bool* exists) {
    auto text = Files::ReadBytes(Paths::DataFile(fileName));
    if (exists) *exists = text.has_value();
    if (!text) return {};
    return Json::Parse(std::string_view(*text));
}

void WriteJson(const std::wstring& fileName, const Json::Value& value, bool indented) {
    Files::CreateDirectories(Paths::DataDir());
    Files::WriteBytes(Paths::DataFile(fileName), value.Serialize(indented));
}

} // namespace SpeedrunLauncher::Config
