#pragma once

#include <vector>
#include <mutex>
#include <chrono>
#include <stdexcept>
#include "kiss_fftr.h"

class FFTProcessor {
public:
    static constexpr int FFT_SIZE = 2048;
    static constexpr float MIN_FREQ = 20.0f;
    static constexpr float MAX_FREQ = 20000.0f;
    static constexpr int MAX_HARMONIC = 8;
    static constexpr int MAX_PEAKS = 3;

    struct FrequencyPeak {
        float frequency;
        float magnitude;
    };

    FFTProcessor();
    ~FFTProcessor();

    void processBuffer(const float* buffer, size_t numSamples, float sampleRate);
    std::vector<FrequencyPeak> getDominantFrequencies() const;
    std::vector<float> getMagnitudesBuffer() const;
    void reset();
    void setEQGains(float low, float mid, float high);

private:
    kiss_fftr_cfg fft_cfg;
    std::vector<float> fft_in;
    std::vector<kiss_fft_cpx> fft_out;

    std::vector<FrequencyPeak> currentPeaks;    
    mutable std::mutex peaksMutex;

    std::vector<float> hannWindow;
    std::vector<float> magnitudesBuffer;

    std::chrono::steady_clock::time_point lastValidPeakTime;
    static constexpr std::chrono::milliseconds PEAK_RETENTION_TIME{100};
    std::vector<FrequencyPeak> retainedPeaks;

    float lowGain;
    float midGain;
    float highGain;
    mutable std::mutex gainsMutex;
    mutable std::mutex processingMutex;

    void applyWindow(const float* buffer, size_t numSamples);
    void findFrequencyPeaks(float sampleRate);
    float interpolateFrequency(int bin, float sampleRate) const;
    float calculateNoiseFloor(const std::vector<float>& magnitudes) const;
    bool isHarmonic(float testFreq, float baseFreq) const;
};
