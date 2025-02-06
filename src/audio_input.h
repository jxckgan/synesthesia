#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <portaudio.h>
#include "fft_processor.h"
#include "colour_mapper.h"

class AudioInput {
public:
    struct DeviceInfo {
        std::string name;
        int paIndex;
    };

    AudioInput();
    ~AudioInput();
    
    std::vector<DeviceInfo> getInputDevices();
    bool initStream(int deviceIndex);
    void getColourForCurrentFrequency(float& r, float& g, float& b, float& freq, float& wavelength);
    const std::vector<FFTProcessor::FrequencyPeak>& getFrequencyPeaks() const;
    FFTProcessor& getFFTProcessor() { return fftProcessor; }

private:
    PaStream* stream;
    FFTProcessor fftProcessor;
    std::mutex bufferMutex;
    float sampleRate;
    void stopStream();
    static int audioCallback(const void* input, void* output,
                             unsigned long frameCount,
                             const PaStreamCallbackTimeInfo* timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void* userData);
};
