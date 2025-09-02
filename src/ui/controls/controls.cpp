#include "controls.h"

#include <imgui.h>
#include <vector>
#include <algorithm>

#include "colour_mapper.h"
#include "ui.h"
#ifdef ENABLE_API_SERVER
#include "api/synesthesia_api_integration.h"
#endif

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
    static bool previousSmoothingState = state.smoothingEnabled;
    
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Advanced Settings")) {
        ImGui::Indent(10);
        if (ImGui::CollapsingHeader("Program Appearance")) {
            ImGui::Text("Sidebar: %s", state.sidebarOnLeft ? "Left" : "Right");
            if (ImGui::Button("Swap Sides")) {
                state.sidebarOnLeft = !state.sidebarOnLeft;
            }
            
            ImGui::Spacing();
            bool currentSmoothingState = state.smoothingEnabled;
            if (ImGui::Checkbox("Enable Smoothing", &currentSmoothingState)) {
                if (currentSmoothingState != state.smoothingEnabled) {
                    if (!currentSmoothingState && state.smoothingEnabled) {
                        // User wants to disable smoothing - show warning first
                        previousSmoothingState = state.smoothingEnabled;
                        ImGui::OpenPopup("Photosensitivity Warning");
                    } else {
                        // User is enabling smoothing - allow immediately
                        state.smoothingEnabled = currentSmoothingState;
                    }
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Smoothing reduces rapid colour changes.\nDisabling will cause rapid flashing.");
            }
            
            // Photosensitivity warning popup
            if (ImGui::BeginPopupModal("Photosensitivity Warning", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 350.0f); // Constrain width
                ImGui::TextWrapped("Warning: Disabling smoothing will cause rapidly flashing colours which can trigger photosensitive epilepsy in sensitive individuals.");
                ImGui::Spacing();
                ImGui::TextWrapped("Are you sure you want to disable smoothing?");
                ImGui::PopTextWrapPos();
                ImGui::Spacing();
                
                if (ImGui::Button("(Yes) Disable Smoothing")) {
                    state.smoothingEnabled = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("(No) Keep Smoothing Enabled")) {
                    state.smoothingEnabled = previousSmoothingState;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
        
#ifdef ENABLE_API_SERVER
        if (ImGui::CollapsingHeader("API Settings")) {
            auto& api = Synesthesia::SynesthesiaAPIIntegration::getInstance();

            ImGui::Text("Server Status: %s", api.isServerRunning() ? "Running" : "Stopped");
            
            auto clients = api.getConnectedClients();
            ImGui::Text("Connected Clients: %zu", clients.size());
            
            if (!clients.empty()) {
                ImGui::Indent();
                for (size_t i = 0; i < clients.size() && i < 5; ++i) {
                    // Truncate long client names to prevent horizontal overflow
                    std::string clientName = clients[i];
                    if (clientName.length() > 25) {
                        clientName = clientName.substr(0, 22) + "...";
                    }
                    ImGui::Text("• %s", clientName.c_str());
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
                ImGui::Text("Performance");
                ImGui::Spacing();
                
                uint32_t current_fps = api.getCurrentFPS();
                bool high_perf = api.isHighPerformanceMode();
                float avg_frame_time = api.getAverageFrameTime();
                
                // Constrain text width to prevent horizontal overflow
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + 220.0f);
                
                ImGui::Text("FPS: %u", current_fps);
                ImGui::Text("Mode: %s", high_perf ? "High Perf" : "Standard");
                if (avg_frame_time > 0) {
                    ImGui::Text("Frame Time: %.2fms", avg_frame_time);
                    float estimated_latency = avg_frame_time;
                    ImGui::Text("Latency: ~%.1fms", estimated_latency);
                    
                    // Visual indicator
                    if (estimated_latency < 5.0f) {
                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ Ultra-Low");
                    } else if (estimated_latency < 10.0f) {
                        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.0f, 1.0f), "✓ Low");
                    } else if (estimated_latency < 20.0f) {
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "⚠ Moderate");
                    } else {
                        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "⚠ High");
                    }
                }
                
                ImGui::Text("Total Frames: %llu", api.getTotalFramesSent());
                
                ImGui::PopTextWrapPos();
                ImGui::Separator();
            }
            
            ImGui::Spacing();
            bool serverRunning = api.isServerRunning();
            
            // Constrain button widths to prevent horizontal overflow
            float buttonWidth = (220.0f - ImGui::GetStyle().ItemSpacing.x) / 2;
            
            if (!serverRunning) {
                if (ImGui::Button("Enable", ImVec2(buttonWidth, 0))) {
                    state.apiServerEnabled = true;
                    api.startServer(); // Now uses optimized settings by default
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.6f, 0.4f));
                ImGui::Button("Enable", ImVec2(buttonWidth, 0));
                ImGui::PopStyleColor();
            }
            
            ImGui::SameLine();
            
            if (serverRunning) {
                if (ImGui::Button("Disable", ImVec2(buttonWidth, 0))) {
                    state.apiServerEnabled = false;
                    api.stopServer();
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.6f, 0.4f));
                ImGui::Button("Disable", ImVec2(buttonWidth, 0));
                ImGui::PopStyleColor();
            }
        }
#endif // ENABLE_API_SERVER
        ImGui::Unindent(10);
    }
}

}