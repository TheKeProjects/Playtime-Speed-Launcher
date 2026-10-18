#pragma once
#include <string>
#include <vector>

// The in-app changelog (newest first). Lines starting with "## " are section headers.
namespace SpeedrunLauncher::Services::Changelog {

struct Entry {
    std::wstring version;
    std::wstring date;
    std::vector<std::wstring> changes;
};

const std::vector<Entry>& Entries();

// The changelog.txt published next to version.txt: one block per version, newest first, each
// starting with a "4.1.1 | 18/10/2026" line (the date is optional), then "## " section headers
// and "- " changes. Text without any version line becomes a single entry for fallbackVersion.
std::vector<Entry> Parse(const std::wstring& text, const std::wstring& fallbackVersion);

} // namespace SpeedrunLauncher::Services::Changelog
