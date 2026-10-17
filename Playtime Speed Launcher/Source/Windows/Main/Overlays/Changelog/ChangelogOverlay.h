#pragma once
#include <string>
#include <vector>
#include "Services/App/ChangelogData.h"
#include "UI/Widgets/Panel.h"
#include "Windows/Main/Overlays/Overlay.h"

namespace SpeedrunLauncher::Main {

// The in-app changelog as a timeline: one card per version hanging from a vertical line (the
// current version's node glows), "## " sections as coloured tags and bulleted changes with
// @mention chips. The layout is computed once per width / language and only the cards on screen
// are drawn, so opening it stays smooth with dozens of versions.
class ChangelogOverlay : public Overlay {
public:
    using Overlay::Overlay;
    bool Visible() const override { return popup.Visible(); }
    void Draw(UI::Ui& ui, const Rect& screen) override;
    void Open();

private:
    enum class SectionKind { Additions, Fixes, Other, Redesigns, Themes, Performance, Generic };
    struct Line {
        std::wstring text;
        bool mentions = false;
        float y = 0.f, h = 0.f;   // relative to the card
    };
    struct Section {
        std::wstring title;       // empty: lines before any "## " header
        SectionKind kind = SectionKind::Generic;
        float y = 0.f;            // tag position, relative to the card
        std::vector<Line> lines;
    };
    struct Card {
        const Services::Changelog::Entry* entry = nullptr;
        bool current = false;
        float y = 0.f, h = 0.f;   // relative to the list top
        std::vector<Section> sections;
    };

    void BuildLayout(UI::Ui& ui, float width);
    void DrawCard(UI::Ui& ui, const Card& card, float x, float y, float w, int index);

    UI::Popup popup;
    bool resetScroll = false;
    double openedAt = -10.0;
    // Cached layout
    std::vector<Card> cards;
    float layoutWidth = -1.f;
    unsigned layoutLang = ~0u;
    float listHeight = 0.f;
};

} // namespace SpeedrunLauncher::Main
