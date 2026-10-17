#include "Services/Platforms/SteamCmdRunner.h"
#include <windows.h>
#include <mutex>
#include <regex>
#include "App/Config.h"
#include "App/Paths.h"
#include "Core/Files.h"
#include "Core/Http.h"
#include "Core/Process.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Core/Zip.h"

namespace SpeedrunLauncher::Services::SteamCmd {

namespace {

// AppData\Local is ASCII-safe; the 2013 SteamCMD binary crashes on unicode paths.
std::wstring LocalDir() { return Paths::DataFile(L"steamcmd"); }

std::optional<std::wstring> LoginUsersVdf() {
    auto steam = SteamInstallPath();
    if (!steam) return std::nullopt;
    const std::wstring vdf = Files::Combine(*steam, L"config", L"loginusers.vdf");
    auto content = Files::ReadText(vdf);
    if (!content) return std::nullopt;
    return content;
}

// Picks the most recently used user block: the one with "MostRecent" "1", else the highest "Timestamp".
std::optional<std::wstring> MostRecentSteamId(const std::wstring& content, std::wstring& block) {
    static const std::wregex idRe(LR"re("(\d{17})")re");
    static const std::wregex mostRecent(LR"re("MostRecent"\s+"1")re");
    static const std::wregex timestampRe(LR"re("Timestamp"\s+"(\d+)")re");
    std::vector<std::pair<std::wstring, size_t>> ids;   // id, position after the match
    std::vector<size_t> starts;
    for (std::wsregex_iterator it(content.begin(), content.end(), idRe), end; it != end; ++it) {
        ids.push_back({ (*it)[1].str(), static_cast<size_t>(it->position() + it->length()) });
        starts.push_back(static_cast<size_t>(it->position()));
    }
    std::optional<std::wstring> bestId;
    std::wstring bestBlock;
    long long bestTimestamp = -1;
    for (size_t i = 0; i < ids.size(); ++i) {
        const size_t from = ids[i].second;
        const size_t to = i + 1 < ids.size() ? starts[i + 1] : content.size();
        const std::wstring candidate = content.substr(from, to - from);
        if (std::regex_search(candidate, mostRecent)) {
            block = candidate;
            return ids[i].first;
        }
        std::wsmatch m;
        if (std::regex_search(candidate, m, timestampRe)) {
            const long long ts = Text::ToInt(m[1].str()).value_or(-1);
            if (ts > bestTimestamp) {
                bestTimestamp = ts;
                bestId = ids[i].first;
                bestBlock = candidate;
            }
        }
    }
    block = bestBlock;
    return bestId;
}

bool IsGuardPrompt(const std::wstring& s) {
    std::wstring t = s;
    while (!t.empty() && iswspace(t.back())) t.pop_back();
    return !t.empty() && t.back() == L':' &&
           (Text::Contains(t, L"Steam Guard", true) || Text::Contains(t, L"Two-factor", true) || Text::Contains(t, L"two factor", true));
}

} // namespace

std::optional<std::wstring> Find() {
    const std::wstring candidates[] = {
        Files::Combine(LocalDir(), L"steamcmd.exe"),
        L"C:\\Program Files (x86)\\Steam\\steamcmd.exe",
        L"C:\\steamcmd\\steamcmd.exe",
        L"C:\\SteamCMD\\steamcmd.exe",
    };
    for (const auto& p : candidates)
        if (Files::Exists(p)) return p;
    return std::nullopt;
}

std::optional<std::wstring> SteamInstallPath() {
    if (auto p = System::RegistryString(System::Hive::LocalMachine, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath");
        p && Files::DirectoryExists(*p))
        return p;
    if (auto p = System::RegistryString(System::Hive::CurrentUser, L"SOFTWARE\\Valve\\Steam", L"SteamPath");
        p && Files::DirectoryExists(*p))
        return Text::Replace(*p, L"/", L"\\");
    return std::nullopt;
}

std::optional<std::wstring> LoggedInUsername() {
    auto content = LoginUsersVdf();
    if (!content) return std::nullopt;
    std::wstring block;
    if (!MostRecentSteamId(*content, block)) return std::nullopt;
    static const std::wregex account(LR"re("AccountName"\s+"([^"]+)")re");
    std::wsmatch m;
    if (std::regex_search(block, m, account)) return m[1].str();
    return std::nullopt;
}

std::optional<SteamUser> ActiveUser() {
    auto content = LoginUsersVdf();
    if (!content) return std::nullopt;
    std::wstring block;
    auto id = MostRecentSteamId(*content, block);
    if (!id || block.empty()) return std::nullopt;
    static const std::wregex persona(LR"re("PersonaName"\s+"([^"]+)")re");
    std::wsmatch m;
    return SteamUser{ std::regex_search(block, m, persona) ? m[1].str() : *id, *id };
}

std::optional<std::wstring> AvatarPath(const std::wstring& steamId64) {
    auto steam = SteamInstallPath();
    if (!steam) return std::nullopt;
    for (const wchar_t* ext : { L".png", L".jpg", L".jpeg" }) {
        const std::wstring path = Files::Combine(*steam, L"config", L"avatarcache", steamId64 + ext);
        if (Files::Exists(path)) return path;
    }
    return std::nullopt;
}

void CopyCredentials(const std::wstring& steamCmdPath) {
    auto steam = SteamInstallPath();
    if (!steam) return;
    const std::wstring source = Files::Combine(*steam, L"config", L"config.vdf");
    if (!Files::Exists(source)) return;
    Files::Copy(source, Files::Combine(Files::DirectoryName(steamCmdPath), L"config", L"config.vdf"), true);
}

std::optional<std::wstring> Download(const DownloadProgress& progress, std::wstring* error, const Async::Cancel& cancel) {
    Files::CreateDirectories(LocalDir());
    const std::wstring zip = Files::Combine(LocalDir(), L"steamcmd.zip");
    const std::wstring exe = Files::Combine(LocalDir(), L"steamcmd.exe");
    if (progress) progress(DownloadStage::Downloading, 0, 0);
    const bool downloaded = Http::Download(Config::Links::SteamCmdZip, zip, [&](uint64_t received, uint64_t total) {
        if (progress) progress(DownloadStage::Downloading, received, total);
        return !cancel.Requested();
    }, error);
    if (cancel.Requested()) {
        if (error) error->clear();
        return std::nullopt;
    }
    if (!downloaded) return std::nullopt;
    if (progress) progress(DownloadStage::Extracting, 0, 0);
    if (!Zip::ExtractToDirectory(zip, LocalDir(), true, error)) return std::nullopt;
    Files::Delete(zip);
    if (!Files::Exists(exe)) {
        if (error) *error = L"No se encontró steamcmd.exe tras la extracción.";
        return std::nullopt;
    }
    return exe;
}

void Run(const std::wstring& steamCmdPath, const std::wstring& username, const std::optional<std::wstring>& password, int appId,
         int depotId, const std::wstring& manifestId, const std::function<void(const std::wstring&)>& line,
         const std::function<std::optional<std::wstring>(const std::wstring&)>& prompt, const Async::Cancel& cancel) {
    std::wstring args = password && !password->empty() ? L"+login " + username + L" \"" + *password + L"\"" : L"+login " + username;
    args += L" +download_depot " + std::to_wstring(appId) + L" " + std::to_wstring(depotId) + L" " + manifestId + L" +quit";

    // Output arrives on the child's reader thread; prompts are answered there too (SteamCMD
    // blocks waiting for stdin, so nothing else is printed meanwhile).
    std::mutex mutex;
    std::string pendingBytes;
    std::wstring current;
    Process::Child* childPtr = nullptr;
    auto child = Process::Child::Launch(steamCmdPath, args, Files::DirectoryName(steamCmdPath), [&](const std::string& chunk) {
        std::lock_guard lock(mutex);
        pendingBytes += chunk;
        // Decode only complete UTF-8 sequences
        size_t cut = pendingBytes.size();
        while (cut > 0 && (static_cast<unsigned char>(pendingBytes[cut - 1]) & 0xC0) == 0x80) --cut;
        if (cut > 0 && static_cast<unsigned char>(pendingBytes[cut - 1]) >= 0xC0) --cut;
        const std::wstring text = Text::Widen(std::string_view(pendingBytes).substr(0, cut));
        pendingBytes.erase(0, cut);
        for (wchar_t c : text) {
            if (c == L'\n') {
                std::wstring done = current;
                while (!done.empty() && done.back() == L'\r') done.pop_back();
                current.clear();
                if (!Text::Trim(done).empty() && line) line(done);
                continue;
            }
            current += c;
            if (IsGuardPrompt(current) && prompt) {
                if (line) line(current);
                const std::wstring promptText = current;
                current.clear();
                auto code = prompt(promptText);
                if (code && !code->empty() && childPtr) childPtr->Write(Text::Narrow(*code) + "\n");
            }
        }
    });
    if (!child) {
        if (line) line(L"No se pudo iniciar SteamCMD.");
        return;
    }
    {
        std::lock_guard lock(mutex);
        childPtr = child.get();
    }
    while (!child->Wait(250)) {
        if (cancel.Requested()) {
            child->Kill();
            child->Wait(5000);
            break;
        }
    }
    std::lock_guard lock(mutex);
    if (!current.empty() && line) line(current);
}

} // namespace SpeedrunLauncher::Services::SteamCmd
