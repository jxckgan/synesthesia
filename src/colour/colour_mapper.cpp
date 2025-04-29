#include "colour_mapper.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <array>

void ColourMapper::interpolateCIE(float wavelength, float& X, float& Y, float& Z) {
    // Clamp wavelength to valid CIE 1931 range
    wavelength = std::clamp(wavelength, 380.0f, 825.0f);
    
    // Calculate index
    float indexFloat = (wavelength - 380.0f) / 5.0f;
    size_t index = static_cast<size_t>(std::floor(indexFloat));
    index = std::min(index, CIE_TABLE_SIZE - 2);

    const auto& entry0 = CIE_1931[index];
    const auto& entry1 = CIE_1931[index + 1];

    float lambda0 = entry0[0];
    float lambda1 = entry1[0];
    float t = (lambda1 != lambda0) ? (wavelength - lambda0) / (lambda1 - lambda0) : 0.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    
    X = std::lerp(entry0[1], entry1[1], t);
    Y = std::lerp(entry0[2], entry1[2], t);
    Z = std::lerp(entry0[3], entry1[3], t);
}

void ColourMapper::XYZtoRGB(float X, float Y, float Z, float& r, float& g, float& b) {
    r =  3.2406f * X - 1.5372f * Y - 0.4986f * Z;
    g = -0.9689f * X + 1.8758f * Y + 0.0415f * Z;
    b =  0.0557f * X - 0.2040f * Y + 1.0570f * Z;

    auto gammaCorrect = [](float c) {
        return (c <= 0.0031308f) ? 
            12.92f * c : 
            1.055f * std::pow(c, 1.0f/2.4f) - 0.055f;
    };

    r = std::clamp(gammaCorrect(r), 0.0f, 1.0f);
    g = std::clamp(gammaCorrect(g), 0.0f, 1.0f);
    b = std::clamp(gammaCorrect(b), 0.0f, 1.0f);
}

void ColourMapper::RGBtoXYZ(float r, float g, float b, float& X, float& Y, float& Z) {
    r = std::clamp(r, 0.0f, 1.0f);
    g = std::clamp(g, 0.0f, 1.0f);
    b = std::clamp(b, 0.0f, 1.0f);
    
    auto inverseGamma = [](float c) {
        return (c <= 0.04045f) ? 
            c / 12.92f : 
            std::pow((c + 0.055f) / 1.055f, 2.4f);
    };
    
    float r_linear = inverseGamma(r);
    float g_linear = inverseGamma(g);
    float b_linear = inverseGamma(b);
    
    X = 0.4124f * r_linear + 0.3576f * g_linear + 0.1805f * b_linear;
    Y = 0.2126f * r_linear + 0.7152f * g_linear + 0.0722f * b_linear;
    Z = 0.0193f * r_linear + 0.1192f * g_linear + 0.9505f * b_linear;
}

// Convert XYZ to Lab colour space
void ColourMapper::XYZtoLab(float X, float Y, float Z, float& L, float& a, float& b) {
    // Prevent division by zero
    const float epsilon = 0.008856f;
    const float kappa = 903.3f;
    
    // Normalise XYZ by reference white point
    float xr = X / (REF_X + epsilon);
    float yr = Y / (REF_Y + epsilon);
    float zr = Z / (REF_Z + epsilon);
    
    // Apply nonlinear compression
    auto f = [epsilon, kappa](float t) {
        return (t > epsilon) ? 
            std::pow(t, 1.0f/3.0f) : 
            (kappa * t + 16.0f) / 116.0f;
    };
    
    float fx = f(xr);
    float fy = f(yr);
    float fz = f(zr);
    
    L = 116.0f * fy - 16.0f;
    a = 500.0f * (fx - fy);
    b = 200.0f * (fy - fz);
    
    // Ensure Lab values are in reasonable ranges
    L = std::clamp(L, 0.0f, 100.0f);
    a = std::clamp(a, -128.0f, 127.0f);
    b = std::clamp(b, -128.0f, 127.0f);
}

// Convert Lab to XYZ colour space
void ColourMapper::LabtoXYZ(float L, float a, float b, float& X, float& Y, float& Z) {
    L = std::clamp(L, 0.0f, 100.0f);
    a = std::clamp(a, -128.0f, 127.0f);
    b = std::clamp(b, -128.0f, 127.0f);
    
    // Compute f(Y) from L
    float fY = (L + 16.0f) / 116.0f;
    float fX = fY + a / 500.0f;
    float fZ = fY - b / 200.0f;
    
    // Constants for the piecewise function
    const float delta = 6.0f / 29.0f;
    const float delta_squared = delta * delta;
    
    // Inverse nonlinear compression
    auto fInv = [delta, delta_squared](float t) {
        return (t > delta) ? 
            std::pow(t, 3.0f) : 
            3.0f * delta_squared * (t - 4.0f / 29.0f);
    };
    
    X = REF_X * fInv(fX);
    Y = REF_Y * fInv(fY);
    Z = REF_Z * fInv(fZ);
    
    // Ensure non-negative values
    X = std::max(0.0f, X);
    Y = std::max(0.0f, Y);
    Z = std::max(0.0f, Z);
}

// Convert RGB to Lab colour space
void ColourMapper::RGBtoLab(float r, float g, float b, float& L, float& a, float& b_comp) {
    float X, Y, Z;
    RGBtoXYZ(r, g, b, X, Y, Z);
    XYZtoLab(X, Y, Z, L, a, b_comp);
}

// Convert Lab to RGB colour space
void ColourMapper::LabtoRGB(float L, float a, float b_comp, float& r, float& g, float& b) {
    float X, Y, Z;
    LabtoXYZ(L, a, b_comp, X, Y, Z);
    XYZtoRGB(X, Y, Z, r, g, b);
}

void ColourMapper::wavelengthToRGBCIE(float wavelength, float& r, float& g, float& b) {
    if (!std::isfinite(wavelength)) {
        wavelength = MIN_WAVELENGTH;
    }
    
    float X, Y, Z;
    interpolateCIE(wavelength, X, Y, Z);
    XYZtoRGB(X, Y, Z, r, g, b); 
}

// Logarithmic frequency to wavelength mapping
float ColourMapper::logFrequencyToWavelength(float freq) {
    if (!std::isfinite(freq) || freq <= 0.001f) return MIN_WAVELENGTH;
    
    // Calculate octave position
    const float MIN_LOG_FREQ = std::log2(MIN_FREQ);
    const float MAX_LOG_FREQ = std::log2(MAX_FREQ);
    const float LOG_FREQ_RANGE = MAX_LOG_FREQ - MIN_LOG_FREQ;
    
    float logFreq = std::log2(freq);
    float normalisedLogFreq = (logFreq - MIN_LOG_FREQ) / LOG_FREQ_RANGE;
    float t = std::clamp(normalisedLogFreq, 0.0f, 1.0f);
    
    // Invert wavelength for an intuitive visualisation (bass notes are dark red, etc)
    return MAX_WAVELENGTH - t * (MAX_WAVELENGTH - MIN_WAVELENGTH);
}

ColourMapper::ColourResult ColourMapper::frequenciesToColour(
    const std::vector<float>& frequencies, const std::vector<float>& magnitudes, 
    float gamma)
{
    // Default dark result for no input
    ColourResult result {0.1f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    
    // Validate inputs
    if (frequencies.empty() || magnitudes.empty()) {
        return result;
    }

    size_t count = std::min(frequencies.size(), magnitudes.size());
    std::vector<float> weights(count, 0.0f);
    float totalWeight = 0.0f;
    
    size_t validCount = 0;
    float maxValidMagnitude = 0.0f;

    // Validate data and find max magnitude for normalisation
    for (size_t i = 0; i < count; ++i) {
        if (!std::isfinite(frequencies[i]) || !std::isfinite(magnitudes[i]) || 
            frequencies[i] <= 0.0f || magnitudes[i] < 0.0f) {
            continue;
        }
        
        validCount++;
        maxValidMagnitude = std::max(maxValidMagnitude, magnitudes[i]);
    }
    
    // Early return if no valid data
    if (validCount == 0 || maxValidMagnitude <= 0.0f) {
        return result;
    }

    // Normalise weighting
    for (size_t i = 0; i < count; ++i) {
        if (!std::isfinite(frequencies[i]) || !std::isfinite(magnitudes[i]) || 
            frequencies[i] <= 0.0f || magnitudes[i] < 0.0f) {
            continue;
        }
        
        float weight = magnitudes[i];
        weights[i] = weight;
        totalWeight += weight;
    }

    if (totalWeight <= 0.0f) {
        return result;
    }

    // Work in LAB space for better perceptual blending
    float L_blend = 0.0f;
    float a_blend = 0.0f;
    float b_blend = 0.0f;
    
    float maxComponent = 0.0f;
    float dominantWavelength = 0.0f;
    float dominantFrequency = 0.0f;

    // Convert each frequency a colour and accumulate in Lab space
    for (size_t i = 0; i < count; ++i) {
        if (weights[i] <= 0.0f) continue;
        
        float weight = weights[i] / totalWeight;
        float wavelength = logFrequencyToWavelength(frequencies[i]);
        
        // Get colour in RGB
        float r, g, b;
        wavelengthToRGBCIE(wavelength, r, g, b);
        
        // Convert to Lab for perceptual blending
        float L, a, b_comp;
        RGBtoLab(r, g, b, L, a, b_comp);
        
        // Weighted contribution to final colour
        L_blend += L * weight;
        a_blend += a * weight;
        b_blend += b_comp * weight;

        if (weight > maxComponent) {
            maxComponent = weight;
            dominantWavelength = wavelength;
            dominantFrequency = frequencies[i];
        }
    }

    // Convert blended Lab back to RGB
    LabtoRGB(L_blend, a_blend, b_blend, result.r, result.g, result.b);
    
    // Apply gamma scaling
    gamma = std::clamp(gamma, 0.1f, 5.0f);
    result.r = std::pow(std::clamp(result.r, 0.0f, 1.0f), gamma);
    result.g = std::pow(std::clamp(result.g, 0.0f, 1.0f), gamma);
    result.b = std::pow(std::clamp(result.b, 0.0f, 1.0f), gamma);

    // Store physical properties
    result.dominantWavelength = dominantWavelength;
    result.dominantFrequency = dominantFrequency;
    result.colourIntensity = 0.2126f*result.r + 0.7152f*result.g + 0.0722f*result.b;
    
    // Store Lab components
    result.L = L_blend;
    result.a = a_blend;
    result.b_comp = b_blend;

    return result;
}
