#include "spectrum_analyser.h"
#include "fft_processor.h"
#include <portaudio.h>
#include <algorithm>
#include <cmath>

void SpectrumAnalyser::drawSpectrumWindow(
    const std::vector<float>& smoothedMagnitudes,
    const std::vector<AudioInput::DeviceInfo>& devices,
    int selectedDeviceIndex,
    const ImVec2& displaySize,
    float sidebarWidth
) {
    auto spectrumPos = ImVec2(0.0f, displaySize.y - SPECTRUM_HEIGHT);
    auto spectrumSize = ImVec2(displaySize.x - sidebarWidth, SPECTRUM_HEIGHT);

    ImGui::SetNextWindowPos(spectrumPos);
    ImGui::SetNextWindowSize(spectrumSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

    ImGui::Begin("##SpectrumAnalyser", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);

    std::vector<float> xData(LINE_COUNT);
    std::vector<float> yData(LINE_COUNT);
    float sampleRate = getSampleRate(devices, selectedDeviceIndex);
    
    prepareSpectrumData(xData, yData, smoothedMagnitudes, sampleRate);
    smoothData(yData);

    ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0, 0));
    ImPlot::PushStyleVar(ImPlotStyleVar_FitPadding, ImVec2(0, 0));
    ImPlot::PushStyleColor(ImPlotCol_PlotBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImPlot::PushStyleColor(ImPlotCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImPlot::PushStyleColor(ImPlotCol_PlotBorder, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImPlot::PushStyleColor(ImPlotCol_AxisGrid, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImPlot::PushStyleColor(ImPlotCol_AxisBgHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImPlot::PushStyleColor(ImPlotCol_AxisBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

    if (ImPlot::BeginPlot("##Spectrum", ImVec2(-1, -1), 
                          ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | 
                          ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect |
                          ImPlotFlags_NoMouseText | ImPlotFlags_NoFrame |
                          ImPlotFlags_NoInputs)) {
        
        ImPlot::SetupAxis(ImAxis_X1, nullptr, 
                         ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoTickMarks | 
                         ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoHighlight);
        ImPlot::SetupAxis(ImAxis_Y1, nullptr, 
                         ImPlotAxisFlags_NoTickLabels | ImPlotAxisFlags_NoTickMarks | 
                         ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoHighlight);
        ImPlot::SetupAxisLimits(ImAxis_X1, FFTProcessor::MIN_FREQ, FFTProcessor::MAX_FREQ);
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
        
        float plotYMax = 2.0f;
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, plotYMax);
        
        ImPlot::SetNextFillStyle(ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
        ImPlot::PlotShaded("##Fill", xData.data(), yData.data(), LINE_COUNT, 0.0);
        
        ImPlot::SetNextLineStyle(ImVec4(1.0f, 1.0f, 1.0f, 0.9f), 1.5f);
        ImPlot::PlotLine("##Line", xData.data(), yData.data(), LINE_COUNT);
        
        ImPlot::EndPlot();
    }

    ImPlot::PopStyleColor(6);
    ImPlot::PopStyleVar(2);

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

float SpectrumAnalyser::getSampleRate(const std::vector<AudioInput::DeviceInfo>& devices, int selectedDeviceIndex) {
    float sampleRate = 44100.0f;
    if (selectedDeviceIndex >= 0 && static_cast<size_t>(selectedDeviceIndex) < devices.size()) {
        if (const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(devices[selectedDeviceIndex].paIndex)) {
            sampleRate = static_cast<float>(deviceInfo->defaultSampleRate);
        }
    }
    return sampleRate;
}

void SpectrumAnalyser::prepareSpectrumData(std::vector<float>& xData, std::vector<float>& yData,
                                           const std::vector<float>& magnitudes, float sampleRate) {
    const float binSize = sampleRate / FFTProcessor::FFT_SIZE;
    constexpr float minFreq = FFTProcessor::MIN_FREQ;
    constexpr float maxFreq = FFTProcessor::MAX_FREQ;
    const float logMinFreq = std::log10(minFreq);
    const float logMaxFreq = std::log10(maxFreq);
    const float logFreqRange = logMaxFreq - logMinFreq;

    if (!magnitudes.empty() && binSize > 0.0f && logFreqRange > 0.0f) {
        for (int i = 0; i < LINE_COUNT; ++i) {
            float logPosition = static_cast<float>(i) / (LINE_COUNT - 1);
            float currentLogFreq = logMinFreq + logPosition * logFreqRange;
            float freq = std::pow(10.0f, currentLogFreq);
            
            xData[i] = freq;

            float binIndex = freq / binSize;
            int binIndexFloor = static_cast<int>(binIndex);
            float t = binIndex - binIndexFloor;

            int idx0 = std::min(std::max(binIndexFloor, 0),
                               static_cast<int>(magnitudes.size()) - 1);
            int idx1 = std::min(idx0 + 1, static_cast<int>(magnitudes.size()) - 1);

            float magnitude = magnitudes[idx0] * (1.0f - t) + magnitudes[idx1] * t;
            
            float normalizedMagnitude = magnitude;
            
            if (normalizedMagnitude > 0.001f) {
                normalizedMagnitude = std::log10(1.0f + normalizedMagnitude * 9.0f);
            }
            
            yData[i] = std::clamp(normalizedMagnitude, 0.0f, 1.0f);
        }
    } else {
        for (int i = 0; i < LINE_COUNT; ++i) {
            float logPosition = static_cast<float>(i) / (LINE_COUNT - 1);
            float currentLogFreq = logMinFreq + logPosition * logFreqRange;
            xData[i] = std::pow(10.0f, currentLogFreq);
            yData[i] = 0.0f;
        }
    }
}

void SpectrumAnalyser::smoothData(std::vector<float>& yData) {
    std::vector<float> smoothedData(LINE_COUNT);
    for (int i = 0; i < LINE_COUNT; ++i) {
        constexpr int smoothingWindow = SMOOTHING_WINDOW_SIZE;
        float sum = 0.0f;
        int count = 0;
        int halfWindow = smoothingWindow / 2;
        
        for (int j = std::max(0, i - halfWindow);
             j <= std::min(LINE_COUNT - 1, i + halfWindow); ++j) {
            sum += yData[j];
            count++;
        }
        smoothedData[i] = count > 0 ? sum / count : yData[i];
    }
    
    yData = std::move(smoothedData);
}
