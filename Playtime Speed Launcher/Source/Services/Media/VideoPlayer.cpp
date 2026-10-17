#include "Services/Media/VideoPlayer.h"
#include <windows.h>
#include <d3d11.h>
#include <mfapi.h>
#include <mfmediaengine.h>
#include <algorithm>
#include "Graphics/Device.h"
#include "Graphics/Graphics.h"

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")

namespace SpeedrunLauncher::Media {

using Microsoft::WRL::ComPtr;

namespace {

bool StartMediaFoundation() {
    static const bool started = SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_FULL));
    return started;
}

// IMFMediaEngineNotify that forwards events to the player.
class Notify : public IMFMediaEngineNotify {
public:
    explicit Notify(VideoPlayer* p) : player(p) {}
    void Detach() { player = nullptr; }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMFMediaEngineNotify)) {
            *ppv = static_cast<IMFMediaEngineNotify*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs); }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG r = InterlockedDecrement(&refs);
        if (r == 0) delete this;
        return r;
    }
    STDMETHODIMP EventNotify(DWORD event, DWORD_PTR param1, DWORD param2) override {
        if (player) player->OnEvent(event, param1, param2);
        return S_OK;
    }

private:
    VideoPlayer* player;
    LONG refs = 1;
};

} // namespace

struct VideoPlayer::Impl {
    ComPtr<IMFDXGIDeviceManager> manager;
    UINT resetToken = 0;
    ComPtr<IMFMediaEngine> engine;
    ComPtr<IMFMediaEngineEx> engineEx;
    Notify* notify = nullptr;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID2D1Bitmap1> bitmap;
    unsigned texW = 0, texH = 0;
};

VideoPlayer::VideoPlayer() : impl(std::make_unique<Impl>()) {}

VideoPlayer::~VideoPlayer() { Stop(); }

bool VideoPlayer::Open(const std::wstring& path, bool autoPlay, bool shouldLoop) {
    Stop();
    ended = false;
    failed = false;
    ready = false;
    loop = shouldLoop;
    Device& device = Device::Get();
    if (!StartMediaFoundation() || !device.D3D()) {
        failed = true;
        return false;
    }
    if (FAILED(MFCreateDXGIDeviceManager(&impl->resetToken, &impl->manager)) ||
        FAILED(impl->manager->ResetDevice(device.D3D(), impl->resetToken))) {
        failed = true;
        return false;
    }
    ComPtr<IMFMediaEngineClassFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) {
        failed = true;
        return false;
    }
    ComPtr<IMFAttributes> attributes;
    MFCreateAttributes(&attributes, 3);
    impl->notify = new Notify(this);
    attributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, impl->notify);
    attributes->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, impl->manager.Get());
    attributes->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, DXGI_FORMAT_B8G8R8A8_UNORM);
    if (FAILED(factory->CreateInstance(0, attributes.Get(), &impl->engine))) {
        failed = true;
        return false;
    }
    impl->engine.As(&impl->engineEx);
    impl->engine->SetLoop(loop ? TRUE : FALSE);
    impl->engine->SetVolume(volume);
    impl->engine->SetMuted(muted ? TRUE : FALSE);
    BSTR url = SysAllocString(path.c_str());
    const HRESULT hr = impl->engine->SetSource(url);
    SysFreeString(url);
    if (FAILED(hr)) {
        failed = true;
        return false;
    }
    impl->engine->SetAutoPlay(autoPlay ? TRUE : FALSE);
    if (autoPlay) impl->engine->Play();
    return true;
}

void VideoPlayer::OnEvent(unsigned long event, unsigned long long, unsigned long) {
    switch (event) {
        case MF_MEDIA_ENGINE_EVENT_ENDED:
            if (!loop) ended = true;
            break;
        case MF_MEDIA_ENGINE_EVENT_ERROR:
            failed = true;
            break;
        case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:
        case MF_MEDIA_ENGINE_EVENT_FORMATCHANGE:
            ready = true;
            break;
        default:
            break;
    }
}

void VideoPlayer::Play() {
    if (impl->engine) {
        ended = false;
        impl->engine->Play();
    }
}

void VideoPlayer::Pause() {
    if (impl->engine) impl->engine->Pause();
}

void VideoPlayer::Stop() {
    if (impl->notify) impl->notify->Detach();
    if (impl->engine) impl->engine->Shutdown();
    impl->engineEx.Reset();
    impl->engine.Reset();
    if (impl->notify) impl->notify->Release();
    impl->notify = nullptr;
    impl->bitmap.Reset();
    impl->texture.Reset();
    impl->manager.Reset();
    impl->texW = impl->texH = 0;
    width = height = 0;
}

bool VideoPlayer::IsOpen() const { return impl->engine != nullptr; }

bool VideoPlayer::IsPlaying() const { return impl->engine && !impl->engine->IsPaused() && !ended; }

void VideoPlayer::SetVolume(double v) {
    volume = std::clamp(v, 0.0, 1.0);
    if (impl->engine) impl->engine->SetVolume(volume);
}

void VideoPlayer::SetMuted(bool on) {
    muted = on;
    if (impl->engine) impl->engine->SetMuted(on ? TRUE : FALSE);
}

double VideoPlayer::Position() const { return impl->engine ? impl->engine->GetCurrentTime() : 0.0; }

double VideoPlayer::Duration() const {
    if (!impl->engine) return 0.0;
    const double d = impl->engine->GetDuration();
    return d > 0.0 && d < 1e9 ? d : 0.0;
}

void VideoPlayer::Seek(double seconds) {
    if (impl->engine) impl->engine->SetCurrentTime(std::max(0.0, seconds));
}

bool VideoPlayer::EnsureTarget() {
    DWORD w = 0, h = 0;
    if (FAILED(impl->engine->GetNativeVideoSize(&w, &h)) || w == 0 || h == 0) return false;
    width = w;
    height = h;
    if (impl->texture && impl->texW == w && impl->texH == h) return true;
    impl->bitmap.Reset();
    impl->texture.Reset();
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(Device::Get().D3D()->CreateTexture2D(&desc, nullptr, &impl->texture))) return false;
    ComPtr<IDXGISurface> surface;
    impl->texture.As(&surface);
    const auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE,
                                               D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
    if (FAILED(Device::Get().ResourceContext()->CreateBitmapFromDxgiSurface(surface.Get(), &props, &impl->bitmap))) {
        impl->texture.Reset();
        return false;
    }
    impl->texW = w;
    impl->texH = h;
    return true;
}

bool VideoPlayer::Update() {
    if (!impl->engine) return false;
    LONGLONG pts = 0;
    if (impl->engine->OnVideoStreamTick(&pts) != S_OK) return false;
    if (!EnsureTarget()) return false;
    const MFVideoNormalizedRect source{ 0.f, 0.f, 1.f, 1.f };
    const RECT dest{ 0, 0, static_cast<LONG>(impl->texW), static_cast<LONG>(impl->texH) };
    const MFARGB border{ 0, 0, 0, 255 };
    return SUCCEEDED(impl->engine->TransferVideoFrame(impl->texture.Get(), &source, &dest, &border));
}

void VideoPlayer::Draw(Graphics& g, const Rect& r, float opacity) {
    if (!impl->bitmap || width == 0 || height == 0) return;
    const float s = std::min(r.w / width, r.h / height);
    const float w = width * s, h = height * s;
    g.DrawBitmap(impl->bitmap.Get(), { r.x + (r.w - w) * 0.5f, r.y + (r.h - h) * 0.5f, w, h }, opacity);
}

} // namespace SpeedrunLauncher::Media
