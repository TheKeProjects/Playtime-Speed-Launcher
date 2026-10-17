#include "Windows/Standalone/Common/VideoPlayerPanel.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "UI/Style.h"
#include "UI/Widgets/Button.h"

namespace SpeedrunLauncher::Windows {

using namespace UI;

namespace {

constexpr wchar_t kPlay = 0xE768, kPause = 0xE769, kVolume = 0xE767, kMute = 0xE74F;

std::wstring FormatTime(double seconds) {
    const long long total = static_cast<long long>(std::max(0.0, seconds));
    wchar_t buf[32];
    if (total >= 3600) swprintf_s(buf, L"%lld:%02lld:%02lld", total / 3600, (total / 60) % 60, total % 60);
    else swprintf_s(buf, L"%lld:%02lld", total / 60, total % 60);
    return buf;
}

// Track with a filled left part and a round thumb (the SeekSlider / VolumeSlider templates).
// Returns true while the value changes; `dragging` reports an active drag.
bool TrackSlider(Ui& ui, Id id, const Rect& r, float& value, float trackH, float thumb, Color thumbColor, bool enabled, bool* dragging = nullptr) {
    auto& g = ui.g;
    const Interaction it = ui.Interact(id, r, enabled, false);
    bool changed = false;
    if (it.held) {
        const float v = std::clamp((ui.MouseX() - r.x - thumb * 0.5f) / std::max(1.f, r.w - thumb), 0.f, 1.f);   // IsMoveToPointEnabled
        if (v != value) {
            value = v;
            changed = true;
        }
    }
    if (dragging) *dragging = it.held;
    const float travel = r.w - thumb;
    const float tx = r.x + thumb * 0.5f + travel * value;
    const float cy = r.CY();
    g.FillRound({ r.x, cy - trackH * 0.5f, tx - r.x, trackH }, trackH * 0.5f, 0.f, 0.f, trackH * 0.5f, Palette::Teal);
    g.FillRound({ tx, cy - trackH * 0.5f, r.R() - tx, trackH }, 0.f, trackH * 0.5f, trackH * 0.5f, 0.f, Hex(0x0D2030));
    g.FillRound({ tx - thumb * 0.5f, cy - thumb * 0.5f, thumb, thumb }, thumb * 0.5f, thumbColor);
    return changed;
}

} // namespace

void VideoPlayerPanel::Create(HWND parent, Style s) {
    style = std::move(s);
    message = style.emptyText;
    player.onOpened = [this] {
        loading = false;
        playing = true;
        if (onOpened) onOpened();
    };
    player.onEnded = [this] {
        player.Seek(0.0);
        player.Play();
        playing = true;
    };
    player.onPlayState = [this](bool on) { playing = on; };
    player.onError = [this](const std::wstring& text) { ShowError(text); };
    player.SetVolume(volume);
    player.Create(parent);
}

void VideoPlayerPanel::Load(const std::wstring& videoId) {
    // No Stop() first: stopVideo() right before loadVideoById() can leave the embed stuck
    hasVideo = true;
    loading = true;
    playing = false;
    message = L"LOADING, PLEASE WAIT";
    messageColor = Rgba(0, 204, 170, 200);
    player.LoadVideo(videoId);
}

void VideoPlayerPanel::Stop() {
    player.Stop();
    hasVideo = loading = playing = false;
    message = style.emptyText;
    messageColor = Hex(0x1A3A55);
}

void VideoPlayerPanel::ShowError(const std::wstring& text) {
    hasVideo = loading = playing = false;
    message = text;
    messageColor = style.errorColor;
    if (onFailed) onFailed();
}

void VideoPlayerPanel::TogglePlay() {
    if (!hasVideo || loading) return;
    if (playing) player.Pause();
    else player.Play();
    playing = !playing;
}

void VideoPlayerPanel::Draw(Ui& ui, const Rect& area, float scale, bool roundBottomLeft, bool roundBottomRight, bool controlsBottomBorder) {
    auto& g = ui.g;
    const Rect video{ area.x, area.y, area.w, area.h - 22.f - 52.f };
    const Rect seekRow{ area.x, video.B(), area.w, 22.f };
    const Rect controls{ area.x, seekRow.B(), area.w, 52.f };

    // Video (the WebView2 window sits on top while a video is loaded or loading)
    g.FillRect(video, Colors::Black);
    const bool showPlayer = hasVideo;
    const RECT bounds{ static_cast<LONG>(std::lround(video.x * scale)), static_cast<LONG>(std::lround(video.y * scale)),
                       static_cast<LONG>(std::lround(video.R() * scale)), static_cast<LONG>(std::lround(video.B() * scale)) };
    if (std::memcmp(&bounds, &lastBounds, sizeof(RECT)) != 0) {
        player.SetBounds(bounds);
        lastBounds = bounds;
    }
    if (showPlayer != lastVisible) {
        player.SetVisible(showPlayer);
        lastVisible = showPlayer;
    }
    if (!hasVideo || loading) {
        // Empty / loading / error state
        const float labelH = g.Measure(L"Ag", Mono(11.f)).h;
        const float glyphH = style.emptyGlyph && !hasVideo ? g.Measure(std::wstring(1, kPlay), IconFont(56.f)).h + 10.f : 0.f;
        const float barH = loading ? 14.f + 4.f : 0.f;
        float y = video.CY() - (glyphH + labelH + barH) * 0.5f;
        if (glyphH > 0.f) {
            g.Text(std::wstring(1, kPlay), { video.x, y, video.w, glyphH - 10.f }, Hex(0x0A1E2E), IconFont(56.f));
            y += glyphH;
        }
        g.Text(message, { video.x + 20.f, y, video.w - 40.f, labelH }, messageColor, Mono(11.f).Center().Ellipsis());
        y += labelH;
        if (loading) {
            const float w = style.emptyGlyph ? 220.f : 200.f;
            g.FillRound({ video.CX() - w * 0.5f, y + 14.f, w, 4.f }, 2.f, Hex(0x0D2030));
            g.FillRound({ video.CX() - w * 0.5f, y + 14.f, w, 4.f }, 2.f, Palette::Teal);
            ui.KeepAnimating();
        }
    }

    // Seek bar
    g.FillRect(seekRow, Hex(0x040810));
    const bool ready = hasVideo && !loading && player.HasDuration();
    float progress = ready ? static_cast<float>(player.Position() / player.Duration()) : 0.f;
    if (dragging) progress = seekValue;
    bool drag = false;
    float value = progress;
    TrackSlider(ui, ui.MakeId("seek"), seekRow.Inset(14.f, 2.f), value, 4.f, 14.f, Palette::Teal, ready, &drag);
    if (drag) {
        dragging = true;
        seekValue = value;
    } else if (dragging) {
        dragging = false;   // released: seek once (Thumb.DragCompleted)
        if (ready) player.Seek(seekValue * player.Duration());
    }

    // Controls
    if (roundBottomLeft || roundBottomRight)
        g.FillRound(controls, 0.f, 0.f, roundBottomRight ? 8.f : 0.f, roundBottomLeft ? 8.f : 0.f, Hex(0x040810));
    else
        g.FillRect(controls, Hex(0x040810));
    g.FillRect({ controls.x, controls.y, controls.w, 1.f }, Palette::Divider);
    if (controlsBottomBorder) g.FillRect({ controls.x, controls.B() - 1.f, controls.w, 1.f }, Palette::Divider);
    const Rect row = controls.Inset(14.f, 0.f);
    const ButtonStyle round{ Hex(0x0D2030), Palette::Teal, 1.f, 20.f, { 0.f }, 0.f, 40.f };
    if (TextButton(ui, ui.MakeId("play-pause"), { row.x, row.CY() - 20.f, 40.f, 40.f }, std::wstring(1, playing ? kPause : kPlay), IconFont(16.f),
                   Palette::Teal, round, ready))
        TogglePlay();
    const std::wstring time = ready ? FormatTime(dragging ? seekValue * player.Duration() : player.Position()) + L" / " + FormatTime(player.Duration())
                                    : L"0:00 / 0:00";
    g.Text(time, { row.x + 40.f + 12.f, row.y, 200.f, row.h }, Palette::Dim, Mono(10.f));
    if (ready) ui.KeepAnimating();   // position ticks

    // Volume: mute toggle + slider
    const Rect slider{ row.R() - 90.f, row.CY() - 6.f, 90.f, 12.f };
    const Rect icon{ slider.x - 8.f - 16.f, row.y, 16.f, row.h };
    const Interaction iconIt = ui.Interact(ui.MakeId("volume-icon"), icon);
    g.Text(std::wstring(1, volume <= 0.f ? kMute : kVolume), icon, Palette::Faint, IconFont(14.f));
    if (iconIt.clicked) {
        if (muted) {
            volume = volumeBeforeMute;
            muted = false;
        } else {
            volumeBeforeMute = volume;
            volume = 0.f;
            muted = true;
        }
        player.SetVolume(volume);
    }
    if (TrackSlider(ui, ui.MakeId("volume"), slider, volume, 3.f, 10.f, Hex(0x8AABBD), true)) player.SetVolume(volume);
}

} // namespace SpeedrunLauncher::Windows
