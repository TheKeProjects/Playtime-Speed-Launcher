#include "Services/Community/SkipReportService.h"
#include "Core/Http.h"
#include "Core/Json.h"
#include "Core/Text.h"
#include "Services/App/AppVersion.h"
#include "App/Config.h"

namespace SpeedrunLauncher::Services::SkipReport {

namespace {

constexpr const wchar_t* kWebhook = Config::Discord::SkipReportWebhook;

Json::Value Field(const char* name, const std::wstring& value, bool isInline) {
    Json::Value f = Json::Value::MakeObject();
    f.Set("name", name);
    f.Set("value", value);
    f.Set("inline", isInline);
    return f;
}

} // namespace

bool IsConfigured() { return !std::wstring(kWebhook).empty(); }

std::wstring Send(const Report& r) {
    if (!IsConfigured()) return L"Reporting isn't configured yet. Try again later.";
    Json::Value fields = Json::Value::MakeArray();
    fields.Push(Field("Skip Name", r.name, true));
    if (!r.chapter.empty()) fields.Push(Field("Chapter", r.chapter, true));
    if (!r.version.empty()) fields.Push(Field("Version", r.version, true));
    if (!r.runCategories.empty()) fields.Push(Field("Run Category", Text::Join(r.runCategories, L", "), true));
    if (!r.routes.empty()) fields.Push(Field("Route", Text::Join(r.routes, L", "), true));
    if (!r.restrictions.empty()) fields.Push(Field("Restrictions", Text::Join(r.restrictions, L", "), true));
    fields.Push(Field("Uses Core", r.cores ? L"Yes" : L"No", true));
    if (!r.author.empty()) fields.Push(Field("Author", r.author, true));
    if (!r.videoUrl.empty()) fields.Push(Field("Video URL", r.videoUrl, false));
    if (!r.description.empty()) fields.Push(Field("Description", r.description, false));

    Json::Value embed = Json::Value::MakeObject();
    embed.Set("title", "New Skip Submission");
    embed.Set("color", 52394);  // #00CCAA
    embed.Set("fields", fields);
    Json::Value footer = Json::Value::MakeObject();
    footer.Set("text", L"Sent via Playtime Turbo \x00B7 " + AppVersion::DisplayVersion());
    embed.Set("footer", footer);
    Json::Value embeds = Json::Value::MakeArray();
    embeds.Push(embed);
    Json::Value payload = Json::Value::MakeObject();
    payload.Set("embeds", embeds);

    const Http::Response response = Http::PostJson(kWebhook, payload.Serialize());
    if (response.Ok()) return {};
    if (response.status == 0) return L"Error: " + response.error;
    return L"Failed (" + std::to_wstring(response.status) + L"). Try again.";
}

} // namespace SpeedrunLauncher::Services::SkipReport
