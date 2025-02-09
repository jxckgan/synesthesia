#include "audio_input.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

AudioInput::AudioInput() 
    : stream(nullptr)
    , sampleRate(44100.0f)
{
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        throw std::runtime_error("PortAudio initialization failed: " + 
            std::string(Pa_GetErrorText(err)));
    }
}

AudioInput::~AudioInput() {
    stopStream();
    Pa_Terminate();
}

// Retrieves a list of available input devices
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
                devices.emplace_back(DeviceInfo{deviceInfo->name, i});
            }
        } else {
            std::cerr << "Warning: Failed to get device info for device index " << i << "\n";
        }
    }    
    return devices;
}

// Initialises the audio stream for a specified device index
bool AudioInput::initStream(int deviceIndex) {
    stopStream();

    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(deviceIndex);
    if (!deviceInfo || deviceInfo->maxInputChannels < 1) {
        std::cerr << "Invalid audio device: " << deviceIndex << "\n";
        return false;
    }

    PaStreamParameters inputParameters{};
    inputParameters.device = deviceIndex;
    inputParameters.channelCount = 1;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = deviceInfo->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(
        &stream,
        &inputParameters,
        nullptr,
        deviceInfo->defaultSampleRate,
        FFTProcessor::FFT_SIZE,
        paClipOff,
        audioCallback,
        this
    );

    if (err != paNoError) {
        std::cerr << "Failed to open audio stream: " << Pa_GetErrorText(err) << "\n";
        stream = nullptr;
        return false;
    }

    if (const PaStreamInfo* streamInfo = Pa_GetStreamInfo(stream)) {
        sampleRate = static_cast<float>(streamInfo->sampleRate);
    }

    PaError startErr = Pa_StartStream(stream);
    if (startErr != paNoError) {
        std::cerr << "Failed to start stream: " << Pa_GetErrorText(startErr) << "\n";
        stopStream();
        return false;
    }

    return true;
}

// Maps the current dominant frequency to a colour
void AudioInput::getColourForCurrentFrequency(float& r, float& g, float& b, float& freq, float& wavelength) {
    std::vector<FFTProcessor::FrequencyPeak> peaks = fftProcessor.getDominantFrequencies();
    std::vector<float> freqs, mags;

    for (const auto &peak : peaks) {
        freqs.push_back(peak.frequency);
        mags.push_back(peak.magnitude);
    }

    auto colourResult = ColourMapper::frequenciesToColour(freqs, mags);
    r = colourResult.r;
    g = colourResult.g;
    b = colourResult.b;
    freq = (!peaks.empty()) ? peaks[0].frequency : 0.0f;
    wavelength = colourResult.dominantWavelength;
}

// Stops and cleans up the current audio stream
void AudioInput::stopStream() {
    if (stream) {
        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        stream = nullptr;
    }
}

// Processes incoming audio data
int AudioInput::audioCallback(const void* input, void* output,
                              unsigned long frameCount,
                              const PaStreamCallbackTimeInfo* timeInfo,
                              PaStreamCallbackFlags statusFlags,
                              void* userData)
{
    auto* audio = static_cast<AudioInput*>(userData);

    if (!input) {
        return paContinue;
    }

    const float* inBuffer = static_cast<const float*>(input);
    audio->fftProcessor.processBuffer(inBuffer, frameCount, audio->sampleRate);

    return paContinue;
}

// Returns the current list of frequency peaks
std::vector<FFTProcessor::FrequencyPeak> AudioInput::getFrequencyPeaks() const {
    return fftProcessor.getDominantFrequencies();
}
