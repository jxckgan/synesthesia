#include "ui.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "audio_input.h"
#include "colour_mapper.h"
#include "fft_processor.h"
#include "smoothing.h"
#include "styling.h"
#include "spectrum_analyser.h"
#include "device_manager.h"

void initialiseApp(UIState& state) {
    if (!state.updateState.hasCheckedThisSession) {
        state.updateChecker.checkForUpdates("jxckgan", "synesthesia");
        state.updateState.hasCheckedThisSession = true;
    }
}

void updateUI(AudioInput& audioInput, const std::vector<AudioInput::DeviceInfo>& devices,
			  float* clear_color, ImGuiIO& io, UIState& state) {
	initialiseApp(state);
	state.updateChecker.update(state.updateState);

	if (state.deviceState.selectedDeviceIndex >= 0) {
        state.updateState.updatePromptVisible = false;
        state.updateState.shouldShowBanner = false;
    }

	// Toggle UI On/Off with 'H' key
	if (ImGui::IsKeyPressed(ImGuiKey_H)) {
		state.showUI = !state.showUI;
	}

	// Populate device names list
	DeviceManager::populateDeviceNames(state.deviceState, devices);

	// Colour smoothing
	float deltaTime = io.DeltaTime;
	static SpringSmoother colourSmoother(8.0f, 1.0f, 0.3f);
	static float smoothingAmount = 0.60f;
	colourSmoother.setSmoothingAmount(smoothingAmount);

	// Define UI layout measurements
	constexpr float SIDEBAR_WIDTH = 280.0f;
	constexpr float SIDEBAR_PADDING = 16.0f;
	constexpr float contentWidth = SIDEBAR_WIDTH - SIDEBAR_PADDING * 2;

	if (state.showUI && state.updateChecker.shouldShowUpdateBanner(state.updateState)) {
        state.updateChecker.drawUpdateBanner(state.updateState, io.DisplaySize.x, SIDEBAR_WIDTH);
    }
    
	// Process audio data and updates visuals when a device is selected
	if (state.deviceState.selectedDeviceIndex >= 0) {
		float whiteMix = 0.0f;
		float gamma = 0.8f;
		auto peaks = audioInput.getFrequencyPeaks();
		std::vector<float> freqs, mags;
		freqs.reserve(peaks.size());
		mags.reserve(peaks.size());
		for (const auto& peak : peaks) {
			freqs.push_back(peak.frequency);
			mags.push_back(peak.magnitude);
		}

		auto colourResult = ColourMapper::frequenciesToColour(freqs, mags, {}, 44100.0f, gamma);

		// Blend colour with white based on transients
		colourResult.r = colourResult.r * (1.0f - whiteMix) + whiteMix;
		colourResult.g = colourResult.g * (1.0f - whiteMix) + whiteMix;
		colourResult.b = colourResult.b * (1.0f - whiteMix) + whiteMix;

		// Ensure valid colour values after mixing
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

		// Only apply new colours if they're valid
		if (newValid) {
			colourSmoother.setTargetColour(colourResult.r, colourResult.g, colourResult.b);
			colourSmoother.update(deltaTime * 1.2f);
			colourSmoother.getCurrentColour(clear_color[0], clear_color[1], clear_color[2]);
		}

		// Update the FFT processor w/ current EQ settings
		audioInput.getFFTProcessor().setEQGains(state.lowGain, state.midGain, state.highGain);

		// Spectrum Analyser data preparation
		const auto& magnitudes = audioInput.getFFTProcessor().getMagnitudesBuffer();
		if (state.smoothedMagnitudes.size() != magnitudes.size()) {
			// Ensure vector has the correct size
			state.smoothedMagnitudes.assign(magnitudes.size(), 0.0f);
		}
		const size_t count = magnitudes.size();

		// Apply smoothing using EMA
		for (size_t i = 0; i < count; ++i) {
			state.smoothedMagnitudes[i] =
				state.spectrumSmoothingFactor * magnitudes[i] +
				(1.0f - state.spectrumSmoothingFactor) * state.smoothedMagnitudes[i];
		}
	} else {
		float decayFactor = std::min(1.0f, deltaTime * 0.5f);
		clear_color[0] = std::clamp(clear_color[0] * (1.0f - decayFactor), 0.0f, 1.0f);
		clear_color[1] = std::clamp(clear_color[1] * (1.0f - decayFactor), 0.0f, 1.0f);
		clear_color[2] = std::clamp(clear_color[2] * (1.0f - decayFactor), 0.0f, 1.0f);
	}

	// Draw UI elements if enabled
	if (state.showUI) {
		ImGuiStyle& style = ImGui::GetStyle();
		UIStyler::applyCustomStyle(style, state.styleState);

		ImVec2 displaySize = io.DisplaySize;

		// Set up sidebar window
		ImGui::SetNextWindowPos(ImVec2(displaySize.x - SIDEBAR_WIDTH, 0));
		ImGui::SetNextWindowSize(ImVec2(SIDEBAR_WIDTH, displaySize.y));

		ImGui::Begin(
			"Sidebar", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

		// App title
		ImGui::SetCursorPosY(20);
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
		ImGui::SetCursorPosX((SIDEBAR_WIDTH - ImGui::CalcTextSize("Synesthesia").x) * 0.5f);
		ImGui::Text("Synesthesia");
		ImGui::PopFont();
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		DeviceManager::renderDeviceSelection(state.deviceState, audioInput, devices);

		// Only shows controls if a device is successfully selected
		if (state.deviceState.selectedDeviceIndex >= 0 && !state.deviceState.streamError) {
			constexpr float BUTTON_HEIGHT = 25.0f;
			constexpr float CONTROL_WIDTH = 150.0f;
			constexpr float LABEL_WIDTH = 90.0f;
			
			// Channel Selection (now handled by DeviceManager)
			DeviceManager::renderChannelSelection(state.deviceState, audioInput, devices);

			if (ImGui::CollapsingHeader("FREQUENCY INFO", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Indent(10);
				auto peaks = audioInput.getFrequencyPeaks();
				auto currentColourResult = ColourMapper::frequenciesToColour(
					[&peaks] {
						std::vector<float> f;
						f.reserve(peaks.size());
						for (const auto& p : peaks)
							f.push_back(p.frequency);
						return f;
					}(),
					[&peaks] {
						std::vector<float> m;
						m.reserve(peaks.size());
						for (const auto& p : peaks)
							m.push_back(p.magnitude);
						return m;
					}(),
					{}, 44100.0f, 0.8f);

				if (!peaks.empty()) {
					ImGui::Text("Dominant: %.1f Hz", peaks[0].frequency);
					ImGui::Text("Wavelength: %.1f nm", currentColourResult.dominantWavelength);
					ImGui::Text("Number of peaks detected: %d", static_cast<int>(peaks.size()));
				} else {
					ImGui::TextDisabled("No significant frequencies");
				}
				ImGui::Text("RGB: (%.2f, %.2f, %.2f)", clear_color[0], clear_color[1],
							clear_color[2]);
				ImGui::Unindent(10);
				ImGui::Spacing();
			}

			if (ImGui::CollapsingHeader("VISUALISER SETTINGS", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Indent(10);

				ImGui::AlignTextToFramePadding();
				ImGui::Text("Smoothing");
				ImGui::SameLine(SIDEBAR_PADDING + LABEL_WIDTH);
				ImGui::SetCursorPosX(SIDEBAR_WIDTH - SIDEBAR_PADDING - CONTROL_WIDTH);
				ImGui::SetNextItemWidth(CONTROL_WIDTH);
				if (ImGui::SliderFloat("##Smoothing", &smoothingAmount, 0.0f, 1.0f, "%.2f")) {
					colourSmoother.setSmoothingAmount(smoothingAmount);
				}

				ImGui::SetCursorPosX(SIDEBAR_PADDING);
				if (ImGui::Button("Reset Smoothing", ImVec2(130, BUTTON_HEIGHT))) {
					smoothingAmount = 0.6f;
					colourSmoother.setSmoothingAmount(smoothingAmount);
				}

				ImGui::Unindent(10);
				ImGui::Spacing();
			}

			if (ImGui::CollapsingHeader("EQ CONTROLS", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Indent(10);

				ImGui::AlignTextToFramePadding();
				ImGui::Text("Lows");
				ImGui::SameLine(SIDEBAR_PADDING + LABEL_WIDTH);
				ImGui::SetCursorPosX(SIDEBAR_WIDTH - SIDEBAR_PADDING - CONTROL_WIDTH);
				ImGui::SetNextItemWidth(CONTROL_WIDTH);
				ImGui::SliderFloat("##LowGain", &state.lowGain, 0.0f, 2.0f);

				ImGui::AlignTextToFramePadding();
				ImGui::Text("Mids");
				ImGui::SameLine(SIDEBAR_PADDING + LABEL_WIDTH);
				ImGui::SetCursorPosX(SIDEBAR_WIDTH - SIDEBAR_PADDING - CONTROL_WIDTH);
				ImGui::SetNextItemWidth(CONTROL_WIDTH);
				ImGui::SliderFloat("##MidGain", &state.midGain, 0.0f, 2.0f);

				ImGui::AlignTextToFramePadding();
				ImGui::Text("Highs");
				ImGui::SameLine(SIDEBAR_PADDING + LABEL_WIDTH);
				ImGui::SetCursorPosX(SIDEBAR_WIDTH - SIDEBAR_PADDING - CONTROL_WIDTH);
				ImGui::SetNextItemWidth(CONTROL_WIDTH);
				ImGui::SliderFloat("##HighGain", &state.highGain, 0.0f, 2.0f);

				float buttonWidth = (contentWidth - ImGui::GetStyle().ItemSpacing.x) / 2;
				ImGui::SetCursorPosX(SIDEBAR_PADDING);
				if (ImGui::Button("Reset EQ", ImVec2(buttonWidth, BUTTON_HEIGHT))) {
					state.lowGain = state.midGain = state.highGain = 1.0f;
				}
				ImGui::SameLine();
				ImGui::SetCursorPosX(SIDEBAR_PADDING + buttonWidth +
									 ImGui::GetStyle().ItemSpacing.x);
				if (ImGui::Button(state.showSpectrumAnalyser ? "Hide Spectrum" : "Show Spectrum",
								  ImVec2(buttonWidth, BUTTON_HEIGHT))) {
					state.showSpectrumAnalyser = !state.showSpectrumAnalyser;
				}

				ImGui::Unindent(10);
				ImGui::Spacing();
			}
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
				SIDEBAR_WIDTH
			);
		}
	}
	UIStyler::restoreOriginalStyle(ImGui::GetStyle(), state.styleState);
}
