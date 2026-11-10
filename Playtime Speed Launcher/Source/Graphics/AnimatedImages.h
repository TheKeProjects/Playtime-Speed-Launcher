#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "Graphics/Images.h"

// Pictures that may move: animated GIFs come back with all their frames (composited like a
// browser does), anything else as a single frame. Profile pictures and banners use these.
namespace SpeedrunLauncher::AnimatedImages {

struct Animation {
    std::vector<ImagePtr> frames;
    std::vector<double> ends;     // when each frame ends (seconds, cumulative)
    double duration = 0.0;
    float w = 0.f, h = 0.f;

    bool Animated() const { return frames.size() > 1; }
    // The frame showing at `time` (loops)
    ImagePtr At(double time) const;
};
using AnimationPtr = std::shared_ptr<const Animation>;

// `source` is a local file or an http(s) URL (downloaded once and kept on disk). The first call
// starts loading on a worker; nullptr until it's ready, or when it can't be read.
// `maxSide` shrinks big pictures (the frames are kept in video memory).
AnimationPtr Get(const std::wstring& source, unsigned maxSide);
// Every frame of a picture file, decoded (worker threads): an animated GIF composited frame by
// frame, anything else as one frame. Premultiplied BGRA, at most `maxSide`; long GIFs keep every
// n-th frame (their delays added up) to stay within `memoryBudget` bytes.
struct Frames {
    unsigned w = 0, h = 0;
    std::vector<std::vector<uint32_t>> frames;
    std::vector<double> delays;   // seconds
};
bool DecodeFile(const std::wstring& path, unsigned maxSide, Frames& out, size_t memoryBudget = 72u * 1024u * 1024u);

// A picture just uploaded: copy the local file into the URL's disk cache so it isn't downloaded back
void Seed(const std::wstring& url, const std::wstring& localFile);

} // namespace SpeedrunLauncher::AnimatedImages
