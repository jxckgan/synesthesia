#include "audio_processor.h"

#include <algorithm>

AudioProcessor::AudioProcessor()
	: writeIndex(0),
	  readIndex(0),
	  running(false),
	  currentColour{0.1f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
	  currentDominantFrequency(0.0f) {}

AudioProcessor::~AudioProcessor() { stop(); }

void AudioProcessor::start() {
	if (running.exchange(true))
		return;
	writeIndex = 0;
	readIndex = 0;

	workerThread = std::thread(&AudioProcessor::processingThreadFunc, this);
}

void AudioProcessor::stop() {
	if (!running.exchange(false))
		return;
	{
		std::lock_guard lock(queueMutex);
		dataAvailable.notify_one();
	}

	if (workerThread.joinable()) {
		workerThread.join();
	}
}

void AudioProcessor::queueAudioData(const float* buffer, const size_t numSamples,
									const float sampleRate) {
	if (!buffer || numSamples == 0 || !running)
		return;

	const size_t currentWrite = writeIndex.load(std::memory_order_relaxed);
	const size_t nextWrite = (currentWrite + 1) % QUEUE_SIZE;

	if (nextWrite == readIndex.load(std::memory_order_relaxed)) {
		return;
	}

	AudioBuffer& queuedBuffer = audioQueue[currentWrite];
	queuedBuffer.sampleRate = sampleRate;
	queuedBuffer.sampleCount = std::min(numSamples, MAX_SAMPLES);
	std::copy_n(buffer, queuedBuffer.sampleCount, queuedBuffer.data.begin());

	writeIndex.store(nextWrite, std::memory_order_release);
	{
		std::lock_guard lock(queueMutex);
		dataAvailable.notify_one();
	}
}

void AudioProcessor::processingThreadFunc() {
	while (running) {
		std::unique_lock lock(queueMutex);
		dataAvailable.wait(lock, [this] {
			return !running || readIndex.load(std::memory_order_relaxed) !=
								   writeIndex.load(std::memory_order_relaxed);
		});

		if (!running)
			break;

		lock.unlock();

		while (running) {
			const size_t currentRead = readIndex.load(std::memory_order_relaxed);
			if (currentRead == writeIndex.load(std::memory_order_relaxed)) {
				break;
			}

			processBuffer(audioQueue[currentRead]);
			readIndex.store((currentRead + 1) % QUEUE_SIZE, std::memory_order_release);
		}
	}
}

void AudioProcessor::processBuffer(const AudioBuffer& buffer) {
	fftProcessor.processBuffer(std::span(buffer.data.data(), buffer.sampleCount),
							   buffer.sampleRate);
	zeroCrossingDetector.processSamples(buffer.data.data(), buffer.sampleCount);

	tempPeaks = fftProcessor.getDominantFrequencies();

	if (const float zcFreq = zeroCrossingDetector.getEstimatedFrequency();
		zcFreq > 20.0f && zcFreq < 20000.0f) {
		bool foundMatch = false;
		for (auto& peak : tempPeaks) {
			if (const float freqRatio = peak.frequency / zcFreq;
				freqRatio > 0.95f && freqRatio < 1.05f) {
				peak.frequency = zcFreq;
				foundMatch = true;
				break;
			}
		}

		if (!foundMatch && tempPeaks.size() < FFTProcessor::MAX_PEAKS) {
			const float zcDensity = zeroCrossingDetector.getZeroCrossingDensity();
			const float estimatedMagnitude = std::min(1.0f, zcDensity / 1000.0f);

			tempPeaks.push_back({zcFreq, estimatedMagnitude});

			std::ranges::sort(tempPeaks, [](const FFTProcessor::FrequencyPeak& a,
										const FFTProcessor::FrequencyPeak& b) {
				return a.magnitude > b.magnitude;
			});

			if (tempPeaks.size() > FFTProcessor::MAX_PEAKS) {
				tempPeaks.resize(FFTProcessor::MAX_PEAKS);
			}
		}
	}

	tempFreqs.clear();
	tempMags.clear();
	tempFreqs.reserve(tempPeaks.size());
	tempMags.reserve(tempPeaks.size());
	
	for (const auto& peak : tempPeaks) {
		tempFreqs.push_back(peak.frequency);
		tempMags.push_back(peak.magnitude);
	}

	{
		std::lock_guard lock(resultsMutex);
		currentPeaks = std::move(tempPeaks);
		currentColour = ColourMapper::frequenciesToColour(tempFreqs, tempMags, {}, 44100.0f, 1.0f, true);
		currentDominantFrequency = !currentPeaks.empty() ? currentPeaks[0].frequency : 0.0f;
	}
}

std::vector<FFTProcessor::FrequencyPeak> AudioProcessor::getFrequencyPeaks() const {
	std::lock_guard lock(resultsMutex);
	return currentPeaks;
}

void AudioProcessor::getColourForCurrentFrequency(float& r, float& g, float& b, float& freq,
												  float& wavelength) const {
	std::lock_guard lock(resultsMutex);
	r = currentColour.r;
	g = currentColour.g;
	b = currentColour.b;
	freq = currentDominantFrequency;
	wavelength = currentColour.dominantWavelength;
}

void AudioProcessor::setEQGains(const float low, const float mid, const float high) {
	fftProcessor.setEQGains(low, mid, high);
}

void AudioProcessor::reset() {
	fftProcessor.reset();
	zeroCrossingDetector.reset();

	std::lock_guard lock(resultsMutex);
	currentColour = {0.1f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	currentDominantFrequency = 0.0f;
	currentPeaks.clear();
}
