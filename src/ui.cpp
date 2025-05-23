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

void applyCustomUIStyle(ImGuiStyle& style, UIState& state) {
	if (!state.styleApplied) {
		state.originalStyle = style;
		state.styleApplied = true;
	}

	ImVec4* colours = style.Colors;
	style.WindowRounding = 0.0f;
	style.FrameRounding = 1.0f;
	style.ScrollbarRounding = 5.0f;
	style.GrabRounding = 1.0f;
	style.TabRounding = 1.0f;
	style.ChildRounding = 1.0f;
	style.PopupRounding = 1.0f;
	style.Alpha = 1.0f;
	style.ItemSpacing = ImVec2(10, 12);
	style.FramePadding = ImVec2(8, 6);
	style.WindowPadding = ImVec2(12, 12);

	colours[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.07f, 0.90f);
	colours[ImGuiCol_Border] = ImVec4(0.40f, 0.40f, 0.45f, 0.50f);
	colours[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colours[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.17f, 0.85f);
	colours[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.22f, 0.90f);
	colours[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.27f, 0.95f);
	colours[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.90f);
	colours[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.10f, 0.12f, 0.90f);
	colours[ImGuiCol_TitleBgCollapsed] = ImVec4(0.05f, 0.05f, 0.05f, 0.75f);
	colours[ImGuiCol_SliderGrab] = ImVec4(0.65f, 0.65f, 0.70f, 0.80f);
	colours[ImGuiCol_SliderGrabActive] = ImVec4(0.80f, 0.80f, 0.85f, 0.90f);
	colours[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.22f, 0.75f);
	colours[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.35f, 0.85f);
	colours[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.40f, 0.45f, 0.90f);
	colours[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.22f, 0.70f);
	colours[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.35f, 0.85f);
	colours[ImGuiCol_HeaderActive] = ImVec4(0.40f, 0.40f, 0.45f, 0.70f);
	colours[ImGuiCol_ScrollbarBg] = ImVec4(0.07f, 0.07f, 0.09f, 0.00f);
	colours[ImGuiCol_ScrollbarGrab] = ImVec4(0.35f, 0.35f, 0.40f, 0.70f);

	colours[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.45f, 0.45f, 0.50f, 0.80f);
	colours[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.55f, 0.55f, 0.60f, 0.85f);

	colours[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.90f, 0.95f, 1.00f);
	colours[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
	colours[ImGuiCol_TextDisabled] = ImVec4(0.65f, 0.65f, 0.65f, 0.80f);
	colours[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.12f, 0.14f, 0.90f);
}

void restoreOriginalStyle(ImGuiStyle& style, UIState& state) {
	if (state.styleApplied) {
		style = state.originalStyle;
		state.styleApplied = false;
	}
}

void updateUI(AudioInput& audioInput, const std::vector<AudioInput::DeviceInfo>& devices,
			  float* clear_color, ImGuiIO& io, UIState& state) {
	// Toggle UI On/Off with 'H' key
	if (ImGui::IsKeyPressed(ImGuiKey_H)) {
		state.showUI = !state.showUI;
	}

	// Populate device names list
	if (!state.deviceNamesPopulated && !devices.empty()) {
		state.deviceNames.reserve(devices.size());
		for (const auto& dev : devices) {
			state.deviceNames.push_back(dev.name.c_str());
		}
		state.deviceNamesPopulated = true;
	}

	// Colour smoothing
	float deltaTime = io.DeltaTime;
	static SpringSmoother colourSmoother(8.0f, 1.0f, 0.3f);
	static float smoothingAmount = 0.60f;
	colourSmoother.setSmoothingAmount(smoothingAmount);

	// Define UI layout measurements
	constexpr float SIDEBAR_WIDTH = 280.0f;
	constexpr float SIDEBAR_PADDING = 16.0f;
	constexpr float contentWidth = SIDEBAR_WIDTH - SIDEBAR_PADDING * 2;

	// Process audio data and updates visuals when a device is selected
	if (state.selectedDeviceIndex >= 0) {
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
		const size_t count = magnitudes.size();	 // Use size_t for consistency

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
		applyCustomUIStyle(style, state);

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

		// Input Selection
		ImGui::Text("INPUT DEVICE");
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (!state.deviceNames.empty()) {
			if (ImGui::Combo("##device", &state.selectedDeviceIndex, state.deviceNames.data(),
							 static_cast<int>(state.deviceNames.size()))) {
				if (state.selectedDeviceIndex >= 0 &&
					static_cast<size_t>(state.selectedDeviceIndex) < devices.size()) {
					// Clear previous channel names
					state.channelNames.clear();

					// Get max channels for this device
					int maxChannels = devices[state.selectedDeviceIndex].maxChannels;

					// Reset selected channel
					state.selectedChannelIndex = 0;

					// Initialise with all available channels
					int channelsToUse = std::min(maxChannels, 16);

					// Start the stream with all available channels
					if (bool success = audioInput.initStream(
							devices[state.selectedDeviceIndex].paIndex, channelsToUse);
						!success) {
						state.streamError = true;
						state.streamErrorMessage = "Error opening device!";
					} else {
						state.streamError = false;
						state.streamErrorMessage.clear();
						state.smoothedMagnitudes.assign(state.smoothedMagnitudes.size(), 0.0f);
						state.previousLoudness = 0.0f;
						state.transientIntensity = 0.0f;
						state.smoothedLoudness = 0.0f;

						// Create channel name list
						static std::vector<std::string> channelNameStrings;
						channelNameStrings.clear();
						channelNameStrings.reserve(channelsToUse);

						for (int i = 0; i < channelsToUse; i++) {
							channelNameStrings.push_back("Channel " + std::to_string(i + 1));
						}

						// Update channel name pointers
						state.channelNames.reserve(channelsToUse);
						for (const auto& name : channelNameStrings) {
							state.channelNames.push_back(name.c_str());
						}
					}
				} else {
					state.streamError = true;
					state.streamErrorMessage = "Invalid device selection index.";
					state.selectedDeviceIndex = -1;
				}
			}
			if (state.streamError) {
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s",
								   state.streamErrorMessage.c_str());
			} else if (state.selectedDeviceIndex < 0) {
				ImGui::TextDisabled("Select an audio device");
			}
		} else {
			ImGui::TextDisabled("No audio input devices found.");
		}
		ImGui::Spacing();

		// Only shows controls if a device is successfully selected
		if (state.selectedDeviceIndex >= 0 && !state.streamError) {
			constexpr float BUTTON_HEIGHT = 25.0f;
			constexpr float CONTROL_WIDTH = 150.0f;
			constexpr float LABEL_WIDTH = 90.0f;
			if (state.selectedDeviceIndex >= 0 && !state.streamError &&
				!state.channelNames.empty() && devices[state.selectedDeviceIndex].maxChannels > 2) {
				ImGui::Text("CHANNEL");
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::Combo("##channel", &state.selectedChannelIndex,
								 state.channelNames.data(),
								 static_cast<int>(state.channelNames.size()))) {
					// Update the active channel in the audio input
					audioInput.setActiveChannel(state.selectedChannelIndex);
				}
				ImGui::Spacing();
			}

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

		// Spectrum Analyser window
		if (state.selectedDeviceIndex >= 0 && !state.streamError && state.showSpectrumAnalyser) {
			constexpr float spectrumHeight = 210.0f;

			auto spectrumPos = ImVec2(0.0f, displaySize.y - spectrumHeight);
			auto spectrumSize = ImVec2(displaySize.x - SIDEBAR_WIDTH, spectrumHeight);

			ImGui::SetNextWindowPos(spectrumPos);
			ImGui::SetNextWindowSize(spectrumSize);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

			ImGui::Begin("##SpectrumAnalyser", nullptr,
						 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
							 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
							 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);

			// Normalise Y-Axis for spectrum dynamically
			float maxMagnitude = 0.0f;
			if (!state.smoothedMagnitudes.empty()) {
				maxMagnitude = *std::ranges::max_element(state.smoothedMagnitudes);
			}
			float yMax = maxMagnitude > 0.0001f ? maxMagnitude * 1.1f : 0.1f;

			ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
			ImVec2 canvas_size = ImGui::GetContentRegionAvail();

			ImDrawList* drawList = ImGui::GetWindowDrawList();

			// Draw subtle background gradient
			drawList->AddRectFilledMultiColor(
				ImVec2(canvas_pos.x, canvas_pos.y),
				ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
				ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.0f)),
				ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.0f)),
				ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.4f)),
				ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.4f)));

			constexpr int lineCount = 500;
			std::vector<ImVec2> points(lineCount);

			float sampleRate = 44100.0f;
			if (state.selectedDeviceIndex >= 0 &&
				static_cast<size_t>(state.selectedDeviceIndex) < devices.size()) {
				if (const PaDeviceInfo* deviceInfo =
						Pa_GetDeviceInfo(devices[state.selectedDeviceIndex].paIndex)) {
					sampleRate = static_cast<float>(deviceInfo->defaultSampleRate);
				}
			}

			const float binSize = sampleRate / FFTProcessor::FFT_SIZE;
			constexpr float minFreq = FFTProcessor::MIN_FREQ;
			constexpr float maxFreq = FFTProcessor::MAX_FREQ;
			const float logMinFreq = std::log10(minFreq);
			const float logMaxFreq = std::log10(maxFreq);

			// Fill point array with spectrum data using logarithmic scale
			if (const float logFreqRange = logMaxFreq - logMinFreq;
				!state.smoothedMagnitudes.empty() && binSize > 0.0f && logFreqRange > 0.0f) {
				for (int i = 0; i < lineCount; ++i) {
					// Calculate frequency for this point using logarithmic interpolation
					float logPosition = static_cast<float>(i) / (lineCount - 1);  // 0.0 to 1.0
					float currentLogFreq = logMinFreq + logPosition * logFreqRange;
					float freq = std::pow(10.0f, currentLogFreq);

					// Calculate corresponding bin index
					float binIndex = freq / binSize;

					// Ensure bin index is in valid range and interpolate
					int binIndexFloor = static_cast<int>(binIndex);
					float t = binIndex - binIndexFloor;	 // Interpolation factor

					// Clamp indices safely
					int idx0 = std::min(std::max(binIndexFloor, 0),
										static_cast<int>(state.smoothedMagnitudes.size()) - 1);
					int idx1 =
						std::min(idx0 + 1, static_cast<int>(state.smoothedMagnitudes.size()) - 1);

					float magnitude = state.smoothedMagnitudes[idx0] * (1.0f - t) +
									  state.smoothedMagnitudes[idx1] * t;

					// Calculate x position
					float xPos =
						canvas_pos.x + static_cast<float>(i) / (lineCount - 1) * canvas_size.x;

					// Calculate y position
					float height = magnitude / yMax * canvas_size.y * 0.7f;
					height = std::min(height, canvas_size.y * 0.75f);
					float yPos = canvas_pos.y + canvas_size.y - height;

					points[i] = ImVec2(xPos, yPos);
				}

				// Apply smoothing to the calculated points
				std::vector<float> smoothedYPositions(lineCount);
				for (int i = 0; i < lineCount; ++i) {
					constexpr int smoothingWindow = 7;
					float sum = 0.0f;
					int count = 0;
					int halfWindow = smoothingWindow / 2;
					for (int j = std::max(0, i - halfWindow);
						 j <= std::min(lineCount - 1, i + halfWindow); ++j) {
						sum += points[j].y;
						count++;
					}
					smoothedYPositions[i] =
						count > 0 ? sum / count : points[i].y;	// Avoid division by zero
				}

				for (int i = 0; i < lineCount; ++i) {
					points[i].y = smoothedYPositions[i];
				}

				// Define colours for drawing
				ImU32 fillColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
				ImU32 lineColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.9f));
				constexpr float lineThickness = 1.5f;

				// Fill area below the curve using quads
				for (int i = 0; i < lineCount - 1; ++i) {
					ImVec2 quad[4] = {points[i], points[i + 1],
									  ImVec2(points[i + 1].x, canvas_pos.y + canvas_size.y),
									  ImVec2(points[i].x, canvas_pos.y + canvas_size.y)};
					drawList->AddConvexPolyFilled(quad, 4, fillColor);
				}

				// Draw the spectrum line on top.
				drawList->AddPolyline(points.data(), lineCount, lineColor, false, lineThickness);
			}

			ImGui::End();
			ImGui::PopStyleColor();
			ImGui::PopStyleVar();
		}
	}
	restoreOriginalStyle(ImGui::GetStyle(), state);
}
