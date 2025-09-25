#include "synesthesia_api_integration.h"
#include <chrono>
#include <algorithm>

namespace Synesthesia {

std::unique_ptr<SynesthesiaAPIIntegration> SynesthesiaAPIIntegration::instance_;
std::mutex SynesthesiaAPIIntegration::instance_mutex_;

SynesthesiaAPIIntegration::SynesthesiaAPIIntegration() 
    : colour_mapper_(std::make_unique<ColourMapper>()) {
}

SynesthesiaAPIIntegration::~SynesthesiaAPIIntegration() {
    stopServer();
}

bool SynesthesiaAPIIntegration::startServer(const API::ServerConfig& config) {
    if (api_server_ && api_server_->isRunning()) {
        return true;
    }
    
    api_server_ = std::make_unique<API::APIServer>(config);
    api_server_->setColourDataProvider([this](uint32_t& sample_rate, uint32_t& fft_size, uint64_t& timestamp) -> std::vector<API::ColourData> {
        std::lock_guard<std::mutex> lock(data_mutex_);
        sample_rate = last_sample_rate_;
        fft_size = last_fft_size_;
        timestamp = last_timestamp_;
        return last_colour_data_;
    });
    
    api_server_->setConfigUpdateCallback([this](const API::ConfigUpdate& config) {
        updateSmoothingConfig(config.smoothing_enabled != 0, config.smoothing_factor);
        updateFrequencyRange(config.frequency_range_min, config.frequency_range_max);
        
        ColourSpace space = ColourSpace::RGB;
        switch (config.colour_space) {
            case 0: space = ColourSpace::RGB; break;
            case 1: space = ColourSpace::LAB; break;
            case 2: space = ColourSpace::XYZ; break;
        }
        updateColourSpace(space);
    });
    
    return api_server_->start();
}

void SynesthesiaAPIIntegration::stopServer() {
    if (api_server_) {
        api_server_->stop();
        api_server_.reset();
    }
}

bool SynesthesiaAPIIntegration::isServerRunning() const {
    return api_server_ && api_server_->isRunning();
}


void SynesthesiaAPIIntegration::updateFinalColour(float r, float g, float b,
                                                const std::vector<float>& frequencies, 
                                                const std::vector<float>& magnitudes,
                                                uint32_t sample_rate,
                                                uint32_t fft_size) {
    if (!api_server_ || !api_server_->isRunning()) {
        return;
    }
    
    std::vector<API::ColourData> colour_data;
    
    size_t data_size = std::min(frequencies.size(), magnitudes.size());
    colour_data.reserve(data_size);
    
    for (size_t i = 0; i < data_size; ++i) {
        float frequency = frequencies[i];
        float magnitude = magnitudes[i];
        
        if (frequency < frequency_range_min_ || frequency > frequency_range_max_) {
            continue;
        }
        
        if (magnitude < 0.001f) {
            continue;
        }
        
        API::ColourData data{};
        data.frequency = frequency;
        data.magnitude = magnitude;
        data.phase = 0.0f;
        data.wavelength = ColourMapper::logFrequencyToWavelength(frequency);
        
        float scale = std::min(magnitude * 2.0f, 1.0f);

        switch (current_colour_space_) {
            case ColourSpace::RGB:
                data.r = r * scale;
                data.g = g * scale;
                data.b = b * scale;
                break;

            case ColourSpace::LAB: {
                float L, a, b_comp;
                ColourMapper::RGBtoLab(r * scale, g * scale, b * scale, L, a, b_comp);
                data.r = L;
                data.g = a;
                data.b = b_comp;
                break;
            }

            case ColourSpace::XYZ: {
                // Fall back to RGB for now
                data.r = r * scale;
                data.g = g * scale;
                data.b = b * scale;
                break;
            }
        }
        
        colour_data.push_back(data);
    }
    
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        last_colour_data_ = std::move(colour_data);
        last_sample_rate_ = sample_rate;
        last_fft_size_ = fft_size;
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
        last_timestamp_ = static_cast<uint64_t>(std::max(duration, static_cast<decltype(duration)>(0)));
    }
}

void SynesthesiaAPIIntegration::updateSmoothingConfig(bool enabled, float factor) {
    smoothing_enabled_ = enabled;
    smoothing_factor_ = std::clamp(factor, 0.0f, 1.0f);
    
    if (api_server_ && api_server_->isRunning()) {
        API::ConfigUpdate config{};
        config.smoothing_enabled = enabled ? 1 : 0;
        config.smoothing_factor = factor;
        config.colour_space = static_cast<uint32_t>(current_colour_space_);
        config.frequency_range_min = frequency_range_min_;
        config.frequency_range_max = frequency_range_max_;
        
        api_server_->broadcastConfigUpdate(config);
    }
}

void SynesthesiaAPIIntegration::updateFrequencyRange(uint32_t min_freq, uint32_t max_freq) {
    frequency_range_min_ = min_freq;
    frequency_range_max_ = max_freq;
}

void SynesthesiaAPIIntegration::updateColourSpace(ColourSpace colour_space) {
    current_colour_space_ = colour_space;
}

std::vector<std::string> SynesthesiaAPIIntegration::getConnectedClients() const {
    if (!api_server_) {
        return {};
    }
    return api_server_->getConnectedClients();
}

size_t SynesthesiaAPIIntegration::getLastDataSize() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return last_colour_data_.size();
}

uint32_t SynesthesiaAPIIntegration::getCurrentFPS() const {
    if (!api_server_) return 0;
    return api_server_->getCurrentFPS();
}

bool SynesthesiaAPIIntegration::isHighPerformanceMode() const {
    if (!api_server_) return false;
    return api_server_->isHighPerformanceMode();
}

float SynesthesiaAPIIntegration::getAverageFrameTime() const {
    if (!api_server_) return 0.0f;
    return api_server_->getAverageFrameTime();
}

uint64_t SynesthesiaAPIIntegration::getTotalFramesSent() const {
    if (!api_server_) return 0;
    return api_server_->getTotalFramesSent();
}

SynesthesiaAPIIntegration& SynesthesiaAPIIntegration::getInstance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) {
        instance_ = std::make_unique<SynesthesiaAPIIntegration>();
    }
    return *instance_;
}


}