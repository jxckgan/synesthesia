#include "spectrum_analyser.h"
#include "fft_processor.h"
#include <portaudio.h>
#include <algorithm>
#include <cmath>

std::vector<float> SpectrumAnalyser::previousFrameData;
std::vector<float> SpectrumAnalyser::smoothingBuffer1;
std::vector<float> SpectrumAnalyser::smoothingBuffer2;
std::vector<float> SpectrumAnalyser::gaussianWeights;
std::vector<float> SpectrumAnalyser::cachedFrequencies;
float SpectrumAnalyser::lastCachedSampleRate = 0.0f;
bool SpectrumAnalyser::buffersInitialised = false;

void SpectrumAnalyser::drawSpectrumWindow(
    const std::vector<float>& smoothedMagnitudes,
    const std::vector<AudioInput::DeviceInfo>& devices,
    int selectedDeviceIndex,
    const ImVec2& displaySize,
    float sidebarWidth,
    bool sidebarOnLeft
) {
    float spectrumX = sidebarOnLeft ? sidebarWidth : 0.0f;
    auto spectrumPos = ImVec2(spectrumX, displaySize.y - SPECTRUM_HEIGHT);
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
    applyTemporalSmoothing(yData);
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
        ImPlot::SetupAxisLimits(ImAxis_X1, static_cast<double>(FFTProcessor::MIN_FREQ), static_cast<double>(FFTProcessor::MAX_FREQ));
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
        
        float plotYMax = 2.0f;
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, static_cast<double>(plotYMax));
        
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
        if (const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(devices[static_cast<size_t>(selectedDeviceIndex)].paIndex)) {
            sampleRate = static_cast<float>(deviceInfo->defaultSampleRate);
        }
    }
    return sampleRate;
}

void SpectrumAnalyser::prepareSpectrumData(std::vector<float>& xData, std::vector<float>& yData,
                                           const std::vector<float>& magnitudes, float sampleRate) {
    if (!buffersInitialised) {
        initialiseBuffers();
    }
    
    const float binSize = sampleRate / FFTProcessor::FFT_SIZE;
    constexpr float minFreq = FFTProcessor::MIN_FREQ;
    constexpr float maxFreq = FFTProcessor::MAX_FREQ;
    const float logMinFreq = std::log10(minFreq);
    const float logMaxFreq = std::log10(maxFreq);
    const float logFreqRange = logMaxFreq - logMinFreq;

    // Cache frequency calculations if sample rate has changed
    if (sampleRate != lastCachedSampleRate) {
        lastCachedSampleRate = sampleRate;
        for (size_t i = 0; i < LINE_COUNT; ++i) {
            float logPosition = static_cast<float>(i) / (LINE_COUNT - 1);
            float currentLogFreq = logMinFreq + logPosition * logFreqRange;
            cachedFrequencies[i] = std::pow(10.0f, currentLogFreq);
        }
    }

    if (!magnitudes.empty() && binSize > 0.0f && logFreqRange > 0.0f) {
        for (size_t i = 0; i < LINE_COUNT; ++i) {
            const float freq = cachedFrequencies[i];
            xData[i] = freq;

            float binIndex = freq / binSize;
            int binIndexFloor = static_cast<int>(binIndex);
            float t = binIndex - binIndexFloor;

            int idx0 = std::min(std::max(binIndexFloor, 0),
                               static_cast<int>(magnitudes.size()) - 1);
            int idx1 = std::min(idx0 + 1, static_cast<int>(magnitudes.size()) - 1);

            // Use cubic interpolation for smoother curves between FFT bins
            int idx_m1 = std::max(idx0 - 1, 0);
            int idx2 = std::min(idx1 + 1, static_cast<int>(magnitudes.size()) - 1);
            
            float magnitude = cubicInterpolate(
                magnitudes[static_cast<size_t>(idx_m1)], magnitudes[static_cast<size_t>(idx0)],
                magnitudes[static_cast<size_t>(idx1)], magnitudes[static_cast<size_t>(idx2)], t);
            
            float normalisedMagnitude = magnitude;
            
            if (normalisedMagnitude > 0.001f) {
                normalisedMagnitude = std::log10(1.0f + normalisedMagnitude * 9.0f);
            }
            
            yData[i] = std::clamp(normalisedMagnitude, 0.0f, 1.0f);
        }
    } else {
        for (size_t i = 0; i < LINE_COUNT; ++i) {
            xData[i] = cachedFrequencies[i];
            yData[i] = 0.0f;
        }
    }
}

void SpectrumAnalyser::applyTemporalSmoothing(std::vector<float>& yData) {
    if (previousFrameData.size() != static_cast<size_t>(LINE_COUNT)) {
        previousFrameData.resize(LINE_COUNT, 0.0f);
    }
    
    for (size_t i = 0; i < LINE_COUNT; ++i) {
        yData[i] = TEMPORAL_SMOOTHING_FACTOR * previousFrameData[i] +
                   (1.0f - TEMPORAL_SMOOTHING_FACTOR) * yData[i];
        previousFrameData[i] = yData[i];
    }
}

void SpectrumAnalyser::smoothData(std::vector<float>& yData) {
    if (!buffersInitialised) {
        initialiseBuffers();
    }
    
    applyGaussianSmoothing(yData);
}

void SpectrumAnalyser::applyGaussianSmoothing(std::vector<float>& yData) {
    // Reuse pre-allocated buffer instead of creating new vector
    std::fill(smoothingBuffer1.begin(), smoothingBuffer1.end(), 0.0f);
    
    for (size_t i = 0; i < LINE_COUNT; ++i) {
        int windowSize = getFrequencyDependentWindowSize(static_cast<int>(i));
        int halfWindow = windowSize / 2;
        
        float weightedSum = 0.0f;
        float totalWeight = 0.0f;
        
        for (int j = std::max(0, static_cast<int>(i) - halfWindow);
             j <= std::min(LINE_COUNT - 1, static_cast<int>(i) + halfWindow); ++j) {
            // Lighter smoothing - reduced sigma for maintaining definition
            float weight = gaussianWeight(std::abs(static_cast<int>(i) - j), GAUSSIAN_SIGMA * 0.7f);
            weightedSum += yData[static_cast<size_t>(j)] * weight;
            totalWeight += weight;
        }
        
        smoothingBuffer1[i] = totalWeight > 0.0f ? weightedSum / totalWeight : yData[i];
    }
    
    // Swap data back to yData (no allocation)
    yData.swap(smoothingBuffer1);
}

int SpectrumAnalyser::getFrequencyDependentWindowSize(int index) {
    float normalisedIndex = static_cast<float>(index) / (LINE_COUNT - 1);
    // Smaller base window for better definition, with more moderate scaling
    return BASE_SMOOTHING_WINDOW_SIZE + static_cast<int>(normalisedIndex * 3.0f);
}

float SpectrumAnalyser::gaussianWeight(int distance, float sigma) {
    if (distance == 0) return 1.0f;
    
    // Use precomputed weights for standard sigma, compute for others
    if (std::abs(sigma - GAUSSIAN_SIGMA) < 0.01f && distance < static_cast<int>(gaussianWeights.size())) {
        return gaussianWeights[static_cast<size_t>(distance)];
    }
    
    // Fallback to computation for non-standard sigma values
    float d = static_cast<float>(distance);
    return std::exp(-(d * d) / (2.0f * sigma * sigma));
}

float SpectrumAnalyser::cubicInterpolate(float y0, float y1, float y2, float y3, float t) {
    // Catmull-Rom cubic interpolation for smooth curves
    float a0, a1, a2, a3;
    a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    a2 = -0.5f * y0 + 0.5f * y2;
    a3 = y1;
    
    return a0 * t * t * t + a1 * t * t + a2 * t + a3;
}

float SpectrumAnalyser::calculateLocalVariance(const std::vector<float>& yData, int centre, int windowSize) {
    int halfWindow = windowSize / 2;
    float sum = 0.0f;
    float sumSquares = 0.0f;
    int count = 0;
    
    for (int j = std::max(0, centre - halfWindow);
         j <= std::min(LINE_COUNT - 1, centre + halfWindow); ++j) {
        sum += yData[static_cast<size_t>(j)];
        sumSquares += yData[static_cast<size_t>(j)] * yData[static_cast<size_t>(j)];
        count++;
    }
    
    if (count <= 1) return 0.0f;
    
    float mean = sum / count;
    float variance = (sumSquares / count) - (mean * mean);
    return std::max(variance, 0.0f);
}

void SpectrumAnalyser::initialiseBuffers() {
    smoothingBuffer1.resize(LINE_COUNT);
    smoothingBuffer2.resize(LINE_COUNT);
    cachedFrequencies.resize(LINE_COUNT);
    precomputeGaussianWeights();
    buffersInitialised = true;
}

void SpectrumAnalyser::precomputeGaussianWeights() {
    // Pre-compute Gaussian weights up to a reasonable distance
    constexpr int maxDistance = 20;
    gaussianWeights.resize(maxDistance + 1);
    
    for (int d = 0; d <= maxDistance; ++d) {
        if (d == 0) {
            gaussianWeights[static_cast<size_t>(d)] = 1.0f;
        } else {
            float distance = static_cast<float>(d);
            gaussianWeights[static_cast<size_t>(d)] = std::exp(-(distance * distance) / (2.0f * GAUSSIAN_SIGMA * GAUSSIAN_SIGMA));
        }
    }
}
