#pragma once
#include <string>
#include "App/Config.h"
#include "Core/Http.h"
#include "Core/Json.h"

// The launcher's Supabase project (online counter, cloud playtime).
namespace SpeedrunLauncher::Supabase {

inline constexpr const wchar_t* Url = Config::Supabase::Url;
inline constexpr const wchar_t* AnonKey = Config::Supabase::AnonKey;

// POST /rest/v1/rpc/<function> with a JSON body. Blocking (worker threads only).
inline Http::Response Rpc(const std::wstring& function, const Json::Value& body) {
    return Http::PostJson(std::wstring(Url) + L"/rest/v1/rpc/" + function, body.Serialize(),
                          { { L"apikey", AnonKey }, { L"Authorization", std::wstring(L"Bearer ") + AnonKey } });
}

} // namespace SpeedrunLauncher::Supabase
