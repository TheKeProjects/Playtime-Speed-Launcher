#include "Windows/Main/Overlays/Mods/ModPopupFrame.h"
#include "UI/Style.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/Controls.h"
#include "Windows/Dialogs/DialogWindow.h"

namespace SpeedrunLauncher::Main {

using namespace UI;

namespace {

ButtonStyle StyleFor(const ModButton& b) {
    return b.danger ? ButtonStyle{ Palette::DangerBg, Palette::DangerBorder, 1.f, 3.f, { b.padX, 0.f }, b.minWidth, 36.f }
                    : ButtonStyle{ Palette::ConfirmBg, Palette::ConfirmBorder, 1.f, 3.f, { b.padX, 0.f }, b.minWidth, 36.f };
}

float Width(Ui& ui, const ModButton& b) { return ButtonWidth(ui, b.text, Mono(b.fontSize, true), StyleFor(b)); }

void DrawButton(Ui& ui, const ModButton& b, float x, float cy) {
    if (TextButton(ui, ui.MakeId(b.id), { x, cy - 18.f, Width(ui, b), 36.f }, b.text, Mono(b.fontSize, true).Center(),
                   b.danger ? Palette::DangerText : Palette::Teal, StyleFor(b), b.enabled) &&
        b.onClick)
        b.onClick();
}

} // namespace

void DrawModPopup(Ui& ui, Popup& popup, const Rect& screen, float width, const std::wstring& title, const std::wstring& question,
                  const std::wstring& warning, const std::vector<ModButton>& left, const std::vector<ModButton>& right) {
    PanelStyle style;
    style.title = title;
    OverlayFrame frame(ui, ui.MakeId(std::string("mod-popup-") + std::to_string(static_cast<int>(width))), screen, width, 0.f, style,
                       popup.Opacity(), popup.Scale());
    // Question (+ warning), centred (Margin 28,24)
    const float innerW = frame.body.w - 56.f;
    TextStyle q = Mono(14.f);
    q.wrap = true;
    q.align = HAlign::Center;
    q.valign = VAlign::Top;
    const float qh = ui.g.Measure(question, q, innerW).h;
    float y = frame.body.y + 24.f;
    ui.g.Text(question, { frame.body.x + 28.f, y, innerW, qh }, Palette::Question, q);
    y += qh;
    if (!warning.empty()) {
        TextStyle w = Mono(11.f, true);
        w.wrap = true;
        w.align = HAlign::Center;
        w.valign = VAlign::Top;
        const float wh = ui.g.Measure(warning, w, innerW).h;
        ui.g.Text(warning, { frame.body.x + 28.f, y + 12.f, innerW, wh }, Palette::Warning, w);
        y += 12.f + wh;
    }
    frame.SetContentHeight(y + 24.f - frame.body.y);

    // Footer groups (Margin 14,0)
    const Rect row = frame.footer.Inset(14.f, 0.f);
    float x = row.x;
    for (size_t i = 0; i < left.size(); ++i) {
        DrawButton(ui, left[i], x, row.CY());
        x += Width(ui, left[i]) + (i + 1 < left.size() ? left[i].gapAfter : 0.f);
    }
    float total = 0.f;
    for (size_t i = 0; i < right.size(); ++i) total += Width(ui, right[i]) + (i + 1 < right.size() ? right[i].gapAfter : 0.f);
    x = row.R() - total;
    for (size_t i = 0; i < right.size(); ++i) {
        DrawButton(ui, right[i], x, row.CY());
        x += Width(ui, right[i]) + right[i].gapAfter;
    }
}

void ShowModMessage(HWND owner, const std::wstring& title, const std::wstring& message, bool success) {
    Dialogs::Options o;
    o.title = title;
    o.close = L"OK";
    Dialogs::ShowText(owner, o, message, success ? Rgba(0, 204, 170, 200) : Rgba(160, 180, 200, 200));
}

void ConfirmDeleteUe4ss(HWND owner, const std::wstring& title, std::function<void()> onYes) {
    Dialogs::Options o;
    o.title = title;
    o.primary = L"YES";
    o.close = L"CANCEL";
    Dialogs::ShowText(owner, o, L"This requires deleting the current UE4SS files.\nDo you want to delete them?", Rgba(160, 180, 200, 200),
                      [onYes](Dialogs::Result r) {
                          if (r == Dialogs::Result::Primary && onYes) onYes();
                      });
}

void ConfirmDeleteLegacyLoadManip(HWND owner, const std::wstring& title, std::function<void()> onYes) {
    Dialogs::Options o;
    o.title = title;
    o.primary = L"DELETE";
    o.close = L"CANCEL";
    Dialogs::ShowText(owner, o,
                      L"An old Load Manip (LoadManipV2_P) was found in Content\\Paks.\n"
                      L"It must be deleted before installing. Delete it now?",
                      Rgba(160, 180, 200, 200), [onYes](Dialogs::Result r) {
                          if (r == Dialogs::Result::Primary && onYes) onYes();
                      });
}

} // namespace SpeedrunLauncher::Main
