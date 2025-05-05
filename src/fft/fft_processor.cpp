#include "fft_processor.h"
#include <cmath>
#include <numeric>
#include <algorithm>
#include <stdexcept>
#include <chrono>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

FFTProcessor::FFTProcessor()
    : fft_in(FFT_SIZE)
    , fft_out(FFT_SIZE / 2 + 1)
    , hannWindow(FFT_SIZE)
    , magnitudesBuffer(FFT_SIZE / 2 + 1, 0.0f)
    , spectralEnvelope(FFT_SIZE / 2 + 1, 0.0f)
    , lastValidPeakTime(std::chrono::steady_clock::now())
    , lowGain(1.0f)
    , midGain(1.0f)
    , highGain(1.0f)
    , currentLoudness(0.0f)
{
    fft_cfg = kiss_fftr_alloc(FFT_SIZE, 0, nullptr, nullptr);
    if (!fft_cfg) {
        throw std::runtime_error("Error allocating FFTR configuration.");
    }
    
    for (size_t i = 0; i < hannWindow.size(); ++i) {
        hannWindow[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i / (FFT_SIZE - 1)));
    }
}

FFTProcessor::~FFTProcessor() {
    if (fft_cfg) {
        kiss_fftr_free(fft_cfg);
        fft_cfg = nullptr;
    }
}

float FFTProcessor::getCurrentLoudness() const {
    std::lock_guard<std::mutex> lock(peaksMutex);
    return currentLoudness;
}

void FFTProcessor::setEQGains(float low, float mid, float high) {
    std::lock_guard<std::mutex> lock(gainsMutex);
    lowGain = std::max(0.0f, low);
    midGain = std::max(0.0f, mid);
    highGain = std::max(0.0f, high);
}

void FFTProcessor::applyWindow(const std::span<const float> buffer) {
    size_t copySize = std::min(buffer.size(), static_cast<size_t>(FFT_SIZE));
    std::fill(fft_in.begin(), fft_in.end(), 0.0f);
    for (size_t i = 0; i < copySize; ++i) {
        fft_in[i] = buffer[i] * hannWindow[i];
    }
}

void FFTProcessor::processBuffer(const std::span<const float> buffer, float sampleRate) {
    if (sampleRate <= 0.0f || buffer.empty()) return;
    std::lock_guard<std::mutex> processingLock(processingMutex);

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

std::vector<FFTProcessor::FrequencyPeak> FFTProcessor::getDominantFrequencies() const {
    std::lock_guard<std::mutex> lock(peaksMutex);
    
    if (currentPeaks.empty() || currentPeaks[0].magnitude < 0.01f)
        return {};
        
    return currentPeaks;
}

std::vector<float> FFTProcessor::getSpectralEnvelope() const {
    std::lock_guard<std::mutex> lock(peaksMutex);
    return spectralEnvelope;
}

std::vector<float> FFTProcessor::getMagnitudesBuffer() const {
    std::lock_guard<std::mutex> lock(peaksMutex);
    return magnitudesBuffer;
}

void FFTProcessor::processMagnitudes(std::vector<float>& magnitudes, float sampleRate, float maxMagnitude) {
    float normalisationFactor = (maxMagnitude > 1e-6f) ? 1.0f / maxMagnitude : 1.0f;
    float currentLowGain, currentMidGain, currentHighGain;
    {
        std::lock_guard<std::mutex> gainsLock(gainsMutex);
        currentLowGain = lowGain;
        currentMidGain = midGain;
        currentHighGain = highGain;
    }
    
    std::fill(spectralEnvelope.begin(), spectralEnvelope.end(), 0.0f);
    
    // Calculate bin frequency limit indices
    const size_t minBinIndex = static_cast<size_t>(MIN_FREQ * FFT_SIZE / sampleRate);
    const size_t maxBinIndex = std::min(static_cast<size_t>(MAX_FREQ * FFT_SIZE / sampleRate) + 1, fft_out.size() - 1);
    float totalEnergy = 0.0f;
    for (size_t i = minBinIndex; i <= maxBinIndex; ++i) {
        // Calculate raw magnitude
        float rawMagnitude = std::hypot(fft_out[i].r, fft_out[i].i);
        float energy = rawMagnitude * rawMagnitude;
        spectralEnvelope[i] = energy;
        totalEnergy += energy;
    }
    
    // Normalise the spectral envelope by energy
    if (totalEnergy > 1e-6f) {
        for (size_t i = minBinIndex; i <= maxBinIndex; ++i) {
            spectralEnvelope[i] /= totalEnergy;
        }
    }
    
    // Apply mel-frequency weighting to spectral envelope
    for (size_t i = minBinIndex; i <= maxBinIndex; ++i) {
        float freq = (static_cast<float>(i) * sampleRate) / FFT_SIZE;
        
        // Convert to mel scale (emphasizes lower frequencies)
        float melFactor = 1.0f + 2.0f * (1.0f - std::min(1.0f, freq / 1000.0f));
        spectralEnvelope[i] *= melFactor;
    }
    
    // Re-normalise after mel weighting
    float maxEnvelope = *std::max_element(spectralEnvelope.begin(), spectralEnvelope.end());
    if (maxEnvelope > 1e-6f) {
        for (size_t i = 0; i < spectralEnvelope.size(); ++i) {
            spectralEnvelope[i] /= maxEnvelope;
        }
    }
    
    // Process magnitudes with EQ
    for (size_t i = minBinIndex; i <= maxBinIndex; ++i) {
        float freq = (static_cast<float>(i) * sampleRate) / FFT_SIZE;

        // Calculate raw magnitude with normalisation
        float normalisedMagnitude = std::hypot(fft_out[i].r, fft_out[i].i) * normalisationFactor;
        
        // Frequency band splitting
        float lowResponse = std::clamp(1.0f - std::max(0.0f, (freq - 200.0f) / 50.0f), 0.0f, 1.0f);
        float highResponse = std::clamp((freq - 1900.0f) / 100.0f, 0.0f, 1.0f);
        float midResponse = std::clamp(1.0f - lowResponse - highResponse, 0.0f, 1.0f);

        // Perceptual Hearing curve
        float f2 = freq * freq;
        float numerator = 12200.0f * 12200.0f * f2 * f2;
        float denominator = (f2 + 20.6f * 20.6f) * 
                           std::sqrt((f2 + 107.7f * 107.7f) * 
                           (f2 + 737.9f * 737.9f)) * 
                           (f2 + 12200.0f * 12200.0f);
                           
        float aWeight = numerator / denominator;
        float dbAdjustment = 2.0f * std::log10(aWeight) + 2.0f;
        float perceptualGain = std::pow(10.0f, dbAdjustment / 20.0f);

        // Combine with user EQ
        float combinedGain = perceptualGain * 
                           (lowResponse * currentLowGain +
                            midResponse * currentMidGain +
                            highResponse * currentHighGain);
        
        combinedGain = std::clamp(combinedGain, 0.0f, 4.0f);
        magnitudes[i] = normalisedMagnitude * combinedGain;
    }
}

void FFTProcessor::calculateMagnitudes(std::vector<float>& rawMagnitudes, float sampleRate,
                                     float& maxMagnitude, float& totalEnergy) {
    maxMagnitude = 0.0f;
    totalEnergy = 0.0f;
    
    for (size_t i = 1; i < fft_out.size() - 1; ++i) {
        float freq = (static_cast<float>(i) * sampleRate) / FFT_SIZE;
        if (freq < MIN_FREQ || freq > MAX_FREQ) continue;

        // Calculate raw magnitude without normalisation
        float magnitude = std::hypot(fft_out[i].r, fft_out[i].i);
        rawMagnitudes[i] = magnitude;
        totalEnergy += magnitude * magnitude;
        maxMagnitude = std::max(maxMagnitude, magnitude);
    }
}

void FFTProcessor::findPeaks(float sampleRate, float noiseFloor, std::vector<FrequencyPeak>& peaks) {
    std::vector<FrequencyPeak> candidatePeaks;
    for (size_t i = 2; i < magnitudesBuffer.size() - 2; ++i) {
        if (magnitudesBuffer[i] > noiseFloor &&
            magnitudesBuffer[i] > magnitudesBuffer[i - 1] &&
            magnitudesBuffer[i] > magnitudesBuffer[i - 2] &&
            magnitudesBuffer[i] > magnitudesBuffer[i + 1] &&
            magnitudesBuffer[i] > magnitudesBuffer[i + 2])
        {
            float freq = interpolateFrequency(static_cast<int>(i), sampleRate);
            if (freq >= MIN_FREQ && freq <= MAX_FREQ) {
                candidatePeaks.push_back({freq, magnitudesBuffer[i]});
            }
        }
    }
    
    // Sort candidates by magnitude
    std::sort(candidatePeaks.begin(), candidatePeaks.end(),
              [](const FrequencyPeak& a, const FrequencyPeak& b) {
                  return a.magnitude > b.magnitude;
              });
              
    // Calculate spectral flatness to determine if the sound is tonal
    float spectralFlatness = calculateSpectralFlatness(magnitudesBuffer);
    float harmonic_threshold = spectralFlatness < 0.2f ? 0.15f : 0.5f;
    
    // Add peaks (removing harmonics based on our adaptive threshold)
    for (const auto& candidate : candidatePeaks) {
        if (peaks.size() >= MAX_PEAKS) break;

        bool isHarmonicFlag = false;
        for (const auto& existing : peaks) {
            if (isHarmonic(candidate.frequency, existing.frequency, harmonic_threshold)) {
                isHarmonicFlag = true;
                break;
            }
        }
        
        if (!isHarmonicFlag) {
            peaks.push_back(candidate);
        }
    }
}

float FFTProcessor::calculateSpectralFlatness(const std::vector<float>& magnitudes) const {
    float geometricMean = 0.0f;
    float arithmeticMean = 0.0f;
    int count = 0;
    
    for (size_t i = 0; i < magnitudes.size(); ++i) {
        float mag = magnitudes[i];
        if (mag > 1e-6f) {
            geometricMean += std::log(mag);
            arithmeticMean += mag;
            count++;
        }
    }
    
    if (count == 0 || arithmeticMean < 1e-6f) return 1.0f;
    
    geometricMean = std::exp(geometricMean / count);
    arithmeticMean /= count;
    
    return geometricMean / arithmeticMean;
}

void FFTProcessor::findFrequencyPeaks(float sampleRate) {
    {
        std::lock_guard<std::mutex> lock(peaksMutex);
        std::fill(magnitudesBuffer.begin(), magnitudesBuffer.end(), 0.0f);
        currentPeaks.clear();
    }

    const size_t binCount = fft_out.size();
    std::vector<float> rawMagnitudes(binCount, 0.0f);
    float maxMagnitude = 0.0f;
    float totalEnergy = 0.0f;
    
    calculateMagnitudes(rawMagnitudes, sampleRate, maxMagnitude, totalEnergy);
    
    // Calculate RMS value and convert to logarithmic scale
    float rmsValue = std::sqrt(totalEnergy / static_cast<float>(binCount));
    float dbFS = 20.0f * std::log10(std::max(rmsValue, 1e-6f));
    
    // Normalise dbFS to a 0-1 range (-60 dB to 0 dB)
    float normalisedLoudness = std::clamp((dbFS + 60.0f) / 60.0f, 0.0f, 1.0f);

    {
        std::lock_guard<std::mutex> lock(peaksMutex);
        currentLoudness = (currentLoudness * 0.7f) + (normalisedLoudness * 0.3f);
    }

    // Process magnitudes with EQ
    processMagnitudes(magnitudesBuffer, sampleRate, maxMagnitude);

    // Find peaks
    float noiseFloor = calculateNoiseFloor(magnitudesBuffer);
    std::vector<FrequencyPeak> rawPeaks;
    findPeaks(sampleRate, noiseFloor, rawPeaks);

    // Lock for updating shared data
    std::lock_guard<std::mutex> lock(peaksMutex);
    
    const auto now = std::chrono::steady_clock::now();
    if (!rawPeaks.empty()) {
        currentPeaks = std::move(rawPeaks);
        retainedPeaks = currentPeaks;
        lastValidPeakTime = now;
    } else if (now - lastValidPeakTime < PEAK_RETENTION_TIME) {
        currentPeaks = retainedPeaks;
    }
}

bool FFTProcessor::isHarmonic(float testFreq, float baseFreq, float threshold) const {
    if (baseFreq <= 0.0f || testFreq <= 0.0f)
        return false;
        
    float toleranceHz = std::max(3.0f, baseFreq * threshold);
    
    for (int h = 2; h <= MAX_HARMONIC; ++h) {
        float expectedHarmonic = baseFreq * h;
        if (std::abs(testFreq - expectedHarmonic) < toleranceHz)
            return true;
    }
    
    for (int h = 2; h <= MAX_HARMONIC; ++h) {
        float expectedHarmonic = testFreq * h;
        if (std::abs(baseFreq - expectedHarmonic) < toleranceHz)
            return true;
    }
    
    return false;
}

float FFTProcessor::interpolateFrequency(int bin, float sampleRate) const {
    if (bin <= 0 || bin >= static_cast<int>(fft_out.size()) - 1) {
        return bin * sampleRate / FFT_SIZE;
    }

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
    const size_t estimatedCapacity = magnitudes.size() > 2 ? magnitudes.size() - 2 : 0;
    std::vector<float> filteredMags;
    filteredMags.reserve(estimatedCapacity);
    
    for (size_t i = 1; i < magnitudes.size() - 1; ++i) {
        if (magnitudes[i] > 1e-6f) {
            filteredMags.push_back(magnitudes[i]);
        }
    }
    
    if (filteredMags.empty()) return 1e-5f;
    const size_t n = filteredMags.size();
    const size_t medianIdx = n / 2;
    std::nth_element(filteredMags.begin(), filteredMags.begin() + medianIdx, filteredMags.end());
    const float median = filteredMags[medianIdx];
    float peak = *std::max_element(filteredMags.begin(), filteredMags.end());
    float adaptiveFactor = 0.1f + (0.05f * std::log2(1.0f + peak / (median + 1e-6f)));
    float noiseFloor = median * (1.0f + adaptiveFactor);

    return std::max(noiseFloor, 1e-5f);
}

void FFTProcessor::reset() {
    std::lock_guard<std::mutex> lock(peaksMutex);
    currentPeaks.clear();
    retainedPeaks.clear();
    std::fill(magnitudesBuffer.begin(), magnitudesBuffer.end(), 0.0f);
    std::fill(spectralEnvelope.begin(), spectralEnvelope.end(), 0.0f);
    lastValidPeakTime = std::chrono::steady_clock::now();
}
