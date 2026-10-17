#include "Windows/Standalone/Common/RemoteImages.h"
#include <map>
#include "Core/Async.h"
#include "Core/Http.h"

namespace SpeedrunLauncher::Windows::RemoteImages {

namespace {

struct Entry {
    bool requested = false;
    ImagePtr image;
};

std::map<std::wstring, Entry>& Cache() {
    static std::map<std::wstring, Entry> cache;
    return cache;
}

} // namespace

ImagePtr Get(const std::wstring& url) {
    Entry& e = Cache()[url];
    if (!e.requested) {
        e.requested = true;
        Async::Run([url] { return Http::Get(url); }, [url](Http::Response response) {
            if (response.Ok() && !response.body.empty()) Cache()[url].image = Images::FromMemory(response.body);
        });
    }
    return e.image;
}

} // namespace SpeedrunLauncher::Windows::RemoteImages
