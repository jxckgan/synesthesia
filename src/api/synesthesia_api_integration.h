#pragma once

#include "server/api_server.h"
#include "client/api_client.h"
#include "../colour/colour_mapper.h"
#include "../fft/fft_processor.h"
#include <memory>
#include <vector>
#include <mutex>

namespace Synesthesia {

enum class ColourSpace {
    RGB = 0,
    LAB = 1,
    XYZ = 2
};

class SynesthesiaAPIIntegration {
public:
    SynesthesiaAPIIntegration();
    ~SynesthesiaAPIIntegration();
    
    bool startServer(const API::ServerConfig& config = {});
    void stopServer();
    bool isServerRunning() const;
    
    void updateFinalColour(float r, float g, float b,
                         const std::vector<float>& frequencies,
                         const std::vector<float>& magnitudes,
                         uint32_t sample_rate,
                         uint32_t fft_size);
    
    void updateSmoothingConfig(bool enabled, float factor);
    void updateFrequencyRange(uint32_t min_freq, uint32_t max_freq);
    void updateColourSpace(ColourSpace colour_space);
    
    std::vector<std::string> getConnectedClients() const;
    size_t getLastDataSize() const;
    
    uint32_t getCurrentFPS() const;
    bool isHighPerformanceMode() const;
    float getAverageFrameTime() const;
    uint64_t getTotalFramesSent() const;
    
    static SynesthesiaAPIIntegration& getInstance();

private:
    std::unique_ptr<API::APIServer> api_server_;
    std::unique_ptr<ColourMapper> colour_mapper_;
    
    mutable std::mutex data_mutex_;
    std::vector<API::ColourData> last_colour_data_;
    uint32_t last_sample_rate_{44100};
    uint32_t last_fft_size_{1024};
    uint64_t last_timestamp_{0};
    
    bool smoothing_enabled_{true};
    float smoothing_factor_{0.8f};
    uint32_t frequency_range_min_{20};
    uint32_t frequency_range_max_{20000};
    ColourSpace current_colour_space_{ColourSpace::RGB};
    
    static std::unique_ptr<SynesthesiaAPIIntegration> instance_;
    static std::mutex instance_mutex_;
};

}