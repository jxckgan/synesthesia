#ifndef STYLING_H
#define STYLING_H

#include <imgui.h>

enum class UITheme {
    Dark,
    Light
};

struct StyleState {
    ImGuiStyle originalStyle;
    bool styleApplied = false;
    UITheme currentTheme = UITheme::Dark;
};

class UIStyler {
public:
    static void applyCustomStyle(ImGuiStyle& style, StyleState& state);
    static void applyCustomStyleWithTheme(ImGuiStyle& style, StyleState& state, UITheme theme);
    static void restoreOriginalStyle(ImGuiStyle& style, StyleState& state);

private:
    static void setColors(ImGuiStyle& style, UITheme theme);
    static void setDimensions(ImGuiStyle& style);
};

#endif
