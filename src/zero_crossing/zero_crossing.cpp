#include "zero_crossing.h"

#include <algorithm>
#include <numeric>

ZeroCrossingDetector::ZeroCrossingDetector()
	: sampleBuffer(BUFFER_SIZE, 0.0f),
	  lastSample(0.0f),
	  sampleRate(44100.0f),
	  estimatedFrequency(0.0f),
	  zeroCrossingDensity(0.0f),
	  zeroCrossings(0),
	  sampleCount(0) {}

void ZeroCrossingDetector::processSamples(const float* buffer, const size_t numSamples) {
	if (!buffer || numSamples == 0)
		return;

	// Count zero crossings
	size_t localCrossings = 0;
	float currentSample = lastSample;

	for (size_t i = 0; i < numSamples; ++i) {
		const float nextSample = buffer[i];
		if (currentSample <= 0.0f && nextSample > 0.0f) {
			localCrossings++;
		}

		currentSample = nextSample;
	}

	lastSample = currentSample;
	{
		std::lock_guard lock(bufferMutex);

		// Add new samples
		for (size_t i = 0; i < numSamples; ++i) {
			const size_t index = (sampleCount + i) % BUFFER_SIZE;
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
	if (sampleCount == 0)
		return;

	// Calculate crossing density
	const float timeSpan = static_cast<float>(sampleCount) / sampleRate;
	const float newDensity = static_cast<float>(zeroCrossings) / timeSpan;

	// Smoothing
	zeroCrossingDensity = zeroCrossingDensity * 0.7f + newDensity * 0.3f;

	// Estimate frequency
	const float roughFreq = zeroCrossingDensity / 2.0f;

	// Refine the frequency
	std::vector<float> periods;
	periods.reserve(zeroCrossings);

	float prevCrossing = 0.0f;
	bool foundFirst = false;

	for (size_t i = 1; i < BUFFER_SIZE; ++i) {
		const float current = sampleBuffer[i];

		if (const float previous = sampleBuffer[i - 1]; previous <= 0.0f && current > 0.0f) {
			const float t = -previous / (current - previous);
			const float exactPosition = static_cast<float>(i - 1) + t;

			if (foundFirst) {
				if (float period = (exactPosition - prevCrossing) / sampleRate;
					period > MIN_PERIOD && period < MAX_PERIOD) {
					periods.push_back(period);
				}
			}

			prevCrossing = exactPosition;
			foundFirst = true;
		}
	}

	// Calculate median period
	if (!periods.empty()) {
		const size_t middle = periods.size() / 2;
		std::ranges::nth_element(periods, periods.begin() + middle);
		const float medianPeriod = periods[middle];

		// Convert period to frequency
		const float refinedFreq = 1.0f / medianPeriod;

		// Apply smoothing for stable output
		estimatedFrequency = estimatedFrequency * 0.8f + refinedFreq * 0.2f;
	} else {
		estimatedFrequency = roughFreq;	 // Fall back to rough estimate if no valid periods
	}

	sampleCount = BUFFER_SIZE / 2;
	zeroCrossings = 0;
}

float ZeroCrossingDetector::getEstimatedFrequency() const {
	std::lock_guard lock(bufferMutex);
	return estimatedFrequency;
}

float ZeroCrossingDetector::getZeroCrossingDensity() const {
	std::lock_guard lock(bufferMutex);
	return zeroCrossingDensity;
}

void ZeroCrossingDetector::reset() {
	std::lock_guard lock(bufferMutex);

	std::ranges::fill(sampleBuffer, 0.0f);
	lastSample = 0.0f;
	estimatedFrequency = 0.0f;
	zeroCrossingDensity = 0.0f;
	zeroCrossings = 0;
	sampleCount = 0;
}
