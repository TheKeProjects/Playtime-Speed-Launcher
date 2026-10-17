#pragma once
#include "Graphics/Rect.h"

// Tiny layout helpers that stand in for XAML StackPanel / Grid columns.
namespace SpeedrunLauncher::UI {

// Vertical StackPanel: hands out rows top to bottom.
struct Column {
    float x = 0.f, y = 0.f, w = 0.f;
    float top = 0.f;

    Column() = default;
    Column(float left, float startY, float width) : x(left), y(startY), w(width), top(startY) {}
    explicit Column(const Rect& r) : x(r.x), y(r.y), w(r.w), top(r.y) {}

    Rect Next(float height) {
        Rect r{ x, y, w, height };
        y += height;
        return r;
    }
    // Row with a bottom margin (XAML Margin="0,0,0,m")
    Rect Next(float height, float marginBottom) {
        Rect r = Next(height);
        y += marginBottom;
        return r;
    }
    void Gap(float g) { y += g; }
    float Height() const { return y - top; }
};

// Splits a row into N equal columns with gaps (Grid with "*" columns and fixed spacers).
inline Rect GridCell(const Rect& row, int columns, int index, float gap) {
    const float cellW = (row.w - gap * (columns - 1)) / columns;
    return { row.x + index * (cellW + gap), row.y, cellW, row.h };
}

// Horizontal StackPanel flowing right to left from `right` (HorizontalAlignment="Right").
struct RowFromRight {
    float right, y, h;
    Rect Next(float width, float gapAfter = 0.f) {
        right -= width;
        Rect r{ right, y, width, h };
        right -= gapAfter;
        return r;
    }
};

// Horizontal StackPanel flowing left to right.
struct Row {
    float x, y, h;
    Rect Next(float width, float gapAfter = 0.f) {
        Rect r{ x, y, width, h };
        x += width + gapAfter;
        return r;
    }
};

} // namespace SpeedrunLauncher::UI
