#include "fft_processor.h"

#include <cmath>
#include <numeric>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

FFTProcessor::FFTProcessor()
    : fft_in(FFT_SIZE)
    , fft_out(FFT_SIZE)
    , hannWindow(FFT_SIZE)
    , magnitudesBuffer(FFT_SIZE / 2, 0.0f)
    , lastValidPeakTime(std::chrono::steady_clock::now())
    , lowGain(1.0f)
    , midGain(1.0f)
    , highGain(1.0f)
{
    fft_cfg = kiss_fft_alloc(FFT_SIZE, 0, nullptr, nullptr);
    if (!fft_cfg) {
        std::cerr << "Error allocating FFT configuration." << std::endl;
        std::exit(1);
    }
    
    for (int i = 0; i < FFT_SIZE; ++i) {
        hannWindow[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i / (FFT_SIZE - 1)));
    }
}

FFTProcessor::~FFTProcessor() {
    kiss_fft_free(fft_cfg);
}

// EQ gains
void FFTProcessor::setEQGains(float low, float mid, float high) {
    std::lock_guard<std::mutex> lock(gainsMutex);
    lowGain = low;
    midGain = mid;
    highGain = high;
}

// Applies the precomputed Hann window
void FFTProcessor::applyWindow(const std::vector<float>& buffer) {
    const size_t copySize = std::min(buffer.size(), static_cast<size_t>(FFT_SIZE));
    for (size_t i = 0; i < FFT_SIZE; ++i) {
        fft_in[i].r = (i < copySize) ? buffer[i] * hannWindow[i] : 0.0f;
        fft_in[i].i = 0.0f;
    }
}

// Processes the provided buffer
void FFTProcessor::processBuffer(const std::vector<float>& buffer, float sampleRate) {
    applyWindow(buffer);
    kiss_fft(fft_cfg, fft_in.data(), fft_out.data()); // Perform FFT

    // Normalise the FFT output
    const float scaleFactor = 1.0f / FFT_SIZE;
    for (auto& bin : fft_out) {
        bin.r *= scaleFactor;
        bin.i *= scaleFactor;
    }

    findFrequencyPeaks(sampleRate);
}

// Returns the current list of dominant frequency peaks
const std::vector<FFTProcessor::FrequencyPeak>& FFTProcessor::getDominantFrequencies() const {
    std::lock_guard<std::mutex> lock(peaksMutex);
    return currentPeaks;
}

// Scans the FFT output to detect frequency peaks
void FFTProcessor::findFrequencyPeaks(float sampleRate) {
    std::lock_guard<std::mutex> lock(peaksMutex);
    
    std::fill(magnitudesBuffer.begin(), magnitudesBuffer.end(), 0.0f);
    currentPeaks.clear();

    float currentLowGain, currentMidGain, currentHighGain;
    {
        std::lock_guard<std::mutex> gainsLock(gainsMutex);
        currentLowGain = lowGain;
        currentMidGain = midGain;
        currentHighGain = highGain;
    }

    // Calculate magnitudes for frequency bins
    for (int i = 1; i < FFT_SIZE / 2; ++i) {
        float magnitude = std::hypot(fft_out[i].r, fft_out[i].i);
        float freq = (i * sampleRate) / FFT_SIZE;
        float lowResponse = 0.0f;
        if (freq <= 200.0f) {
            lowResponse = 1.0f;
        } else if (freq < 250.0f) {
            lowResponse = (250.0f - freq) / 50.0f;
        }

        float highResponse = 0.0f;
        if (freq >= 2000.0f) {
            highResponse = 1.0f;
        } else if (freq > 1900.0f) {
            highResponse = (freq - 1900.0f) / 100.0f;
        }

        float midResponse = 0.0f;
        if (freq >= 250.0f && freq <= 1900.0f) {
            midResponse = 1.0f;
        }
        else if (freq > 200.0f && freq < 250.0f) {
            midResponse = (freq - 200.0f) / 50.0f;
        }
        else if (freq > 1900.0f && freq < 2000.0f) {
            midResponse = (2000.0f - freq) / 100.0f;
        }

        // Adjustments for a fairer representation of frequencies (in most cases)
        lowResponse *= 0.3f;
        midResponse *= 0.4f;
        highResponse *= 1.4f;

        float combinedGain = (lowResponse * currentLowGain) +
                             (midResponse * currentMidGain) +
                             (highResponse * currentHighGain);

        magnitude *= combinedGain;
        magnitudesBuffer[i] = magnitude;
    }

    const float NOISE_FLOOR = calculateNoiseFloor(magnitudesBuffer);

    std::vector<FrequencyPeak> rawPeaks;
    for (int i = 1; i < FFT_SIZE / 2 - 1; ++i) {
        if (magnitudesBuffer[i] > NOISE_FLOOR &&
            magnitudesBuffer[i] > magnitudesBuffer[i - 1] &&
            magnitudesBuffer[i] > magnitudesBuffer[i + 1])
        {
            float freq = interpolateFrequency(i, sampleRate);
            rawPeaks.push_back({freq, magnitudesBuffer[i]});
        }
    }

    // Sort peaks in descending order by magnitude
    std::sort(rawPeaks.begin(), rawPeaks.end(),
              [](const FrequencyPeak& a, const FrequencyPeak& b) {
                  return a.magnitude > b.magnitude;
              });

    // Filter out harmonics
    std::vector<FrequencyPeak> newPeaks;
    for (const auto& peak : rawPeaks) {
        if (newPeaks.size() >= MAX_PEAKS)
            break;
        bool is_harmonic = false;
        for (const auto& existing : newPeaks) {
            if (isHarmonic(peak.frequency, existing.frequency, sampleRate)) {
                is_harmonic = true;
                break;
            }
        }
        if (!is_harmonic) {
            newPeaks.push_back(peak);
        }
    }

    const auto now = std::chrono::steady_clock::now();
    if (!newPeaks.empty()) {
        currentPeaks = newPeaks;
        retainedPeaks = newPeaks;
        lastValidPeakTime = now;
    } else {
        const auto timeSinceValid = now - lastValidPeakTime;
        if (timeSinceValid < PEAK_RETENTION_TIME) {
            currentPeaks = retainedPeaks;
        } else {
            currentPeaks.clear();
        }
    }
}

// Determines if testFreq is a harmonic of baseFreq
bool FFTProcessor::isHarmonic(float testFreq, float baseFreq, float sampleRate) const {
    float dynamicTolerance = 0.05f;
    if (baseFreq < 250.0f) {
        dynamicTolerance = 0.05f * (baseFreq / 250.0f);
    }
    float ratio = testFreq / baseFreq;
    for (int h = 2; h <= 8; ++h) {
        if (std::abs(ratio - static_cast<float>(h)) < dynamicTolerance)
            return true;
    }
    return false;
}

// Quadratic interpolation to estimate the true frequency at the peak
float FFTProcessor::interpolateFrequency(int bin, float sampleRate) const {
    auto magnitude = [](const kiss_fft_cpx& v) {
        return std::hypot(v.r, v.i);
    };

    const float m0 = magnitude(fft_out[bin - 1]);
    const float m1 = magnitude(fft_out[bin]);
    const float m2 = magnitude(fft_out[bin + 1]);

    const float denominator = m0 - 2.0f * m1 + m2;
    if (std::abs(denominator) < 1e-12f)
        return bin * sampleRate / FFT_SIZE;

    const float alpha = (m0 - m2) / (2.0f * denominator);
    return (bin + alpha) * sampleRate / FFT_SIZE;
}

// Calculates a noise floor
float FFTProcessor::calculateNoiseFloor(const std::vector<float>& magnitudes) const {
    // Use nth_element for median computation
    std::vector<float> mags(magnitudes.begin() + 1, magnitudes.begin() + magnitudes.size());
    size_t n = mags.size();
    auto mid_it = mags.begin() + n / 2;
    std::nth_element(mags.begin(), mid_it, mags.end());
    float median = *mid_it;

    std::vector<float> deviations(n);
    for (size_t i = 0; i < n; ++i) {
        deviations[i] = std::abs(mags[i] - median);
    }
    auto mid_dev_it = deviations.begin() + n / 2;
    std::nth_element(deviations.begin(), mid_dev_it, deviations.end());
    float MAD = *mid_dev_it;
    return median + 2.0f * MAD * 1.4826f;
}

const std::vector<float>& FFTProcessor::getMagnitudesBuffer() const {
    return magnitudesBuffer;
}

void FFTProcessor::reset() {
    std::lock_guard<std::mutex> lock(peaksMutex);
    currentPeaks.clear();
    retainedPeaks.clear();
    std::fill(magnitudesBuffer.begin(), magnitudesBuffer.end(), 0.0f);
    lastValidPeakTime = std::chrono::steady_clock::now();
}
