#pragma once

#ifdef __ARM_NEON
#include <arm_neon.h>
#include <span>
#include <vector>

namespace ColourMapperNEON {
    void rgbToLab(std::span<const float> r, std::span<const float> g, std::span<const float> b,
                  std::span<float> L, std::span<float> a, std::span<float> b_comp, size_t count);
    
    void labToRgb(std::span<const float> L, std::span<const float> a, std::span<const float> b_comp,
                  std::span<float> r, std::span<float> g, std::span<float> b, size_t count);
    
    void xyzToRgb(std::span<const float> X, std::span<const float> Y, std::span<const float> Z,
                  std::span<float> r, std::span<float> g, std::span<float> b, size_t count);
    
    void rgbToXyz(std::span<const float> r, std::span<const float> g, std::span<const float> b,
                  std::span<float> X, std::span<float> Y, std::span<float> Z, size_t count);
    
    void vectorLerp(std::span<float> result, std::span<const float> a, std::span<const float> b,
                   std::span<const float> t, size_t count);
    
    void vectorClamp(std::span<float> data, float min_val, float max_val, size_t count);
    
    void vectorPow(std::span<float> data, float exponent, size_t count);
    
    void vectorLog(std::span<float> result, std::span<const float> input, size_t count);
    
    void vectorExp(std::span<float> result, std::span<const float> input, size_t count);
    
    void vectorSqrt(std::span<float> result, std::span<const float> input, size_t count);
    
    void frequenciesToWavelengths(std::span<float> wavelengths, std::span<const float> frequencies,
                                 size_t count);
    
    void weightedColorBlend(std::span<float> result_L, std::span<float> result_a, std::span<float> result_b,
                           std::span<const float> L_values, std::span<const float> a_values, 
                           std::span<const float> b_values, std::span<const float> weights, size_t count);
    
    bool isNEONAvailable();
}

#endif
