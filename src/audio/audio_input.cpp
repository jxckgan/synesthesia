#include "audio_input.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

AudioInput::AudioInput() 
    : stream(nullptr), sampleRate(44100.0f)
{
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::string errMsg = "PortAudio initialization failed: ";
        errMsg += Pa_GetErrorText(err);
        throw std::runtime_error(errMsg);
    }
}

AudioInput::~AudioInput() {
    stopStream();
    Pa_Terminate();
}

// Retrieves a list of available input devices
std::vector<AudioInput::DeviceInfo> AudioInput::getInputDevices() {
    std::vector<DeviceInfo> devices;
    
    int deviceCount = Pa_GetDeviceCount();
    if (deviceCount < 0) {
        std::cerr << "Failed to get device count: " 
                  << Pa_GetErrorText(deviceCount) << std::endl;
        return devices;
    }
    
    for (int i = 0; i < deviceCount; ++i) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo && deviceInfo->maxInputChannels > 0) {
            devices.emplace_back(DeviceInfo{ deviceInfo->name, i });
        }
    }
    return devices;
}

// Initialises the audio stream for a specified device index
bool AudioInput::initStream(int deviceIndex) {
    stopStream();

    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(deviceIndex);
    if (!deviceInfo) {
        std::cerr << "Error: Device not found!" << std::endl;
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
        std::cerr << "Failed to open audio stream: " 
                  << Pa_GetErrorText(err) << std::endl;
        stream = nullptr;
        return false;
    }

    if (const PaStreamInfo* streamInfo = Pa_GetStreamInfo(stream)) {
        sampleRate = static_cast<float>(streamInfo->sampleRate);
    }

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        std::cerr << "Failed to start audio stream: " 
                  << Pa_GetErrorText(err) << std::endl;
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
    const float* in = static_cast<const float*>(input);

    std::vector<float> buffer(in, in + frameCount);
    audio->fftProcessor.processBuffer(buffer, audio->sampleRate);

    return paContinue;
}

// Returns the current list of frequency peaks
std::vector<FFTProcessor::FrequencyPeak> AudioInput::getFrequencyPeaks() const {
    return fftProcessor.getDominantFrequencies();
}
