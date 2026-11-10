#include "Services/Account/PictureCompressor.h"
#include <windows.h>
#include <wincodec.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <wrl/client.h>
#include "App/Config.h"
#include "Core/Files.h"
#include "Core/Text.h"
#include "Graphics/AnimatedImages.h"
#include "Graphics/Device.h"

namespace SpeedrunLauncher::Services::Account::PictureCompressor {

using Microsoft::WRL::ComPtr;

namespace {

// Files this small go up as they are (best quality, nothing to gain)
constexpr size_t kKeepStillBytes = 1536u * 1024u;
constexpr size_t kKeepGifBytes = 3u * 1024u * 1024u;
// What the compressed file should weigh at most (everyone in the ranking downloads it)
constexpr size_t kAvatarTarget = 4u * 1024u * 1024u;
constexpr size_t kBannerTarget = 7u * 1024u * 1024u;
// Sizes tried, biggest first (longest side, pixels)
const unsigned kAvatarSides[] = { 320, 256, 200, 160, 128 };
const unsigned kBannerSides[] = { 960, 800, 640, 520, 400 };
constexpr size_t kDecodeBudget = 320u * 1024u * 1024u;

void SetUi2(IWICMetadataQueryWriter* w, const wchar_t* name, unsigned value) {
    PROPVARIANT v;
    PropVariantInit(&v);
    v.vt = VT_UI2;
    v.uiVal = static_cast<USHORT>(std::min(value, 65535u));
    w->SetMetadataByName(name, &v);
}

void SetUi1(IWICMetadataQueryWriter* w, const wchar_t* name, unsigned value) {
    PROPVARIANT v;
    PropVariantInit(&v);
    v.vt = VT_UI1;
    v.bVal = static_cast<UCHAR>(std::min(value, 255u));
    w->SetMetadataByName(name, &v);
}

void SetBool(IWICMetadataQueryWriter* w, const wchar_t* name, bool value) {
    PROPVARIANT v;
    PropVariantInit(&v);
    v.vt = VT_BOOL;
    v.boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
    w->SetMetadataByName(name, &v);
}

void SetBytes(IWICMetadataQueryWriter* w, const wchar_t* name, UCHAR* data, ULONG count) {
    PROPVARIANT v;
    PropVariantInit(&v);
    v.vt = VT_UI1 | VT_VECTOR;
    v.caub.cElems = count;
    v.caub.pElems = data;
    w->SetMetadataByName(name, &v);
}

ComPtr<IWICBitmapEncoder> NewEncoder(const GUID& container, const std::wstring& path, ComPtr<IWICStream>& stream) {
    IWICImagingFactory* wic = Device::Get().Wic();
    ComPtr<IWICBitmapEncoder> encoder;
    if (!wic || FAILED(wic->CreateStream(&stream)) || FAILED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE)) ||
        FAILED(wic->CreateEncoder(container, nullptr, &encoder)) || FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)))
        return nullptr;
    return encoder;
}

ComPtr<IWICBitmap> ToBitmap(const AnimatedImages::Frames& f, const std::vector<uint32_t>& pixels) {
    ComPtr<IWICBitmap> bitmap;
    Device::Get().Wic()->CreateBitmapFromMemory(f.w, f.h, GUID_WICPixelFormat32bppPBGRA, f.w * 4, static_cast<UINT>(pixels.size() * 4),
                                                reinterpret_cast<BYTE*>(const_cast<uint32_t*>(pixels.data())), &bitmap);
    return bitmap;
}

// Animated GIF that loops forever; `step` keeps every n-th frame (adding up the delays)
bool EncodeGif(const std::wstring& path, const AnimatedImages::Frames& f, size_t step) {
    IWICImagingFactory* wic = Device::Get().Wic();
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> encoder = NewEncoder(GUID_ContainerFormatGif, path, stream);
    if (!encoder) return false;
    ComPtr<IWICMetadataQueryWriter> global;
    if (SUCCEEDED(encoder->GetMetadataQueryWriter(&global))) {
        static UCHAR app[] = { 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0' };
        static UCHAR loop[] = { 3, 1, 0, 0, 0 };   // loop count 0 = forever
        SetBytes(global.Get(), L"/appext/application", app, 11);
        SetBytes(global.Get(), L"/appext/data", loop, 5);
        SetUi2(global.Get(), L"/logscrdesc/Width", f.w);
        SetUi2(global.Get(), L"/logscrdesc/Height", f.h);
    }
    for (size_t i = 0; i < f.frames.size(); i += step) {
        double delay = 0.0;
        for (size_t k = i; k < std::min(i + step, f.frames.size()); ++k) delay += f.delays[k];

        ComPtr<IWICBitmap> bitmap = ToBitmap(f, f.frames[i]);
        ComPtr<IWICPalette> palette;
        ComPtr<IWICFormatConverter> indexed;
        if (!bitmap || FAILED(wic->CreatePalette(&palette)) || FAILED(palette->InitializeFromBitmap(bitmap.Get(), 256, TRUE)) ||
            FAILED(wic->CreateFormatConverter(&indexed)) ||
            FAILED(indexed->Initialize(bitmap.Get(), GUID_WICPixelFormat8bppIndexed, WICBitmapDitherTypeErrorDiffusion, palette.Get(), 50.0,
                                       WICBitmapPaletteTypeCustom)))
            return false;

        ComPtr<IWICBitmapFrameEncode> frame;
        ComPtr<IPropertyBag2> props;
        if (FAILED(encoder->CreateNewFrame(&frame, &props)) || FAILED(frame->Initialize(props.Get())) || FAILED(frame->SetSize(f.w, f.h)))
            return false;
        WICPixelFormatGUID format = GUID_WICPixelFormat8bppIndexed;
        frame->SetPixelFormat(&format);
        frame->SetPalette(palette.Get());
        ComPtr<IWICMetadataQueryWriter> meta;
        if (SUCCEEDED(frame->GetMetadataQueryWriter(&meta))) {
            SetUi2(meta.Get(), L"/grctlext/Delay", std::max(2u, static_cast<unsigned>(std::lround(delay * 100.0))));
            SetUi1(meta.Get(), L"/grctlext/Disposal", 2);   // every frame is whole: clear before the next one
            // The transparent palette entry (if the frame has see-through pixels)
            UINT count = 0;
            palette->GetColorCount(&count);
            std::vector<WICColor> colors(count);
            UINT actual = 0;
            if (count && SUCCEEDED(palette->GetColors(count, colors.data(), &actual))) {
                for (UINT c = 0; c < actual; ++c) {
                    if ((colors[c] >> 24) == 0) {
                        SetBool(meta.Get(), L"/grctlext/TransparencyFlag", true);
                        SetUi1(meta.Get(), L"/grctlext/TransparentColorIndex", c);
                        break;
                    }
                }
            }
        }
        if (FAILED(frame->WriteSource(indexed.Get(), nullptr)) || FAILED(frame->Commit())) return false;
    }
    return SUCCEEDED(encoder->Commit()) && SUCCEEDED(stream->Commit(STGC_DEFAULT));
}

bool HasTransparency(const std::vector<uint32_t>& pixels) {
    for (uint32_t p : pixels)
        if ((p >> 24) < 250) return true;
    return false;
}

// JPEG (quality 0..1) or PNG
bool EncodeStill(const std::wstring& path, const AnimatedImages::Frames& f, bool jpeg, float quality) {
    IWICImagingFactory* wic = Device::Get().Wic();
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> encoder = NewEncoder(jpeg ? GUID_ContainerFormatJpeg : GUID_ContainerFormatPng, path, stream);
    if (!encoder) return false;
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> props;
    if (FAILED(encoder->CreateNewFrame(&frame, &props))) return false;
    if (jpeg && props) {
        PROPBAG2 option{};
        option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_R4;
        value.fltVal = quality;
        props->Write(1, &option, &value);
    }
    if (FAILED(frame->Initialize(props.Get())) || FAILED(frame->SetSize(f.w, f.h))) return false;
    WICPixelFormatGUID format = jpeg ? GUID_WICPixelFormat24bppBGR : GUID_WICPixelFormat32bppBGRA;
    frame->SetPixelFormat(&format);
    ComPtr<IWICBitmap> bitmap = ToBitmap(f, f.frames.front());
    ComPtr<IWICFormatConverter> converter;
    if (!bitmap || FAILED(wic->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(bitmap.Get(), format, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
        return false;
    if (FAILED(frame->WriteSource(converter.Get(), nullptr)) || FAILED(frame->Commit())) return false;
    return SUCCEEDED(encoder->Commit()) && SUCCEEDED(stream->Commit(STGC_DEFAULT));
}

size_t FileSize(const std::wstring& path) { return static_cast<size_t>(Files::Size(path).value_or(SIZE_MAX)); }

} // namespace

Result Prepare(const std::wstring& source, bool banner) {
    Result result;
    const auto sourceSize = Files::Size(source);
    if (!sourceSize) {
        result.errorKey = "acc_err_picture";
        return result;
    }
    const size_t target = std::min(banner ? kBannerTarget : kAvatarTarget, Config::Supabase::MaxPictureBytes);
    const std::wstring ext = Text::Lower(Files::Extension(source));
    const bool gifFile = ext == L".gif";

    // Small enough already: upload it as it is
    if (*sourceSize <= (gifFile ? kKeepGifBytes : kKeepStillBytes) && *sourceSize <= target) {
        result.ok = true;
        result.path = source;
        return result;
    }

    const auto& sides = banner ? kBannerSides : kAvatarSides;
    bool decodedAny = false;
    bool animated = false;
    std::wstring output;
    const auto attempt = [&](unsigned side, size_t step, float quality) -> bool {
        AnimatedImages::Frames frames;
        if (!AnimatedImages::DecodeFile(source, side, frames, kDecodeBudget) || frames.frames.empty()) return false;
        decodedAny = true;
        animated = frames.frames.size() > 1;
        if (animated && step >= frames.frames.size()) return false;
        const bool jpeg = !animated && !HasTransparency(frames.frames.front());
        if (!output.empty()) Files::Delete(output);
        output = Files::UniqueTempFile(animated ? L".gif" : jpeg ? L".jpg" : L".png");
        const bool ok = animated ? EncodeGif(output, frames, step) : EncodeStill(output, frames, jpeg, quality);
        return ok && FileSize(output) <= target;
    };

    // Still pictures: smaller side / lower quality until it fits. GIFs: smaller side first (keeps
    // the movement smooth), then every other frame, then every third.
    for (size_t step = 1; step <= 3; ++step) {
        for (unsigned side : sides) {
            if (attempt(side, step, step == 1 ? 0.9f : 0.75f)) {
                result.ok = true;
                result.path = output;
                result.temporary = true;
                return result;
            }
            if (!decodedAny) {
                result.errorKey = "acc_err_picture";
                return result;
            }
        }
        if (!animated && step >= 2) break;
    }
    if (!output.empty()) Files::Delete(output);
    // Couldn't re-encode it: the original still goes if the server takes its size
    if (*sourceSize <= Config::Supabase::MaxPictureBytes) {
        result.ok = true;
        result.path = source;
        return result;
    }
    result.errorKey = "acc_err_too_big";
    return result;
}

} // namespace SpeedrunLauncher::Services::Account::PictureCompressor
