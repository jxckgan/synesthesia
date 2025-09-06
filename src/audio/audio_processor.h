#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include "colour_mapper.h"
#include "fft_processor.h"
#include "zero_crossing.h"

class AudioProcessor {
public:
	AudioProcessor();
	~AudioProcessor();

	void queueAudioData(const float* buffer, size_t numSamples, float sampleRate);

	std::vector<FFTProcessor::FrequencyPeak> getFrequencyPeaks() const;
	void getColourForCurrentFrequency(float& r, float& g, float& b, float& freq,
									  float& wavelength) const;
	void setEQGains(float low, float mid, float high);
	void setNoiseGateThreshold(float threshold);
	void reset();
	void start();
	void stop();

	FFTProcessor& getFFTProcessor() { return fftProcessor; }
	ZeroCrossingDetector& getZeroCrossingDetector() { return zeroCrossingDetector; }

private:
	static constexpr size_t QUEUE_SIZE = 16;
	static constexpr size_t MAX_SAMPLES = 4096;

	struct AudioBuffer {
		std::vector<float> data;
		size_t sampleCount;
		float sampleRate;

		AudioBuffer() : data(MAX_SAMPLES), sampleCount(0), sampleRate(44100.0f) {}
	};

	AudioBuffer audioQueue[QUEUE_SIZE];
	std::atomic<size_t> writeIndex;
	std::atomic<size_t> readIndex;
	std::thread workerThread;
	std::atomic<bool> running;
	std::condition_variable dataAvailable;
	std::mutex queueMutex;

	FFTProcessor fftProcessor;
	ZeroCrossingDetector zeroCrossingDetector;

	mutable std::mutex resultsMutex;
	ColourMapper::ColourResult currentColour;
	float currentDominantFrequency;
	std::vector<FFTProcessor::FrequencyPeak> currentPeaks;
	
	// Pre-allocated buffers for hot path optimization
	std::vector<FFTProcessor::FrequencyPeak> tempPeaks;
	std::vector<float> tempFreqs;
	std::vector<float> tempMags;

	void processingThreadFunc();
	void processBuffer(const AudioBuffer& buffer);
};
