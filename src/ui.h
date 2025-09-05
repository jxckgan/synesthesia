#ifndef UI_H
#define UI_H

#include "audio_input.h"
#include "update.h"
#include "styling.h"
#include "device_manager.h"
#include "imgui.h"
#include <vector>

namespace UIConstants {
    static constexpr float DEFAULT_SAMPLE_RATE = 44100.0f;
    static constexpr float DEFAULT_SMOOTHING_SPEED = 0.6f;
    static constexpr float DEFAULT_GAMMA = 0.8f;
    static constexpr float COLOUR_SMOOTH_UPDATE_FACTOR = 1.2f;
    static constexpr float COLOUR_DECAY_RATE = 0.5f;
}

struct UIState {
    bool showUI = true;

    DeviceState deviceState;

    float colourSmoothingSpeed = 0.3f;

    float lowGain = 1.0f;
    float midGain = 1.0f;
    float highGain = 1.0f;
    bool showSpectrumAnalyser = true;

    std::vector<float> smoothedMagnitudes;
    float spectrumSmoothingFactor = 0.2f;

    StyleState styleState;

    UpdateState updateState;
    UpdateChecker updateChecker;
    
    // UI state
    bool showAdvancedSettings = false;
    bool showAPISettings = false;
    bool sidebarOnLeft = false; // false = right side (default), true = left side
    
    // API state
    bool apiServerEnabled = false;
    
    // Smoothing control
    bool smoothingEnabled = true;
};

void updateUI(AudioInput &audioInput,
              const std::vector<AudioInput::DeviceInfo>& devices,
              float* clear_color,
              ImGuiIO &io,
              UIState& state);

#endif