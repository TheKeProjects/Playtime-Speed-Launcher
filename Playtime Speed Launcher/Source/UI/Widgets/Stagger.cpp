#include "UI/Widgets/Stagger.h"
#include <algorithm>
#include "Graphics/Animation.h"

namespace SpeedrunLauncher::UI {

namespace {
constexpr int kMaxSlot = 14;   // items past this one start together with it
}

StaggerItem::StaggerItem(Ui& u, double start, int index, float fromX, float fromY, double delay, double duration) : ui(u) {
    const double begin = start + std::min(index, kMaxSlot) * delay;
    const float t = static_cast<float>((ui.Time() - begin) / duration);
    if (t >= 1.f) return;
    // Ease out with a hint of overshoot, so items land softly instead of stopping dead
    const float p = Anim::Clamp01(t);
    const float move = Anim::EaseOutBack(p, 0.45f);
    const float fade = Anim::EaseOutCubic(std::min(1.f, p * 1.6f));
    ui.g.PushOpacity(fade);
    ui.PushTranslate((1.f - move) * fromX, (1.f - move) * fromY);
    ui.KeepAnimating();
    active = true;
}

StaggerItem::~StaggerItem() {
    if (!active) return;
    ui.PopTransform();
    ui.g.PopLayer();
}

} // namespace SpeedrunLauncher::UI
