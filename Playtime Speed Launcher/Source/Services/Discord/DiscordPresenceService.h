#pragma once
#include <memory>
#include <optional>
#include <string>
#include "Core/Json.h"
#include "Models/Chapter.h"
#include "Services/Discord/DiscordIpcClient.h"

namespace SpeedrunLauncher::Services {

// Discord Rich Presence for the launcher.
//   details (upper line) = activity / chapter
//   state   (lower line) = version  ·  ⏱ LiveSplit timer  (per DiscordPresenceSettings)
class DiscordPresenceService {
public:
    DiscordPresenceService();
    ~DiscordPresenceService();

    void ApplySettings(bool showActivity, bool showVersion, bool showLiveSplit);
    void RefreshIconTheme();
    void UpdateLiveSplitLine(const std::wstring& line);

    void SetBrowsing();
    void SetChapterSelected(const Models::Chapter& chapter, const std::wstring& version);
    void SetInstalling(const Models::Chapter& chapter, const std::wstring& presetName);
    void SetGameRunning(const Models::Chapter& chapter, const std::wstring& version);
    void SetWatchingTutorial(const std::wstring& videoTitle, const std::wstring& chapter);
    void SetSelectingCheckpoint(const Models::Chapter& chapter, const std::wstring& version);

    std::optional<DiscordIpcClient::User> ConnectedUser() const;

private:
    struct Presence {
        std::wstring details;
        long long startUnix = 0;
        std::wstring largeImage, largeText, smallImage, smallText;
    };
    void Set(const Presence& presence, const std::wstring& version);
    void Send();
    std::wstring BuildState() const;

    std::unique_ptr<DiscordIpcClient> client;
    long long sessionStart = 0;
    std::wstring liveSplitLine = L"LiveSplit not connected";
    std::wstring versionLine;
    std::optional<Presence> current;
    bool showActivity = true, showVersion = true, showLiveSplit = false;
    unsigned long long lastLiveSplitSend = 0;
};

} // namespace SpeedrunLauncher::Services
