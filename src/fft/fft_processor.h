#pragma once

#include <chrono>
#include <mutex>
#include <span>
#include <vector>

#include "kiss_fftr.h"

#ifdef USE_NEON_OPTIMIZATIONS
#include "fft_processor_neon.h"
#endif

class FFTProcessor {
public:
	static constexpr int FFT_SIZE = 2048;
	static constexpr float MIN_FREQ = 20.0f;
	static constexpr float MAX_FREQ = 20000.0f;
	static constexpr int MAX_HARMONIC = 8;
	static constexpr int MAX_PEAKS = 100;

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

	void processBuffer(std::span<const float> buffer, float sampleRate);
	std::vector<FrequencyPeak> getDominantFrequencies() const;
	std::vector<float> getMagnitudesBuffer() const;
	std::vector<float> getSpectralEnvelope() const;
	float getCurrentLoudness() const;
	void reset();
	void setEQGains(float low, float mid, float high);

private:
	kiss_fftr_cfg fft_cfg;
	std::vector<float> fft_in;
	std::vector<kiss_fft_cpx> fft_out;

	std::vector<FrequencyPeak> currentPeaks;
	mutable std::vector<FrequencyPeak> candidatePeaksBuffer; // Pre-allocated buffer for hot path
	mutable std::mutex peaksMutex;

	std::vector<float> hannWindow;
	std::vector<float> magnitudesBuffer;
	std::vector<float> spectralEnvelope;

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

	void applyWindow(std::span<const float> buffer);
	void findFrequencyPeaks(float sampleRate);
	float interpolateFrequency(int bin, float sampleRate) const;
	static float calculateNoiseFloor(const std::vector<float>& magnitudes);
	static bool isHarmonic(float testFreq, float baseFreq, float threshold = 0.03f);

	static float calculateSpectralFlatness(const std::vector<float>& magnitudes);

	void calculateMagnitudes(std::vector<float>& rawMagnitudes, float sampleRate,
							 float& maxMagnitude, float& totalEnergy) const;

	void processMagnitudes(std::vector<float>& magnitudes, float sampleRate, float maxMagnitude);
	void findPeaks(float sampleRate, float noiseFloor, std::vector<FrequencyPeak>& peaks) const;
};
