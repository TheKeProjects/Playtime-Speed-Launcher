#include "Windows/Standalone/ReportSkipWindow.h"
#include <algorithm>
#include <functional>
#include "Core/Async.h"
#include "Core/Text.h"
#include "Graphics/Animation.h"
#include "Seasonal/SeasonalCardArt.h"
#include "Seasonal/SeasonalTheme.h"
#include "Services/Community/SkipReportService.h"
#include "Services/Community/VideoTutorialService.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "UI/Widgets/Panel.h"
#include "UI/Widgets/ScrollArea.h"
#include "UI/Widgets/Stagger.h"
#include "UI/Widgets/TextBox.h"
#include "Windows/Dialogs/DialogParts.h"
#include "Windows/Popups/TutorialWindows.h"
#include "Windows/Standalone/Common/Pills.h"
#include "Localization/Localization.h"

namespace SpeedrunLauncher::Windows {

using namespace UI;
namespace VT = Services::VideoTutorials;

namespace {

bool Has(const std::vector<std::wstring>& list, const std::wstring& v) {
    return std::any_of(list.begin(), list.end(), [&](const std::wstring& x) { return _wcsicmp(x.c_str(), v.c_str()) == 0; });
}

void Toggle(std::vector<std::wstring>& list, const std::wstring& v) {
    auto it = std::find_if(list.begin(), list.end(), [&](const std::wstring& x) { return _wcsicmp(x.c_str(), v.c_str()) == 0; });
    if (it != list.end()) list.erase(it);
    else list.push_back(v);
}

std::vector<std::wstring> Distinct(std::vector<std::wstring> values, bool versionOrder = false) {
    std::vector<std::wstring> out;
    for (auto& v : values)
        if (!v.empty() && !Has(out, v)) out.push_back(v);
    std::sort(out.begin(), out.end(), [versionOrder](const std::wstring& a, const std::wstring& b) {
        const std::wstring ka = versionOrder && a[0] == L'<' ? L"0" + a : a;
        const std::wstring kb = versionOrder && b[0] == L'<' ? L"0" + b : b;
        return ka < kb;
    });
    return out;
}

std::vector<std::wstring> SplitList(const std::wstring& s) {
    std::vector<std::wstring> out;
    for (const auto& part : Text::Split(s, L',')) out.push_back(Text::Trim(part));
    return out;
}

TextBoxStyle FieldStyle(bool multiline = false, size_t maxLength = 0) {
    TextBoxStyle s;
    s.text = Mono(11.f);
    s.foreground = Hex(0xC8D8E4);
    s.background = Hex(0x060D18);
    s.border = Hex(0x123049);
    s.focusBorder = Hex(0x123049);
    s.caret = Palette::Teal;
    s.selection = Rgba(0, 204, 170, 100);
    s.padding = { 10.f, 8.f };
    s.radius = 6.f;
    s.multiline = multiline;
    s.maxLength = maxLength;
    return s;
}

constexpr float kCardPad = 14.f;

} // namespace

bool ReportSkipWindow::Open(HWND owner) {
    ownerWindow = owner;
    UI::Window::Options o;
    o.title = Loc::Get("rs_window_title");
    o.style = WS_POPUP;
    o.owner = owner;
    o.transparent = true;
    o.topmost = true;
    o.width = 460;
    o.height = 740;
    o.showCommand = SW_HIDE;
    if (!Create(o)) return false;
    g.SetColorFilter(&Seasonal::Filter);   // follows the seasonal theme like the main window
    // Seasonal particles: idle frames at 30 fps (input / tweens still at full rate)
    if (Seasonal::IsSeasonal()) {
        SetContinuous(true, true);
        SetIdleFps(30.0);
    }
    CenterOnOwner(*this, owner, 460.f, 740.f);
    if (owner) EnableWindow(owner, FALSE);   // ShowDialog
    openedAt = Async::Now();
    Show(SW_SHOW);
    Activate();
    return true;
}

void ReportSkipWindow::OnClosing(bool& cancel) {
    cancel = false;
    if (ownerWindow) {
        EnableWindow(ownerWindow, TRUE);
        SetForegroundWindow(ownerWindow);
    }
}

void ReportSkipWindow::BeginClose() {
    if (closingAt < 0.0) closingAt = ui.Time();
}

void ReportSkipWindow::Send() {
    const std::wstring skip = Text::Trim(name);
    if (skip.empty()) {
        status = Loc::Get("rs_err_name");
        statusKind = Form::Status::Warning;
        return;
    }
    if (!Services::SkipReport::IsConfigured()) {
        status = Loc::Get("rs_err_config");
        statusKind = Form::Status::Warning;
        return;
    }
    sending = true;
    status.clear();
    Services::SkipReport::Report r;
    r.name = skip;
    r.chapter = chapter.value_or(L"");
    r.version = Text::Join(versions, L", ");
    r.videoUrl = Text::Trim(videoUrl);
    r.author = Text::Trim(author);
    r.description = Text::Trim(description);
    r.runCategories = runCategories;
    r.routes = routes;
    r.restrictions = restrictions;
    r.cores = cores;
    auto guard = alive;
    Async::Run([r] { return Services::SkipReport::Send(r); }, [this, guard](std::wstring error) {
        if (!*guard) return;
        sending = false;
        if (error.empty()) {
            sent = true;
            status = Loc::Get("rs_sent_message");
            statusKind = Form::Status::Success;
            Async::After(1800, [this, guard] {
                if (*guard) BeginClose();
            });
        } else {
            status = error;
            statusKind = Form::Status::Error;
        }
        Invalidate();
    });
}

void ReportSkipWindow::RefreshOptions() {
    if (options.built && options.chapter == chapter && options.customCount == customRunCategories.size()) return;
    const auto& videos = VT::All();
    std::vector<std::wstring> chapters, versionList, runCats, routeList, restrictionList;
    for (const auto& v : videos) {
        chapters.push_back(v.chapter);
        runCats.insert(runCats.end(), v.runCategories.begin(), v.runCategories.end());
        for (const auto& s : SplitList(v.restrictions)) restrictionList.push_back(s);
        if (chapter && v.chapter != *chapter) continue;   // versions and routes follow the chapter
        for (const auto& s : SplitList(v.version)) versionList.push_back(s);
        routeList.insert(routeList.end(), v.routes.begin(), v.routes.end());
    }
    runCats.insert(runCats.end(), customRunCategories.begin(), customRunCategories.end());
    options.chapters = Distinct(chapters);
    options.versions = Distinct(versionList, true);
    options.runCategories = Distinct(runCats);
    options.routes = Distinct(routeList);
    options.restrictions = Distinct(restrictionList);
    options.chapter = chapter;
    options.customCount = customRunCategories.size();
    options.built = true;
}

void ReportSkipWindow::SectionCard(Column& col, int index, wchar_t glyph, const std::wstring& title, const std::function<void(Column&)>& draw) {
    // The card's height comes from the previous frame (the rows decide it), so draw the
    // background first and measure while drawing the rows
    const Id id = ui.MakeId("section", index);
    float& measured = ui.Slot(id, 0.f);
    StaggerItem in(ui, openedAt + 0.1, index, 36.f, 0.f, 0.08, 0.42);
    const Rect card{ col.x, col.y, col.w, measured };
    if (measured > 0.f) g.Border(card, 10.f, Rgba(9, 20, 30, 230), Hex(0x123049), 1.f);
    Column inner(col.x + kCardPad, col.y + kCardPad, col.w - kCardPad * 2.f);
    // Caption: glyph in a small teal square + title
    {
        const Rect row = inner.Next(22.f, 12.f);
        g.Border({ row.x, row.y, 22.f, 22.f }, 6.f, Rgba(0, 204, 170, 30), Rgba(0, 204, 170, 90), 1.f);
        Glyph(ui, glyph, { row.x, row.y, 22.f, 22.f }, 11.f, Palette::Teal);
        g.Text(title, { row.x + 32.f, row.y, row.w - 32.f, 22.f }, Rgba(214, 228, 238), Mono(11.f, true));
    }
    draw(inner);
    const float height = inner.y - col.y + kCardPad - 4.f;
    if (std::abs(height - measured) > 0.5f) {
        measured = height;
        ui.KeepAnimating();
    }
    col.Gap(height + 14.f);
}

void ReportSkipWindow::OnFrame() {
    const Rect all{ 0.f, 0.f, g.Width(), g.Height() };
    const double now = ui.Time();

    // Closing: fade + shrink, then really close
    float closeT = 0.f;
    if (closingAt >= 0.0) {
        closeT = Anim::Clamp01(static_cast<float>((now - closingAt) / 0.18));
        if (closeT >= 1.f) {
            Close();
            return;
        }
        ui.KeepAnimating();
    }
    if (closingAt < 0.0 && ui.KeyPressed(VK_ESCAPE) && !addingCustom) BeginClose();

    // Pop-in: fade + scale 0.92 → 1 with a slight overshoot
    const float openT = Anim::Clamp01(static_cast<float>((now - openedAt) / 0.34));
    const float fade = Anim::EaseOutCubic(std::min(1.f, openT * 1.4f)) * (1.f - Anim::EaseInCubic(closeT));
    const float scale = Anim::Lerp(0.92f, 1.f, Anim::EaseOutBack(openT, 0.7f)) * Anim::Lerp(1.f, 0.96f, Anim::EaseInCubic(closeT));
    if (openT < 1.f) ui.KeepAnimating();
    const bool layered = fade < 0.999f;
    if (layered) g.PushOpacity(fade);
    const bool scaled = scale != 1.f;
    if (scaled) ui.PushScale(scale, all.CX(), all.CY());

    const Rect panel = all.Inset(8.f);   // room for the glow inside the transparent window
    g.GlowRound(panel, 12.f, Palette::Teal, 16.f, 0.25f);
    g.Border(panel, 12.f, Argb(0xF407101A), Hex(0x153048), 1.f);

    // Header: icon · title / subtitle ............ ✕
    const Rect header{ panel.x + 1.f, panel.y + 1.f, panel.w - 2.f, 64.f };
    g.FillRound(header, 11.f, 11.f, 0.f, 0.f, Hex(0x05090F));
    const Rect row = header.Inset(16.f, 0.f);
    g.FillEllipse(row.x + 18.f, row.CY(), 19.f, 19.f, Rgba(0, 204, 170, 26));
    g.Border({ row.x + 2.f, row.CY() - 16.f, 32.f, 32.f }, 16.f, Rgba(0, 204, 170, 36), Rgba(0, 204, 170, 120), 1.f);
    Glyph(ui, 0xEA80, { row.x + 2.f, row.CY() - 16.f, 32.f, 32.f }, 14.f, Palette::Teal);   // light bulb
    g.Text(Loc::Get("rs_title"), { row.x + 46.f, row.y + 13.f, row.w - 90.f, 20.f }, Palette::Teal, Mono(14.f, true));
    g.Text(Loc::Get("rs_subtitle"), { row.x + 46.f, row.y + 33.f, row.w - 90.f, 16.f }, Rgba(90, 130, 160), Mono(10.f));
    if (TextButton(ui, ui.MakeId("close"), { row.R() - 30.f, row.CY() - 15.f, 30.f, 30.f }, L"\x2715", Mono(12.f, true).WithFont(Font::Ui).Center(),
                   Palette::Dim, { Palette::ButtonBg, Palette::ButtonBorder, 1.f, 6.f, { 0.f }, 0.f, 30.f }))
        BeginClose();

    // Bottom: status banner + send button
    const Rect content{ panel.x + 18.f, header.B() + 16.f, panel.w - 36.f, panel.B() - header.B() - 16.f - 18.f };
    const float statusH = Form::StatusBannerHeight(ui, status, content.w);
    const Rect send{ content.x, content.B() - 42.f, content.w, 42.f };
    if (statusH > 0.f) Form::StatusBannerAt(ui, ui.MakeId("status"), { content.x, send.y - 12.f - statusH, content.w, statusH }, status, statusKind);
    else Form::StatusBannerAt(ui, ui.MakeId("status"), {}, status, statusKind);
    const Form::SendState state = sent ? Form::SendState::Sent : sending ? Form::SendState::Sending : Form::SendState::Idle;
    if (Form::SendButton(ui, ui.MakeId("send"), send, sent ? Loc::Get("rs_sent") : sending ? Loc::Get("rs_sending") : Loc::Get("rs_send"), Palette::Teal, Rgba(3, 17, 13), state))
        Send();
    const Rect form{ content.x, content.y, content.w, send.y - 14.f - (statusH > 0.f ? statusH + 12.f : 0.f) - content.y };

    // Form
    {
        ScrollArea scroll(ui, ui.MakeId("form"), form);
        Column col(form.x, form.y, scroll.ContentWidth() - 10.f);
        RefreshOptions();

        // ── Basics: name, chapter, version ──
        SectionCard(col, 0, 0xE8EC, Loc::Get("rs_section_skip"), [&](Column& c) {
            Form::Label(ui, c, 0xE70F, Loc::Get("rs_skip_name"), Palette::Teal, true);   // edit
            Form::Field(ui, ui.MakeId("name"), c.Next(TextBoxHeight(ui, FieldStyle()), 18.f), name, FieldStyle(false, 80), Palette::Teal);

            Form::Label(ui, c, 0xE8F1, Loc::Get("rs_chapter"), Palette::Teal);   // library
            const auto& chapters = options.chapters;
            const int clicked = Pills::Wrap(ui, c, "chapter", chapters, [&](size_t i) { return chapter && *chapter == chapters[i]; });
            if (clicked >= 0) {
                const std::wstring picked = chapters[static_cast<size_t>(clicked)];
                if (chapter && *chapter == picked) chapter.reset();
                else chapter = picked;
                versions.clear();
                routes.clear();
            }
            c.Gap(10.f);
            RefreshOptions();   // the chapter may have just changed

            Form::Label(ui, c, 0xE81C, Loc::Get("rs_version"), Palette::Teal);   // history
            const auto& versionOptions = options.versions;
            const int version = Pills::Wrap(ui, c, "version", versionOptions, [&](size_t i) { return Has(versions, versionOptions[i]); });
            if (version >= 0) Toggle(versions, versionOptions[static_cast<size_t>(version)]);
        });

        // ── Categories: run category (+ custom), route, restrictions, cores ──
        SectionCard(col, 1, 0xE71D, Loc::Get("rs_section_categories"), [&](Column& c) {
            Form::Label(ui, c, 0xE7C1, Loc::Get("rs_run_category"), Palette::Teal);   // flag
            const auto runOptions = options.runCategories;   // copy: adding a custom category rebuilds the cache
            Rect trailing{};
            const int clicked = Pills::Wrap(ui, c, "runcat", runOptions, [&](size_t i) { return Has(runCategories, runOptions[i]); },
                                            addingCustom ? 90.f : 24.f, &trailing);
            if (clicked >= 0) Toggle(runCategories, runOptions[static_cast<size_t>(clicked)]);
            if (addingCustom) {
                TextBoxStyle s;
                s.text = Mono(9.f);
                s.foreground = Hex(0xC8D8E4);
                s.background = Rgba(255, 255, 255, 30);
                s.border = Rgba(0, 204, 170, 90);
                s.focusBorder = Rgba(0, 204, 170, 90);
                s.caret = Palette::Teal;
                s.padding = { 5.f, 3.f };
                s.radius = trailing.h * 0.5f;
                s.maxLength = 30;
                const Id id = ui.MakeId("custom-runcat");
                if (!customHadFocus) ui.SetFocus(id);
                bool enter = false;
                TextBox(ui, id, trailing, customText, s, &enter);
                const bool escape = ui.Focus() == id && ui.KeyPressed(VK_ESCAPE);
                const bool lostFocus = customHadFocus && ui.Focus() != id;
                customHadFocus = ui.Focus() == id;
                if (escape) {
                    addingCustom = false;
                } else if (enter || lostFocus) {
                    const std::wstring value = Text::Trim(customText);
                    addingCustom = false;
                    if (!value.empty()) {
                        customRunCategories.push_back(value);
                        if (!Has(runCategories, value)) runCategories.push_back(value);
                    }
                }
            } else {
                const Interaction it = ui.Interact(ui.MakeId("add-runcat"), trailing);
                const float hover = ui.Animate(ui.MakeId("add-runcat-hover"), it.hovered ? 1.f : 0.f, 18.f);
                if (it.hovered) ui.SetCursor(IDC_HAND);
                ui.Tooltip(ui.MakeId("add-runcat"), Loc::Get("rs_add_run_category"));
                g.Border(trailing, 12.f, Lerp(Rgba(255, 255, 255, 20), Rgba(0, 204, 170, 40), hover), Rgba(0, 204, 170, static_cast<int>(90 + 80 * hover)), 1.f);
                g.Text(L"+", trailing, Rgba(0, 204, 170, 200), Mono(11.f, true).Center());
                if (it.clicked) {
                    addingCustom = true;
                    customHadFocus = false;
                    customText.clear();
                }
            }
            c.Gap(10.f);

            Form::Label(ui, c, 0xE81E, Loc::Get("rs_route"), Palette::Teal);   // map pin
            const auto& routeOptions = options.routes;
            const int route = Pills::Wrap(ui, c, "route", routeOptions, [&](size_t i) { return Has(routes, routeOptions[i]); });
            if (route >= 0) Toggle(routes, routeOptions[static_cast<size_t>(route)]);
            c.Gap(10.f);

            Form::Label(ui, c, 0xE72E, Loc::Get("rs_restrictions"), Palette::Teal);   // lock
            const auto& restrictionOptions = options.restrictions;
            const int restriction = Pills::Wrap(ui, c, "restrictions", restrictionOptions, [&](size_t i) { return Has(restrictions, restrictionOptions[i]); });
            if (restriction >= 0) Toggle(restrictions, restrictionOptions[static_cast<size_t>(restriction)]);
            c.Gap(8.f);
            Dialogs::Parts::CheckBox(ui, ui.MakeId("cores"), c, Loc::Get("rs_cores"), cores, 10.f, Hex(0x8AABBD));
        });

        // ── Details: video, author, description ──
        SectionCard(col, 2, 0xE8A5, Loc::Get("rs_section_details"), [&](Column& c) {
            Form::Label(ui, c, 0xE714, Loc::Get("rs_video_url"), Palette::Teal);   // video
            Form::Field(ui, ui.MakeId("url"), c.Next(TextBoxHeight(ui, FieldStyle()), 18.f), videoUrl, FieldStyle(false, 200), Palette::Teal);
            Form::Label(ui, c, 0xE77B, Loc::Get("rs_author"), Palette::Teal);   // person
            Form::Field(ui, ui.MakeId("author"), c.Next(TextBoxHeight(ui, FieldStyle()), 18.f), author, FieldStyle(false, 60), Palette::Teal);
            Form::Label(ui, c, 0xE8BD, Loc::Get("rs_description"), Palette::Teal);   // message
            Form::Field(ui, ui.MakeId("description"), c.Next(80.f, 18.f), description, FieldStyle(true, 400), Palette::Teal);
        });
        scroll.SetContentSize(col.Height());
    }

    const float sinceOpen = static_cast<float>(now - openedAt);
    Seasonal::CardArt::DrawPanelParticles(g, panel, 12.f, now, 0.6f);   // seasonal theme
    PanelDecor(ui, panel, 12.f, Palette::Teal, sinceOpen, header.B() - 1.f);
    if (PanelEntranceRunning(sinceOpen)) ui.KeepAnimating();
    if (scaled) ui.PopTransform();
    if (layered) g.PopLayer();
}

} // namespace SpeedrunLauncher::Windows
