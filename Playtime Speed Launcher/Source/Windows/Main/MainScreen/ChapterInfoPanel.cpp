#include "Windows/Main/MainScreen/ChapterInfoPanel.h"
#include <algorithm>
#include "App/Launcher.h"
#include "Core/Files.h"
#include "Core/Process.h"
#include "Graphics/Animation.h"
#include "Localization/Localization.h"
#include "Seasonal/SeasonalTheme.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "Windows/Main/MainWindow.h"

namespace SpeedrunLauncher::Main {

namespace {

constexpr float kTitleSize = 24.f;
constexpr float kDescriptionMax = 900.f;

TextStyle TitleStyle() { return UI::Mono(kTitleSize, true); }
TextStyle DescriptionStyle() {
    TextStyle s = UI::Mono(14.f);
    s.wrap = true;
    s.align = HAlign::Center;
    s.valign = VAlign::Top;
    return s;
}

} // namespace

void ChapterInfoPanel::Refresh() {
    auto& l = App::L();
    const auto& ch = l.Selected();
    const std::string n = std::to_string(ch.number);
    title = Loc::Get("ch" + n + "_title");
    description = Loc::Get("ch" + n + "_desc");

    const std::wstring prefix = Loc::Get("version_prefix") + L" ";
    if (auto selected = l.store.SelectedPath(ch.number)) {
        std::wstring name = Files::FileNameWithoutExtension(*selected);
        for (const auto& custom : l.store.Customs(ch.number))
            if (custom.exePath == *selected) name = custom.name;
        version = prefix + name;
    } else if (l.epic.IsEnabled() && l.epic.ExePath(ch.number)) {
        version = prefix + Loc::Get("version_auto_epic");
    } else if (l.epic.IsEnabled()) {
        version = prefix + (ch.isAvailable ? Loc::Get("version_not_installed") : Loc::Get("version_none"));
    } else {
        version = prefix + (ch.IsInstalled() ? Loc::Get("version_auto_steam")
                                             : ch.isAvailable ? Loc::Get("version_not_installed") : Loc::Get("version_none"));
    }

    auto exe = l.ActiveExePath(ch);
    canPlay = exe && !exe->empty() && Files::Exists(*exe);
    running = exe && !exe->empty() && !Process::FindByName(Files::FileNameWithoutExtension(*exe)).empty();
    status = running ? Loc::Get("status_playing")
             : canPlay ? Loc::Get("status_installed")
             : ch.isAvailable ? Loc::Get("status_not_found") : Loc::Get("status_coming_soon");
}

float ChapterInfoPanel::Height(UI::Ui& ui, float width) {
    const float titleRow = std::max(ui.g.Measure(L"Ag", TitleStyle()).h, 30.f);
    const float desc = std::max(40.f, ui.g.Measure(description, DescriptionStyle(), std::min(kDescriptionMax, width - 48.f)).h);
    return 10.f + titleRow + 6.f + desc + 8.f;   // StackPanel Margin="0,10,0,8", description Margin="0,6,0,0"
}

void ChapterInfoPanel::Draw(UI::Ui& ui, const Rect& area) {
    auto& g = ui.g;
    const auto& palette = Seasonal::Current();
    const bool seasonal = Seasonal::IsSeasonal();

    // Chapter change: the new info fades in while rising 10 px
    if (title != shownTitle) {
        if (!shownTitle.empty()) changedAt = ui.Time();
        shownTitle = title;
    }
    const float enter = Anim::EaseOutCubic(static_cast<float>((ui.Time() - changedAt) / 0.32));
    const bool entering = enter < 1.f;
    if (entering) {
        g.PushOpacity(enter);
        ui.PushTranslate(0.f, (1.f - enter) * 10.f);
        ui.KeepAnimating();
    }
    DrawContent(ui, area, palette, seasonal);
    if (entering) {
        ui.PopTransform();
        g.PopLayer();
    }
}

void ChapterInfoPanel::DrawContent(UI::Ui& ui, const Rect& area, const Seasonal::Palette& palette, bool seasonal) {
    auto& g = ui.g;
    auto& l = App::L();

    // ── Row: title · status · version button · platform toggle (centred as a group) ──
    const TextStyle titleStyle = TitleStyle();
    const TextStyle statusStyle = UI::Mono(12.f);
    const TextStyle versionStyle = UI::Mono(13.f);
    const TextStyle platformStyle = UI::Mono(11.f, true);
    const bool epic = l.epic.IsEnabled();
    const std::wstring platformText = epic ? L"EPIC" : L"STEAM";

    const float titleW = g.Measure(title, titleStyle).w;
    const float statusW = g.Measure(status, statusStyle).w;
    const float versionW = g.Measure(version, versionStyle).w + 6.f + 10.f + 20.f + 2.f;   // text + chevron + Padding 10,4 + border
    const float platformW = g.Measure(platformText, platformStyle).w + 20.f + 2.f;
    const float rowH = std::max(g.Measure(L"Ag", titleStyle).h, 30.f);
    // Chapters that aren't out yet (chapter 6) have no versions to pick: no button
    const bool canPickVersion = l.Selected().isAvailable;
    const float total = titleW + 14.f + statusW + 14.f + (canPickVersion ? versionW + 8.f : 0.f) + platformW;
    float x = area.CX() - total * 0.5f;
    const float y = area.y + 10.f;
    const float cy = y + rowH * 0.5f;

    // Title (glows in the season's colour)
    const Rect titleRect{ x, y, titleW + 2.f, rowH };
    if (seasonal) g.GlowText(title, titleRect, titleStyle, palette.title, 22.f, 0.75f);   // DropShadowEffect BlurRadius 22
    g.Text(title, titleRect, palette.title, titleStyle);
    x += titleW + 14.f;
    g.Text(status, { x, y, statusW + 2.f, rowH }, palette.muted, statusStyle);
    x += statusW + 14.f;

    if (canPickVersion) {
        const float h = g.Measure(L"Ag", versionStyle).h + 8.f + 2.f;
        UI::ButtonStyle style{ Colors::Transparent, Hex(0x1A3A55), 1.f, 3.f, { 10.f, 4.f }, 0.f, h };
        UI::ButtonScope b(ui, ui.MakeId("versionBtn"), { x, cy - h * 0.5f, versionW, h }, style);
        const float tw = g.Measure(version, versionStyle).w;
        g.Text(version, { b.content.x, b.content.y, tw + 2.f, b.content.h }, palette.accent, versionStyle);
        UI::Glyph(ui, UI::Glyphs::ChevronDown, { b.content.x + tw + 6.f, b.content.y, 10.f, b.content.h }, 10.f, Hex(0x1A4060));
        if (b.Clicked()) main.OpenVersions();
        x += versionW + 8.f;
    }
    {
        const float h = g.Measure(L"Ag", platformStyle).h + 8.f + 2.f;
        UI::ButtonStyle style{ Colors::Transparent, epic ? Rgba(150, 60, 0, 180) : Rgba(26, 58, 85), 1.f, 3.f, { 10.f, 4.f }, 0.f, h };
        if (UI::TextButton(ui, ui.MakeId("platformBtn"), { x, cy - h * 0.5f, platformW, h }, platformText, platformStyle,
                           epic ? Rgba(232, 120, 0) : Rgba(42, 90, 122), style))
            main.TogglePlatform();
    }

    // ── Description ──
    const float descW = std::min(kDescriptionMax, area.w - 48.f);
    const float descY = y + rowH + 6.f;
    g.Text(description, { area.CX() - descW * 0.5f, descY, descW, area.B() - descY }, palette.muted, DescriptionStyle());
}

} // namespace SpeedrunLauncher::Main
