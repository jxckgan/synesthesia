#ifndef SPECTRUM_ANALYSER_H
#define SPECTRUM_ANALYSER_H

#include <imgui.h>
#include <vector>
#include "audio_input.h"

class SpectrumAnalyser {
public:
    static void drawSpectrumWindow(
        const std::vector<float>& smoothedMagnitudes,
        const std::vector<AudioInput::DeviceInfo>& devices,
        int selectedDeviceIndex,
        const ImVec2& displaySize,
        float sidebarWidth
    );

private:
    struct SpectrumRenderContext {
        const std::vector<float>& magnitudes;
        float sampleRate;
        ImVec2 canvasPos;
        ImVec2 canvasSize;
    };

    static constexpr float SPECTRUM_HEIGHT = 210.0f;
    static constexpr int LINE_COUNT = 500;
    static constexpr int SMOOTHING_WINDOW_SIZE = 7;
    static constexpr float SPECTRUM_LINE_THICKNESS = 1.5f;
    
    static float getSampleRate(const std::vector<AudioInput::DeviceInfo>& devices, int selectedDeviceIndex);
    static void drawBackground(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize);
    static void calculateSpectrumPoints(std::vector<ImVec2>& points, 
                                       const SpectrumRenderContext& context);
    static void smoothPoints(std::vector<ImVec2>& points);
    static void drawSpectrum(ImDrawList* drawList, const std::vector<ImVec2>& points, const ImVec2& canvasPos, const ImVec2& canvasSize);
};

#endif
