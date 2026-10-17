#include "Services/Community/BugReportService.h"
#include <windows.h>
#include "Core/Files.h"
#include "Core/Http.h"
#include "Core/Json.h"
#include "Core/Text.h"
#include "Services/App/AppVersion.h"
#include "App/Config.h"

namespace SpeedrunLauncher::Services::BugReport {

namespace {

constexpr const wchar_t* kWebhook = Config::Discord::BugReportWebhook;

std::string MimeType(const std::wstring& path) {
    const std::wstring ext = Text::Lower(Files::Extension(path));
    if (ext == L".jpg" || ext == L".jpeg") return "image/jpeg";
    if (ext == L".png") return "image/png";
    if (ext == L".gif") return "image/gif";
    if (ext == L".webp") return "image/webp";
    if (ext == L".bmp") return "image/bmp";
    return "application/octet-stream";
}

// DateTime.UtcNow.ToString("o")
std::wstring UtcNowIso() {
    SYSTEMTIME t;
    GetSystemTime(&t);
    wchar_t buf[40];
    swprintf_s(buf, L"%04u-%02u-%02uT%02u:%02u:%02u.%03u0000Z", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    return buf;
}

} // namespace

bool Send(const std::wstring& discordId, const std::wstring& discordName, const std::wstring& title, const std::wstring& description,
          const std::optional<std::wstring>& imagePath) {
    Json::Value embed = Json::Value::MakeObject();
    Json::Value author = Json::Value::MakeObject();
    author.Set("name", discordName + L"  \x00B7  ID: " + discordId);
    embed.Set("author", author);
    embed.Set("title", title);
    embed.Set("description", description);
    embed.Set("color", 0x7B2FBE);
    const bool withImage = imagePath && Files::Exists(*imagePath);
    const std::wstring fileName = withImage ? Files::FileName(*imagePath) : L"";
    if (withImage) {
        Json::Value image = Json::Value::MakeObject();
        image.Set("url", L"attachment://" + fileName);
        embed.Set("image", image);
    }
    Json::Value footer = Json::Value::MakeObject();
    footer.Set("text", L"Playtime Speed Launcher  \x00B7  " + AppVersion::DisplayVersion());
    embed.Set("footer", footer);
    embed.Set("timestamp", UtcNowIso());
    Json::Value embeds = Json::Value::MakeArray();
    embeds.Push(embed);
    Json::Value payload = Json::Value::MakeObject();
    payload.Set("content", L"<@" + discordId + L">");
    payload.Set("embeds", embeds);

    if (!withImage) return Http::PostJson(kWebhook, payload.Serialize()).Ok();

    auto bytes = Files::ReadBytes(*imagePath);
    if (!bytes) return false;
    Http::Multipart form;
    form.AddField("payload_json", payload.Serialize());
    form.AddFile("files[0]", Text::Narrow(fileName), MimeType(*imagePath), *bytes);
    Http::Request request;
    request.method = L"POST";
    request.url = kWebhook;
    request.body = form.Body();
    request.contentType = form.ContentType();
    request.timeoutMs = 30000;
    return Http::Send(request).Ok();
}

} // namespace SpeedrunLauncher::Services::BugReport
