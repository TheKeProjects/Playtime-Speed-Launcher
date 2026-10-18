#pragma once
#include <string>
#include "Graphics/Images.h"

// Images loaded from the web (YouTube thumbnails, runner pictures): the first Get starts a background
// download, later calls return the decoded image once it has arrived (nullptr until then / on failure).
// `maxSide` shrinks big pictures shown small (the first Get of a URL decides it).
namespace SpeedrunLauncher::Windows::RemoteImages {

ImagePtr Get(const std::wstring& url, unsigned maxSide = 2048);

} // namespace SpeedrunLauncher::Windows::RemoteImages
