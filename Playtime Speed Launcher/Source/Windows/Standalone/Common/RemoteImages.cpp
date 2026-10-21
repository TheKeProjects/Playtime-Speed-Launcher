#include "Windows/Standalone/Common/RemoteImages.h"
#include <windows.h>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <vector>
#include "App/Paths.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/Http.h"
#include "Core/Text.h"
#include "UI/Window.h"

namespace SpeedrunLauncher::Windows::RemoteImages {

namespace {

constexpr int kAttempts = 4;              // speedrun.com's Cloudflare answers 403 now and then: try again
constexpr unsigned kDiskCacheMaxSide = 256;   // small pictures (runner avatars) are kept decoded on disk

struct Entry {
    bool requested = false;
    ImagePtr image;
};

std::map<std::wstring, Entry>& Cache() {
    static std::map<std::wstring, Entry> cache;
    return cache;
}

struct Decoded {
    bool ok = false;
    unsigned w = 0, h = 0;
    std::vector<uint32_t> pixels;
};

// TempDir\RemoteImages\<hash of url + size>.bin: width, height, then the BGRA pixels. The URLs carry
// a version ("?v=..."), so a changed picture gets a new file.
std::wstring DiskPath(const std::wstring& url, unsigned maxSide) {
    wchar_t name[32];
    swprintf_s(name, L"%016llx.bin", static_cast<unsigned long long>(std::hash<std::wstring>{}(url + L'|' + std::to_wstring(maxSide))));
    return Files::Combine(Paths::TempDir(), L"RemoteImages", name);
}

bool ReadDisk(const std::wstring& path, Decoded& out) {
    auto bytes = Files::ReadBytes(path);
    if (!bytes || bytes->size() < 8) return false;
    std::memcpy(&out.w, bytes->data(), 4);
    std::memcpy(&out.h, bytes->data() + 4, 4);
    const size_t count = static_cast<size_t>(out.w) * out.h;
    if (count == 0 || bytes->size() != 8 + count * 4) return false;
    out.pixels.resize(count);
    std::memcpy(out.pixels.data(), bytes->data() + 8, count * 4);
    return true;
}

void WriteDisk(const std::wstring& path, const Decoded& in) {
    std::string bytes(8 + in.pixels.size() * 4, '\0');
    std::memcpy(bytes.data(), &in.w, 4);
    std::memcpy(bytes.data() + 4, &in.h, 4);
    std::memcpy(bytes.data() + 8, in.pixels.data(), in.pixels.size() * 4);
    Files::CreateDirectories(Files::DirectoryName(path));
    Files::WriteBytes(path, bytes);
}

bool Contains(const std::wstring& s, const wchar_t* part) { return s.find(part) != std::wstring::npos; }

// speedrun.com profile pictures ("/static/user/<id>/image?v=x") sit behind a Cloudflare challenge
// that turns away most requests from apps (403), and it depends on the player's IP: for some people
// none of them load. The same picture as "/image.png?v=x" comes from Cloudflare's cache instead.
// (The extension doesn't change the format: it's still whatever was uploaded, WebP included.)
std::wstring Direct(const std::wstring& url) {
    if (!Contains(url, L"speedrun.com/static/")) return url;
    const size_t query = url.find(L'?');
    const size_t end = query == std::wstring::npos ? url.size() : query;
    if (end >= 6 && url.compare(end - 6, 6, L"/image") == 0) return url.substr(0, end) + L".png" + url.substr(end);
    return url;
}

// The same picture converted to PNG by wsrv.nl's image CDN: for WebP pictures on PCs without the
// WebP codec (stripped-down Windows installs don't have it, so WIC can't open them) and for pictures
// speedrun.com keeps refusing
std::wstring ViaProxy(const std::wstring& url) { return L"https://wsrv.nl/?url=" + Text::UrlEncode(url) + L"&output=png"; }

// Downloads and decodes `url` (retrying refusals); `undecodable` is set when it arrived but Windows
// couldn't read it
bool Fetch(const std::wstring& url, unsigned maxSide, Decoded& out, int attempts, bool& undecodable) {
    for (int attempt = 1; attempt <= attempts; ++attempt) {
        const Http::Response response = Http::Get(url, {}, 15000);
        if (response.Ok() && !response.body.empty()) {
            if (Images::DecodeMemory(response.body, maxSide, out.w, out.h, out.pixels)) return true;
            undecodable = true;   // arrived fine but Windows can't read the format: retrying won't help
            return false;
        }
        if (response.status == 404) return false;   // gone: no point retrying
        if (attempt < attempts) Sleep(400 * attempt);
    }
    return false;
}

} // namespace

ImagePtr Get(const std::wstring& url, unsigned maxSide) {
    Entry& e = Cache()[url];
    if (!e.requested) {
        e.requested = true;
        // Download and decode on a worker (big pictures would stall a frame); only the GPU upload is on the UI thread
        Async::Run([url, maxSide] {
            auto decoded = std::make_shared<Decoded>();
            const bool diskCached = maxSide <= kDiskCacheMaxSide;
            const std::wstring disk = diskCached ? DiskPath(url, maxSide) : L"";
            if (diskCached && ReadDisk(disk, *decoded)) {
                decoded->ok = true;
                return decoded;
            }
            const std::wstring direct = Direct(url);
            bool undecodable = false;
            decoded->ok = Fetch(direct, maxSide, *decoded, kAttempts, undecodable);
            // Couldn't get it or couldn't read it: the PNG copy from the image CDN
            if (!decoded->ok) decoded->ok = Fetch(ViaProxy(direct), maxSide, *decoded, 2, undecodable);
            if (decoded->ok && diskCached) WriteDisk(disk, *decoded);
            return decoded;
        }, [url](std::shared_ptr<Decoded> decoded) {
            if (!decoded->ok) return;
            Cache()[url].image = Images::FromPixels(decoded->w, decoded->h, decoded->pixels.data());
            // Show it right away, even in a window that stopped animating while it waited
            for (UI::Window* window : UI::Window::All()) window->Invalidate();
        });
    }
    return e.image;
}

} // namespace SpeedrunLauncher::Windows::RemoteImages
