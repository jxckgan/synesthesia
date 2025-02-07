#ifndef UI_H
#define UI_H

#include "audio_input.h"
#include "imgui.h"
#include <vector>

// updateUI(): All UI and application logic that is not directly
// related to window creation and rendering backends
void updateUI(AudioInput &audioInput, 
              const std::vector<AudioInput::DeviceInfo>& devices, 
              float* clear_color, 
              ImGuiIO &io);

#endif
