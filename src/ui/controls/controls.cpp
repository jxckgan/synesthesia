#include "controls.h"

#include <imgui.h>
#include <vector>
#include <algorithm>

#include "colour_mapper.h"

namespace Controls {

void renderFrequencyInfoPanel(AudioInput& audioInput, float* clear_color) {
    if (ImGui::CollapsingHeader("FREQUENCY INFO", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(10);
        
        auto peaks = audioInput.getFrequencyPeaks();
        auto currentColourResult = ColourMapper::frequenciesToColour(
            [&peaks] {
                std::vector<float> f;
                f.reserve(peaks.size());
                for (const auto& p : peaks)
                    f.push_back(p.frequency);
                return f;
            }(),
            [&peaks] {
                std::vector<float> m;
                m.reserve(peaks.size());
                for (const auto& p : peaks)
                    m.push_back(p.magnitude);
                return m;
            }(),
            {}, 44100.0f, 0.8f);

        if (!peaks.empty()) {
            ImGui::Text("Dominant: %.1f Hz", peaks[0].frequency);
            ImGui::Text("Wavelength: %.1f nm", currentColourResult.dominantWavelength);
            ImGui::Text("Number of peaks detected: %d", static_cast<int>(peaks.size()));
        } else {
            ImGui::TextDisabled("No significant frequencies");
        }
        
        ImGui::Text("RGB: (%.2f, %.2f, %.2f)", clear_color[0], clear_color[1], clear_color[2]);
        
        ImGui::Unindent(10);
        ImGui::Spacing();
    }
}

void renderVisualiserSettingsPanel(SpringSmoother& colourSmoother, 
                                 float& smoothingAmount,
                                 float sidebarWidth,
                                 float sidebarPadding,
                                 float labelWidth,
                                 float controlWidth,
                                 float buttonHeight) {
    if (ImGui::CollapsingHeader("VISUALISER SETTINGS", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(10);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Smoothing");
        ImGui::SameLine(sidebarPadding + labelWidth);
        ImGui::SetCursorPosX(sidebarWidth - sidebarPadding - controlWidth);
        ImGui::SetNextItemWidth(controlWidth);
        if (ImGui::SliderFloat("##Smoothing", &smoothingAmount, 0.0f, 1.0f, "%.2f")) {
            colourSmoother.setSmoothingAmount(smoothingAmount);
        }

        ImGui::SetCursorPosX(sidebarPadding);
        if (ImGui::Button("Reset Smoothing", ImVec2(130, buttonHeight))) {
            smoothingAmount = 0.6f;
            colourSmoother.setSmoothingAmount(smoothingAmount);
        }

        ImGui::Unindent(10);
        ImGui::Spacing();
    }
}

void renderEQControlsPanel(float& lowGain,
                          float& midGain, 
                          float& highGain,
                          bool& showSpectrumAnalyser,
                          float sidebarWidth,
                          float sidebarPadding,
                          float labelWidth,
                          float controlWidth,
                          float buttonHeight,
                          float contentWidth) {
    if (ImGui::CollapsingHeader("EQ CONTROLS", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(10);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Lows");
        ImGui::SameLine(sidebarPadding + labelWidth);
        ImGui::SetCursorPosX(sidebarWidth - sidebarPadding - controlWidth);
        ImGui::SetNextItemWidth(controlWidth);
        ImGui::SliderFloat("##LowGain", &lowGain, 0.0f, 2.0f);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Mids");
        ImGui::SameLine(sidebarPadding + labelWidth);
        ImGui::SetCursorPosX(sidebarWidth - sidebarPadding - controlWidth);
        ImGui::SetNextItemWidth(controlWidth);
        ImGui::SliderFloat("##MidGain", &midGain, 0.0f, 2.0f);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Highs");
        ImGui::SameLine(sidebarPadding + labelWidth);
        ImGui::SetCursorPosX(sidebarWidth - sidebarPadding - controlWidth);
        ImGui::SetNextItemWidth(controlWidth);
        ImGui::SliderFloat("##HighGain", &highGain, 0.0f, 2.0f);

        float buttonWidth = (contentWidth - ImGui::GetStyle().ItemSpacing.x) / 2;
        ImGui::SetCursorPosX(sidebarPadding);
        if (ImGui::Button("Reset EQ", ImVec2(buttonWidth, buttonHeight))) {
            lowGain = midGain = highGain = 1.0f;
        }
        
        ImGui::SameLine();
        ImGui::SetCursorPosX(sidebarPadding + buttonWidth + ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::Button(showSpectrumAnalyser ? "Hide Spectrum" : "Show Spectrum",
                          ImVec2(buttonWidth, buttonHeight))) {
            showSpectrumAnalyser = !showSpectrumAnalyser;
        }

        ImGui::Unindent(10);
        ImGui::Spacing();
    }
}

}