#pragma once
#include <optional>
#include <string>

// Bug reports go to a Discord webhook as an embed (author = the reporter's Discord account),
// with an optional screenshot attached.
namespace SpeedrunLauncher::Services::BugReport {

// Blocking: call from a worker thread. Returns true on success.
bool Send(const std::wstring& discordId, const std::wstring& discordName, const std::wstring& title, const std::wstring& description,
          const std::optional<std::wstring>& imagePath);

} // namespace SpeedrunLauncher::Services::BugReport
