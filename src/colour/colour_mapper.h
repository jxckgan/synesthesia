#pragma once

#include <vector>

class ColourMapper {
public:
    struct ColourResult {
        float r;
        float g;
        float b;
        float dominantWavelength;
    };

    static constexpr float MIN_WAVELENGTH = 380.0f;
    static constexpr float MAX_WAVELENGTH = 750.0f;
    static constexpr float MIN_FREQ = 20.0f;
    static constexpr float MAX_FREQ = 20000.0f;
    
    static ColourResult frequenciesToColour(const std::vector<float>& frequencies, 
                                          const std::vector<float>& magnitudes,
                                          float gamma = 0.8f);
    
private:
    static float frequencyToWavelength(float freq);
    static void wavelengthToRGB(float wavelength, float& r, float& g, float& b);
};