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
    static constexpr int LINE_COUNT = 800;
    static constexpr int BASE_SMOOTHING_WINDOW_SIZE = 5;
    static constexpr float TEMPORAL_SMOOTHING_FACTOR = 0.65f;
    static constexpr float GAUSSIAN_SIGMA = 2.0f;
    static constexpr float CUBIC_TENSION = 0.5f;
    static constexpr float ADAPTIVE_SMOOTHING_MIN = 0.8f;
    static constexpr float ADAPTIVE_SMOOTHING_MAX = 2.5f;
    
    static std::vector<float> previousFrameData;
    static std::vector<float> smoothingBuffer1;
    static std::vector<float> smoothingBuffer2;
    static std::vector<float> gaussianWeights;
    static bool buffersInitialized;
    
    static float getSampleRate(const std::vector<AudioInput::DeviceInfo>& devices, int selectedDeviceIndex);
    static void prepareSpectrumData(std::vector<float>& xData, std::vector<float>& yData, 
                                   const std::vector<float>& magnitudes, float sampleRate);
    static void smoothData(std::vector<float>& yData);
    static void applyTemporalSmoothing(std::vector<float>& yData);
    static void applyGaussianSmoothing(std::vector<float>& yData);
    static void applyAdaptiveSmoothing(std::vector<float>& yData);
    static float cubicInterpolate(float y0, float y1, float y2, float y3, float t);
    static float catmullRomSpline(const std::vector<float>& points, int index, float t);
    static int getFrequencyDependentWindowSize(int index);
    static float gaussianWeight(int distance, float sigma);
    static void initializeBuffers();
    static void precomputeGaussianWeights();
    static float calculateLocalVariance(const std::vector<float>& yData, int center, int windowSize);
};

#endif
