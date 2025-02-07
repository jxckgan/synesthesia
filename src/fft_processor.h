#pragma once

#include <vector>
#include <mutex>
#include <chrono>
#include "kiss_fft.h"

class FFTProcessor {
public:
    static constexpr int FFT_SIZE = 4096;
    static constexpr float MIN_FREQ = 20.0f;
    static constexpr float MAX_FREQ = 20000.0f;
    static constexpr float MAX_HARMONIC = 8;
    static constexpr int MAX_PEAKS = 3;

    struct FrequencyPeak {
        float frequency;
        float magnitude;
    };

    FFTProcessor();
    ~FFTProcessor();

    void processBuffer(const std::vector<float>& buffer, float sampleRate);
    const std::vector<FrequencyPeak>& getDominantFrequencies() const;
    void reset();
    void setEQGains(float low, float mid, float high);
    const std::vector<float>& getMagnitudesBuffer() const;

private:
    kiss_fft_cfg fft_cfg;
    std::vector<kiss_fft_cpx> fft_in;
    std::vector<kiss_fft_cpx> fft_out;
    std::vector<FrequencyPeak> currentPeaks;

    mutable std::mutex peaksMutex;
    
    std::vector<float> hannWindow;
    std::vector<float> magnitudesBuffer;
    std::chrono::steady_clock::time_point lastValidPeakTime;
    static constexpr std::chrono::milliseconds PEAK_RETENTION_TIME{200};
    std::vector<FrequencyPeak> retainedPeaks;

    float lowGain;
    float midGain;
    float highGain;

    mutable std::mutex gainsMutex;    

    void applyWindow(const std::vector<float>& buffer);
    void findFrequencyPeaks(float sampleRate);
    float interpolateFrequency(int bin, float sampleRate) const;
    float calculateNoiseFloor(const std::vector<float>& magnitudes) const;
    bool isHarmonic(float testFreq, float baseFreq, float sampleRate) const;
};
