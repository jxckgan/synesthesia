#include "audio_processor.h"

AudioProcessor::AudioProcessor()
    : writeIndex(0)
    , readIndex(0)
    , running(false)
    , currentColour{0.1f, 0.1f, 0.1f, 0.0f}
    , currentDominantFrequency(0.0f)
{
}

AudioProcessor::~AudioProcessor() {
    stop();
}

void AudioProcessor::start() {
    if (running.exchange(true)) return;
    writeIndex = 0;
    readIndex = 0;

    workerThread = std::thread(&AudioProcessor::processingThreadFunc, this);
}

void AudioProcessor::stop() {
    if (!running.exchange(false)) return;
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        dataAvailable.notify_one();
    }

    if (workerThread.joinable()) {
        workerThread.join();
    }
}

void AudioProcessor::queueAudioData(const float* buffer, size_t numSamples, float sampleRate) {
    if (!buffer || numSamples == 0 || !running) return;
    
    // Calculate next write position
    size_t currentWrite = writeIndex.load(std::memory_order_relaxed);
    size_t nextWrite = (currentWrite + 1) % QUEUE_SIZE;
    
    if (nextWrite == readIndex.load(std::memory_order_acquire)) {
        return;
    }
    
    // Copy buffer data
    AudioBuffer& queuedBuffer = audioQueue[currentWrite];
    queuedBuffer.sampleRate = sampleRate;
    queuedBuffer.sampleCount = std::min(numSamples, MAX_SAMPLES);
    std::copy(buffer, buffer + queuedBuffer.sampleCount, queuedBuffer.data.begin());
    
    // Update write index
    writeIndex.store(nextWrite, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        dataAvailable.notify_one();
    }
}

void AudioProcessor::processingThreadFunc() {
    while (running) {
        // Wait for data or shutdown signal
        std::unique_lock<std::mutex> lock(queueMutex);
        dataAvailable.wait(lock, [this] {
            return !running || readIndex.load(std::memory_order_relaxed) != 
                              writeIndex.load(std::memory_order_relaxed);
        });
        
        if (!running) break;
        
        lock.unlock();
        
        // Process available buffers
        while (running) {
            size_t currentRead = readIndex.load(std::memory_order_relaxed);
            if (currentRead == writeIndex.load(std::memory_order_acquire)) {
                break;
            }

            processBuffer(audioQueue[currentRead]);
            readIndex.store((currentRead + 1) % QUEUE_SIZE, std::memory_order_release);
        }
    }
}

void AudioProcessor::processBuffer(const AudioBuffer& buffer) {
    // Process with both FFT and zero-crossing detector
    fftProcessor.processBuffer(buffer.data.data(), buffer.sampleCount, buffer.sampleRate);
    zeroCrossingDetector.processSamples(buffer.data.data(), buffer.sampleCount, buffer.sampleRate);
    
    // Get new frequency peaks
    std::vector<FFTProcessor::FrequencyPeak> peaks = fftProcessor.getDominantFrequencies();
    
    // Check for valid frequency
    float zcFreq = zeroCrossingDetector.getEstimatedFrequency();
    if (zcFreq > 20.0f && zcFreq < 20000.0f) {
        bool foundMatch = false;
        
        // Check for similar frequency in peaks
        for (auto& peak : peaks) {
            float freqRatio = peak.frequency / zcFreq;
            if (freqRatio > 0.9f && freqRatio < 1.1f) {
                // Average the frequencies
                peak.frequency = peak.frequency * 0.3f + zcFreq * 0.7f;
                foundMatch = true;
                break;
            }
        }
        
        if (!foundMatch && peaks.size() < FFTProcessor::MAX_PEAKS) {
            // Estimate magnitude based on zero-crossing density
            float zcDensity = zeroCrossingDetector.getZeroCrossingDensity();
            float estimatedMagnitude = std::min(1.0f, zcDensity / 1000.0f);
            
            peaks.push_back({zcFreq, estimatedMagnitude});
            
            // Sort peaks by magnitude
            std::sort(peaks.begin(), peaks.end(),
                [](const FFTProcessor::FrequencyPeak& a, const FFTProcessor::FrequencyPeak& b) {
                    return a.magnitude > b.magnitude;
                });
                
            // Keep (only) the top peaks
            if (peaks.size() > FFTProcessor::MAX_PEAKS) {
                peaks.resize(FFTProcessor::MAX_PEAKS);
            }
        }
    }
    
    // Extract frequencies and magnitudes for colour mapping
    std::vector<float> freqs, mags;
    for (const auto& peak : peaks) {
        freqs.push_back(peak.frequency);
        mags.push_back(peak.magnitude);
    }
    
    // Update results
    {
        std::lock_guard<std::mutex> lock(resultsMutex);
        currentPeaks = peaks;
        currentColour = ColourMapper::frequenciesToColour(freqs, mags);
        currentDominantFrequency = (!peaks.empty()) ? peaks[0].frequency : 0.0f;
    }
}

std::vector<FFTProcessor::FrequencyPeak> AudioProcessor::getFrequencyPeaks() const {
    std::lock_guard<std::mutex> lock(resultsMutex);
    return currentPeaks;
}

void AudioProcessor::getColourForCurrentFrequency(float& r, float& g, float& b, float& freq, float& wavelength) {
    std::lock_guard<std::mutex> lock(resultsMutex);
    r = currentColour.r;
    g = currentColour.g;
    b = currentColour.b;
    freq = currentDominantFrequency;
    wavelength = currentColour.dominantWavelength;
}

void AudioProcessor::setEQGains(float low, float mid, float high) {
    fftProcessor.setEQGains(low, mid, high);
}

void AudioProcessor::reset() {
    fftProcessor.reset();
    zeroCrossingDetector.reset();
    
    std::lock_guard<std::mutex> lock(resultsMutex);
    currentColour = {0.1f, 0.1f, 0.1f, 0.0f};
    currentDominantFrequency = 0.0f;
    currentPeaks.clear();
}
