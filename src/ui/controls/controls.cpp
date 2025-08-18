#include "controls.h"

#include <imgui.h>
#include <vector>
#include <algorithm>

#include "colour_mapper.h"
#include "ui.h"
#include "api/synesthesia_api_integration.h"

namespace Controls {

void renderFrequencyInfoPanel(AudioInput& audioInput, float* clear_color) {
    if (ImGui::CollapsingHeader("FREQUENCY INFO", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent(10);
        
        auto peaks = audioInput.getFrequencyPeaks();
        
        std::vector<float> frequencies, magnitudes;
        frequencies.reserve(peaks.size());
        magnitudes.reserve(peaks.size());
        for (const auto& peak : peaks) {
            frequencies.push_back(peak.frequency);
            magnitudes.push_back(peak.magnitude);
        }
        
        auto currentColourResult = ColourMapper::frequenciesToColour(
            frequencies, magnitudes, {}, UIConstants::DEFAULT_SAMPLE_RATE, UIConstants::DEFAULT_GAMMA);

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
            smoothingAmount = UIConstants::DEFAULT_SMOOTHING_SPEED;
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
    }
}

void renderAdvancedSettingsPanel(UIState& state) {
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Advanced Settings")) {
        if (ImGui::CollapsingHeader("Program Appearance")) {
            ImGui::Text("Sidebar Position:");
            ImGui::SameLine();
            if (ImGui::Button("Swap")) {
                state.sidebarOnLeft = !state.sidebarOnLeft;
            }
            ImGui::Text("Currently: %s", state.sidebarOnLeft ? "Left" : "Right");
            
            ImGui::Spacing();
            if (ImGui::Checkbox("Enable Colour Smoothing", &state.smoothingEnabled)) {
                if (!state.smoothingEnabled) {
                    // Show photosensitivity warning when disabling smoothing
                    ImGui::OpenPopup("Photosensitivity Warning");
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Smoothing reduces rapid colour changes. Disabling may cause rapid flashing.");
            }
            
            // Photosensitivity warning popup
            if (ImGui::BeginPopupModal("Photosensitivity Warning", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Warning: Disabling colour smoothing may cause rapid flashing colours.");
                ImGui::Text("This could potentially trigger photosensitive epilepsy in sensitive individuals.");
                ImGui::Spacing();
                ImGui::Text("Are you sure you want to disable smoothing?");
                ImGui::Spacing();
                
                if (ImGui::Button("Yes, Disable Smoothing")) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("No, Keep Smoothing Enabled")) {
                    state.smoothingEnabled = true; // Re-enable smoothing
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
        
        if (ImGui::CollapsingHeader("API Settings")) {
            auto& api = Synesthesia::SynesthesiaAPIIntegration::getInstance();

            ImGui::Text("Server Status: %s", api.isServerRunning() ? "Running" : "Stopped");
            
            auto clients = api.getConnectedClients();
            ImGui::Text("Connected Clients: %zu", clients.size());
            
            if (!clients.empty()) {
                ImGui::Indent();
                for (size_t i = 0; i < clients.size() && i < 5; ++i) {
                    ImGui::Text("• %s", clients[i].c_str());
                }
                if (clients.size() > 5) {
                    ImGui::Text("... and %zu more", clients.size() - 5);
                }
                ImGui::Unindent();
            }
            
            ImGui::Text("Data Points: %zu", api.getLastDataSize());
            
            if (api.isServerRunning()) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Text("Performance Status");
                ImGui::Spacing();
                
                uint32_t current_fps = api.getCurrentFPS();
                bool high_perf = api.isHighPerformanceMode();
                float avg_frame_time = api.getAverageFrameTime();
                
                ImGui::Text("Current FPS: %u", current_fps);
                ImGui::Text("Mode: %s", high_perf ? "High Performance" : "Standard");
                if (avg_frame_time > 0) {
                    ImGui::Text("Avg Frame Time: %.2fms", avg_frame_time);
                    float estimated_latency = avg_frame_time;
                    ImGui::Text("Estimated Latency: ~%.1fms", estimated_latency);
                    
                    // Visual indicator
                    if (estimated_latency < 5.0f) {
                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Ultra-Low Latency");
                    } else if (estimated_latency < 10.0f) {
                        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.0f, 1.0f), "✓ Low Latency");
                    } else if (estimated_latency < 20.0f) {
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "⚠ Moderate Latency");
                    } else {
                        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "⚠ High Latency");
                    }
                }
                
                ImGui::Text("Total Frames: %llu", api.getTotalFramesSent());
                ImGui::Separator();
            }
            
            ImGui::Spacing();
            bool serverRunning = api.isServerRunning();
            
            if (!serverRunning) {
                if (ImGui::Button("Enable")) {
                    state.apiServerEnabled = true;
                    api.startServer(); // Now uses optimized settings by default
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.6f, 0.4f));
                ImGui::Button("Enable");
                ImGui::PopStyleColor();
            }
            
            ImGui::SameLine();
            
            if (serverRunning) {
                if (ImGui::Button("Disable")) {
                    state.apiServerEnabled = false;
                    api.stopServer();
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.6f, 0.4f));
                ImGui::Button("Disable");
                ImGui::PopStyleColor();
            }
        }
    }
}

}