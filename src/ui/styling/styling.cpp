#include "styling.h"

void UIStyler::applyCustomStyle(ImGuiStyle& style, StyleState& state) {
    if (!state.styleApplied) {
        state.originalStyle = style;
        state.styleApplied = true;
    }

    setDimensions(style);
    setColors(style);
}

void UIStyler::restoreOriginalStyle(ImGuiStyle& style, StyleState& state) {
    if (state.styleApplied) {
        style = state.originalStyle;
        state.styleApplied = false;
    }
}

void UIStyler::setDimensions(ImGuiStyle& style) {
    style.WindowRounding = 0.0f;
    style.FrameRounding = 1.0f;
    style.ScrollbarRounding = 5.0f;
    style.GrabRounding = 1.0f;
    style.TabRounding = 1.0f;
    style.ChildRounding = 1.0f;
    style.PopupRounding = 1.0f;
    style.Alpha = 1.0f;
    style.ItemSpacing = ImVec2(10, 12);
    style.FramePadding = ImVec2(8, 6);
    style.WindowPadding = ImVec2(12, 12);
}

void UIStyler::setColors(ImGuiStyle& style) {
    ImVec4* colours = style.Colors;

    colours[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.07f, 0.90f);
    
    colours[ImGuiCol_Border] = ImVec4(0.40f, 0.40f, 0.45f, 0.50f);
    colours[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    
    colours[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.17f, 0.85f);
    colours[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.22f, 0.90f);
    colours[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.27f, 0.95f);
    
    colours[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.90f);
    colours[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.10f, 0.12f, 0.90f);
    colours[ImGuiCol_TitleBgCollapsed] = ImVec4(0.05f, 0.05f, 0.05f, 0.75f);
    
    colours[ImGuiCol_SliderGrab] = ImVec4(0.65f, 0.65f, 0.70f, 0.80f);
    colours[ImGuiCol_SliderGrabActive] = ImVec4(0.80f, 0.80f, 0.85f, 0.90f);
    
    colours[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.22f, 0.75f);
    colours[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.35f, 0.85f);
    colours[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.40f, 0.45f, 0.90f);
    
    colours[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.22f, 0.70f);
    colours[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.35f, 0.85f);
    colours[ImGuiCol_HeaderActive] = ImVec4(0.40f, 0.40f, 0.45f, 0.70f);
    
    colours[ImGuiCol_ScrollbarBg] = ImVec4(0.07f, 0.07f, 0.09f, 0.00f);
    colours[ImGuiCol_ScrollbarGrab] = ImVec4(0.35f, 0.35f, 0.40f, 0.70f);
    colours[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.45f, 0.45f, 0.50f, 0.80f);
    colours[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.55f, 0.55f, 0.60f, 0.85f);
    
    colours[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.90f, 0.95f, 1.00f);
    colours[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colours[ImGuiCol_TextDisabled] = ImVec4(0.65f, 0.65f, 0.65f, 0.80f);
    colours[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.12f, 0.14f, 0.90f);
}
