#include "ui.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "audio_input.h"
#include "controls.h"
#include "colour_mapper.h"
#include "fft_processor.h"
#include "smoothing.h"
#include "styling.h"
#include "spectrum_analyser.h"
#include "device_manager.h"
#ifdef ENABLE_API_SERVER
#include "api/synesthesia_api_integration.h"
#endif

void initialiseApp(UIState& state) {
    if (!state.updateState.hasCheckedThisSession) {
        state.updateChecker.checkForUpdates("jxckgan", "synesthesia");
        state.updateState.hasCheckedThisSession = true;
    }
    
#ifdef ENABLE_API_SERVER
    auto& api = Synesthesia::SynesthesiaAPIIntegration::getInstance();
    if (api.isServerRunning() && !state.apiServerEnabled) {
        api.stopServer();
    }
#endif
}

void updateUI(AudioInput& audioInput, const std::vector<AudioInput::DeviceInfo>& devices,
			  float* clear_color, ImGuiIO& io, UIState& state) {
	initialiseApp(state);
	state.updateChecker.update(state.updateState);

	if (ImGui::IsKeyPressed(ImGuiKey_H)) {
		state.showUI = !state.showUI;
	}

	DeviceManager::populateDeviceNames(state.deviceState, devices);

	float deltaTime = io.DeltaTime;
	static SpringSmoother colourSmoother(8.0f, 1.0f, 0.3f);
	static float smoothingAmount = 0.60f;
	colourSmoother.setSmoothingAmount(smoothingAmount);

	constexpr float SIDEBAR_WIDTH = 280.0f;
	constexpr float SIDEBAR_PADDING = 16.0f;
	constexpr float contentWidth = SIDEBAR_WIDTH - SIDEBAR_PADDING * 2;

	if (state.showUI && state.updateChecker.shouldShowUpdateBanner(state.updateState)) {
        state.updateChecker.drawUpdateBanner(state.updateState, io.DisplaySize.x, SIDEBAR_WIDTH);
    }
    
	if (state.deviceState.selectedDeviceIndex >= 0) {
		float whiteMix = 0.0f;
		float gamma = 0.8f;
		
		// Set EQ gains before getting peaks to ensure they're applied
		audioInput.getFFTProcessor().setEQGains(state.lowGain, state.midGain, state.highGain);
		
		// Get peaks only once per frame
		auto peaks = audioInput.getFrequencyPeaks();
		std::vector<float> freqs, mags;
		freqs.reserve(peaks.size());
		mags.reserve(peaks.size());
		for (const auto& peak : peaks) {
			freqs.push_back(peak.frequency);
			mags.push_back(peak.magnitude);
		}

		auto colourResult = ColourMapper::frequenciesToColour(freqs, mags, {}, UIConstants::DEFAULT_SAMPLE_RATE, gamma);

		colourResult.r = colourResult.r * (1.0f - whiteMix) + whiteMix;
		colourResult.g = colourResult.g * (1.0f - whiteMix) + whiteMix;
		colourResult.b = colourResult.b * (1.0f - whiteMix) + whiteMix;

		colourResult.r = std::clamp(colourResult.r, 0.0f, 1.0f);
		colourResult.g = std::clamp(colourResult.g, 0.0f, 1.0f);
		colourResult.b = std::clamp(colourResult.b, 0.0f, 1.0f);

		bool currentValid = std::isfinite(clear_color[0]) && std::isfinite(clear_color[1]) &&
							std::isfinite(clear_color[2]);

		bool newValid = std::isfinite(colourResult.r) && std::isfinite(colourResult.g) &&
						std::isfinite(colourResult.b);

		if (!currentValid) {
			clear_color[0] = clear_color[1] = clear_color[2] = 0.1f;
		}

		if (newValid) {
			if (state.smoothingEnabled) {
				colourSmoother.setTargetColour(colourResult.r, colourResult.g, colourResult.b);
				colourSmoother.update(deltaTime * UIConstants::COLOUR_SMOOTH_UPDATE_FACTOR);
				colourSmoother.getCurrentColour(clear_color[0], clear_color[1], clear_color[2]);
			} else {
				clear_color[0] = colourResult.r;
				clear_color[1] = colourResult.g;
				clear_color[2] = colourResult.b;
			}
		}
		
#ifdef ENABLE_API_SERVER
		auto& api = Synesthesia::SynesthesiaAPIIntegration::getInstance();
		api.updateFinalColour(clear_color[0], clear_color[1], clear_color[2],
		                     freqs, mags, static_cast<uint32_t>(UIConstants::DEFAULT_SAMPLE_RATE), 
		                     1024);
#endif

		const auto& magnitudes = audioInput.getFFTProcessor().getMagnitudesBuffer();
		if (state.smoothedMagnitudes.size() != magnitudes.size()) {
			state.smoothedMagnitudes.assign(magnitudes.size(), 0.0f);
		}
		const size_t count = magnitudes.size();

		for (size_t i = 0; i < count; ++i) {
			state.smoothedMagnitudes[i] =
				state.spectrumSmoothingFactor * magnitudes[i] +
				(1.0f - state.spectrumSmoothingFactor) * state.smoothedMagnitudes[i];
		}
	} else {
		float decayFactor = std::min(1.0f, deltaTime * UIConstants::COLOUR_DECAY_RATE);
		clear_color[0] = std::clamp(clear_color[0] * (1.0f - decayFactor), 0.0f, 1.0f);
		clear_color[1] = std::clamp(clear_color[1] * (1.0f - decayFactor), 0.0f, 1.0f);
		clear_color[2] = std::clamp(clear_color[2] * (1.0f - decayFactor), 0.0f, 1.0f);
	}

	if (state.showUI) {
		ImGuiStyle& style = ImGui::GetStyle();
		UIStyler::applyCustomStyle(style, state.styleState);

		ImVec2 displaySize = io.DisplaySize;

		float sidebarX = state.sidebarOnLeft ? 0 : (displaySize.x - SIDEBAR_WIDTH);
		ImGui::SetNextWindowPos(ImVec2(sidebarX, 0));
		ImGui::SetNextWindowSize(ImVec2(SIDEBAR_WIDTH, displaySize.y));

		ImGui::Begin(
			"Sidebar", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
		
		ImGui::SetCursorPosY(20);
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
		ImGui::SetCursorPosX((SIDEBAR_WIDTH - ImGui::CalcTextSize("Synesthesia").x) * 0.5f);
		ImGui::Text("Synesthesia");
		ImGui::PopFont();
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		DeviceManager::renderDeviceSelection(state.deviceState, audioInput, devices);

		if (state.deviceState.selectedDeviceIndex >= 0 && !state.deviceState.streamError) {
			constexpr float BUTTON_HEIGHT = 25.0f;
			constexpr float CONTROL_WIDTH = 130.0f;
			constexpr float LABEL_WIDTH = 90.0f;
			
			DeviceManager::renderChannelSelection(state.deviceState, audioInput, devices);

			Controls::renderFrequencyInfoPanel(audioInput, clear_color);
			
			Controls::renderVisualiserSettingsPanel(
				colourSmoother, 
				smoothingAmount,
				SIDEBAR_WIDTH,
				SIDEBAR_PADDING,
				LABEL_WIDTH,
				CONTROL_WIDTH,
				BUTTON_HEIGHT
			);
			
			Controls::renderEQControlsPanel(
				state.lowGain,
				state.midGain,
				state.highGain,
				state.showSpectrumAnalyser,
				SIDEBAR_WIDTH,
				SIDEBAR_PADDING,
				LABEL_WIDTH,
				CONTROL_WIDTH,
				BUTTON_HEIGHT,
				contentWidth
			);
			
			Controls::renderAdvancedSettingsPanel(state);
		}

		float bottomTextHeight = ImGui::GetTextLineHeightWithSpacing() + 12;
		float currentCursorY = ImGui::GetCursorPosY();
		if (float spaceToBottom = ImGui::GetWindowHeight() - currentCursorY - bottomTextHeight -
										  style.WindowPadding.y;
					spaceToBottom > 0.0f) {
					ImGui::Dummy(ImVec2(0.0f, spaceToBottom));
		}

		ImGui::Separator();
		float textWidth = ImGui::CalcTextSize("Press H to hide/show interface").x;
		ImGui::SetCursorPosX((SIDEBAR_WIDTH - textWidth) * 0.5f);
		ImGui::TextDisabled("Press H to hide/show interface");

		ImGui::End();

		if (state.deviceState.selectedDeviceIndex >= 0 && !state.deviceState.streamError && state.showSpectrumAnalyser) {
			SpectrumAnalyser::drawSpectrumWindow(
				state.smoothedMagnitudes,
				devices,
				state.deviceState.selectedDeviceIndex,
				displaySize,
				SIDEBAR_WIDTH,
				state.sidebarOnLeft
			);
		}
	}
	UIStyler::restoreOriginalStyle(ImGui::GetStyle(), state.styleState);
}