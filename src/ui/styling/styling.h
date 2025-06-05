#ifndef STYLING_H
#define STYLING_H

#include <imgui.h>

struct StyleState {
    ImGuiStyle originalStyle;
    bool styleApplied = false;
};

class UIStyler {
public:
    static void applyCustomStyle(ImGuiStyle& style, StyleState& state);
    static void restoreOriginalStyle(ImGuiStyle& style, StyleState& state);
    
private:
    static void setColors(ImGuiStyle& style);
    static void setDimensions(ImGuiStyle& style);
};

#endif
