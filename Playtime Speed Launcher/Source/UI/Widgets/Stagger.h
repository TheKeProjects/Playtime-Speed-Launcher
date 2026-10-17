#pragma once
#include "UI/Ui.h"

namespace SpeedrunLauncher::UI {

// Staggered entrance ("one after another"): while alive, the drawing of item `index` fades in and
// slides from (fromX, fromY) to its place, starting index × delay after `start` (Ui::Time clock).
// Items far down a long list share the last slot so the list never takes long to appear.
class StaggerItem {
public:
    StaggerItem(Ui& ui, double start, int index, float fromX, float fromY = 0.f, double delay = 0.04, double duration = 0.38);
    ~StaggerItem();
    StaggerItem(const StaggerItem&) = delete;
    StaggerItem& operator=(const StaggerItem&) = delete;

private:
    Ui& ui;
    bool active = false;
};

} // namespace SpeedrunLauncher::UI
