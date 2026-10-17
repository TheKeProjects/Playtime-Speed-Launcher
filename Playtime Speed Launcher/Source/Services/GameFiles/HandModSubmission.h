#pragma once
#include <string>
#include <vector>

// Sends a community hand mod (zipped, split into Discord-sized parts) to the review webhook.
namespace SpeedrunLauncher::Services::HandModSubmission {

// Hand colors a submission can be tagged with (matches the hand.txt convention).
const std::vector<std::wstring>& Colors();
bool IsConfigured();

// Blocking (worker thread). Returns an empty string on success, otherwise the error message.
std::wstring Submit(const std::wstring& modName, int chapter, const std::vector<std::wstring>& colors,
                    const std::vector<std::wstring>& filePaths, const std::wstring& submitterName, const std::wstring& submitterId);

} // namespace SpeedrunLauncher::Services::HandModSubmission
