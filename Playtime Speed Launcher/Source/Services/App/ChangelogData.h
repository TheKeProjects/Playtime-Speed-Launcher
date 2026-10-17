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

} // namespace SpeedrunLauncher::Services::Changelog
