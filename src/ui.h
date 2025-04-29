#ifndef UI_H
#define UI_H

#include "audio_input.h"
#include "imgui.h"
#include <vector>

struct UIState {
    bool showUI = true;
    int selectedDeviceIndex = -1;
    bool streamError = false;
    std::string streamErrorMessage;

    float colourSmoothingSpeed = 0.3f;
    bool dynamicGammaEnabled = false;
    float previousLoudness = 0.0f;
    float transientIntensity = 0.0f;
    float smoothedLoudness = 0.0f;

    float lowGain = 1.0f;
    float midGain = 1.0f;
    float highGain = 1.0f;
    bool showSpectrumAnalyser = true;

    std::vector<float> smoothedMagnitudes;
    float spectrumSmoothingFactor = 0.2f;

    ImGuiStyle originalStyle;
    bool styleApplied = false;

    std::vector<const char*> deviceNames;
    bool deviceNamesPopulated = false;
};

void updateUI(AudioInput &audioInput,
              const std::vector<AudioInput::DeviceInfo>& devices,
              float* clear_color,
              ImGuiIO &io,
              UIState& state);

#endif