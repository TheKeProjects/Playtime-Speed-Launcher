#pragma once
#include <string>
#include "Models/Chapter.h"
#include "UI/Ui.h"
#include "UI/Widgets/TextBox.h"

// The look shared by the STORE and the VERSIONS menu: cover tiles that lift / zoom / glow, badges,
// section titles, big glowing buttons and the form bits of the store's product page.
namespace SpeedrunLauncher::Main::StoreKit {

inline constexpr float kPad = 24.f, kGap = 18.f, kColumns = 5.f, kCoverAspect = 1.2f, kRadius = 10.f;
inline constexpr Color kTitle = Rgba(225, 232, 240), kDimText = Rgba(110, 140, 165);
inline constexpr Color kAmber = Rgba(255, 176, 32);

std::wstring ChapterTitle(const Models::Chapter& ch);         // in the launcher's language
std::wstring VersionsCount(size_t n);                          // "1 version" / "4 versions"
bool Listed(const Models::Chapter& ch);                        // shown on the store's front page
// Store art (Assets\Store\...), each falling back to the next when its file isn't there:
//   CoverPath         Chapters\Chapter<N>.jpg → the chapter card art
//   BannerPath        Banners\Chapter <N>.png / .jpg → the cover
//   VersionCoverPath  Versions\Chapter <N>\<version name>.png / .jpg → the cover
//                     (name as shown, or with "Parche" / "Patch", minus characters files can't have)
std::wstring CoverPath(int chapterNumber);
std::wstring BannerPath(int chapterNumber);
std::wstring VersionCoverPath(int chapterNumber, const std::wstring& versionName);
// Image filling r (cropped, never stretched), zoomed around its centre by `zoom`. Banners pass a
// bigger `maxSide` so the wide art stays sharp.
void DrawCoverImage(UI::Ui& ui, const std::wstring& path, const Rect& r, float zoom, unsigned maxSide = 768);

struct Tile {
    UI::Interaction it;
    Rect cover;      // where the cover ended up (lifted while hovered)
    float hover = 0.f;
};
// Cover tile: lifts, zooms its art and lights its border while hovered
// `lit` (0..1) keeps it glowing and outlined (e.g. the selected one); the glow goes under the art.
// `keepHover`: the mouse is on one of the tile's own buttons (they take the hover from the tile)
Tile CoverTile(UI::Ui& ui, UI::Id id, const Rect& slot, const std::wstring& image, bool enabled = true, float lit = 0.f, bool keepHover = false);
void Badge(UI::Ui& ui, const Rect& cover, const std::wstring& text, Color background, Color foreground);   // top-left label
float SectionTitle(UI::Ui& ui, float x, float y, float w, const std::wstring& title, const std::wstring& hint);   // returns its height
// Wide primary action (glyph + label), glowing while hovered
bool BigButton(UI::Ui& ui, UI::Id id, const Rect& b, wchar_t glyph, const std::wstring& label, bool enabled = true);

UI::TextBoxStyle FieldStyle(bool password = false, const std::wstring& placeholder = L"");
float CheckRow(UI::Ui& ui, UI::Id id, float x, float y, float w, const std::wstring& text, bool& value);   // returns its height
float CodeBox(UI::Ui& ui, float x, float y, float w, const std::wstring& text, Color accent, float size = 10.f);   // returns its height

} // namespace SpeedrunLauncher::Main::StoreKit
