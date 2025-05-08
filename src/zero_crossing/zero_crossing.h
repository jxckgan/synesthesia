#pragma once

#include <mutex>
#include <vector>

class ZeroCrossingDetector {
public:
	ZeroCrossingDetector();

	void processSamples(const float* buffer, size_t numSamples);
	float getEstimatedFrequency() const;
	float getZeroCrossingDensity() const;
	void reset();

private:
	static constexpr size_t BUFFER_SIZE = 4096;
	static constexpr float MIN_PERIOD = 0.00005f;
	static constexpr float MAX_PERIOD = 0.05f;

	std::vector<float> sampleBuffer;
	mutable std::mutex bufferMutex;

	float lastSample;
	float sampleRate;
	float estimatedFrequency;
	float zeroCrossingDensity;

	size_t zeroCrossings;
	size_t sampleCount;

	void analyseZeroCrossings();
};
