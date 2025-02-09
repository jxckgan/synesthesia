#include "colour_mapper.h"

#include <cmath>
#include <algorithm>
#include <numeric>

// Converts a set of frequencies and magnitudes to a blended colour and a dominant wavelength
ColourMapper::ColourResult ColourMapper::frequenciesToColour(
    const std::vector<float>& frequencies, const std::vector<float>& magnitudes)
{
    // Default result
    ColourResult result { 0.1f, 0.1f, 0.1f, 0.0f };
    if (frequencies.empty() || magnitudes.empty())
        return result;

    // Use the minimum count in case the two vectors differ
    size_t count = std::min(frequencies.size(), magnitudes.size());

    // Calculate total magnitude
    float totalMagnitude = std::accumulate(magnitudes.begin(), magnitudes.begin() + count, 0.0f);
    if (totalMagnitude <= 0.0f)
        return result;

    float r = 0.0f, g = 0.0f, b = 0.0f;
    float wavelengthSum = 0.0f;
    
    // Blend colours from all frequencies based on their magnitude weights
    for (size_t i = 0; i < count; ++i) {
        float weight = magnitudes[i] / totalMagnitude;
        float wavelength = frequencyToWavelength(frequencies[i]);

        float cr, cg, cb;
        wavelengthToRGB(wavelength, cr, cg, cb);
        
        r += cr * weight;
        g += cg * weight;
        b += cb * weight;
        wavelengthSum += wavelength * weight;
    }

    // Apply gamma correction to the blended colour
    constexpr float gamma = 0.8f;
    result.r = std::pow(r, gamma);
    result.g = std::pow(g, gamma);
    result.b = std::pow(b, gamma);
    result.dominantWavelength = wavelengthSum;

    return result;
}

float ColourMapper::frequencyToWavelength(float freq) {
    if (freq <= 0)
        return 0;
    
    constexpr float MIN_FREQ = 20.0f;
    constexpr float MAX_FREQ = 20000.0f;
    
    // Clamp the frequency within [MIN_FREQ, MAX_FREQ]
    float clampedFreq = std::clamp(freq, MIN_FREQ, MAX_FREQ);
    float logFreq    = std::log(clampedFreq);
    float logMinFreq = std::log(MIN_FREQ);
    float logMaxFreq = std::log(MAX_FREQ);
    
    float t = (logFreq - logMinFreq) / (logMaxFreq - logMinFreq);
    return MAX_WAVELENGTH - t * (MAX_WAVELENGTH - MIN_WAVELENGTH);
}

// Converts a wavelength (nm) to an RGB colour value
// Based on mapping from: https://www.endolith.com/wordpress/2010/09/15/a-mapping-between-musical-notes-and-colors/
void ColourMapper::wavelengthToRGB(float wavelength, float& r, float& g, float& b) {
    // Return a default colour if wavelength is zero
    if (wavelength == 0) {
        r = g = b = 0.1f;
        return;
    }

    constexpr float gamma = 0.8f;
    float intensity = 1.0f;

    // Mapping wavelength ranges to RGB values
    if (wavelength >= 696.0f && wavelength <= 737.0f) { // Dark red to red
        float t = (wavelength - 696.0f) / (737.0f - 696.0f);
        r = (255.0f - t * 81.0f) / 255.0f;
        g = 0.0f;
        b = 0.0f;
    } else if (wavelength >= 657.0f && wavelength <= 696.0f) { // Pure red
        r = 1.0f;
        g = 0.0f;
        b = 0.0f;
    } else if (wavelength >= 620.0f && wavelength <= 657.0f) { // Red to orange-red
        float t = (wavelength - 620.0f) / (657.0f - 620.0f);
        r = 1.0f;
        g = (102.0f - t * 102.0f) / 255.0f;
        b = 0.0f;
    } else if (wavelength >= 585.0f && wavelength <= 620.0f) { // Orange-red to yellow
        float t = (wavelength - 585.0f) / (620.0f - 585.0f);
        r = 1.0f;
        g = (239.0f - t * 137.0f) / 255.0f;
        b = 0.0f;
    } else if (wavelength >= 552.0f && wavelength <= 585.0f) { // Yellow to chartreuse
        float t = (wavelength - 552.0f) / (585.0f - 552.0f);
        r = (153.0f + t * 102.0f) / 255.0f;
        g = (255.0f - t * 16.0f) / 255.0f;
        b = 0.0f;
    } else if (wavelength >= 521.0f && wavelength <= 552.0f) { // Chartreuse to lime green
        float t = (wavelength - 521.0f) / (552.0f - 521.0f);
        r = (40.0f + t * 113.0f) / 255.0f;
        g = 1.0f;
        b = 0.0f;
    } else if (wavelength >= 492.0f && wavelength <= 521.0f) { // Green to aqua
        float t = (wavelength - 492.0f) / (521.0f - 492.0f);
        r = t * 40.0f / 255.0f;
        g = 1.0f;
        b = t * 242.0f / 255.0f;
    } else if (wavelength >= 464.0f && wavelength <= 492.0f) { // Aqua to sky blue
        float t = (wavelength - 464.0f) / (492.0f - 464.0f);
        r = 0.0f;
        g = (122.0f + t * 133.0f) / 255.0f;
        b = (255.0f - t * 13.0f) / 255.0f;
    } else if (wavelength >= 438.0f && wavelength <= 464.0f) { // Sky blue to blue
        float t = (wavelength - 438.0f) / (464.0f - 438.0f);
        r = (5.0f - t * 5.0f) / 255.0f;
        g = (t * 122.0f) / 255.0f;
        b = 1.0f;
    } else if (wavelength >= 414.0f && wavelength <= 438.0f) { // Blue to darker blue
        float t = (wavelength - 414.0f) / (438.0f - 414.0f);
        r = (5.0f + t * 66.0f) / 255.0f;
        b = (237.0f + t * 18.0f) / 255.0f;
        g = 0.0f;
    } else if (wavelength >= 390.0f && wavelength <= 414.0f) { // Darker blue to indigo
        float t = (wavelength - 390.0f) / (414.0f - 390.0f);
        r = (71.0f + t * 28.0f) / 255.0f;
        b = (237.0f - t * 59.0f) / 255.0f;
        g = 0.0f;
    } else {
        r = g = b = 0.0f;
    }

    // Adjust intensity for wavelengths outside the optimal visible range
    if (wavelength > 700.0f) {
        intensity = 0.3f + 0.7f * (750.0f - wavelength) / (750.0f - 700.0f);
    } else if (wavelength < 420.0f) {
        intensity = 0.3f + 0.7f * (wavelength - 380.0f) / (420.0f - 380.0f);
    }

    // Apply gamma correction combined with intensity
    r = std::pow(r * intensity, gamma);
    g = std::pow(g * intensity, gamma);
    b = std::pow(b * intensity, gamma);
}
