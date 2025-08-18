#ifndef SPECTRUM_ANALYSER_H
#define SPECTRUM_ANALYSER_H

#include <imgui.h>
#include <implot.h>
#include <vector>
#include "audio_input.h"

class SpectrumAnalyser {
public:
    static void drawSpectrumWindow(
        const std::vector<float>& smoothedMagnitudes,
        const std::vector<AudioInput::DeviceInfo>& devices,
        int selectedDeviceIndex,
        const ImVec2& displaySize,
        float sidebarWidth,
        bool sidebarOnLeft = false
    );

private:
    static constexpr float SPECTRUM_HEIGHT = 210.0f;
    static constexpr int LINE_COUNT = 500;
    static constexpr int SMOOTHING_WINDOW_SIZE = 7;
    
    static float getSampleRate(const std::vector<AudioInput::DeviceInfo>& devices, int selectedDeviceIndex);
    static void prepareSpectrumData(std::vector<float>& xData, std::vector<float>& yData, 
                                   const std::vector<float>& magnitudes, float sampleRate);
    static void smoothData(std::vector<float>& yData);
};

#endif
