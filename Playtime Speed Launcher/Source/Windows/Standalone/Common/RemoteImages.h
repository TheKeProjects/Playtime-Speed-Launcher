#pragma once
#include <string>
#include "Graphics/Images.h"

// Images loaded from the web (YouTube thumbnails): the first Get starts a background download,
// later calls return the decoded image once it has arrived (nullptr until then / on failure).
namespace SpeedrunLauncher::Windows::RemoteImages {

ImagePtr Get(const std::wstring& url);

} // namespace SpeedrunLauncher::Windows::RemoteImages
