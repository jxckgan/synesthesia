#ifndef CONTROLS_H
#define CONTROLS_H

#include "audio_input.h"
#include "smoothing.h"
#include "imgui.h"
#include <vector>

struct UIState;

namespace Controls {
    void renderFrequencyInfoPanel(AudioInput& audioInput, float* clear_color);
    
    void renderVisualiserSettingsPanel(SpringSmoother& colourSmoother, 
                                     float& smoothingAmount,
                                     float sidebarWidth,
                                     float sidebarPadding,
                                     float labelWidth,
                                     float controlWidth,
                                     float buttonHeight);

    void renderEQControlsPanel(float& lowGain,
                              float& midGain, 
                              float& highGain,
                              bool& showSpectrumAnalyser,
                              float sidebarWidth,
                              float sidebarPadding,
                              float labelWidth,
                              float controlWidth,
                              float buttonHeight,
                              float contentWidth);

    void renderAdvancedSettingsPanel(UIState& state);
}

#endif