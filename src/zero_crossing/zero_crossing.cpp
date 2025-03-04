#include "zero_crossing.h"
#include <algorithm>
#include <numeric>
#include <cmath>

ZeroCrossingDetector::ZeroCrossingDetector() 
    : sampleBuffer(BUFFER_SIZE, 0.0f)
    , lastSample(0.0f)
    , sampleRate(44100.0f)
    , estimatedFrequency(0.0f)
    , zeroCrossingDensity(0.0f)
    , zeroCrossings(0)
    , sampleCount(0)
{
}

void ZeroCrossingDetector::processSamples(const float* buffer, size_t numSamples, float rate) {
    if (!buffer || numSamples == 0) return;
    
    sampleRate = rate;
    
    // Count zero crossings
    size_t localCrossings = 0;
    float currentSample = lastSample;
    
    for (size_t i = 0; i < numSamples; ++i) {
        float nextSample = buffer[i];
        if (currentSample <= 0.0f && nextSample > 0.0f) {
            localCrossings++;
        }
        
        currentSample = nextSample;
    }
    
    lastSample = currentSample;
    {
        std::lock_guard<std::mutex> lock(bufferMutex);
        
        // Add new samples
        for (size_t i = 0; i < numSamples; ++i) {
            size_t index = (sampleCount + i) % BUFFER_SIZE;
            sampleBuffer[index] = buffer[i];
        }
        
        sampleCount += numSamples;
        zeroCrossings += localCrossings;
        
        if (sampleCount >= BUFFER_SIZE / 2) {
            analyseZeroCrossings();
        }
    }
}

void ZeroCrossingDetector::analyseZeroCrossings() {
    if (sampleCount == 0) return;
    
    // Calculate crossing density
    float timeSpan = static_cast<float>(sampleCount) / sampleRate;
    float newDensity = static_cast<float>(zeroCrossings) / timeSpan;
    
    // Smoothing
    zeroCrossingDensity = zeroCrossingDensity * 0.7f + newDensity * 0.3f;
    
    // Estimate frequency
    float roughFreq = zeroCrossingDensity / 2.0f;
    
    // Refine the frequency
    std::vector<float> periods;
    periods.reserve(zeroCrossings);
    
    float prevCrossing = 0.0f;
    bool foundFirst = false;
    
    for (size_t i = 1; i < BUFFER_SIZE; ++i) {
        float current = sampleBuffer[i];
        float previous = sampleBuffer[i-1];
        
        if (previous <= 0.0f && current > 0.0f) {
            float t = -previous / (current - previous);
            float exactPosition = static_cast<float>(i - 1) + t;
            
            if (foundFirst) {
                float period = (exactPosition - prevCrossing) / sampleRate;
                if (period > MIN_PERIOD && period < MAX_PERIOD) {
                    periods.push_back(period);
                }
            }
            
            prevCrossing = exactPosition;
            foundFirst = true;
        }
    }
    
    // Calculate median period
    if (!periods.empty()) {
        size_t middle = periods.size() / 2;
        std::nth_element(periods.begin(), periods.begin() + middle, periods.end());
        float medianPeriod = periods[middle];
        
        // Convert period to frequency
        float refinedFreq = 1.0f / medianPeriod;
        
        // Apply smoothing for stable output
        estimatedFrequency = estimatedFrequency * 0.8f + refinedFreq * 0.2f;
    } else {
        estimatedFrequency = roughFreq; // Fall back to rough estimate if no valid periods
    }

    sampleCount = BUFFER_SIZE / 2;
    zeroCrossings = 0;
}

float ZeroCrossingDetector::getEstimatedFrequency() const {
    std::lock_guard<std::mutex> lock(bufferMutex);
    return estimatedFrequency;
}

float ZeroCrossingDetector::getZeroCrossingDensity() const {
    std::lock_guard<std::mutex> lock(bufferMutex);
    return zeroCrossingDensity;
}

void ZeroCrossingDetector::reset() {
    std::lock_guard<std::mutex> lock(bufferMutex);
    
    std::fill(sampleBuffer.begin(), sampleBuffer.end(), 0.0f);
    lastSample = 0.0f;
    estimatedFrequency = 0.0f;
    zeroCrossingDensity = 0.0f;
    zeroCrossings = 0;
    sampleCount = 0;
}
