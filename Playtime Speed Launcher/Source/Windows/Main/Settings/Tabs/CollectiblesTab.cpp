// Settings → 100%: Chapters 3, 4 and 5 collectibles. The overlay over the game, the OBS link, what the
// overlay shows and each chapter's progress (read live from the game or from its save).
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include "App/Paths.h"
#include "Core/Async.h"
#include "Core/System.h"
#include "Core/Text.h"
#include "Graphics/Images.h"
#include "Input/KeyNames.h"
#include "Localization/Localization.h"
#include "Services/Collectibles/CollectiblesData.h"
#include "Services/Collectibles/CollectiblesMemory.h"
#include "Services/Collectibles/CollectiblesProgress.h"
#include "Services/Collectibles/CollectiblesServer.h"
#include "Services/Collectibles/InGameOverlay.h"
#include "Settings/CollectiblesSettings.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "Features/InputRemaps.h"
#include "Windows/Main/MainWindow.h"
#include "Windows/Main/Settings/SettingsKit.h"

namespace SpeedrunLauncher::Main::SettingsTabs {

using namespace UI;
namespace Collect = Services::Collectibles;

namespace {

constexpr Color kGold = Hex(0xFFD23F);

double copiedAt = -10.0;
int resetAsked = 0;            // the chapter whose RESET was clicked once, -1 for everything (asks to click again)...
double resetAskedAt = -10.0;   // ...and when

// "http://127.0.0.1:8745/overlay?..." -> "http://•••••••/overlay?..."
std::wstring MaskedLink(const std::wstring& link) {
    const size_t host = link.find(L"://");
    const size_t path = host == std::wstring::npos ? std::wstring::npos : link.find(L'/', host + 3);
    if (path == std::wstring::npos) return link;
    return link.substr(0, host + 3) + std::wstring(7, L'\x2022') + link.substr(path);
}

void Changed(Settings::Collectibles& s) {
    s.Save();
    Collect::InGameOverlay::Apply();
}

// Small grey caption above a group of options
void Caption(SettingsContext& c, const std::wstring& text) {
    auto reveal = c.Reveal();
    c.ui.g.Text(text, c.col.Next(16.f, 6.f), Palette::Hint, Mono(9.f, true));
}

void InGameSection(SettingsContext& c, Settings::Collectibles& s) {
    auto& ui = c.ui;
    c.Section(Loc::Get("collect_ingame_section"));
    if (c.ToggleRow(ui.MakeId("collect-ingame"), Loc::Get("collect_ingame_label"), s.inGame)) {
        s.inGame = !s.inGame;
        Changed(s);
    }
    c.Paragraph(Loc::Get("collect_ingame_hint"), 9.f, Palette::Hint, 6.f, 10.f);
    if (c.GlyphOption(ui.MakeId("collect-preview"), Glyphs::Play, Loc::Get("collect_preview"), 8.f)) Collect::InGameOverlay::Preview();

    // Height on screen: move mode lets the overlay be dragged up and down until DONE
    const bool moving = Collect::InGameOverlay::Moving();
    const int move = c.OptionGrid("collect-move", { Loc::Get(moving ? "collect_move_done" : "collect_move"), Loc::Get("collect_move_reset") },
                                  moving ? 0 : -1, 2);
    if (move == 0) {
        if (moving) Collect::InGameOverlay::EndMove();
        else Collect::InGameOverlay::StartMove();
    } else if (move == 1) {
        Collect::InGameOverlay::ResetPosition();
    }
    if (moving) c.Paragraph(Loc::Get("collect_move_hint"), 9.f, Palette::Warning, 6.f);
}

void ObsSection(SettingsContext& c, const Settings::Collectibles& s, int port) {
    auto& ui = c.ui;
    auto& g = ui.g;
    c.Section(Loc::Get("collect_obs_section"));
    c.Paragraph(Loc::Get("collect_obs_hint"), 9.f, Palette::Hint, 0.f, 10.f);
    if (!port) {
        c.Paragraph(Loc::Get("collect_server_error"), 10.f, Palette::Error, 0.f, 0.f, true);
        return;
    }
    const std::wstring link = Collect::Server::Link(s.Query());
    {
        auto reveal = c.Reveal();
        const Rect row = c.col.Next(32.f, 8.f);
        const bool copied = Async::Now() - copiedAt < 1.5;
        const std::wstring copyText = Loc::Get(copied ? "collect_copied" : "collect_copy");
        const TextStyle copyStyle = Mono(10.f, true);
        const ButtonStyle copyButton{ copied ? Palette::ConfirmBg : Palette::ButtonBg, copied ? Palette::ConfirmBorder : Palette::ControlBorder, 1.f, 4.f,
                                      { 12.f, 0.f }, 96.f, 32.f };
        const float bw = ButtonWidth(ui, copyText, copyStyle, copyButton);
        const Rect box{ row.x, row.y, row.w - bw - 8.f, row.h };
        g.Border(box, 4.f, Palette::OptionBg, Palette::ControlBorder, 1.f);
        // The address stays hidden (the settings are often on stream): COPY still copies the real link
        g.Text(MaskedLink(link), box.Inset(10.f, 0.f), Palette::Label, Mono(9.5f).Ellipsis());
        if (TextButton(ui, ui.MakeId("collect-copy"), { box.R() + 8.f, row.y, bw, row.h }, copyText, copyStyle, copied ? Palette::Teal : Palette::Label,
                       copyButton)) {
            System::SetClipboardText(link);
            copiedAt = Async::Now();
        }
        if (copied) ui.KeepAnimatingThrottled();
    }
    const int clicked = c.OptionGrid("collect-obs", { Loc::Get("collect_open_browser"), Loc::Get("collect_test") }, -1, 2);
    if (clicked == 0) ShellExecuteW(nullptr, L"open", link.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (clicked == 1) Collect::Server::FireTest();
}

void ShowSection(SettingsContext& c, Settings::Collectibles& s) {
    auto& ui = c.ui;
    c.Section(Loc::Get("collect_show_section"));
    Caption(c, Loc::Get("collect_chapter_label"));
    // Automatic, then one per chapter
    std::vector<std::wstring> chapterTexts{ Loc::Get("collect_chapter_auto") };
    int chapterIndex = 0;
    for (const int n : Collect::Data::kChapters) {
        if (n == s.chapter) chapterIndex = static_cast<int>(chapterTexts.size());
        chapterTexts.push_back(Loc::Get("collect_chapter_" + std::to_string(n)));
    }
    const int ch = c.OptionGrid("collect-chapter", chapterTexts, chapterIndex, static_cast<int>(chapterTexts.size()), 14.f);
    if (ch >= 0) {
        s.chapter = ch ? Collect::Data::kChapters[ch - 1] : 0;
        Changed(s);
    }

    struct Flag { const char* id; const char* key; bool Settings::Collectibles::*field; };
    static constexpr Flag kFlags[] = {
        { "collect-counters", "collect_counters", &Settings::Collectibles::counters },
        { "collect-list", "collect_list", &Settings::Collectibles::list },
        { "collect-missing", "collect_missing", &Settings::Collectibles::missingOnly },
        { "collect-toasts", "collect_toasts", &Settings::Collectibles::toasts },
        { "collect-anim", "collect_animations", &Settings::Collectibles::animations },
        { "collect-nobg", "collect_nobg", &Settings::Collectibles::noBackground },
    };
    for (const Flag& f : kFlags) {
        if (f.field == &Settings::Collectibles::missingOnly && !s.list) continue;   // only with the list
        if (c.ToggleRow(ui.MakeId(f.id), Loc::Get(f.key), s.*f.field)) {
            s.*f.field = !(s.*f.field);
            Changed(s);
        }
        c.Gap(6.f);
    }
    c.Gap(10.f);

    Caption(c, Loc::Get("collect_side_label"));
    if (const int side = c.OptionGrid("collect-side", { Loc::Get("collect_side_left"), Loc::Get("collect_side_right") }, s.right ? 1 : 0, 2, 14.f); side >= 0) {
        s.right = side == 1;
        Changed(s);
    }
    Caption(c, Loc::Get("collect_lang_label"));
    if (const int lang = c.OptionGrid("collect-lang", { L"Español", L"English" }, s.english ? 1 : 0, 2, 14.f); lang >= 0) {
        s.english = lang == 1;
        Changed(s);
    }
    Caption(c, Loc::Get("collect_size_label"));
    static constexpr double kScales[] = { 0.75, 1.0, 1.25, 1.5 };
    int scaleIndex = 1;
    for (int i = 0; i < 4; ++i)
        if (std::abs(kScales[i] - s.scale) < 0.01) scaleIndex = i;
    if (const int sc = c.OptionGrid("collect-scale", { L"75%", L"100%", L"125%", L"150%" }, scaleIndex, 4, s.list ? 14.f : 0.f); sc >= 0) {
        s.scale = kScales[sc];
        Changed(s);
    }
    if (s.list) {
        Caption(c, Loc::Get("collect_columns_label"));
        if (const int cols = c.OptionGrid("collect-cols", { L"1", L"2", L"3" }, s.listColumns - 1, 3); cols >= 0) {
            s.listColumns = cols + 1;
            Changed(s);
        }
    }
}

// Everything back to 0: the key (works with the game in front) and a button (click twice)
void ResetSection(SettingsContext& c, Settings::Collectibles& s) {
    auto& ui = c.ui;
    c.Section(Loc::Get("collect_reset_section"));
    c.Paragraph(Loc::Get("collect_reset_hint"), 9.f, Palette::Hint, 0.f, 10.f);
    const bool capturing = c.capture.Active(CaptureCollectReset);
    const std::wstring keyText = capturing ? Loc::Get("hotkey_press_keys")
                                 : s.resetVk ? ::SpeedrunLauncher::Input::KeyNames::Upper(s.resetVk)
                                             : Loc::Get("collect_reset_nokey");
    if (c.KeyRow(ui.MakeId("collect-reset-key"), Loc::Get("collect_reset_key_label"), keyText, capturing)) {
        auto* remaps = &c.main.Remaps();
        c.capture.Toggle(CaptureCollectReset, false, [remaps](const CapturedInput& in) {
            auto& settings = Settings::Collectibles::Current();
            settings.resetVk = in.escape ? 0 : in.vk;   // Escape: no key
            settings.Save();
            remaps->RefreshCollectiblesReset();
        });
    }
    c.Gap(10.f);
    const bool asking = resetAsked == -1 && Async::Now() - resetAskedAt < 3.0;
    if (asking) ui.KeepAnimatingThrottled();
    if (c.OptionGrid("collect-reset-all", { Loc::Get(asking ? "collect_reset_all_confirm" : "collect_reset_all") }, asking ? 0 : -1, 1) == 0) {
        if (asking) {
            resetAsked = 0;
            c.main.Remaps().ResetCollectibles();
        } else {
            resetAsked = -1;
            resetAskedAt = Async::Now();
        }
    }
}

std::wstring SourceText(const Collect::Progress::ChapterProgress& p) {
    using Collect::Progress::Source;
    switch (p.source) {
        case Source::Live: return Loc::Get("collect_source_live");
        case Source::Memory: return Loc::Get("collect_source_memory");
        default: return Loc::Get("collect_source_empty");
    }
}

void ProgressSection(SettingsContext& c, const Collect::Progress::Snapshot& snap, int shownChapter) {
    auto& ui = c.ui;
    auto& g = ui.g;
    c.Section(Loc::Get("collect_progress_section"));
    if (!snap.ready) {
        c.Paragraph(Loc::Get("collect_reading"), 10.f, Palette::Hint);
        return;
    }
    for (const int n : Collect::Data::kChapters) {
        const auto& p = snap.Of(n);
        const auto& data = Collect::Data::Get(n);
        auto reveal = c.Reveal();
        const Rect r = c.col.Next(66.f, 8.f);
        const bool active = n == shownChapter;
        const bool done = p.total > 0 && p.got == p.total;
        g.Border(r, 6.f, Palette::OptionBg, active ? Palette::Teal.WithAlpha(0.6f) : Palette::ButtonBorder, 1.f);
        const Rect art{ r.x + 10.f, r.y + 11.f, 94.f, 44.f };
        if (auto img = Images::Load(Paths::Asset(L"Collectibles\\Chapters\\Chapter" + std::to_wstring(n) + L".jpg")))
            g.DrawImageRounded(*img, art, 4.f);
        const float tx = art.R() + 12.f;
        const std::wstring count = std::to_wstring(p.got) + L"/" + std::to_wstring(p.total);
        const TextStyle countStyle = Mono(16.f, true).Right();
        const float cw = g.Measure(count, countStyle).w;
        const Rect countArea{ r.R() - 12.f - cw, r.y + 10.f, cw, 22.f };
        g.Text(count, countArea, done ? kGold : Palette::Bright, countStyle);
        // RESET (forgets the chapter's run): a second click within a few seconds confirms
        float titleRight = countArea.x - 8.f;
        if (p.got > 0) {
            const bool asking = resetAsked == n && Async::Now() - resetAskedAt < 3.0;
            const std::wstring resetText = Loc::Get(asking ? "collect_reset_confirm" : "collect_reset");
            const TextStyle resetStyle = Mono(8.5f, true);
            const ButtonStyle resetButton{ asking ? Palette::Warning.WithAlpha(0.15f) : Palette::ButtonBg, asking ? Palette::Warning : Palette::ControlBorder,
                                           1.f, 4.f, { 8.f, 0.f }, 0.f, 20.f };
            const float bw = ButtonWidth(ui, resetText, resetStyle, resetButton);
            const Rect resetRect{ countArea.x - 10.f - bw, r.y + 10.f, bw, 20.f };
            if (TextButton(ui, ui.MakeId("collect-reset", n), resetRect, resetText, resetStyle, asking ? Palette::Warning : Palette::Hint, resetButton)) {
                if (asking) {
                    resetAsked = 0;
                    Collect::Memory::Reset(n);
                    Collect::Progress::Refresh();
                } else {
                    resetAsked = n;
                    resetAskedAt = Async::Now();
                }
            }
            if (asking) ui.KeepAnimatingThrottled();
            titleRight = resetRect.x - 8.f;
        }
        const Rect titleArea{ tx, r.y + 10.f, titleRight - tx, 18.f };
        std::wstring title = Text::Fmt(Loc::Get("collect_chapter_title"), std::to_wstring(n), data.title);
        if (p.running) title += L"  ·  " + Loc::Get("collect_running");
        g.Text(title, titleArea, active ? Palette::Teal : Palette::Label, Mono(10.5f, true).Ellipsis());
        const bool live = p.source == Collect::Progress::Source::Live;
        float sourceRight = r.R() - 12.f;
        if (p.playedTotal > 0) {
            const std::wstring played = L"\u25B6 " + Loc::Get("collect_played") + L" " + std::to_wstring(p.played) + L"/" + std::to_wstring(p.playedTotal);
            const TextStyle playedStyle = Mono(9.f, true).Right();
            const float pw = g.Measure(played, playedStyle).w;
            const bool allPlayed = p.played == p.playedTotal;
            g.Text(played, { sourceRight - pw - 2.f, r.y + 29.f, pw + 2.f, 14.f }, allPlayed ? kGold : Hex(0xFF5A76).WithAlpha(p.played ? 1.f : 0.55f), playedStyle);
            sourceRight -= pw + 14.f;
        }
        g.Text(SourceText(p), { tx, r.y + 29.f, sourceRight - tx, 14.f }, live ? Hex(0xFF5A6E) : Palette::Hint, Mono(9.f).Ellipsis());
        ProgressBar(ui, { tx, r.y + 49.f, r.R() - 12.f - tx, 5.f }, p.total ? static_cast<float>(p.got) / p.total : 0.f, done ? kGold : Palette::Teal);
    }
}

} // namespace

void Collectibles(SettingsContext& c) {
    auto& s = Settings::Collectibles::Current();
    // The OBS link needs the server; from now on it also starts with the launcher
    const int port = Collect::Server::Ensure();
    Collect::Progress::Poll();
    const auto& snap = Collect::Progress::Latest();
    const int shownChapter = s.chapter ? s.chapter : snap.autoChapter;

    c.Paragraph(Loc::Get("collect_intro"), 10.f, Palette::Label, 0.f, 8.f);
    // With higher graphics the game takes the whole GPU and everything else (overlay, OBS, browser) stutters
    c.Paragraph(Loc::Get("collect_graphics_warning"), 9.5f, Palette::Warning, 0.f, 16.f, true);
    InGameSection(c, s);
    c.Rule();
    ObsSection(c, s, port);
    c.Rule();
    ShowSection(c, s);
    c.Rule();
    ResetSection(c, s);
    c.Rule();
    ProgressSection(c, snap, shownChapter);
}

} // namespace SpeedrunLauncher::Main::SettingsTabs
