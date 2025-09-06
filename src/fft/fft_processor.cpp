#include "fft_processor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

FFTProcessor::FFTProcessor()
	: fft_in(FFT_SIZE),
	  fft_out(FFT_SIZE / 2 + 1),
	  hannWindow(FFT_SIZE),
	  magnitudesBuffer(FFT_SIZE / 2 + 1, 0.0f),
	  spectralEnvelope(FFT_SIZE / 2 + 1, 0.0f),
	  lastValidPeakTime(std::chrono::steady_clock::now()),
	  lowGain(1.0f),
	  midGain(1.0f),
	  highGain(1.0f),
	  currentLoudness(0.0f) {
	fft_cfg = kiss_fftr_alloc(FFT_SIZE, 0, nullptr, nullptr);
	if (!fft_cfg) {
		throw std::runtime_error("Error allocating FFTR configuration.");
	}

	for (size_t i = 0; i < hannWindow.size(); ++i) {
		hannWindow[i] =
			0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i / (FFT_SIZE - 1)));
	}
}

FFTProcessor::~FFTProcessor() {
	if (fft_cfg) {
		kiss_fftr_free(fft_cfg);
		fft_cfg = nullptr;
	}
}

float FFTProcessor::getCurrentLoudness() const {
	std::lock_guard lock(peaksMutex);
	return currentLoudness;
}

void FFTProcessor::setEQGains(const float low, const float mid, const float high) {
	std::lock_guard lock(gainsMutex);
	lowGain = std::max(0.0f, low);
	midGain = std::max(0.0f, mid);
	highGain = std::max(0.0f, high);
}

void FFTProcessor::applyWindow(const std::span<const float> buffer) {
	const size_t copySize = std::min(buffer.size(), static_cast<size_t>(FFT_SIZE));
	std::ranges::fill(fft_in, 0.0f);
	
#ifdef USE_NEON_OPTIMIZATIONS
	if (FFTProcessorNEON::isNEONAvailable() && copySize >= 4) {
		FFTProcessorNEON::applyHannWindow(
			std::span<float>(fft_in.data(), copySize),
			std::span<const float>(buffer.data(), copySize),
			std::span<const float>(hannWindow.data(), copySize)
		);
	} else
#endif
	{
		for (size_t i = 0; i < copySize; ++i) {
			fft_in[i] = buffer[i] * hannWindow[i];
		}
	}
}

void FFTProcessor::processBuffer(const std::span<const float> buffer, const float sampleRate) {
	if (sampleRate <= 0.0f || buffer.empty())
		return;
	std::lock_guard processingLock(processingMutex);

	applyWindow(buffer);
	kiss_fftr(fft_cfg, fft_in.data(), fft_out.data());

	constexpr float scaleFactor = 2.0f / FFT_SIZE;
	for (auto& i : fft_out) {
		i.r *= scaleFactor;
		i.i *= scaleFactor;
	}
	fft_out[0].r *= 0.5f;
	fft_out[0].i *= 0.5f;
	if (fft_out.size() > 1) {
		fft_out[fft_out.size() - 1].r *= 0.5f;
		fft_out[fft_out.size() - 1].i *= 0.5f;
	}

	findFrequencyPeaks(sampleRate);
}

std::vector<FFTProcessor::FrequencyPeak> FFTProcessor::getDominantFrequencies() const {
	std::lock_guard lock(peaksMutex);

	if (currentPeaks.empty() || currentPeaks[0].magnitude < 0.01f)
		return {};

	return currentPeaks;
}

std::vector<float> FFTProcessor::getSpectralEnvelope() const {
	std::lock_guard lock(peaksMutex);
	return spectralEnvelope;
}

std::vector<float> FFTProcessor::getMagnitudesBuffer() const {
	std::lock_guard lock(peaksMutex);
	return magnitudesBuffer;
}

void FFTProcessor::processMagnitudes(std::vector<float>& magnitudes, const float sampleRate,
									 const float maxMagnitude) {
	const float normalisationFactor = maxMagnitude > 1e-6f ? 1.0f / maxMagnitude : 1.0f;
	float currentLowGain, currentMidGain, currentHighGain;
	{
		std::lock_guard gainsLock(gainsMutex);
		currentLowGain = lowGain;
		currentMidGain = midGain;
		currentHighGain = highGain;
	}

	std::ranges::fill(spectralEnvelope, 0.0f);

	const auto minBinIndex = static_cast<size_t>(MIN_FREQ * FFT_SIZE / sampleRate);
	const size_t maxBinIndex =
		std::min(static_cast<size_t>(MAX_FREQ * FFT_SIZE / sampleRate) + 1, fft_out.size() - 1);
	float totalEnergy = 0.0f;
	for (size_t i = minBinIndex; i <= maxBinIndex; ++i) {
		const float energy = fft_out[i].r * fft_out[i].r + fft_out[i].i * fft_out[i].i;
		spectralEnvelope[i] = energy;
		totalEnergy += energy;
	}

	if (totalEnergy > 1e-6f) {
		for (size_t i = minBinIndex; i <= maxBinIndex; ++i) {
			spectralEnvelope[i] /= totalEnergy;
		}
	}

	for (size_t i = minBinIndex; i <= maxBinIndex; ++i) {
		const float freq = static_cast<float>(i) * sampleRate / FFT_SIZE;

		const float melFactor = 1.0f + 2.0f * (1.0f - std::min(1.0f, freq / 1000.0f));
		spectralEnvelope[i] *= melFactor;
	}

	if (const float maxEnvelope = *std::ranges::max_element(spectralEnvelope);
		maxEnvelope > 1e-6f) {
		for (float& i : spectralEnvelope) {
			i /= maxEnvelope;
		}
	}

	for (size_t i = minBinIndex; i <= maxBinIndex; ++i) {
		const float freq = static_cast<float>(i) * sampleRate / FFT_SIZE;

		const float normalisedMagnitude =
			std::sqrt(fft_out[i].r * fft_out[i].r + fft_out[i].i * fft_out[i].i) * normalisationFactor;

		const float lowResponse =
			std::clamp(1.0f - std::max(0.0f, (freq - 200.0f) / 50.0f), 0.0f, 1.0f);
		const float highResponse = std::clamp((freq - 1900.0f) / 100.0f, 0.0f, 1.0f);
		const float midResponse = std::clamp(1.0f - lowResponse - highResponse, 0.0f, 1.0f);

		const float f2 = freq * freq;
		const float numerator = 12200.0f * 12200.0f * f2 * f2;
		const float denominator = (f2 + 20.6f * 20.6f) *
								  std::sqrt((f2 + 107.7f * 107.7f) * (f2 + 737.9f * 737.9f)) *
								  (f2 + 12200.0f * 12200.0f);

		const float aWeight = numerator / denominator;
		const float dbAdjustment = 2.0f * std::log10(aWeight) + 2.0f;
		const float perceptualGain = std::exp(dbAdjustment * 0.11512925f); // ln(10)/20 ≈ 0.11512925

		float combinedGain =
			perceptualGain * (lowResponse * currentLowGain + midResponse * currentMidGain +
							  highResponse * currentHighGain);

		combinedGain = std::clamp(combinedGain, 0.0f, 4.0f);
		magnitudes[i] = normalisedMagnitude * combinedGain;
	}
}

void FFTProcessor::calculateMagnitudes(std::vector<float>& rawMagnitudes, const float sampleRate,
									   float& maxMagnitude, float& totalEnergy) const {
	maxMagnitude = 0.0f;
	totalEnergy = 0.0f;

#ifdef USE_NEON_OPTIMIZATIONS
	if (FFTProcessorNEON::isNEONAvailable() && fft_out.size() >= 4) {
		// Use optimized SIMD function directly on FFT output
		FFTProcessorNEON::calculateMagnitudesFromComplex(
			std::span<float>(rawMagnitudes.data(), rawMagnitudes.size()),
			fft_out.data(), fft_out.size());
		
		for (size_t i = 1; i < fft_out.size() - 1; ++i) {
			const float freq = static_cast<float>(i) * sampleRate / FFT_SIZE;
			if (freq < MIN_FREQ || freq > MAX_FREQ) {
				rawMagnitudes[i] = 0.0f;
				continue;
			}
			
			totalEnergy += rawMagnitudes[i] * rawMagnitudes[i];
			maxMagnitude = std::max(maxMagnitude, rawMagnitudes[i]);
		}
	} else
#endif
	{
		for (size_t i = 1; i < fft_out.size() - 1; ++i) {
			if (const float freq = static_cast<float>(i) * sampleRate / FFT_SIZE;
				freq < MIN_FREQ || freq > MAX_FREQ)
				continue;

			const float magnitudeSquared = fft_out[i].r * fft_out[i].r + fft_out[i].i * fft_out[i].i;
			const float magnitude = std::sqrt(magnitudeSquared);
			rawMagnitudes[i] = magnitude;
			totalEnergy += magnitudeSquared;
			maxMagnitude = std::max(maxMagnitude, magnitude);
		}
	}
}

void FFTProcessor::findPeaks(const float sampleRate, const float noiseFloor,
							 std::vector<FrequencyPeak>& peaks) const {
	candidatePeaksBuffer.clear();
	for (size_t i = 2; i < magnitudesBuffer.size() - 2; ++i) {
		if (magnitudesBuffer[i] > noiseFloor && magnitudesBuffer[i] > magnitudesBuffer[i - 1] &&
			magnitudesBuffer[i] > magnitudesBuffer[i - 2] &&
			magnitudesBuffer[i] > magnitudesBuffer[i + 1] &&
			magnitudesBuffer[i] > magnitudesBuffer[i + 2]) {
			if (const float freq = interpolateFrequency(static_cast<int>(i), sampleRate);
				freq >= MIN_FREQ && freq <= MAX_FREQ) {
				candidatePeaksBuffer.push_back({freq, magnitudesBuffer[i]});
			}
		}
	}

	std::ranges::sort(candidatePeaksBuffer, [](const FrequencyPeak& a, const FrequencyPeak& b) {
		return a.magnitude > b.magnitude;
	});

	const float spectralFlatness = calculateSpectralFlatness(magnitudesBuffer);
	const float harmonic_threshold = spectralFlatness < 0.2f ? 0.15f : 0.5f;

	for (const auto& candidate : candidatePeaksBuffer) {
		if (peaks.size() >= MAX_PEAKS)
			break;

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

float FFTProcessor::calculateSpectralFlatness(const std::vector<float>& magnitudes) {
	float geometricMean = 0.0f;
	float arithmeticMean = 0.0f;
	int count = 0;

	for (const float mag : magnitudes) {
		if (mag > 1e-6f) {
			geometricMean += std::log(mag);
			arithmeticMean += mag;
			count++;
		}
	}

	if (count == 0 || arithmeticMean < 1e-6f)
		return 1.0f;

	geometricMean = std::exp(geometricMean / count);
	arithmeticMean /= count;

	return geometricMean / arithmeticMean;
}

void FFTProcessor::findFrequencyPeaks(const float sampleRate) {
	const size_t binCount = fft_out.size();
	std::vector rawMagnitudes(binCount, 0.0f);
	float maxMagnitude = 0.0f;
	float totalEnergy = 0.0f;

	calculateMagnitudes(rawMagnitudes, sampleRate, maxMagnitude, totalEnergy);

	// Calculate loudness without locking first  
	const float rmsValue = std::sqrt(totalEnergy / static_cast<float>(binCount));
	const float dbFS = 20.0f * std::log10(std::max(rmsValue, 1e-6f));
	const float normalisedLoudness = std::clamp((dbFS + 60.0f) / 60.0f, 0.0f, 1.0f);

	// Update magnitudes buffer under lock (read by UI thread)
	{
		std::lock_guard lock(peaksMutex);
		std::ranges::fill(magnitudesBuffer, 0.0f);
		processMagnitudes(magnitudesBuffer, sampleRate, maxMagnitude);
	}

	const float noiseFloor = calculateNoiseFloor(magnitudesBuffer);
	std::vector<FrequencyPeak> rawPeaks;
	findPeaks(sampleRate, noiseFloor, rawPeaks);

	// Single atomic update of all shared state
	std::lock_guard lock(peaksMutex);
	
	// Update loudness
	currentLoudness = currentLoudness * 0.7f + normalisedLoudness * 0.3f;
	
	// Update peaks
	const auto now = std::chrono::steady_clock::now();
	if (!rawPeaks.empty()) {
		currentPeaks = std::move(rawPeaks);
		retainedPeaks = currentPeaks;
		lastValidPeakTime = now;
	} else if (now - lastValidPeakTime < PEAK_RETENTION_TIME) {
		currentPeaks = retainedPeaks;
	}
}

bool FFTProcessor::isHarmonic(const float testFreq, const float baseFreq, const float threshold) {
	if (baseFreq <= 0.0f || testFreq <= 0.0f)
		return false;

	const float toleranceHz = std::max(3.0f, baseFreq * threshold);

	for (int h = 2; h <= MAX_HARMONIC; ++h) {
		if (const float expectedHarmonic = baseFreq * h;
			std::abs(testFreq - expectedHarmonic) < toleranceHz)
			return true;
	}

	for (int h = 2; h <= MAX_HARMONIC; ++h) {
		if (const float expectedHarmonic = testFreq * h;
			std::abs(baseFreq - expectedHarmonic) < toleranceHz)
			return true;
	}

	return false;
}

float FFTProcessor::interpolateFrequency(const int bin, const float sampleRate) const {
	if (bin <= 0 || bin >= static_cast<int>(fft_out.size()) - 1) {
		return bin * sampleRate / FFT_SIZE;
	}

	auto magnitude = [](const kiss_fft_cpx& v) { return std::sqrt(v.r * v.r + v.i * v.i); };

	const float m0 = magnitude(fft_out[bin - 1]);
	const float m1 = magnitude(fft_out[bin]);
	const float m2 = magnitude(fft_out[bin + 1]);

	const float denominator = m0 - 2.0f * m1 + m2;
	if (std::abs(denominator) < 1e-3f)
		return bin * sampleRate / FFT_SIZE;

	const float alpha = 0.5f * (m0 - m2) / denominator;
	return (bin + alpha) * sampleRate / FFT_SIZE;
}

float FFTProcessor::calculateNoiseFloor(const std::vector<float>& magnitudes) {
	const size_t estimatedCapacity = magnitudes.size() > 2 ? magnitudes.size() - 2 : 0;
	std::vector<float> filteredMags;
	filteredMags.reserve(estimatedCapacity);

	for (size_t i = 1; i < magnitudes.size() - 1; ++i) {
		if (magnitudes[i] > 1e-6f) {
			filteredMags.push_back(magnitudes[i]);
		}
	}

	if (filteredMags.empty())
		return 1e-5f;
	const size_t n = filteredMags.size();
	const size_t medianIdx = n / 2;
	std::ranges::nth_element(filteredMags, filteredMags.begin() + medianIdx);
	const float median = filteredMags[medianIdx];
	const float peak = *std::ranges::max_element(filteredMags);
	const float adaptiveFactor = 0.1f + 0.05f * std::log2(1.0f + peak / (median + 1e-6f));
	const float noiseFloor = median * (1.0f + adaptiveFactor);

	return std::max(noiseFloor, 1e-5f);
}

void FFTProcessor::reset() {
	std::lock_guard lock(peaksMutex);
	currentPeaks.clear();
	retainedPeaks.clear();
	std::ranges::fill(magnitudesBuffer, 0.0f);
	std::ranges::fill(spectralEnvelope, 0.0f);
	lastValidPeakTime = std::chrono::steady_clock::now();
}
