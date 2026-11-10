#include "Graphics/AnimatedImages.h"
#include <windows.h>
#include <shlwapi.h>
#include <wincodec.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <optional>
#include "App/Paths.h"
#include "Core/Async.h"
#include "Core/Files.h"
#include "Core/Http.h"
#include "Core/Text.h"
#include "Graphics/Device.h"
#include "UI/Window.h"

#pragma comment(lib, "shlwapi.lib")

namespace SpeedrunLauncher::AnimatedImages {

using Microsoft::WRL::ComPtr;

namespace {

constexpr size_t kMaxFrames = 360;
constexpr size_t kMaxBytes = 72u * 1024u * 1024u;   // all the frames of one picture, decoded

struct Decoded : Frames {
    bool ok = false;
};

struct Entry {
    bool requested = false;
    AnimationPtr animation;
};

std::map<std::wstring, Entry>& Cache() {
    static std::map<std::wstring, Entry> cache;
    return cache;
}

bool IsUrl(const std::wstring& s) { return Text::StartsWith(s, L"http://", true) || Text::StartsWith(s, L"https://", true); }

std::wstring DiskPath(const std::wstring& url) {
    wchar_t name[40];
    swprintf_s(name, L"%016llx.img", static_cast<unsigned long long>(std::hash<std::wstring>{}(url)));
    return Files::Combine(Paths::TempDir(), L"ProfilePictures", name);
}

unsigned ReadUInt(IWICMetadataQueryReader* reader, const wchar_t* name, unsigned fallback) {
    if (!reader) return fallback;
    PROPVARIANT value;
    PropVariantInit(&value);
    unsigned out = fallback;
    if (SUCCEEDED(reader->GetMetadataByName(name, &value))) {
        if (value.vt == VT_UI1) out = value.bVal;
        else if (value.vt == VT_UI2) out = value.uiVal;
        else if (value.vt == VT_UI4) out = value.ulVal;
    }
    PropVariantClear(&value);
    return out;
}

// Pixels of a WIC source as premultiplied BGRA, optionally scaled
bool ToPixels(IWICBitmapSource* source, unsigned w, unsigned h, std::vector<uint32_t>& pixels) {
    IWICImagingFactory* wic = Device::Get().Wic();
    ComPtr<IWICBitmapSource> src = source;
    UINT sw = 0, sh = 0;
    source->GetSize(&sw, &sh);
    if (sw != w || sh != h) {
        ComPtr<IWICBitmapScaler> scaler;
        if (FAILED(wic->CreateBitmapScaler(&scaler)) || FAILED(scaler->Initialize(source, w, h, WICBitmapInterpolationModeFant))) return false;
        src = scaler;
    }
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(wic->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(src.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeMedianCut)))
        return false;
    pixels.resize(static_cast<size_t>(w) * h);
    return SUCCEEDED(converter->CopyPixels(nullptr, w * 4, static_cast<UINT>(pixels.size() * 4), reinterpret_cast<BYTE*>(pixels.data())));
}

void FitSize(unsigned w, unsigned h, unsigned maxSide, unsigned& outW, unsigned& outH) {
    outW = w;
    outH = h;
    if (std::max(w, h) <= maxSide) return;
    const double s = static_cast<double>(maxSide) / std::max(w, h);
    outW = std::max(1u, static_cast<unsigned>(w * s));
    outH = std::max(1u, static_cast<unsigned>(h * s));
}

bool DecodeStill(IWICBitmapDecoder* decoder, unsigned maxSide, Frames& out) {
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return false;
    UINT w = 0, h = 0;
    frame->GetSize(&w, &h);
    if (w == 0 || h == 0) return false;
    FitSize(w, h, maxSide, out.w, out.h);
    out.frames.emplace_back();
    if (!ToPixels(frame.Get(), out.w, out.h, out.frames.back())) return false;
    out.delays.push_back(1.0);
    return true;
}

// GIF frames drawn onto the logical screen one after another, honouring each frame's disposal
bool DecodeGif(IWICBitmapDecoder* decoder, UINT count, unsigned maxSide, Frames& out, size_t memoryBudget) {
    IWICImagingFactory* wic = Device::Get().Wic();
    ComPtr<IWICMetadataQueryReader> global;
    decoder->GetMetadataQueryReader(&global);
    ComPtr<IWICBitmapFrameDecode> first;
    if (FAILED(decoder->GetFrame(0, &first))) return false;
    UINT fw = 0, fh = 0;
    first->GetSize(&fw, &fh);
    const unsigned canvasW = std::max(1u, ReadUInt(global.Get(), L"/logscrdesc/Width", fw));
    const unsigned canvasH = std::max(1u, ReadUInt(global.Get(), L"/logscrdesc/Height", fh));
    FitSize(canvasW, canvasH, maxSide, out.w, out.h);

    // Long GIFs keep every n-th frame so they fit in memory (the delays add up, so the speed stays)
    const size_t frameBytes = static_cast<size_t>(out.w) * out.h * 4;
    const size_t budget = std::max<size_t>(1, std::min(kMaxFrames, memoryBudget / std::max<size_t>(frameBytes, 1)));
    const UINT step = static_cast<UINT>((count + budget - 1) / budget);

    std::vector<uint32_t> canvas(static_cast<size_t>(canvasW) * canvasH, 0u), saved;
    std::vector<uint32_t> pixels;
    double pendingDelay = 0.0;
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IWICBitmapFrameDecode> frame;
        if (FAILED(decoder->GetFrame(i, &frame))) break;
        ComPtr<IWICMetadataQueryReader> meta;
        frame->GetMetadataQueryReader(&meta);
        const unsigned left = ReadUInt(meta.Get(), L"/imgdesc/Left", 0), top = ReadUInt(meta.Get(), L"/imgdesc/Top", 0);
        unsigned delay = ReadUInt(meta.Get(), L"/grctlext/Delay", 10);
        if (delay <= 1) delay = 10;   // what browsers do with 0 / 1 (1/100 s)
        const unsigned disposal = ReadUInt(meta.Get(), L"/grctlext/Disposal", 0);
        UINT w = 0, h = 0;
        frame->GetSize(&w, &h);
        if (!ToPixels(frame.Get(), w, h, pixels)) break;
        if (disposal == 3) saved = canvas;

        // Source-over (premultiplied)
        for (UINT y = 0; y < h; ++y) {
            const unsigned cy = top + y;
            if (cy >= canvasH) break;
            for (UINT x = 0; x < w; ++x) {
                const unsigned cx = left + x;
                if (cx >= canvasW) break;
                const uint32_t s = pixels[static_cast<size_t>(y) * w + x];
                const uint32_t sa = s >> 24;
                if (sa == 0) continue;
                uint32_t& d = canvas[static_cast<size_t>(cy) * canvasW + cx];
                if (sa == 255) {
                    d = s;
                    continue;
                }
                const uint32_t inv = 255 - sa;
                uint32_t result = 0;
                for (int shift = 0; shift < 32; shift += 8) {
                    const uint32_t sc = (s >> shift) & 0xFF, dc = (d >> shift) & 0xFF;
                    result |= std::min<uint32_t>(255, sc + (dc * inv + 127) / 255) << shift;
                }
                d = result;
            }
        }

        pendingDelay += delay / 100.0;
        if (i % step == 0 || i + 1 == count) {
            out.frames.emplace_back();
            if (out.w == canvasW && out.h == canvasH) {
                out.frames.back() = canvas;
            } else {
                ComPtr<IWICBitmap> bitmap;
                if (FAILED(wic->CreateBitmapFromMemory(canvasW, canvasH, GUID_WICPixelFormat32bppPBGRA, canvasW * 4,
                                                       static_cast<UINT>(canvas.size() * 4), reinterpret_cast<BYTE*>(canvas.data()), &bitmap)) ||
                    !ToPixels(bitmap.Get(), out.w, out.h, out.frames.back())) {
                    out.frames.pop_back();
                    break;
                }
            }
            out.delays.push_back(pendingDelay);
            pendingDelay = 0.0;
        }

        if (disposal == 2) {
            for (UINT y = 0; y < h && top + y < canvasH; ++y)
                for (UINT x = 0; x < w && left + x < canvasW; ++x) canvas[static_cast<size_t>(top + y) * canvasW + left + x] = 0u;
        } else if (disposal == 3 && !saved.empty()) {
            canvas.swap(saved);
        }
    }
    return !out.frames.empty();
}

bool DecodeDecoder(IWICBitmapDecoder* decoder, unsigned maxSide, Frames& out, size_t memoryBudget) {
    GUID format{};
    decoder->GetContainerFormat(&format);
    UINT count = 0;
    decoder->GetFrameCount(&count);
    if (format == GUID_ContainerFormatGif && count > 1) {
        Frames gif;
        if (DecodeGif(decoder, count, maxSide, gif, memoryBudget)) {
            out = std::move(gif);
            return true;
        }
    }
    return DecodeStill(decoder, maxSide, out);
}

bool DecodeBytes(const std::string& bytes, unsigned maxSide, Frames& out) {
    IWICImagingFactory* wic = Device::Get().Wic();
    if (!wic || bytes.empty()) return false;
    ComPtr<IStream> stream;
    stream.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(bytes.data()), static_cast<UINT>(bytes.size())));
    if (!stream) return false;
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(wic->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder))) return false;
    return DecodeDecoder(decoder.Get(), maxSide, out, kMaxBytes);
}

std::optional<std::string> Fetch(const std::wstring& source) {
    if (!IsUrl(source)) return Files::ReadBytes(source);
    const std::wstring disk = DiskPath(source);
    if (auto cached = Files::ReadBytes(disk); cached && !cached->empty()) return cached;
    for (int attempt = 0; attempt < 3; ++attempt) {
        const Http::Response response = Http::Get(source, {}, 30000);
        if (response.Ok() && !response.body.empty()) {
            Files::CreateDirectories(Files::DirectoryName(disk));
            Files::WriteBytes(disk, response.body);
            return response.body;
        }
        if (response.status == 404 || response.status == 400) break;
        Sleep(500 * (attempt + 1));
    }
    return std::nullopt;
}

} // namespace

bool DecodeFile(const std::wstring& path, unsigned maxSide, Frames& out, size_t memoryBudget) {
    IWICImagingFactory* wic = Device::Get().Wic();
    if (!wic) return false;
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder))) return false;
    return DecodeDecoder(decoder.Get(), maxSide, out, memoryBudget);
}

ImagePtr Animation::At(double time) const {
    if (frames.empty()) return nullptr;
    if (frames.size() == 1 || duration <= 0.0) return frames.front();
    const double t = std::fmod(std::max(0.0, time), duration);
    const auto it = std::upper_bound(ends.begin(), ends.end(), t);
    const size_t index = std::min(static_cast<size_t>(it - ends.begin()), frames.size() - 1);
    return frames[index];
}

AnimationPtr Get(const std::wstring& source, unsigned maxSide) {
    if (source.empty()) return nullptr;
    const std::wstring key = source + L'|' + std::to_wstring(maxSide);
    Entry& entry = Cache()[key];
    if (!entry.requested) {
        entry.requested = true;
        Async::Run([source, maxSide] {
            auto decoded = std::make_shared<Decoded>();
            if (auto bytes = Fetch(source)) decoded->ok = DecodeBytes(*bytes, maxSide, *decoded);
            return decoded;
        }, [key](std::shared_ptr<Decoded> decoded) {
            if (!decoded->ok) return;
            // GPU upload on the UI thread
            auto animation = std::make_shared<Animation>();
            animation->w = static_cast<float>(decoded->w);
            animation->h = static_cast<float>(decoded->h);
            double t = 0.0;
            for (size_t i = 0; i < decoded->frames.size(); ++i) {
                ImagePtr image = Images::FromPixels(decoded->w, decoded->h, decoded->frames[i].data());
                if (!image) continue;
                animation->frames.push_back(std::move(image));
                t += decoded->delays[i];
                animation->ends.push_back(t);
            }
            animation->duration = t;
            if (animation->frames.empty()) return;
            Cache()[key].animation = std::move(animation);
            for (UI::Window* window : UI::Window::All()) window->Invalidate();
        });
    }
    return entry.animation;
}

void Seed(const std::wstring& url, const std::wstring& localFile) {
    if (url.empty() || localFile.empty()) return;
    const std::wstring disk = DiskPath(url);
    Files::CreateDirectories(Files::DirectoryName(disk));
    Files::Copy(localFile, disk, true);
}

} // namespace SpeedrunLauncher::AnimatedImages
