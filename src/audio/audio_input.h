#pragma once

#include <string>
#include <vector>
#include <portaudio.h>
#include "audio_processor.h"

class AudioInput {
public:
    struct DeviceInfo {
        std::string name;
        int paIndex;
        int maxChannels;
    };

    AudioInput();
    ~AudioInput();

    static std::vector<DeviceInfo> getInputDevices();
    bool initStream(int deviceIndex, int numChannels = 1);
    void getColourForCurrentFrequency(float& r, float& g, float& b, float& freq, float& wavelength) const;
    std::vector<FFTProcessor::FrequencyPeak> getFrequencyPeaks() const;
    FFTProcessor& getFFTProcessor() { return processor.getFFTProcessor(); }

    void setNoiseGateThreshold(const float threshold) { noiseGateThreshold = threshold; processor.setNoiseGateThreshold(threshold); }
    void setDcRemovalAlpha(const float alpha) { dcRemovalAlpha = alpha; }
    void setEQGains(const float low, const float mid, const float high) { processor.setEQGains(low, mid, high); }

    int getChannelCount() const { return channelCount; }
    int getActiveChannel() const { return activeChannel; }
    void setActiveChannel(const int channel) { activeChannel = channel >= 0 && channel < channelCount ? channel : 0; }

private:
    PaStream* stream;
    AudioProcessor processor;
    float sampleRate;
    int channelCount;
    int activeChannel;

    std::vector<float> previousInputs;
    std::vector<float> previousOutputs;
    float noiseGateThreshold;
    float dcRemovalAlpha;

    void stopStream();
    static int audioCallback(const void* input, void* output,
                             unsigned long frameCount,
                             const PaStreamCallbackTimeInfo* timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void* userData);
};
