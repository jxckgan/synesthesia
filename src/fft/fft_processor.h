#pragma once

#include <vector>
#include <mutex>
#include <chrono>
#include <stdexcept>
#include <atomic>
#include <span>
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

    FFTProcessor(const FFTProcessor&) = delete;
    FFTProcessor& operator=(const FFTProcessor&) = delete;
    FFTProcessor(FFTProcessor&&) noexcept = delete;
    FFTProcessor& operator=(FFTProcessor&&) noexcept = delete;

    void processBuffer(const std::span<const float> buffer, float sampleRate);
    std::vector<FrequencyPeak> getDominantFrequencies() const;
    std::vector<float> getMagnitudesBuffer() const;
    float getCurrentLoudness() const;
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

    float currentLoudness;
    static constexpr float LOUDNESS_SMOOTHING = 0.2f;

    void applyWindow(const std::span<const float> buffer);
    void findFrequencyPeaks(float sampleRate);
    float interpolateFrequency(int bin, float sampleRate) const;
    float calculateNoiseFloor(const std::vector<float>& magnitudes) const;
    bool isHarmonic(float testFreq, float baseFreq) const;
    
    void calculateMagnitudes(std::vector<float>& rawMagnitudes, float sampleRate, 
                            float& maxMagnitude, float& totalEnergy);
    
    void processMagnitudes(std::vector<float>& magnitudes, float sampleRate, float maxMagnitude);
    void findPeaks(float sampleRate, float noiseFloor, std::vector<FrequencyPeak>& peaks);
};
