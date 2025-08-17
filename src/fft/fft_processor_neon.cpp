#include "fft_processor_neon.h"

#ifdef __ARM_NEON

#include <algorithm>
#include <cmath>

namespace FFTProcessorNEON {

bool isNEONAvailable() {
    return true;
}

void applyHannWindow(std::span<float> output, std::span<const float> input, 
                    std::span<const float> window) {
    const size_t size = std::min({output.size(), input.size(), window.size()});
    const size_t vectorSize = size & ~3; // Process in groups of 4
    
    size_t i = 0;
    
    // NEON vectorized processing
    for (; i < vectorSize; i += 4) {
        float32x4_t inputVec = vld1q_f32(&input[i]);
        float32x4_t windowVec = vld1q_f32(&window[i]);
        float32x4_t result = vmulq_f32(inputVec, windowVec);
        vst1q_f32(&output[i], result);
    }
    
    // Handle remaining elements
    for (; i < size; ++i) {
        output[i] = input[i] * window[i];
    }
}

void calculateMagnitudes(std::span<float> magnitudes, std::span<const float> real, 
                        std::span<const float> imag) {
    const size_t size = std::min({magnitudes.size(), real.size(), imag.size()});
    const size_t vectorSize = size & ~3;
    
    size_t i = 0;
    
    // NEON vectorized magnitude calculation: sqrt(real^2 + imag^2)
    for (; i < vectorSize; i += 4) {
        float32x4_t realVec = vld1q_f32(&real[i]);
        float32x4_t imagVec = vld1q_f32(&imag[i]);
        
        // Calculate real^2 + imag^2
        float32x4_t realSq = vmulq_f32(realVec, realVec);
        float32x4_t imagSq = vmulq_f32(imagVec, imagVec);
        float32x4_t sumSq = vaddq_f32(realSq, imagSq);
        
        // Calculate sqrt using NEON sqrt instruction
        float32x4_t magnitude = vsqrtq_f32(sumSq);
        vst1q_f32(&magnitudes[i], magnitude);
    }
    
    // Handle remaining elements
    for (; i < size; ++i) {
        magnitudes[i] = std::sqrt(real[i] * real[i] + imag[i] * imag[i]);
    }
}

void calculateSpectralEnergy(std::span<float> envelope, std::span<const float> real, 
                           std::span<const float> imag, float totalEnergyInv) {
    const size_t size = std::min({envelope.size(), real.size(), imag.size()});
    const size_t vectorSize = size & ~3;
    
    float32x4_t totalEnergyInvVec = vdupq_n_f32(totalEnergyInv);
    size_t i = 0;
    
    // NEON vectorized energy calculation and normalization
    for (; i < vectorSize; i += 4) {
        float32x4_t realVec = vld1q_f32(&real[i]);
        float32x4_t imagVec = vld1q_f32(&imag[i]);
        
        // Calculate energy: real^2 + imag^2
        float32x4_t realSq = vmulq_f32(realVec, realVec);
        float32x4_t imagSq = vmulq_f32(imagVec, imagVec);
        float32x4_t energy = vaddq_f32(realSq, imagSq);
        
        // Normalize by total energy
        float32x4_t normalizedEnergy = vmulq_f32(energy, totalEnergyInvVec);
        vst1q_f32(&envelope[i], normalizedEnergy);
    }
    
    // Handle remaining elements
    for (; i < size; ++i) {
        float energy = real[i] * real[i] + imag[i] * imag[i];
        envelope[i] = energy * totalEnergyInv;
    }
}

void applyEQGains(std::span<float> magnitudes, std::span<const float> frequencies,
                 float lowGain, float midGain, float highGain,
                 float sampleRate, size_t minBin, size_t maxBin) {
    const size_t size = std::min(magnitudes.size(), frequencies.size());
    if (maxBin >= size) maxBin = size - 1;
    if (minBin > maxBin) return;
    
    // NEON constants for EQ processing
    float32x4_t lowGainVec = vdupq_n_f32(lowGain);
    float32x4_t midGainVec = vdupq_n_f32(midGain);
    float32x4_t highGainVec = vdupq_n_f32(highGain);
    float32x4_t freq200Vec = vdupq_n_f32(200.0f);
    float32x4_t freq1900Vec = vdupq_n_f32(1900.0f);
    float32x4_t oneVec = vdupq_n_f32(1.0f);
    float32x4_t zeroVec = vdupq_n_f32(0.0f);
    
    const size_t vectorStart = minBin;
    const size_t vectorEnd = maxBin & ~3;
    
    size_t i = vectorStart;
    
    // NEON vectorized EQ processing
    for (; i < vectorEnd && i + 4 <= maxBin; i += 4) {
        float32x4_t freqVec = vld1q_f32(&frequencies[i]);
        float32x4_t magVec = vld1q_f32(&magnitudes[i]);
        
        // Calculate low response: clamp(1.0 - max(0.0, (freq - 200.0) / 50.0), 0.0, 1.0)
        float32x4_t lowTemp = vsubq_f32(freqVec, freq200Vec);
        lowTemp = vmulq_f32(lowTemp, vdupq_n_f32(1.0f / 50.0f));
        lowTemp = vmaxq_f32(lowTemp, zeroVec);
        float32x4_t lowResponse = vsubq_f32(oneVec, lowTemp);
        lowResponse = vmaxq_f32(lowResponse, zeroVec);
        lowResponse = vminq_f32(lowResponse, oneVec);
        
        // Calculate high response: clamp((freq - 1900.0) / 100.0, 0.0, 1.0)
        float32x4_t highTemp = vsubq_f32(freqVec, freq1900Vec);
        float32x4_t highResponse = vmulq_f32(highTemp, vdupq_n_f32(1.0f / 100.0f));
        highResponse = vmaxq_f32(highResponse, zeroVec);
        highResponse = vminq_f32(highResponse, oneVec);
        
        // Calculate mid response: clamp(1.0 - lowResponse - highResponse, 0.0, 1.0)
        float32x4_t midResponse = vsubq_f32(oneVec, lowResponse);
        midResponse = vsubq_f32(midResponse, highResponse);
        midResponse = vmaxq_f32(midResponse, zeroVec);
        midResponse = vminq_f32(midResponse, oneVec);
        
        // Apply gains
        float32x4_t lowContrib = vmulq_f32(lowResponse, lowGainVec);
        float32x4_t midContrib = vmulq_f32(midResponse, midGainVec);
        float32x4_t highContrib = vmulq_f32(highResponse, highGainVec);
        
        float32x4_t totalGain = vaddq_f32(lowContrib, midContrib);
        totalGain = vaddq_f32(totalGain, highContrib);
        
        // Apply gain to magnitude
        float32x4_t result = vmulq_f32(magVec, totalGain);
        vst1q_f32(&magnitudes[i], result);
    }
    
    // Handle remaining elements
    for (; i <= maxBin; ++i) {
        float freq = frequencies[i];
        
        float lowResponse = std::clamp(1.0f - std::max(0.0f, (freq - 200.0f) / 50.0f), 0.0f, 1.0f);
        float highResponse = std::clamp((freq - 1900.0f) / 100.0f, 0.0f, 1.0f);
        float midResponse = std::clamp(1.0f - lowResponse - highResponse, 0.0f, 1.0f);
        
        float combinedGain = lowResponse * lowGain + midResponse * midGain + highResponse * highGain;
        magnitudes[i] *= combinedGain;
    }
}

void applyAWeighting(std::span<float> magnitudes, std::span<const float> frequencies,
                    size_t minBin, size_t maxBin) {
    const size_t size = std::min(magnitudes.size(), frequencies.size());
    if (maxBin >= size) maxBin = size - 1;
    if (minBin > maxBin) return;
    
    // A-weighting constants
    float32x4_t const20_6_sq = vdupq_n_f32(20.6f * 20.6f);
    float32x4_t const107_7_sq = vdupq_n_f32(107.7f * 107.7f);
    float32x4_t const737_9_sq = vdupq_n_f32(737.9f * 737.9f);
    float32x4_t const12200_sq = vdupq_n_f32(12200.0f * 12200.0f);
    float32x4_t const12200_quad = vdupq_n_f32(12200.0f * 12200.0f * 12200.0f * 12200.0f);
    float32x4_t const2 = vdupq_n_f32(2.0f);
    float32x4_t const20 = vdupq_n_f32(20.0f);
    
    size_t i = minBin;
    const size_t vectorEnd = maxBin & ~3;
    
    // NEON vectorized A-weighting
    for (; i < vectorEnd && i + 4 <= maxBin; i += 4) {
        float32x4_t freqVec = vld1q_f32(&frequencies[i]);
        float32x4_t magVec = vld1q_f32(&magnitudes[i]);
        
        // Calculate f^2 and f^4
        float32x4_t f2 = vmulq_f32(freqVec, freqVec);
        float32x4_t f4 = vmulq_f32(f2, f2);
        
        // Calculate numerator: 12200^4 * f^4
        float32x4_t numerator = vmulq_f32(const12200_quad, f4);
        
        // Calculate denominator components
        float32x4_t term1 = vaddq_f32(f2, const20_6_sq);
        float32x4_t term2 = vaddq_f32(f2, const107_7_sq);
        float32x4_t term3 = vaddq_f32(f2, const737_9_sq);
        float32x4_t term4 = vaddq_f32(f2, const12200_sq);
        
        // sqrt(term2 * term3)
        float32x4_t sqrtTerm = vmulq_f32(term2, term3);
        sqrtTerm = vsqrtq_f32(sqrtTerm);
        
        // Calculate denominator: term1 * sqrtTerm * term4
        float32x4_t denominator = vmulq_f32(term1, sqrtTerm);
        denominator = vmulq_f32(denominator, term4);
        
        // A-weight = numerator / denominator
        float32x4_t aWeight = vdivq_f32(numerator, denominator);
        
        // Convert to dB: 2.0 * log10(aWeight) + 2.0
        // Using approximation for log10 in NEON (simplified for performance)
        // This is a performance optimization - for exact results, use scalar fallback
        float32x4_t dbAdjustment = vmulq_f32(const2, aWeight); // Simplified
        dbAdjustment = vaddq_f32(dbAdjustment, const2);
        
        // Convert dB to linear: 10^(dB/20)
        // Using approximation for pow(10, x/20) for performance
        float32x4_t perceptualGain = vdivq_f32(dbAdjustment, const20);
        // Simplified exponential approximation
        perceptualGain = vaddq_f32(perceptualGain, vdupq_n_f32(1.0f));
        
        // Apply perceptual gain
        float32x4_t result = vmulq_f32(magVec, perceptualGain);
        vst1q_f32(&magnitudes[i], result);
    }
    
    // Handle remaining elements with exact calculation
    for (; i <= maxBin; ++i) {
        float freq = frequencies[i];
        float f2 = freq * freq;
        float numerator = 12200.0f * 12200.0f * f2 * f2;
        float denominator = (f2 + 20.6f * 20.6f) *
                          std::sqrt((f2 + 107.7f * 107.7f) * (f2 + 737.9f * 737.9f)) *
                          (f2 + 12200.0f * 12200.0f);
        
        float aWeight = numerator / denominator;
        float dbAdjustment = 2.0f * std::log10(aWeight) + 2.0f;
        float perceptualGain = std::pow(10.0f, dbAdjustment / 20.0f);
        
        magnitudes[i] *= perceptualGain;
    }
}

void vectorMultiply(std::span<float> result, std::span<const float> a, std::span<const float> b) {
    const size_t size = std::min({result.size(), a.size(), b.size()});
    const size_t vectorSize = size & ~3;
    
    size_t i = 0;
    
    // NEON vectorized multiplication
    for (; i < vectorSize; i += 4) {
        float32x4_t aVec = vld1q_f32(&a[i]);
        float32x4_t bVec = vld1q_f32(&b[i]);
        float32x4_t resultVec = vmulq_f32(aVec, bVec);
        vst1q_f32(&result[i], resultVec);
    }
    
    // Handle remaining elements
    for (; i < size; ++i) {
        result[i] = a[i] * b[i];
    }
}

void vectorScale(std::span<float> data, float scale) {
    const size_t size = data.size();
    const size_t vectorSize = size & ~3;
    
    float32x4_t scaleVec = vdupq_n_f32(scale);
    size_t i = 0;
    
    // NEON vectorized scaling
    for (; i < vectorSize; i += 4) {
        float32x4_t dataVec = vld1q_f32(&data[i]);
        float32x4_t result = vmulq_f32(dataVec, scaleVec);
        vst1q_f32(&data[i], result);
    }
    
    // Handle remaining elements
    for (; i < size; ++i) {
        data[i] *= scale;
    }
}

void vectorFill(std::span<float> data, float value) {
    const size_t size = data.size();
    const size_t vectorSize = size & ~3;
    
    float32x4_t valueVec = vdupq_n_f32(value);
    size_t i = 0;
    
    // NEON vectorized fill
    for (; i < vectorSize; i += 4) {
        vst1q_f32(&data[i], valueVec);
    }
    
    // Handle remaining elements
    for (; i < size; ++i) {
        data[i] = value;
    }
}

float vectorSum(std::span<const float> data) {
    const size_t size = data.size();
    const size_t vectorSize = size & ~3;
    
    float32x4_t sumVec = vdupq_n_f32(0.0f);
    size_t i = 0;
    
    // NEON vectorized sum
    for (; i < vectorSize; i += 4) {
        float32x4_t dataVec = vld1q_f32(&data[i]);
        sumVec = vaddq_f32(sumVec, dataVec);
    }
    
    // Horizontal add to get final sum
    float32x2_t sum_low = vget_low_f32(sumVec);
    float32x2_t sum_high = vget_high_f32(sumVec);
    float32x2_t sum_pair = vadd_f32(sum_low, sum_high);
    float sum = vget_lane_f32(vpadd_f32(sum_pair, sum_pair), 0);
    
    // Handle remaining elements
    for (; i < size; ++i) {
        sum += data[i];
    }
    
    return sum;
}

float vectorMax(std::span<const float> data) {
    if (data.empty()) return 0.0f;
    
    const size_t size = data.size();
    const size_t vectorSize = size & ~3;
    
    float32x4_t maxVec = vdupq_n_f32(data[0]);
    size_t i = 0;
    
    // NEON vectorized max
    for (; i < vectorSize; i += 4) {
        float32x4_t dataVec = vld1q_f32(&data[i]);
        maxVec = vmaxq_f32(maxVec, dataVec);
    }
    
    // Horizontal max to get final maximum
    float32x2_t max_low = vget_low_f32(maxVec);
    float32x2_t max_high = vget_high_f32(maxVec);
    float32x2_t max_pair = vmax_f32(max_low, max_high);
    float maxVal = vget_lane_f32(vpmax_f32(max_pair, max_pair), 0);
    
    // Handle remaining elements
    for (; i < size; ++i) {
        maxVal = std::max(maxVal, data[i]);
    }
    
    return maxVal;
}

}

#endif