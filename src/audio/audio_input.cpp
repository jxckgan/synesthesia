#include "audio_input.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

AudioInput::AudioInput() : stream(nullptr), sampleRate(44100.0f) {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio initialization failed: " 
                  << Pa_GetErrorText(err) << std::endl;
        exit(1);
    }
}

AudioInput::~AudioInput() {
    stopStream();
    Pa_Terminate();
}

// Retrieves a list of available input devices
std::vector<AudioInput::DeviceInfo> AudioInput::getInputDevices() {
    std::vector<DeviceInfo> devices;
    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        std::cerr << "Failed to get device count: " 
                  << Pa_GetErrorText(numDevices) << std::endl;
        return devices;
    }

    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo && deviceInfo->maxInputChannels > 0) {
            devices.push_back({ deviceInfo->name, i });
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
        return false;
    }

    const PaStreamInfo* streamInfo = Pa_GetStreamInfo(stream);
    sampleRate = streamInfo ? static_cast<float>(streamInfo->sampleRate) : 44100.0f;

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        std::cerr << "Failed to start audio stream: " 
                  << Pa_GetErrorText(err) << std::endl;
        return false;
    }

    return true;
}

// Maps the current dominant frequency to a colour
void AudioInput::getColourForCurrentFrequency(float& r, float& g, float& b, float& freq, float& wavelength) {
    auto& peaks = fftProcessor.getDominantFrequencies();
    std::vector<float> freqs, mags;
    for (const auto& peak : peaks) {
        freqs.push_back(peak.frequency);
        mags.push_back(peak.magnitude);
    }
    
    auto colourResult = ColourMapper::frequenciesToColour(freqs, mags);
    r = colourResult.r;
    g = colourResult.g;
    b = colourResult.b;
    freq = !peaks.empty() ? peaks[0].frequency : 0.0f;
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
int AudioInput::audioCallback(const void* input, void*,
                              unsigned long frameCount,
                              const PaStreamCallbackTimeInfo*,
                              PaStreamCallbackFlags,
                              void* userData) {
    AudioInput* audio = static_cast<AudioInput*>(userData);
    const float* in = static_cast<const float*>(input);

    // Copy the input buffer into a vector
    std::vector<float> buffer(frameCount);
    std::copy(in, in + frameCount, buffer.begin());
    
    // Lock the mutex
    std::lock_guard<std::mutex> lock(audio->bufferMutex);
    audio->fftProcessor.processBuffer(buffer, audio->sampleRate);
    
    return paContinue;
}

// Returns the current list of frequency peaks
const std::vector<FFTProcessor::FrequencyPeak>& AudioInput::getFrequencyPeaks() const {
    return fftProcessor.getDominantFrequencies();
}
