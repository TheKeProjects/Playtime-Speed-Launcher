#pragma once
#include "Graphics/Rect.h"
#include "UI/Ui.h"

namespace SpeedrunLauncher::Main {

class MainWindow;

// An in-window panel drawn over the main screen (settings, versions, updates, ...).
class Overlay {
public:
    explicit Overlay(MainWindow& window) : main(window) {}
    virtual ~Overlay() = default;
    virtual bool Visible() const = 0;
    virtual void Draw(UI::Ui& ui, const Rect& screen) = 0;
    // Language changed / data refreshed
    virtual void Refresh() {}

protected:
    MainWindow& main;
};

} // namespace SpeedrunLauncher::Main
