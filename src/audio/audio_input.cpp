#include "audio_input.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

AudioInput::AudioInput()
	: stream(nullptr),
	  sampleRate(44100.0f),
	  channelCount(1),
	  activeChannel(0),
	  noiseGateThreshold(0.0001f),
	  dcRemovalAlpha(0.995f) {
	previousInputs.push_back(0.0f);
	previousOutputs.push_back(0.0f);

	if (const PaError err = Pa_Initialize(); err != paNoError) {
		throw std::runtime_error("PortAudio initialization failed: " +
								 std::string(Pa_GetErrorText(err)));
	}

	processor.start();
}

AudioInput::~AudioInput() {
	stopStream();
	Pa_Terminate();

	processor.stop();
}

std::vector<AudioInput::DeviceInfo> AudioInput::getInputDevices() {
	std::vector<DeviceInfo> devices;
	const int deviceCount = Pa_GetDeviceCount();

	if (deviceCount < 0) {
		throw std::runtime_error("Failed to get device count: " +
								 std::string(Pa_GetErrorText(deviceCount)));
	}

	devices.reserve(static_cast<size_t>(deviceCount));
	for (int i = 0; i < deviceCount; ++i) {
		if (const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i)) {
			if (deviceInfo->maxInputChannels > 0) {
				devices.emplace_back(DeviceInfo{deviceInfo->name, i, deviceInfo->maxInputChannels});
			}
		} else {
			std::cerr << "Warning: Failed to get device info for device index " << i << "\n";
		}
	}
	return devices;
}

bool AudioInput::initStream(const int deviceIndex, const int numChannels) {
	stopStream();

	const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(deviceIndex);
	if (!deviceInfo || deviceInfo->maxInputChannels < 1) {
		std::cerr << "Invalid audio device: " << deviceIndex << "\n";
		return false;
	}

	channelCount = std::min(numChannels, deviceInfo->maxInputChannels);
	if (channelCount < 1)
		channelCount = 1;

	activeChannel = 0;

	previousInputs.resize(channelCount, 0.0f);
	previousOutputs.resize(channelCount, 0.0f);

	PaStreamParameters inputParameters{};
	inputParameters.device = deviceIndex;
	inputParameters.channelCount = channelCount;
	inputParameters.sampleFormat = paFloat32;
	inputParameters.suggestedLatency = deviceInfo->defaultLowInputLatency;
	inputParameters.hostApiSpecificStreamInfo = nullptr;

	const PaError err =
		Pa_OpenStream(&stream, &inputParameters, nullptr, deviceInfo->defaultSampleRate,
					  FFTProcessor::FFT_SIZE, paClipOff, audioCallback, this);

	if (err != paNoError) {
		std::cerr << "Failed to open audio stream: " << Pa_GetErrorText(err) << "\n";
		stream = nullptr;
		return false;
	}

	if (const PaStreamInfo* streamInfo = Pa_GetStreamInfo(stream)) {
		sampleRate = static_cast<float>(streamInfo->sampleRate);
	}

	if (const PaError startErr = Pa_StartStream(stream); startErr != paNoError) {
		std::cerr << "Failed to start stream: " << Pa_GetErrorText(startErr) << "\n";
		stopStream();
		return false;
	}

	return true;
}

void AudioInput::getColourForCurrentFrequency(float& r, float& g, float& b, float& freq,
											  float& wavelength) const {
	processor.getColourForCurrentFrequency(r, g, b, freq, wavelength);
}

std::vector<FFTProcessor::FrequencyPeak> AudioInput::getFrequencyPeaks() const {
	return processor.getFrequencyPeaks();
}

void AudioInput::stopStream() {
	if (stream) {
		Pa_StopStream(stream);
		Pa_CloseStream(stream);
		stream = nullptr;
	}
}

int AudioInput::audioCallback(const void* input, void* output, const unsigned long frameCount,
							  const PaStreamCallbackTimeInfo* timeInfo,
							  PaStreamCallbackFlags statusFlags, void* userData) {
	auto* audio = static_cast<AudioInput*>(userData);

	if (!input) {
		return paContinue;
	}

	try {
		const auto* inBuffer = static_cast<const float*>(input);
		thread_local std::vector<float> processedBuffer;
		if (processedBuffer.size() < frameCount) {
			processedBuffer.resize(frameCount);
		}

		int activeChannel = audio->activeChannel.load();
		const int channelCount = audio->channelCount;

		if (activeChannel >= channelCount) {
			activeChannel = 0;
		}

		for (unsigned long i = 0; i < frameCount; ++i) {
			const float sample = inBuffer[i * channelCount + activeChannel];

			float filteredSample = sample - audio->previousInputs[activeChannel] +
								   audio->dcRemovalAlpha * audio->previousOutputs[activeChannel];
			audio->previousInputs[activeChannel] = sample;
			audio->previousOutputs[activeChannel] = filteredSample;

			if (std::abs(filteredSample) < audio->noiseGateThreshold) {
				filteredSample = 0.0f;
			}

			processedBuffer[i] = filteredSample;
		}

		audio->processor.queueAudioData(processedBuffer.data(), frameCount, audio->sampleRate);
	}

	catch (const std::exception& ex) {
		std::cerr << "Warning in audio callback: " << ex.what() << "\n";
		return paContinue;
	} catch (...) {
		std::cerr << "Unknown warning in audio callback.\n";
		return paContinue;
	}

	return paContinue;
}