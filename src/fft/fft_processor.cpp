#include "fft_processor.h"

#include <cmath>
#include <numeric>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

FFTProcessor::FFTProcessor()
    : fft_in(FFT_SIZE)
    , fft_out(FFT_SIZE / 2 + 1)
    , hannWindow(FFT_SIZE)
    , magnitudesBuffer(FFT_SIZE / 2 + 1, 0.0f)
    , lastValidPeakTime(std::chrono::steady_clock::now())
    , lowGain(1.0f)
    , midGain(1.0f)
    , highGain(1.0f)
{
    fft_cfg = kiss_fftr_alloc(FFT_SIZE, 0, nullptr, nullptr);
    if (!fft_cfg) {
        std::cerr << "Error allocating FFTR configuration." << std::endl;
        std::exit(1);
    }
    
    // Precompute Hann window
    for (int i = 0; i < FFT_SIZE; ++i) {
        hannWindow[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i / (FFT_SIZE - 1)));
    }
}

FFTProcessor::~FFTProcessor() {
    kiss_fftr_free(fft_cfg);
}

void FFTProcessor::setEQGains(float low, float mid, float high) {
    std::lock_guard<std::mutex> lock(gainsMutex);
    lowGain = std::max(0.0f, low);
    midGain = std::max(0.0f, mid);
    highGain = std::max(0.0f, high);
}

void FFTProcessor::applyWindow(const std::vector<float>& buffer) {
    const size_t copySize = std::min(buffer.size(), static_cast<size_t>(FFT_SIZE));
    std::fill(fft_in.begin(), fft_in.end(), 0.0f);
    for (size_t i = 0; i < copySize; ++i) {
        fft_in[i] = buffer[i] * hannWindow[i];
    }
}

void FFTProcessor::processBuffer(const std::vector<float>& buffer, float sampleRate) {
    if (sampleRate <= 0.0f) return;
    
    applyWindow(buffer);
    kiss_fftr(fft_cfg, fft_in.data(), fft_out.data());

    // Normalise FFT output
    const float scaleFactor = 2.0f / FFT_SIZE;
    for (size_t i = 0; i < fft_out.size(); ++i) {
        fft_out[i].r *= scaleFactor;
        fft_out[i].i *= scaleFactor;
    }
    fft_out[0].r *= 0.5f;
    fft_out[fft_out.size() - 1].r *= 0.5f;

    findFrequencyPeaks(sampleRate);
}

const std::vector<FFTProcessor::FrequencyPeak>& FFTProcessor::getDominantFrequencies() const {
    std::lock_guard<std::mutex> lock(peaksMutex);
    return currentPeaks;
}

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

    // Calculate magnitudes with EQ applied
    const size_t binCount = fft_out.size();
    
    for (size_t i = 1; i < binCount - 1; ++i) {
        float freq = (i * sampleRate) / FFT_SIZE;
        if (freq < MIN_FREQ || freq > MAX_FREQ) continue;

        float magnitude = std::hypot(fft_out[i].r, fft_out[i].i);

        // Frequency band splitting
        float lowResponse = freq <= 200.0f ? 1.0f : 
                          (freq < 250.0f ? (250.0f - freq) / 50.0f : 0.0f);

        float highResponse = freq >= 2000.0f ? 1.0f :
                           (freq > 1900.0f ? (freq - 1900.0f) / 100.0f : 0.0f);

        float midResponse = freq >= 250.0f && freq <= 1900.0f ? 1.0f :
                          (freq > 200.0f && freq < 250.0f ? (freq - 200.0f) / 50.0f :
                          (freq > 1900.0f && freq < 2000.0f ? (2000.0f - freq) / 100.0f : 0.0f));

        // EQ balance
        lowResponse *= 0.4f;
        midResponse *= 0.5f;
        highResponse *= 1.2f;

        float combinedGain = (lowResponse * currentLowGain) +
                           (midResponse * currentMidGain) +
                           (highResponse * currentHighGain);

        magnitudesBuffer[i] = magnitude * combinedGain;
    }

    const float NOISE_FLOOR = calculateNoiseFloor(magnitudesBuffer);

    std::vector<FrequencyPeak> rawPeaks;
    for (size_t i = 2; i < binCount - 1; ++i) {
        if (magnitudesBuffer[i] > NOISE_FLOOR &&
            magnitudesBuffer[i] > magnitudesBuffer[i - 1] &&
            magnitudesBuffer[i] > magnitudesBuffer[i + 1])
        {
            float freq = interpolateFrequency(i, sampleRate);
            if (freq >= MIN_FREQ && freq <= MAX_FREQ) {
                rawPeaks.push_back({freq, magnitudesBuffer[i]});
            }
        }
    }

    // Sort peaks by magnitude
    std::sort(rawPeaks.begin(), rawPeaks.end(),
              [](const FrequencyPeak& a, const FrequencyPeak& b) {
                  return a.magnitude > b.magnitude;
              });

    // Filter harmonics with improved accuracy
    std::vector<FrequencyPeak> newPeaks;
    for (const auto& peak : rawPeaks) {
        if (newPeaks.size() >= MAX_PEAKS) break;
        
        bool is_harmonic = false;
        for (const auto& existing : newPeaks) {
            if (isHarmonic(peak.frequency, existing.frequency)) {
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
        currentPeaks = std::move(newPeaks);
        retainedPeaks = currentPeaks;
        lastValidPeakTime = now;
    } else if (now - lastValidPeakTime < PEAK_RETENTION_TIME) {
        currentPeaks = retainedPeaks;
    }
}

bool FFTProcessor::isHarmonic(float testFreq, float baseFreq) const {
    // Dynamic tolerance based on frequency
    float dynamicTolerance = std::min(0.05f, 0.05f * (baseFreq / 250.0f));
    float ratio = testFreq / baseFreq;
    
    for (int h = 2; h <= MAX_HARMONIC; ++h) {
        if (std::abs(ratio - static_cast<float>(h)) < dynamicTolerance)
            return true;
    }
    return false;
}

float FFTProcessor::interpolateFrequency(int bin, float sampleRate) const {
    auto magnitude = [](const kiss_fft_cpx& v) {
        return std::hypot(v.r, v.i);
    };

    const float m0 = magnitude(fft_out[bin - 1]);
    const float m1 = magnitude(fft_out[bin]);
    const float m2 = magnitude(fft_out[bin + 1]);

    const float denominator = m0 - 2.0f * m1 + m2;
    if (std::abs(denominator) < 1e-6f)
        return bin * sampleRate / FFT_SIZE;

    const float alpha = 0.5f * (m0 - m2) / denominator;
    return (bin + alpha) * sampleRate / FFT_SIZE;
}

float FFTProcessor::calculateNoiseFloor(const std::vector<float>& magnitudes) const {
    std::vector<float> mags(magnitudes.begin() + 1, magnitudes.end() - 1);
    if (mags.empty()) return 0.0f;

    const size_t n = mags.size();
    const auto mid_it = mags.begin() + n / 2;
    std::nth_element(mags.begin(), mid_it, mags.end());
    const float median = *mid_it;

    std::vector<float> deviations(n);
    std::transform(mags.begin(), mags.end(), deviations.begin(),
                  [median](float x) { return std::abs(x - median); });

    const auto mid_dev_it = deviations.begin() + n / 2;
    std::nth_element(deviations.begin(), mid_dev_it, deviations.end());
    const float MAD = *mid_dev_it;
    
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
