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

    // Create a window for audio input device selection
    if (showUI) {
        ImGui::SetNextWindowPos(ImVec2(15, 15), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(200, 55), ImGuiCond_Always);
        ImGui::Begin("Input Selection");
        if (!devices.empty()) {
            if (ImGui::Combo("##", &selectedDeviceIndex, deviceNames.data(), static_cast<int>(deviceNames.size()))) {
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
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error opening device!");
            }
        }
        else {
            ImGui::Text("No audio input devices found.");
        }
        ImGui::End();
    }

    // Process audio data and update visuals when a device is selected
    if (selectedDeviceIndex >= 0) {
        auto peaks = audioInput.getFrequencyPeaks();
        std::vector<float> freqs, mags;
        for (const auto &peak : peaks) {
            freqs.push_back(peak.frequency);
            mags.push_back(peak.magnitude);
        }

        auto colourResult = ColourMapper::frequenciesToColour(freqs, mags);
        
        // Check if current colours are valid
        bool currentValid = std::isfinite(clear_color[0]) && 
                          std::isfinite(clear_color[1]) && 
                          std::isfinite(clear_color[2]);
        
        // Check if new colours are valid
        bool newValid = std::isfinite(colourResult.r) && 
                       std::isfinite(colourResult.g) && 
                       std::isfinite(colourResult.b);

        // Reset to default if current colours are invalid
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

        // Show/Hide Text
        if (showUI) {
            ImVec2 textSize = ImGui::CalcTextSize("Press (H) to Show/Hide");
            ImVec2 textPos = ImVec2(io.DisplaySize.x - textSize.x - 15, io.DisplaySize.y - textSize.y - 15);
            ImGui::SetNextWindowPos(textPos);
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::Begin("InfoOverlay", nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground |
                         ImGuiWindowFlags_NoDecoration);
            ImGui::Text("Press (H) to Show/Hide");
            ImGui::End();
        }

        // Display frequency and colour information
        if (showUI) {
            ImGui::SetNextWindowPos(ImVec2(15, 78), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(200, 140), ImGuiCond_Always);
            ImGui::Begin("Frequency Info");
            if (!peaks.empty()) {
                ImGui::Text("Dominant: %.1f Hz", peaks[0].frequency);
                ImGui::Text("Wavelength: %.1f nm", colourResult.dominantWavelength);
                for (size_t i = 0; i < peaks.size(); ++i)
                {
                    ImGui::Text("Peak %d: %.1f Hz", static_cast<int>(i) + 1, peaks[i].frequency);
                }
            }
            else {
                ImGui::Text("No significant frequencies");
            }
            ImGui::Text("RGB: (%.2f, %.2f, %.2f)", clear_color[0], clear_color[1], clear_color[2]);
            ImGui::End();
        }

        // Colour Settings
        static std::vector<float> smoothedMagnitudes(FFTProcessor::FFT_SIZE / 2 + 1, 0.0f);
        static float smoothingFactor = 0.2f;

        if (showUI) {
            ImVec2 displaySize = io.DisplaySize;
            ImVec2 windowSize = ImVec2(250, 80);
            ImGui::SetNextWindowPos(ImVec2(15, displaySize.y - windowSize.y - 150), ImGuiCond_Always);
            ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
            ImGui::Begin("Colour Settings");
            
            ImGui::SliderFloat("Smoothing", &colourSmoothingSpeed, 0.1f, 20.0f, "%.1f");
            if (ImGui::Button("Reset")) {
                colourSmoothingSpeed = 5.5f;
            }
            
            ImGui::End();
        }

        // EQ controls
        static float lowGain = 1.0f;
        static float midGain = 1.0f;
        static float highGain = 1.0f;
        static bool showSpectrumAnalyser = true;

        // EQ window
        if (showUI) {
            ImVec2 displaySize = io.DisplaySize;
            ImVec2 windowSize = ImVec2(375, 127);
            ImGui::SetNextWindowPos(ImVec2(15, displaySize.y - windowSize.y - 15), ImGuiCond_Always);
            ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
            ImGui::Begin("EQ Controls");

            ImGui::SliderFloat("Low (20-250 Hz)", &lowGain, 0.0f, 2.0f);
            ImGui::SliderFloat("Mid (250-2K Hz)", &midGain, 0.0f, 2.0f);
            ImGui::SliderFloat("High (2K-20K Hz)", &highGain, 0.0f, 2.0f);
            ImGui::Spacing();
            if (ImGui::Button("Reset EQ")) {
                lowGain = midGain = highGain = 1.0f;
            }
            ImGui::SameLine();
            if (ImGui::Button(showSpectrumAnalyser ? "Hide Spectrum" : "Show Spectrum")) {
                showSpectrumAnalyser = !showSpectrumAnalyser;
            }
            ImGui::End();
        }

        // Update the FFT processor w/ current EQ settings
        audioInput.getFFTProcessor().setEQGains(lowGain, midGain, highGain);

        // Spectrum Analyser window
        if (showSpectrumAnalyser && showUI) {
            ImGui::SetNextWindowSize(ImVec2(617, 185), ImGuiCond_Always);
            ImGui::Begin("Spectrum Analyser", &showSpectrumAnalyser);

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

            // Normalise Y-Axis
            float maxMagnitude = *std::max_element(smoothedMagnitudes.begin(), smoothedMagnitudes.end());
            float yMin = 0.0f;
            float yMax = (maxMagnitude > 0.0001f) ? maxMagnitude * 1.2f : 0.1f;

            // Plot frequencies
            ImGui::PlotLines("##", smoothedMagnitudes.data(), count, 0, nullptr, yMin, yMax, ImVec2(600, 150));
            ImGui::End();
        }
    }
}
