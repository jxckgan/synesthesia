#include "zero_crossing.h"

#include <algorithm>
#include <numeric>

constexpr float DENSITY_SMOOTH_FACTOR = 0.7f;
constexpr float DENSITY_NEW_FACTOR = 0.3f;
constexpr float FREQ_SMOOTH_FACTOR = 0.8f;
constexpr float FREQ_NEW_FACTOR = 0.2f;

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

	const float timeSpan = static_cast<float>(sampleCount) / sampleRate;
	const float newDensity = static_cast<float>(zeroCrossings) / timeSpan;

	zeroCrossingDensity = zeroCrossingDensity * DENSITY_SMOOTH_FACTOR + newDensity * DENSITY_NEW_FACTOR;

	const float roughFreq = zeroCrossingDensity / 2.0f;

	std::vector<float> periods;
	periods.reserve(zeroCrossings);

	float prevCrossing = 0.0f;
	bool foundFirst = false;

	const size_t samplesToAnalyse = std::min(sampleCount, BUFFER_SIZE);
	
	for (size_t i = 1; i < samplesToAnalyse; ++i) {
		const size_t currentIdx = i % BUFFER_SIZE;
		const size_t prevIdx = (i - 1) % BUFFER_SIZE;
		
		const float current = sampleBuffer[currentIdx];
		const float previous = sampleBuffer[prevIdx];

		if (previous <= 0.0f && current > 0.0f) {
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

	if (!periods.empty()) {
		const size_t middle = periods.size() / 2;
		std::ranges::nth_element(periods, periods.begin() + middle);
		const float medianPeriod = periods[middle];

		const float refinedFreq = 1.0f / medianPeriod;

		estimatedFrequency = estimatedFrequency * FREQ_SMOOTH_FACTOR + refinedFreq * FREQ_NEW_FACTOR;
	} else {
		estimatedFrequency = roughFreq;
	}

	const size_t keepSamples = BUFFER_SIZE / 2;
	if (sampleCount > keepSamples) {
		for (size_t i = 0; i < keepSamples; ++i) {
			const size_t srcIdx = (sampleCount - keepSamples + i) % BUFFER_SIZE;
			sampleBuffer[i] = sampleBuffer[srcIdx];
		}
		sampleCount = keepSamples;
	}
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
