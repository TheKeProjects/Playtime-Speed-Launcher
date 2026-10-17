#pragma once
#include <string>

// Button / chapter sounds (PlaySfx in the old code): winmm PlaySound with the volume slider.
namespace SpeedrunLauncher::Services::Sfx {

inline constexpr const wchar_t* Hover = L"OpcionMover.WAV";
inline constexpr const wchar_t* Click = L"SelecOption.WAV";
inline constexpr const wchar_t* Chapter = L"SelecChapter.WAV";
// Dragging a version tile: picked up, trembling while held (loop), put down
inline constexpr const wchar_t* Grab = L"Grab.WAV";
inline constexpr const wchar_t* Drag = L"Drag.WAV";
inline constexpr const wchar_t* Drop = L"Drop.WAV";
// The on-screen keyboard (key to click wizard): a quiet tactile key, three variants
void PlayKeyPress();

void SetVolume(float volume);   // 0..1
float Volume();
// `noStop`: don't interrupt a sound that is already playing (hover sounds)
void Play(const std::wstring& fileName, bool noStop = false);
// Repeats until another sound plays or Stop()
void PlayLoop(const std::wstring& fileName);
void Stop();

} // namespace SpeedrunLauncher::Services::Sfx
