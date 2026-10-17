#include "Services/GameFiles/HandModSubmission.h"
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <ctime>
#include "Core/Files.h"
#include "Core/Http.h"
#include "Core/Json.h"
#include "Core/Text.h"
#include "Core/Zip.h"
#include "Services/App/AppVersion.h"
#include "App/Config.h"

namespace SpeedrunLauncher::Services::HandModSubmission {

namespace {

constexpr const wchar_t* kWebhook = Config::Discord::HandModSubmissionWebhook;
// Unboosted servers only guarantee 8 MB per message; leave room for the JSON part.
constexpr long long kMaxAttachment = 8ll * 1024 * 1024;
constexpr long long kChunk = kMaxAttachment - 128 * 1024;
constexpr int kMaxParts = 24;

std::wstring SanitizeFileName(const std::wstring& name) {
    std::wstring out;
    for (wchar_t c : name)
        if (c >= 32 && !wcschr(L"<>:\"/\\|?*", c)) out += c;
    out = Text::Trim(out);
    return out.empty() ? L"mod" : out;
}

std::string IsoNow() {
    std::time_t t = std::time(nullptr);
    std::tm utc{};
    gmtime_s(&utc, &t);
    char buffer[40];
    strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S.0000000Z", &utc);
    return buffer;
}

Json::Value Field(const char* name, const std::wstring& value) {
    Json::Value f = Json::Value::MakeObject();
    f.Set("name", name);
    f.Set("value", value);
    f.Set("inline", true);
    return f;
}

} // namespace

const std::vector<std::wstring>& Colors() {
    static const std::vector<std::wstring> colors = { L"Blue", L"Red", L"Green", L"Purple", L"Flare", L"OmiHand", L"Pressurized",
                                                      L"Conductive" };
    return colors;
}

bool IsConfigured() { return !std::wstring(kWebhook).empty(); }

std::wstring Submit(const std::wstring& modName, int chapter, const std::vector<std::wstring>& colors,
                    const std::vector<std::wstring>& filePaths, const std::wstring& submitterName, const std::wstring& submitterId) {
    if (!IsConfigured()) return L"Submissions aren't configured yet. Try again later.";
    if (filePaths.empty()) return L"Attach at least one mod file first.";

    const std::wstring zipName = SanitizeFileName(modName) + L".zip";
    Zip::Writer writer;
    for (const auto& path : filePaths) {
        auto bytes = Files::ReadBytes(path);
        if (!bytes) return L"Couldn't package your files: cannot read " + Files::FileName(path);
        writer.Add(Files::FileName(path), *bytes, true);
    }
    const std::string zip = writer.Finish();

    const int parts = std::max(1, static_cast<int>(std::ceil(static_cast<double>(zip.size()) / static_cast<double>(kChunk))));
    if (parts > kMaxParts) {
        const double zippedMb = zip.size() / 1024.0 / 1024.0;
        const double maxMb = kMaxParts * kChunk / 1024.0 / 1024.0;
        return L"Your mod is too large even zipped (" + Text::FromDouble(zippedMb, 1) + L" MB). Keep it under about " +
               Text::FromDouble(maxMb, 0) + L" MB total, then try again.";
    }

    Json::Value fields = Json::Value::MakeArray();
    fields.Push(Field("Mod Name", modName));
    fields.Push(Field("Chapter", L"Chapter " + std::to_wstring(chapter)));
    if (!colors.empty()) fields.Push(Field("Hand Color(s)", Text::Join(colors, L", ")));
    if (!submitterName.empty()) fields.Push(Field("Submitted By", submitterName));

    for (int part = 0; part < parts; ++part) {
        const size_t offset = static_cast<size_t>(part) * static_cast<size_t>(kChunk);
        const std::string chunk = zip.substr(offset, std::min<size_t>(static_cast<size_t>(kChunk), zip.size() - offset));

        Json::Value payload = Json::Value::MakeObject();
        if (part == 0) {
            // Ping the submitter above the embed so reviewers can follow up
            const std::wstring mention = submitterId.empty() ? L"" : L"<@" + submitterId + L">";
            if (parts > 1) {
                const std::wstring n = std::to_wstring(parts);
                std::wstring text = L"This mod's zip didn't fit in one message, so it's split into " + n + L" parts below. Download all " +
                                    n + L" attachments into the same folder, then merge them back into `" + zipName +
                                    L"` before extracting:\n\x2022 Windows (cmd): `copy /b " + zipName + L".001+" + zipName + L".002+... " +
                                    zipName + L"`\n\x2022 Mac/Linux: `cat " + zipName + L".* > " + zipName + L"`";
                payload.Set("content", mention.empty() ? text : mention + L" " + text);
            } else if (!mention.empty()) {
                payload.Set("content", mention);
            }
            Json::Value embed = Json::Value::MakeObject();
            embed.Set("title", "New Hand Mod Submission");
            embed.Set("color", 52394);
            embed.Set("fields", fields);
            Json::Value footer = Json::Value::MakeObject();
            footer.Set("text", L"Sent via Playtime Turbo \x00B7 " + AppVersion::DisplayVersion());
            embed.Set("footer", footer);
            embed.Set("timestamp", IsoNow());
            Json::Value embeds = Json::Value::MakeArray();
            embeds.Push(embed);
            payload.Set("embeds", embeds);
        } else {
            payload.Set("content", L"**" + modName + L"** \x2014 part " + std::to_wstring(part + 1) + L"/" + std::to_wstring(parts));
        }

        wchar_t suffix[16];
        swprintf_s(suffix, L".%03d", part + 1);
        const std::wstring partName = parts > 1 ? zipName + suffix : zipName;

        Http::Response response;
        for (int attempt = 0; attempt < 5; ++attempt) {
            Http::Multipart form;
            form.AddField("payload_json", payload.Serialize());
            form.AddFile("files[0]", Text::Narrow(partName), "application/octet-stream", chunk);
            Http::Request request;
            request.method = L"POST";
            request.url = kWebhook;
            request.body = form.Body();
            request.contentType = form.ContentType();
            request.timeoutMs = 60000;
            response = Http::Send(request);
            if (response.status != 429 || attempt == 4) break;
            const double retry = Text::ToDouble(response.Header(L"retry-after")).value_or(1.5);
            Sleep(static_cast<DWORD>((std::max(retry, 0.5) + 0.25) * 1000));
        }
        if (!response.Ok()) {
            if (response.status == 0) return L"Error: " + response.error;
            if (response.status == 413) {
                return parts > 1 ? L"Part " + std::to_wstring(part + 1) + L" of " + std::to_wstring(parts) +
                                       L" was still too large for Discord. Try trimming your mod files."
                                 : L"Those files are too large for Discord to accept. Zip them tighter or remove some files, then try again.";
            }
            return L"Failed sending part " + std::to_wstring(part + 1) + L"/" + std::to_wstring(parts) + L" (" +
                   std::to_wstring(response.status) + L"). Try again.";
        }
        if (part < parts - 1) Sleep(1200);   // Discord webhook rate limit: 5 requests / 2 s
    }
    return {};
}

} // namespace SpeedrunLauncher::Services::HandModSubmission
