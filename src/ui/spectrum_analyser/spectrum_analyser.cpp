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

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw background
    drawBackground(drawList, canvasPos, canvasSize);

    // Calculate and draw spectrum
    std::vector<ImVec2> points(LINE_COUNT);
    float sampleRate = getSampleRate(devices, selectedDeviceIndex);
    
    calculateSpectrumPoints(points, smoothedMagnitudes, sampleRate, canvasPos, canvasSize);
    smoothPoints(points);
    drawSpectrum(drawList, points, canvasPos, canvasSize);

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

void SpectrumAnalyser::drawBackground(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& canvasSize) {
    // Draw subtle background gradient
    drawList->AddRectFilledMultiColor(
        ImVec2(canvasPos.x, canvasPos.y),
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.0f)),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.0f)),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.4f)),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.4f)));
}

void SpectrumAnalyser::calculateSpectrumPoints(
    std::vector<ImVec2>& points,
    const std::vector<float>& smoothedMagnitudes,
    float sampleRate,
    const ImVec2& canvasPos,
    const ImVec2& canvasSize
) {
    // Normalise Y-Axis for spectrum dynamically
    float maxMagnitude = 0.0f;
    if (!smoothedMagnitudes.empty()) {
        maxMagnitude = *std::ranges::max_element(smoothedMagnitudes);
    }
    float yMax = maxMagnitude > 0.0001f ? maxMagnitude * 1.1f : 0.1f;

    const float binSize = sampleRate / FFTProcessor::FFT_SIZE;
    constexpr float minFreq = FFTProcessor::MIN_FREQ;
    constexpr float maxFreq = FFTProcessor::MAX_FREQ;
    const float logMinFreq = std::log10(minFreq);
    const float logMaxFreq = std::log10(maxFreq);

    // Fill point array with spectrum data using logarithmic scale
    if (const float logFreqRange = logMaxFreq - logMinFreq;
        !smoothedMagnitudes.empty() && binSize > 0.0f && logFreqRange > 0.0f) {
        
        for (int i = 0; i < LINE_COUNT; ++i) {
            // Calculate frequency for this point using logarithmic interpolation
            float logPosition = static_cast<float>(i) / (LINE_COUNT - 1);  // 0.0 to 1.0
            float currentLogFreq = logMinFreq + logPosition * logFreqRange;
            float freq = std::pow(10.0f, currentLogFreq);

            // Calculate corresponding bin index
            float binIndex = freq / binSize;

            // Ensure bin index is in valid range and interpolate
            int binIndexFloor = static_cast<int>(binIndex);
            float t = binIndex - binIndexFloor;  // Interpolation factor

            // Clamp indices safely
            int idx0 = std::min(std::max(binIndexFloor, 0),
                               static_cast<int>(smoothedMagnitudes.size()) - 1);
            int idx1 = std::min(idx0 + 1, static_cast<int>(smoothedMagnitudes.size()) - 1);

            float magnitude = smoothedMagnitudes[idx0] * (1.0f - t) +
                             smoothedMagnitudes[idx1] * t;

            // Calculate x position
            float xPos = canvasPos.x + static_cast<float>(i) / (LINE_COUNT - 1) * canvasSize.x;

            // Calculate y position
            float height = magnitude / yMax * canvasSize.y * 0.7f;
            height = std::min(height, canvasSize.y * 0.75f);
            float yPos = canvasPos.y + canvasSize.y - height;

            points[i] = ImVec2(xPos, yPos);
        }
    }
}

void SpectrumAnalyser::smoothPoints(std::vector<ImVec2>& points) {
    // Apply smoothing to the calculated points
    std::vector<float> smoothedYPositions(LINE_COUNT);
    for (int i = 0; i < LINE_COUNT; ++i) {
        constexpr int smoothingWindow = 7;
        float sum = 0.0f;
        int count = 0;
        int halfWindow = smoothingWindow / 2;
        for (int j = std::max(0, i - halfWindow);
             j <= std::min(LINE_COUNT - 1, i + halfWindow); ++j) {
            sum += points[j].y;
            count++;
        }
        smoothedYPositions[i] = count > 0 ? sum / count : points[i].y;  // Avoid division by zero
    }

    for (int i = 0; i < LINE_COUNT; ++i) {
        points[i].y = smoothedYPositions[i];
    }
}

void SpectrumAnalyser::drawSpectrum(ImDrawList* drawList, const std::vector<ImVec2>& points, const ImVec2& canvasPos, const ImVec2& canvasSize) {
    // Define colours for drawing
    ImU32 fillColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
    ImU32 lineColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.9f));
    constexpr float lineThickness = 1.5f;

    // Fill area below the curve using quads
    for (int i = 0; i < LINE_COUNT - 1; ++i) {
        ImVec2 quad[4] = {points[i], points[i + 1],
                         ImVec2(points[i + 1].x, canvasPos.y + canvasSize.y),
                         ImVec2(points[i].x, canvasPos.y + canvasSize.y)};
        drawList->AddConvexPolyFilled(quad, 4, fillColor);
    }

    // Draw the spectrum line on top
    drawList->AddPolyline(points.data(), LINE_COUNT, lineColor, false, lineThickness);
}
