#include "ui.h"
#include "colour_mapper.h"
#include "fft_processor.h"
#include "audio_input.h"
#include <algorithm>
#include <cmath>

void updateUI(AudioInput &audioInput, 
              const std::vector<AudioInput::DeviceInfo>& devices, 
              float* clear_color, 
              ImGuiIO &io)
{
    // Toggle UI On/Off with 'H' key
    static bool showUI = true;
    if (ImGui::IsKeyPressed(ImGuiKey_H)) {
        showUI = !showUI;
    }

    // Setup audio input
    static int selectedDeviceIndex = -1;
    static bool streamError = false;

    // Get list of available audio input devices
    std::vector<const char*> deviceNames;
    for (const auto &dev : devices) {
        deviceNames.push_back(dev.name.c_str());
    }

    // Colour smoothing
    float deltaTime = io.DeltaTime;
    static float colourSmoothingSpeed = 5.5f;
    const float SMOOTHING = std::min(1.0f, deltaTime * colourSmoothingSpeed);

    static bool dynamicGammaEnabled = false;

    // Define consistent UI layout measurements
    const float LABEL_WIDTH = 90.0f;
    const float CONTROL_WIDTH = 150.0f;
    const float BUTTON_HEIGHT = 25.0f;
    const float SIDEBAR_WIDTH = 280.0f;
    const float SIDEBAR_PADDING = 16.0f;
    const float contentWidth = SIDEBAR_WIDTH - (SIDEBAR_PADDING * 2);
    
    // Process audio data and updates visuals when a device is selected
    if (selectedDeviceIndex >= 0) {
        auto peaks = audioInput.getFrequencyPeaks();
        std::vector<float> freqs, mags;
        for (const auto &peak : peaks) {
            freqs.push_back(peak.frequency);
            mags.push_back(peak.magnitude);
        }

        // Calculate dynamic gamma (based on loudness)
        float gamma = 0.8f;
        float whiteMix = 0.0f;

        if (dynamicGammaEnabled) {
            static float previousLoudness = 0.0f;
            static float transientIntensity = 0.0f;
            static float smoothedLoudness = 0.0f;
            constexpr float smoothingFactor = 0.9f;
            constexpr float transientDecay = 0.92f;
            constexpr float transientSensitivity = 4.0f;
            float rawLoudness = audioInput.getFFTProcessor().getCurrentLoudness();
        
            // Apply a simple low-pass filter to smooth out the loudness
            smoothedLoudness = (smoothingFactor * smoothedLoudness) + ((1.0f - smoothingFactor) * rawLoudness);
            float deltaLoudness = smoothedLoudness - previousLoudness;
            previousLoudness = smoothedLoudness;
            
            // Only consider positive spikes
            float transientDelta = std::max(deltaLoudness, 0.0f);
            transientIntensity = std::clamp(
                (transientIntensity * transientDecay) + (transientDelta * transientSensitivity),
                0.0f, 1.0f
            );
        
            // Gamma calculation based on transient intensity
            constexpr float baseGamma = 1.5f;
            constexpr float minGamma = 0.15f;
            float gammaRange = baseGamma - minGamma;
            
            float normalisedTransient = std::pow(transientIntensity, 0.75f);
            gamma = baseGamma - (gammaRange * normalisedTransient);
            gamma = std::clamp(gamma, minGamma, baseGamma);

            float whiteIntensity = std::pow(transientIntensity, 1.5f);
            whiteMix = std::clamp(whiteIntensity * 2.5f, 0.0f, 0.85f);
            if (transientIntensity > 0.9f) {
                whiteMix = std::clamp(whiteMix * 1.5f, 0.0f, 1.0f);
            }
        }        

        auto colourResult = ColourMapper::frequenciesToColour(freqs, mags, gamma);

        // Blend color
        colourResult.r = colourResult.r * (1 - whiteMix) + whiteMix;
        colourResult.g = colourResult.g * (1 - whiteMix) + whiteMix;
        colourResult.b = colourResult.b * (1 - whiteMix) + whiteMix;

        // Ensure valid color values after mixing
        colourResult.r = std::clamp(colourResult.r, 0.0f, 1.0f);
        colourResult.g = std::clamp(colourResult.g, 0.0f, 1.0f);
        colourResult.b = std::clamp(colourResult.b, 0.0f, 1.0f);
        bool currentValid = std::isfinite(clear_color[0]) && 
                          std::isfinite(clear_color[1]) && 
                          std::isfinite(clear_color[2]);
        
        bool newValid = std::isfinite(colourResult.r) && 
                       std::isfinite(colourResult.g) && 
                       std::isfinite(colourResult.b);

        if (!currentValid) {
            clear_color[0] = clear_color[1] = clear_color[2] = 0.1f;
        }

        // Only apply new colours if they're valid
        if (newValid) {
            // Apply smoothing with clamping
            for (int i = 0; i < 3; i++) {
                float newValue = clear_color[i] * (1.0f - SMOOTHING);
                float colorValue = (i == 0) ? colourResult.r : 
                                 (i == 1) ? colourResult.g : colourResult.b;
                newValue += colorValue * SMOOTHING;
                clear_color[i] = std::clamp(newValue, 0.0f, 1.0f);
            }
        }

        // EQ controls
        static float lowGain = 1.0f;
        static float midGain = 1.0f;
        static float highGain = 1.0f;
        static bool showSpectrumAnalyser = true;

        // Update the FFT processor w/ current EQ settings
        audioInput.getFFTProcessor().setEQGains(lowGain, midGain, highGain);
        
        // Spectrum Analyser data preparation
        static std::vector<float> smoothedMagnitudes(FFTProcessor::FFT_SIZE / 2 + 1, 0.0f);
        static float smoothingFactor = 0.2f;
        
        const auto& magnitudes = audioInput.getFFTProcessor().getMagnitudesBuffer();
        if (smoothedMagnitudes.size() != magnitudes.size()) {
            smoothedMagnitudes.assign(magnitudes.size(), 0.0f);
        }
        const int count = static_cast<int>(magnitudes.size());

        // Apply smoothing using EMA
        for (int i = 0; i < count; ++i) {
            smoothedMagnitudes[i] = smoothingFactor * magnitudes[i] +
                                    (1.0f - smoothingFactor) * smoothedMagnitudes[i];
        }
        
        // Create sidebar UI
        if (showUI) {
            // Setup black sidebar style
            ImGuiStyle& style = ImGui::GetStyle();
            ImVec4* colors = style.Colors;
            
            // Store original style to restore later
            ImGuiStyle originalStyle = style;
            
            // Apply custom style for sidebar
            style.WindowRounding = 0.0f;
            style.FrameRounding = 3.0f;
            style.ScrollbarRounding = 2.0f;
            style.GrabRounding = 3.0f;
            style.Alpha = 1.0f;
            style.ItemSpacing = ImVec2(8, 10);
            style.FramePadding = ImVec2(6, 4);
            
            // Set dark theme colours with minimal grey outlines
            colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.94f);
            colors[ImGuiCol_Border] = ImVec4(0.3f, 0.3f, 0.3f, 0.24f);
            colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
            colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
            colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
            colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
            colors[ImGuiCol_TitleBgActive] = ImVec4(0.07f, 0.07f, 0.07f, 1.00f);
            colors[ImGuiCol_SliderGrab] = ImVec4(0.5f, 0.5f, 0.5f, 1.00f);
            colors[ImGuiCol_SliderGrabActive] = ImVec4(0.7f, 0.7f, 0.7f, 1.00f);
            colors[ImGuiCol_Button] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
            colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
            colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
            colors[ImGuiCol_Header] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
            colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
            colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
            colors[ImGuiCol_CheckMark] = ImVec4(0.8f, 0.8f, 0.8f, 1.00f);
            colors[ImGuiCol_Text] = ImVec4(0.9f, 0.9f, 0.9f, 1.00f);
            colors[ImGuiCol_TextDisabled] = ImVec4(0.6f, 0.6f, 0.6f, 1.00f);
            ImVec2 displaySize = io.DisplaySize;
            
            // Set up sidebar window
            ImGui::SetNextWindowPos(ImVec2(displaySize.x - SIDEBAR_WIDTH, 0));
            ImGui::SetNextWindowSize(ImVec2(SIDEBAR_WIDTH, displaySize.y));
            
            ImGui::Begin("Sidebar", nullptr, 
                ImGuiWindowFlags_NoTitleBar | 
                ImGuiWindowFlags_NoResize | 
                ImGuiWindowFlags_NoMove);
            
            // App title
            ImGui::SetCursorPosY(20);
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
            ImGui::SetCursorPosX((SIDEBAR_WIDTH - ImGui::CalcTextSize("Synesthesia").x) * 0.5f);
            ImGui::Text("Synesthesia");
            ImGui::PopFont();
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            // Input Selection
            ImGui::Text("INPUT DEVICE");
            if (!devices.empty()) {
                if (ImGui::Combo("##device", &selectedDeviceIndex, deviceNames.data(), static_cast<int>(deviceNames.size()))) {
                    if (selectedDeviceIndex >= 0 && static_cast<size_t>(selectedDeviceIndex) < devices.size()) {
                        if (!audioInput.initStream(devices[selectedDeviceIndex].paIndex)) {
                            streamError = true;
                        }
                        else {
                            streamError = false;
                        }
                    }
                }
                if (streamError) {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error opening device!");
                }
            }
            else {
                ImGui::TextDisabled("No audio input devices found.");
            }
            ImGui::Spacing();
            
            // Frequency Info
            if (ImGui::CollapsingHeader("FREQUENCY INFO", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent(10);
                if (!peaks.empty()) {
                    ImGui::Text("Dominant: %.1f Hz", peaks[0].frequency);
                    ImGui::Text("Wavelength: %.1f nm", colourResult.dominantWavelength);
                    
                    const int MAX_PEAKS_TO_SHOW = 3;
                    for (size_t i = 0; i < std::min(peaks.size(), static_cast<size_t>(MAX_PEAKS_TO_SHOW)); ++i) {
                        ImGui::Text("Peak %d: %.1f Hz", static_cast<int>(i) + 1, peaks[i].frequency);
                    }
                }
                else {
                    ImGui::TextDisabled("No significant frequencies");
                }
                ImGui::Text("RGB: (%.2f, %.2f, %.2f)", clear_color[0], clear_color[1], clear_color[2]);
                ImGui::Unindent(10);
                ImGui::Spacing();
            }
            
            // Colour Settings
            if (ImGui::CollapsingHeader("COLOUR SETTINGS", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent(10);

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Dynamic Gamma");
                ImGui::SameLine(SIDEBAR_PADDING + LABEL_WIDTH); 
                ImGui::SetCursorPosX(SIDEBAR_WIDTH - SIDEBAR_PADDING - 20);
                ImGui::Checkbox("##DynamicGamma", &dynamicGammaEnabled);
                
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Smoothing");
                ImGui::SameLine(SIDEBAR_PADDING + LABEL_WIDTH);
                ImGui::SetCursorPosX(SIDEBAR_WIDTH - SIDEBAR_PADDING - CONTROL_WIDTH);
                ImGui::SetNextItemWidth(CONTROL_WIDTH);
                ImGui::SliderFloat("##Smoothing", &colourSmoothingSpeed, 0.1f, 20.0f, "%.1f");

                ImGui::SetCursorPosX(SIDEBAR_PADDING);
                if (ImGui::Button("Reset Smoothing", ImVec2(130, BUTTON_HEIGHT))) {
                    colourSmoothingSpeed = 5.5f;
                }
                
                ImGui::Unindent(10);
                ImGui::Spacing();
            }
            
            // EQ Controls
            if (ImGui::CollapsingHeader("EQ CONTROLS", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent(10);

                ImGui::AlignTextToFramePadding();
                ImGui::Text("Lows");
                ImGui::SameLine(SIDEBAR_PADDING + LABEL_WIDTH);
                ImGui::SetCursorPosX(SIDEBAR_WIDTH - SIDEBAR_PADDING - CONTROL_WIDTH);
                ImGui::SetNextItemWidth(CONTROL_WIDTH);
                ImGui::SliderFloat("##LowGain", &lowGain, 0.0f, 2.0f);
                
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Mids");
                ImGui::SameLine(SIDEBAR_PADDING + LABEL_WIDTH);
                ImGui::SetCursorPosX(SIDEBAR_WIDTH - SIDEBAR_PADDING - CONTROL_WIDTH);
                ImGui::SetNextItemWidth(CONTROL_WIDTH);
                ImGui::SliderFloat("##MidGain", &midGain, 0.0f, 2.0f);
                
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Highs");
                ImGui::SameLine(SIDEBAR_PADDING + LABEL_WIDTH);
                ImGui::SetCursorPosX(SIDEBAR_WIDTH - SIDEBAR_PADDING - CONTROL_WIDTH);
                ImGui::SetNextItemWidth(CONTROL_WIDTH);
                ImGui::SliderFloat("##HighGain", &highGain, 0.0f, 2.0f);

                float buttonWidth = (contentWidth - ImGui::GetStyle().ItemSpacing.x) / 2;
                ImGui::SetCursorPosX(SIDEBAR_PADDING);
                if (ImGui::Button("Reset EQ", ImVec2(buttonWidth, BUTTON_HEIGHT))) {
                    lowGain = midGain = highGain = 1.0f;
                }
                ImGui::SameLine();
                if (ImGui::Button(showSpectrumAnalyser ? "Hide Spectrum" : "Show Spectrum", ImVec2(buttonWidth, BUTTON_HEIGHT))) {
                    showSpectrumAnalyser = !showSpectrumAnalyser;
                }
                
                ImGui::Unindent(10);
                ImGui::Spacing();
            }
            
            // Show/Hide UI
            const float bottomTextHeight = ImGui::GetTextLineHeightWithSpacing() + 12;
            ImGui::SetCursorPos(ImVec2(SIDEBAR_PADDING, displaySize.y - bottomTextHeight));
            ImGui::Separator();
            float textWidth = ImGui::CalcTextSize("Press H to hide/show interface").x;
            ImGui::SetCursorPosX((SIDEBAR_WIDTH - textWidth) * 0.5f);
            ImGui::TextDisabled("Press H to hide/show interface");
            
            ImGui::End();
            
            // Restore original style
            style = originalStyle;
            
            // Spectrum Analyser window
            if (showSpectrumAnalyser) {
                const float spectrumHeight = 210.0f;
                
                // Position spectrum analyser at the bottom of the screen
                ImVec2 spectrumPos = ImVec2(0.0f, displaySize.y - spectrumHeight);
                ImVec2 spectrumSize = ImVec2(displaySize.x - SIDEBAR_WIDTH, spectrumHeight);
                
                // Create completely transparent window with no decorations and no padding
                ImGui::SetNextWindowPos(spectrumPos);
                ImGui::SetNextWindowSize(spectrumSize);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::Begin("##SpectrumAnalyser", nullptr, 
                    ImGuiWindowFlags_NoTitleBar | 
                    ImGuiWindowFlags_NoResize | 
                    ImGuiWindowFlags_NoMove | 
                    ImGuiWindowFlags_NoScrollbar |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoBackground);
                
                // Normalise Y-Axis for spectrum
                float maxMagnitude = *std::max_element(smoothedMagnitudes.begin(), smoothedMagnitudes.end());
                float yMax = (maxMagnitude > 0.0001f) ? maxMagnitude * 1.1f : 0.1f;
                
                // Get window dimensions and cursor position with no padding
                ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
                ImVec2 canvas_size = ImGui::GetContentRegionAvail();
                
                // Get draw list for custom rendering
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                
                // Create a subtle background that fades from transparent to slightly darker at the bottom
                drawList->AddRectFilledMultiColor(
                    ImVec2(canvas_pos.x, canvas_pos.y),
                    ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.0f)),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.0f)),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.3f)),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.3f))
                );
                
                // Create efficient point array for spectrum line
                const int lineCount = std::min(16384, static_cast<int>(smoothedMagnitudes.size()));
                ImVec2* points = (ImVec2*)alloca(sizeof(ImVec2) * lineCount);
                
                // Fill point array with spectrum data (curve points only)
                for (int i = 0; i < lineCount; ++i) {
                    // Apply logarithmic scaling for frequency axis
                    float logPosition = std::pow(static_cast<float>(i) / lineCount, 0.4f);
                    float xPos = canvas_pos.x + (logPosition * canvas_size.x);
                    
                    // More smoothing for visual appeal
                    float magnitude = 0.0f;
                    int count = 0;
                    
                    // Simplified smoothing with immediate neighbors
                    for (int j = std::max(0, i-2); j <= std::min(lineCount-1, i+2); ++j) {
                        float weight = 1.0f - (std::abs(i - j) / 3.0f);
                        magnitude += smoothedMagnitudes[j] * weight;
                        count++;
                    }
                    magnitude /= count;
                    
                    float height = (magnitude / yMax) * canvas_size.y * 0.9f; // 90% max height for visual margin
                    height = std::min(height, canvas_size.y * 0.95f);
                    
                    float yPos = canvas_pos.y + canvas_size.y - height;
                    points[i] = ImVec2(xPos, yPos);
                }
                
                // Define colours
                ImU32 fillColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
                ImU32 lineColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.9f));
                
                // Fill everything below the curve
                for (int i = 0; i < lineCount - 1; ++i) {
                    // Create a quad for each segment between adjacent points
                    ImVec2 quad[4];
                    quad[0] = points[i];
                    quad[1] = points[i+1];
                    quad[2] = ImVec2(points[i+1].x, canvas_pos.y + canvas_size.y);
                    quad[3] = ImVec2(points[i].x, canvas_pos.y + canvas_size.y);
                    
                    drawList->AddConvexPolyFilled(quad, 4, fillColor);
                }
                
                // Draw the spectrum line on top
                const float lineThickness = 1.5f;
                for (int i = 0; i < lineCount - 1; ++i) {
                    drawList->AddLine(points[i], points[i+1], lineColor, lineThickness);
                }
                
                ImGui::PopStyleVar();
                ImGui::End();
                ImGui::PopStyleColor();
            }
        }
    }
    // Show only device selection when no device is selected
    else if (showUI) {        
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;
        
        ImGuiStyle originalStyle = style;

        style.WindowRounding = 0.0f;
        style.FrameRounding = 3.0f;
        style.ScrollbarRounding = 2.0f;
        style.GrabRounding = 3.0f;
        style.Alpha = 1.0f;
        style.ItemSpacing = ImVec2(8, 10);
        style.FramePadding = ImVec2(6, 4);

        colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.94f);
        colors[ImGuiCol_Border] = ImVec4(0.3f, 0.3f, 0.3f, 0.24f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.07f, 0.07f, 0.07f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.5f, 0.5f, 0.5f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.7f, 0.7f, 0.7f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.8f, 0.8f, 0.8f, 1.00f);
        colors[ImGuiCol_Text] = ImVec4(0.9f, 0.9f, 0.9f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.6f, 0.6f, 0.6f, 1.00f);
        ImVec2 displaySize = io.DisplaySize;

        ImGui::SetNextWindowPos(ImVec2(displaySize.x - SIDEBAR_WIDTH, 0));
        ImGui::SetNextWindowSize(ImVec2(SIDEBAR_WIDTH, displaySize.y));
        
        ImGui::Begin("Sidebar", nullptr, 
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove);
        
        ImGui::SetCursorPosY(20);
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
        ImGui::SetCursorPosX((SIDEBAR_WIDTH - ImGui::CalcTextSize("Synesthesia").x) * 0.5f);
        ImGui::Text("Synesthesia");
        ImGui::PopFont();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::Text("INPUT DEVICE");
        if (!devices.empty()) {
            if (ImGui::Combo("##device", &selectedDeviceIndex, deviceNames.data(), static_cast<int>(deviceNames.size()))) {
                if (selectedDeviceIndex >= 0 && static_cast<size_t>(selectedDeviceIndex) < devices.size()) {
                    if (!audioInput.initStream(devices[selectedDeviceIndex].paIndex)) {
                        streamError = true;
                    }
                    else {
                        streamError = false;
                    }
                }
            }
            if (streamError) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error opening device!");
            }
            else {
                ImGui::TextDisabled("Select an audio device");
            }
        }
        else {
            ImGui::TextDisabled("No audio input devices found.");
        }
        ImGui::Spacing();
        ImGui::Spacing();
        
        const float bottomTextHeight = ImGui::GetTextLineHeightWithSpacing() + 12;
        ImGui::SetCursorPos(ImVec2(SIDEBAR_PADDING, displaySize.y - bottomTextHeight));
        ImGui::Separator();
        float textWidth = ImGui::CalcTextSize("Press H to hide/show interface").x;
        ImGui::SetCursorPosX((SIDEBAR_WIDTH - textWidth) * 0.5f);
        ImGui::TextDisabled("Press H to hide/show interface");
        
        ImGui::End();
        
        // Restore original style
        style = originalStyle;
    }
}