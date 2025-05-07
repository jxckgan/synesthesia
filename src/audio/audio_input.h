#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cmath>
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
    
    std::vector<DeviceInfo> getInputDevices();
    bool initStream(int deviceIndex, int numChannels = 1);
    void getColourForCurrentFrequency(float& r, float& g, float& b, float& freq, float& wavelength);
    std::vector<FFTProcessor::FrequencyPeak> getFrequencyPeaks() const;
    FFTProcessor& getFFTProcessor() { return processor.getFFTProcessor(); }
    
    void setNoiseGateThreshold(float threshold) { noiseGateThreshold = threshold; processor.setNoiseGateThreshold(threshold); }
    void setDcRemovalAlpha(float alpha) { dcRemovalAlpha = alpha; }
    void setEQGains(float low, float mid, float high) { processor.setEQGains(low, mid, high); }
    
    int getChannelCount() const { return channelCount; }
    int getActiveChannel() const { return activeChannel; }
    void setActiveChannel(int channel) { activeChannel = (channel >= 0 && channel < channelCount) ? channel : 0; }
    
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
