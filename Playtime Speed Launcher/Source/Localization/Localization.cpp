#include "Localization/Localization.h"
#include <unordered_map>
#include "App/Paths.h"
#include "Core/Files.h"
#include "Settings/ConfigFile.h"

namespace SpeedrunLauncher::Loc {

namespace {

std::unordered_map<std::string, std::wstring> strings;
std::unordered_map<std::string, std::wstring> missing;   // keys returned as-is
std::wstring currentLang = L"es";
unsigned revision = 0;

} // namespace

void Load(const std::wstring& lang) {
    currentLang = lang;
    const std::wstring path = Paths::Translation(lang + L".txt");
    auto text = Files::ReadText(path);
    if (!text) return;

    strings.clear();
    for (const auto& line : Text::SplitLines(*text)) {
        const std::wstring trimmed = Text::TrimStart(line);
        if (Text::Trim(line).empty() || (!trimmed.empty() && trimmed[0] == L'#')) continue;
        const size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        const std::wstring key = Text::Trim(line.substr(0, eq));
        const std::wstring value = Text::Replace(Text::TrimStart(line.substr(eq + 1)), L"\\n", L"\n");
        strings[Text::Narrow(key)] = value;
    }
    ++revision;
    Config::WriteText(L"language.cfg", lang);
}

std::wstring LoadSaved() {
    if (auto saved = Config::ReadText(L"language.cfg"); saved && !saved->empty()) {
        Load(*saved);
        return *saved;
    }
    Load(L"es");
    return L"es";
}

const std::wstring& CurrentLang() { return currentLang; }

unsigned Revision() { return revision; }

const std::wstring& Get(const std::string& key) {
    auto it = strings.find(key);
    if (it != strings.end()) return it->second;
    auto miss = missing.find(key);
    if (miss == missing.end()) miss = missing.emplace(key, Text::Widen(key)).first;
    return miss->second;
}

std::wstring Get(const std::string& key, const std::vector<std::wstring>& args) {
    auto it = strings.find(key);
    if (it == strings.end()) return Text::Widen(key);
    return args.empty() ? it->second : Text::Format(it->second, args);
}

} // namespace SpeedrunLauncher::Loc
