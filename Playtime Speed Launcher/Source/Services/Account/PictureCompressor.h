#pragma once
#include <string>

// Shrinks a profile picture / banner before it's uploaded, so big GIFs can be picked: animated GIFs
// are scaled down and re-encoded (and, if still too heavy, keep every other frame), still pictures
// become a smaller JPEG (or PNG when they have transparency). Small files go up untouched.
// Blocking: worker threads only.
namespace SpeedrunLauncher::Services::Account::PictureCompressor {

struct Result {
    bool ok = false;
    std::wstring path;        // the file to upload (`source` itself, or a temp file)
    bool temporary = false;   // delete `path` once uploaded
    std::string errorKey;     // "acc_err_picture" / "acc_err_too_big"
};

Result Prepare(const std::wstring& source, bool banner);

} // namespace SpeedrunLauncher::Services::Account::PictureCompressor
