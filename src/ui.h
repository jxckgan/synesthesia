#ifndef UI_H
#define UI_H

#include "audio_input.h"
#include "update.h"
#include "styling.h"
#include "device_manager.h"
#include "imgui.h"
#include <vector>

struct UIState {
    bool showUI = true;

    DeviceState deviceState;

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

    StyleState styleState;

    UpdateState updateState;
    UpdateChecker updateChecker;
};

void updateUI(AudioInput &audioInput,
              const std::vector<AudioInput::DeviceInfo>& devices,
              float* clear_color,
              ImGuiIO &io,
              UIState& state);

#endif