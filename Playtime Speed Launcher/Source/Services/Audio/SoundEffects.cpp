#include "Services/Audio/SoundEffects.h"
#include <windows.h>
#include <mmsystem.h>
#include "App/Paths.h"
#include "Core/Files.h"

#pragma comment(lib, "winmm.lib")

namespace SpeedrunLauncher::Services::Sfx {

namespace {
float volume = 0.5f;
}

void SetVolume(float v) { volume = v < 0.f ? 0.f : v > 1.f ? 1.f : v; }

float Volume() { return volume; }

namespace {

void PlayFile(const std::wstring& fileName, DWORD flags) {
    if (volume <= 0.f) return;
    const DWORD level = static_cast<DWORD>(volume * 0xFFFF) & 0xFFFF;
    waveOutSetVolume(nullptr, level | (level << 16));
    const std::wstring path = Paths::Asset(L"Sounds\\" + fileName);
    if (!Files::Exists(path)) return;
    PlaySoundW(path.c_str(), nullptr, SND_ASYNC | SND_FILENAME | SND_NODEFAULT | flags);
}

} // namespace

void Play(const std::wstring& fileName, bool noStop) { PlayFile(fileName, noStop ? SND_NOSTOP : 0); }

void PlayLoop(const std::wstring& fileName) { PlayFile(fileName, SND_LOOP); }

void Stop() { PlaySoundW(nullptr, nullptr, 0); }

void PlayKeyPress() {
    static int next = 0;
    static const wchar_t* variants[3] = { L"KeyPress1.WAV", L"KeyPress2.WAV", L"KeyPress3.WAV" };
    next = (next + 1 + (GetTickCount() & 1)) % 3;   // not the same one twice in a row
    PlayFile(variants[next], 0);
}

} // namespace SpeedrunLauncher::Services::Sfx
