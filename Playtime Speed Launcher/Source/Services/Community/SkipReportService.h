#pragma once
#include <string>
#include <vector>

namespace SpeedrunLauncher::Services::SkipReport {

struct Report {
    std::wstring name, chapter, author, videoUrl, description, version;
    std::vector<std::wstring> runCategories, routes, restrictions;
    bool cores = false;
};

bool IsConfigured();
// Blocking (worker thread). Returns an empty string on success, otherwise the error text.
std::wstring Send(const Report& report);

} // namespace SpeedrunLauncher::Services::SkipReport
