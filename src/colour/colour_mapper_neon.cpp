#include "colour_mapper_neon.h"

#ifdef __ARM_NEON

#include <algorithm>
#include <cmath>

namespace ColourMapperNEON {

bool isNEONAvailable() {
    return true;
}
namespace AccurateMath {
    inline float32x4_t accuratePow(float32x4_t x, float exponent) {
        float results[4];
        float inputs[4];
        vst1q_f32(inputs, x);
        
        for (int i = 0; i < 4; ++i) {
            results[i] = std::pow(inputs[i], exponent);
        }
        
        return vld1q_f32(results);
    }
    
    inline float32x4_t accurateLog(float32x4_t x) {
        float results[4];
        float inputs[4];
        vst1q_f32(inputs, x);
        
        for (int i = 0; i < 4; ++i) {
            results[i] = std::log(inputs[i]);
        }
        
        return vld1q_f32(results);
    }
    
    inline float32x4_t accurateLog2(float32x4_t x) {
        float results[4];
        float inputs[4];
        vst1q_f32(inputs, x);
        
        for (int i = 0; i < 4; ++i) {
            results[i] = std::log2(inputs[i]);
        }
        
        return vld1q_f32(results);
    }
    
    inline float32x4_t accurateExp(float32x4_t x) {
        float results[4];
        float inputs[4];
        vst1q_f32(inputs, x);
        
        for (int i = 0; i < 4; ++i) {
            results[i] = std::exp(inputs[i]);
        }
        
        return vld1q_f32(results);
    }
}

void rgbToXyz(std::span<const float> r, std::span<const float> g, std::span<const float> b,
              std::span<float> X, std::span<float> Y, std::span<float> Z, size_t count) {
    const size_t size = std::min({r.size(), g.size(), b.size(), X.size(), Y.size(), Z.size(), count});
    const size_t vectorSize = size & ~3;
    
    // sRGB to XYZ matrix coefficients
    float32x4_t r_to_x = vdupq_n_f32(0.4124f);
    float32x4_t g_to_x = vdupq_n_f32(0.3576f);
    float32x4_t b_to_x = vdupq_n_f32(0.1805f);
    
    float32x4_t r_to_y = vdupq_n_f32(0.2126f);
    float32x4_t g_to_y = vdupq_n_f32(0.7152f);
    float32x4_t b_to_y = vdupq_n_f32(0.0722f);
    
    float32x4_t r_to_z = vdupq_n_f32(0.0193f);
    float32x4_t g_to_z = vdupq_n_f32(0.1192f);
    float32x4_t b_to_z = vdupq_n_f32(0.9505f);
    
    // Gamma correction constants (use inverse for multiplication instead of division)
    float32x4_t gamma_threshold = vdupq_n_f32(0.04045f);
    float32x4_t gamma_factor = vdupq_n_f32(1.0f / 12.92f);
    float32x4_t gamma_offset = vdupq_n_f32(0.055f);
    float32x4_t gamma_divisor_inv = vdupq_n_f32(1.0f / 1.055f);
    
    size_t i = 0;
    
    for (; i < vectorSize; i += 4) {
        float32x4_t rVec = vld1q_f32(&r[i]);
        float32x4_t gVec = vld1q_f32(&g[i]);
        float32x4_t bVec = vld1q_f32(&b[i]);
        
        rVec = vmaxq_f32(vminq_f32(rVec, vdupq_n_f32(1.0f)), vdupq_n_f32(0.0f));
        gVec = vmaxq_f32(vminq_f32(gVec, vdupq_n_f32(1.0f)), vdupq_n_f32(0.0f));
        bVec = vmaxq_f32(vminq_f32(bVec, vdupq_n_f32(1.0f)), vdupq_n_f32(0.0f));
        
        uint32x4_t r_mask = vcleq_f32(rVec, gamma_threshold);
        uint32x4_t g_mask = vcleq_f32(gVec, gamma_threshold);
        uint32x4_t b_mask = vcleq_f32(bVec, gamma_threshold);
        
        // Linear portion
        float32x4_t r_linear_low = vmulq_f32(rVec, gamma_factor);
        float32x4_t g_linear_low = vmulq_f32(gVec, gamma_factor);
        float32x4_t b_linear_low = vmulq_f32(bVec, gamma_factor);
        
        float32x4_t r_plus_offset = vaddq_f32(rVec, gamma_offset);
        float32x4_t g_plus_offset = vaddq_f32(gVec, gamma_offset);
        float32x4_t b_plus_offset = vaddq_f32(bVec, gamma_offset);
        
        float32x4_t r_normalized = vmulq_f32(r_plus_offset, gamma_divisor_inv);
        float32x4_t g_normalized = vmulq_f32(g_plus_offset, gamma_divisor_inv);
        float32x4_t b_normalized = vmulq_f32(b_plus_offset, gamma_divisor_inv);
        
        float32x4_t r_linear_high = AccurateMath::accuratePow(r_normalized, 2.4f);
        float32x4_t g_linear_high = AccurateMath::accuratePow(g_normalized, 2.4f);
        float32x4_t b_linear_high = AccurateMath::accuratePow(b_normalized, 2.4f);
        
        // Select between linear and power portions
        float32x4_t r_linear = vbslq_f32(r_mask, r_linear_low, r_linear_high);
        float32x4_t g_linear = vbslq_f32(g_mask, g_linear_low, g_linear_high);
        float32x4_t b_linear = vbslq_f32(b_mask, b_linear_low, b_linear_high);
        
        float32x4_t X_result = vmulq_f32(r_linear, r_to_x);
        X_result = vmlaq_f32(X_result, g_linear, g_to_x);
        X_result = vmlaq_f32(X_result, b_linear, b_to_x);
        
        float32x4_t Y_result = vmulq_f32(r_linear, r_to_y);
        Y_result = vmlaq_f32(Y_result, g_linear, g_to_y);
        Y_result = vmlaq_f32(Y_result, b_linear, b_to_y);
        
        float32x4_t Z_result = vmulq_f32(r_linear, r_to_z);
        Z_result = vmlaq_f32(Z_result, g_linear, g_to_z);
        Z_result = vmlaq_f32(Z_result, b_linear, b_to_z);
        
        vst1q_f32(&X[i], X_result);
        vst1q_f32(&Y[i], Y_result);
        vst1q_f32(&Z[i], Z_result);
    }
    
    for (; i < size; ++i) {
        float rVal = std::clamp(r[i], 0.0f, 1.0f);
        float gVal = std::clamp(g[i], 0.0f, 1.0f);
        float bVal = std::clamp(b[i], 0.0f, 1.0f);
        
        auto inverseGamma = [](float c) {
            return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
        };
        
        float r_linear = inverseGamma(rVal);
        float g_linear = inverseGamma(gVal);
        float b_linear = inverseGamma(bVal);
        
        X[i] = 0.4124f * r_linear + 0.3576f * g_linear + 0.1805f * b_linear;
        Y[i] = 0.2126f * r_linear + 0.7152f * g_linear + 0.0722f * b_linear;
        Z[i] = 0.0193f * r_linear + 0.1192f * g_linear + 0.9505f * b_linear;
    }
}

void xyzToRgb(std::span<const float> X, std::span<const float> Y, std::span<const float> Z,
              std::span<float> r, std::span<float> g, std::span<float> b, size_t count) {
    const size_t size = std::min({X.size(), Y.size(), Z.size(), r.size(), g.size(), b.size(), count});
    const size_t vectorSize = size & ~3;
    
    // XYZ to sRGB matrix coefficients
    float32x4_t x_to_r = vdupq_n_f32(3.2406f);
    float32x4_t y_to_r = vdupq_n_f32(-1.5372f);
    float32x4_t z_to_r = vdupq_n_f32(-0.4986f);
    
    float32x4_t x_to_g = vdupq_n_f32(-0.9689f);
    float32x4_t y_to_g = vdupq_n_f32(1.8758f);
    float32x4_t z_to_g = vdupq_n_f32(0.0415f);
    
    float32x4_t x_to_b = vdupq_n_f32(0.0557f);
    float32x4_t y_to_b = vdupq_n_f32(-0.2040f);
    float32x4_t z_to_b = vdupq_n_f32(1.0570f);
    
    // Gamma correction constants
    float32x4_t gamma_threshold = vdupq_n_f32(0.0031308f);
    float32x4_t gamma_factor = vdupq_n_f32(12.92f);
    float32x4_t gamma_multiplier = vdupq_n_f32(1.055f);
    float32x4_t gamma_offset = vdupq_n_f32(0.055f);
    
    size_t i = 0;
    
    for (; i < vectorSize; i += 4) {
        float32x4_t XVec = vld1q_f32(&X[i]);
        float32x4_t YVec = vld1q_f32(&Y[i]);
        float32x4_t ZVec = vld1q_f32(&Z[i]);
        
        float32x4_t r_linear = vmulq_f32(XVec, x_to_r);
        r_linear = vmlaq_f32(r_linear, YVec, y_to_r);
        r_linear = vmlaq_f32(r_linear, ZVec, z_to_r);
        
        float32x4_t g_linear = vmulq_f32(XVec, x_to_g);
        g_linear = vmlaq_f32(g_linear, YVec, y_to_g);
        g_linear = vmlaq_f32(g_linear, ZVec, z_to_g);
        
        float32x4_t b_linear = vmulq_f32(XVec, x_to_b);
        b_linear = vmlaq_f32(b_linear, YVec, y_to_b);
        b_linear = vmlaq_f32(b_linear, ZVec, z_to_b);
        
        uint32x4_t r_mask = vcleq_f32(r_linear, gamma_threshold);
        uint32x4_t g_mask = vcleq_f32(g_linear, gamma_threshold);
        uint32x4_t b_mask = vcleq_f32(b_linear, gamma_threshold);
        
        // Linear portion
        float32x4_t r_gamma_low = vmulq_f32(r_linear, gamma_factor);
        float32x4_t g_gamma_low = vmulq_f32(g_linear, gamma_factor);
        float32x4_t b_gamma_low = vmulq_f32(b_linear, gamma_factor);
        
        float32x4_t r_gamma_high = AccurateMath::accuratePow(r_linear, 1.0f / 2.4f);
        r_gamma_high = vmulq_f32(r_gamma_high, gamma_multiplier);
        r_gamma_high = vsubq_f32(r_gamma_high, gamma_offset);
        
        float32x4_t g_gamma_high = AccurateMath::accuratePow(g_linear, 1.0f / 2.4f);
        g_gamma_high = vmulq_f32(g_gamma_high, gamma_multiplier);
        g_gamma_high = vsubq_f32(g_gamma_high, gamma_offset);
        
        float32x4_t b_gamma_high = AccurateMath::accuratePow(b_linear, 1.0f / 2.4f);
        b_gamma_high = vmulq_f32(b_gamma_high, gamma_multiplier);
        b_gamma_high = vsubq_f32(b_gamma_high, gamma_offset);
        
        // Select between linear and power portions
        float32x4_t r_result = vbslq_f32(r_mask, r_gamma_low, r_gamma_high);
        float32x4_t g_result = vbslq_f32(g_mask, g_gamma_low, g_gamma_high);
        float32x4_t b_result = vbslq_f32(b_mask, b_gamma_low, b_gamma_high);
        
        r_result = vmaxq_f32(vminq_f32(r_result, vdupq_n_f32(1.0f)), vdupq_n_f32(0.0f));
        g_result = vmaxq_f32(vminq_f32(g_result, vdupq_n_f32(1.0f)), vdupq_n_f32(0.0f));
        b_result = vmaxq_f32(vminq_f32(b_result, vdupq_n_f32(1.0f)), vdupq_n_f32(0.0f));
        
        vst1q_f32(&r[i], r_result);
        vst1q_f32(&g[i], g_result);
        vst1q_f32(&b[i], b_result);
    }
    
    for (; i < size; ++i) {
        float r_linear = 3.2406f * X[i] - 1.5372f * Y[i] - 0.4986f * Z[i];
        float g_linear = -0.9689f * X[i] + 1.8758f * Y[i] + 0.0415f * Z[i];
        float b_linear = 0.0557f * X[i] - 0.2040f * Y[i] + 1.0570f * Z[i];
        
        auto gammaCorrect = [](float c) {
            return c <= 0.0031308f ? 12.92f * c : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
        };
        
        r[i] = std::clamp(gammaCorrect(r_linear), 0.0f, 1.0f);
        g[i] = std::clamp(gammaCorrect(g_linear), 0.0f, 1.0f);
        b[i] = std::clamp(gammaCorrect(b_linear), 0.0f, 1.0f);
    }
}

void rgbToLab(std::span<const float> r, std::span<const float> g, std::span<const float> b,
              std::span<float> L, std::span<float> a, std::span<float> b_comp, size_t count) {
    // Use temporary storage for XYZ conversion
    std::vector<float> X_temp(count), Y_temp(count), Z_temp(count);
    
    // First convert RGB to XYZ
    rgbToXyz(r, g, b, X_temp, Y_temp, Z_temp, count);
    
    // Then convert XYZ to Lab
    const size_t vectorSize = count & ~3;
    
    // Reference white constants (D65)
    float32x4_t ref_x = vdupq_n_f32(0.95047f);
    float32x4_t ref_y = vdupq_n_f32(1.0f);
    float32x4_t ref_z = vdupq_n_f32(1.08883f);
    
    // Lab conversion constants
    float32x4_t epsilon = vdupq_n_f32(0.008856f);
    float32x4_t kappa = vdupq_n_f32(903.3f);
    float32x4_t const_116 = vdupq_n_f32(116.0f);
    float32x4_t const_16 = vdupq_n_f32(16.0f);
    float32x4_t const_500 = vdupq_n_f32(500.0f);
    float32x4_t const_200 = vdupq_n_f32(200.0f);
    
    size_t i = 0;
    
    for (; i < vectorSize; i += 4) {
        float32x4_t XVec = vld1q_f32(&X_temp[i]);
        float32x4_t YVec = vld1q_f32(&Y_temp[i]);
        float32x4_t ZVec = vld1q_f32(&Z_temp[i]);
        
        float32x4_t xr = vdivq_f32(XVec, ref_x);
        float32x4_t yr = vdivq_f32(YVec, ref_y);
        float32x4_t zr = vdivq_f32(ZVec, ref_z);
        
        uint32x4_t x_mask = vcgtq_f32(xr, epsilon);
        uint32x4_t y_mask = vcgtq_f32(yr, epsilon);
        uint32x4_t z_mask = vcgtq_f32(zr, epsilon);
        
        float32x4_t fx_high = AccurateMath::accuratePow(xr, 1.0f / 3.0f);
        float32x4_t fy_high = AccurateMath::accuratePow(yr, 1.0f / 3.0f);
        float32x4_t fz_high = AccurateMath::accuratePow(zr, 1.0f / 3.0f);
        
        float32x4_t fx_low = vdivq_f32(vmlaq_f32(const_16, kappa, xr), const_116);
        float32x4_t fy_low = vdivq_f32(vmlaq_f32(const_16, kappa, yr), const_116);
        float32x4_t fz_low = vdivq_f32(vmlaq_f32(const_16, kappa, zr), const_116);
        
        float32x4_t fx = vbslq_f32(x_mask, fx_high, fx_low);
        float32x4_t fy = vbslq_f32(y_mask, fy_high, fy_low);
        float32x4_t fz = vbslq_f32(z_mask, fz_high, fz_low);
        
        float32x4_t L_result = vmlsq_f32(vmulq_f32(const_116, fy), const_16, vdupq_n_f32(1.0f));
        float32x4_t a_result = vmulq_f32(const_500, vsubq_f32(fx, fy));
        float32x4_t b_result = vmulq_f32(const_200, vsubq_f32(fy, fz));
        
        L_result = vmaxq_f32(vminq_f32(L_result, vdupq_n_f32(100.0f)), vdupq_n_f32(0.0f));
        a_result = vmaxq_f32(vminq_f32(a_result, vdupq_n_f32(127.0f)), vdupq_n_f32(-128.0f));
        b_result = vmaxq_f32(vminq_f32(b_result, vdupq_n_f32(127.0f)), vdupq_n_f32(-128.0f));
        
        vst1q_f32(&L[i], L_result);
        vst1q_f32(&a[i], a_result);
        vst1q_f32(&b_comp[i], b_result);
    }
    
    // Handle remaining elements
    for (; i < count; ++i) {
        const float xr = X_temp[i] / 0.95047f;
        const float yr = Y_temp[i] / 1.0f;
        const float zr = Z_temp[i] / 1.08883f;
        
        auto f = [](float t) {
            constexpr float epsilon = 0.008856f;
            constexpr float kappa = 903.3f;
            return t > epsilon ? std::pow(t, 1.0f / 3.0f) : (kappa * t + 16.0f) / 116.0f;
        };
        
        const float fx = f(xr);
        const float fy = f(yr);
        const float fz = f(zr);
        
        L[i] = std::clamp(116.0f * fy - 16.0f, 0.0f, 100.0f);
        a[i] = std::clamp(500.0f * (fx - fy), -128.0f, 127.0f);
        b_comp[i] = std::clamp(200.0f * (fy - fz), -128.0f, 127.0f);
    }
}

void labToRgb(std::span<const float> L, std::span<const float> a, std::span<const float> b_comp,
              std::span<float> r, std::span<float> g, std::span<float> b, size_t count) {
    // Use temporary storage for XYZ conversion
    std::vector<float> X_temp(count), Y_temp(count), Z_temp(count);
    
    // First convert Lab to XYZ
    const size_t vectorSize = count & ~3;
    
    // Reference white constants (D65)
    float32x4_t ref_x = vdupq_n_f32(0.95047f);
    float32x4_t ref_y = vdupq_n_f32(1.0f);
    float32x4_t ref_z = vdupq_n_f32(1.08883f);
    
    // Lab conversion constants
    float32x4_t const_116 = vdupq_n_f32(116.0f);
    float32x4_t const_16 = vdupq_n_f32(16.0f);
    float32x4_t const_500 = vdupq_n_f32(500.0f);
    float32x4_t const_200 = vdupq_n_f32(200.0f);
    float32x4_t delta = vdupq_n_f32(6.0f / 29.0f);
    float32x4_t delta_sq_3 = vdupq_n_f32(3.0f * (6.0f / 29.0f) * (6.0f / 29.0f));
    float32x4_t const_4_29 = vdupq_n_f32(4.0f / 29.0f);
    
    size_t i = 0;
    
    for (; i < vectorSize; i += 4) {
        float32x4_t L_vec = vld1q_f32(&L[i]);
        float32x4_t a_vec = vld1q_f32(&a[i]);
        float32x4_t b_vec = vld1q_f32(&b_comp[i]);
        
        L_vec = vmaxq_f32(vminq_f32(L_vec, vdupq_n_f32(100.0f)), vdupq_n_f32(0.0f));
        a_vec = vmaxq_f32(vminq_f32(a_vec, vdupq_n_f32(127.0f)), vdupq_n_f32(-128.0f));
        b_vec = vmaxq_f32(vminq_f32(b_vec, vdupq_n_f32(127.0f)), vdupq_n_f32(-128.0f));
        
        float32x4_t fY = vdivq_f32(vaddq_f32(L_vec, const_16), const_116);
        float32x4_t fX = vaddq_f32(fY, vdivq_f32(a_vec, const_500));
        float32x4_t fZ = vsubq_f32(fY, vdivq_f32(b_vec, const_200));
        
        uint32x4_t x_mask = vcgtq_f32(fX, delta);
        uint32x4_t y_mask = vcgtq_f32(fY, delta);
        uint32x4_t z_mask = vcgtq_f32(fZ, delta);
        
        float32x4_t x_high = AccurateMath::accuratePow(fX, 3.0f);
        float32x4_t y_high = AccurateMath::accuratePow(fY, 3.0f);
        float32x4_t z_high = AccurateMath::accuratePow(fZ, 3.0f);
        
        float32x4_t x_low = vmulq_f32(delta_sq_3, vsubq_f32(fX, const_4_29));
        float32x4_t y_low = vmulq_f32(delta_sq_3, vsubq_f32(fY, const_4_29));
        float32x4_t z_low = vmulq_f32(delta_sq_3, vsubq_f32(fZ, const_4_29));
        
        float32x4_t x_norm = vbslq_f32(x_mask, x_high, x_low);
        float32x4_t y_norm = vbslq_f32(y_mask, y_high, y_low);
        float32x4_t z_norm = vbslq_f32(z_mask, z_high, z_low);
        
        float32x4_t X_result = vmulq_f32(ref_x, x_norm);
        float32x4_t Y_result = vmulq_f32(ref_y, y_norm);
        float32x4_t Z_result = vmulq_f32(ref_z, z_norm);
        
        X_result = vmaxq_f32(X_result, vdupq_n_f32(0.0f));
        Y_result = vmaxq_f32(Y_result, vdupq_n_f32(0.0f));
        Z_result = vmaxq_f32(Z_result, vdupq_n_f32(0.0f));
        
        vst1q_f32(&X_temp[i], X_result);
        vst1q_f32(&Y_temp[i], Y_result);
        vst1q_f32(&Z_temp[i], Z_result);
    }
    
    // Handle remaining elements
    for (; i < count; ++i) {
        float L_val = std::clamp(L[i], 0.0f, 100.0f);
        float a_val = std::clamp(a[i], -128.0f, 127.0f);
        float b_val = std::clamp(b_comp[i], -128.0f, 127.0f);
        
        const float fY = (L_val + 16.0f) / 116.0f;
        const float fX = fY + a_val / 500.0f;
        const float fZ = fY - b_val / 200.0f;
        
        auto fInv = [](float t) {
            constexpr float delta = 6.0f / 29.0f;
            constexpr float delta_squared = delta * delta;
            return t > delta ? std::pow(t, 3.0f) : 3.0f * delta_squared * (t - 4.0f / 29.0f);
        };
        
        X_temp[i] = std::max(0.0f, 0.95047f * fInv(fX));
        Y_temp[i] = std::max(0.0f, 1.0f * fInv(fY));
        Z_temp[i] = std::max(0.0f, 1.08883f * fInv(fZ));
    }
    
    xyzToRgb(X_temp, Y_temp, Z_temp, r, g, b, count);
}

void frequenciesToWavelengths(std::span<float> wavelengths, std::span<const float> frequencies, size_t count) {
    const size_t size = std::min({wavelengths.size(), frequencies.size(), count});
    const size_t vectorSize = size & ~3;
    
    float32x4_t minFreq = vdupq_n_f32(20.0f);
    float32x4_t maxFreq = vdupq_n_f32(20000.0f);
    float32x4_t minWavelength = vdupq_n_f32(380.0f);
    float32x4_t maxWavelength = vdupq_n_f32(750.0f);
    float32x4_t extendedRedWavelength = vdupq_n_f32(825.0f);
    
    size_t i = 0;
    
    for (; i < vectorSize; i += 4) {
        float32x4_t freqVec = vld1q_f32(&frequencies[i]);
        
        uint32x4_t subAudioMask = vcltq_f32(freqVec, minFreq);
        
        float32x4_t subAudioT = vdivq_f32(vsubq_f32(freqVec, vdupq_n_f32(0.1f)), 
                                         vsubq_f32(minFreq, vdupq_n_f32(0.1f)));
        subAudioT = vmaxq_f32(vminq_f32(subAudioT, vdupq_n_f32(1.0f)), vdupq_n_f32(0.0f));
        float32x4_t subAudioWavelength = vsubq_f32(extendedRedWavelength, 
                                                   vmulq_f32(subAudioT, vdupq_n_f32(75.0f)));
        
        float32x4_t clampedFreq = vmaxq_f32(vminq_f32(freqVec, maxFreq), minFreq);
        
        float32x4_t logFreq = AccurateMath::accurateLog2(vdivq_f32(clampedFreq, minFreq));
        float32x4_t logRange = AccurateMath::accurateLog2(vdivq_f32(maxFreq, minFreq));
        float32x4_t t = vdivq_f32(logFreq, logRange);
        t = vmaxq_f32(vminq_f32(t, vdupq_n_f32(1.0f)), vdupq_n_f32(0.0f));
        
        float32x4_t audibleWavelength = vsubq_f32(maxWavelength, 
                                                 vmulq_f32(t, vsubq_f32(maxWavelength, minWavelength)));
        
        float32x4_t result = vbslq_f32(subAudioMask, subAudioWavelength, audibleWavelength);
        
        vst1q_f32(&wavelengths[i], result);
    }
    
    for (; i < size; ++i) {
        float freq = frequencies[i];
        
        if (freq < 20.0f) {
            float t = std::clamp((freq - 0.1f) / (20.0f - 0.1f), 0.0f, 1.0f);
            wavelengths[i] = 825.0f - t * 75.0f;
        } else {
            float clampedFreq = std::clamp(freq, 20.0f, 20000.0f);
            float logFreq = std::log2(clampedFreq / 20.0f);
            float logRange = std::log2(20000.0f / 20.0f);
            float t = std::clamp(logFreq / logRange, 0.0f, 1.0f);
            wavelengths[i] = 750.0f - t * (750.0f - 380.0f);
        }
    }
}

void weightedColorBlend(std::span<float> result_L, std::span<float> result_a, std::span<float> result_b,
                       std::span<const float> L_values, std::span<const float> a_values, 
                       std::span<const float> b_values, std::span<const float> weights, size_t count) {
    const size_t size = std::min({result_L.size(), result_a.size(), result_b.size(), 
                                 L_values.size(), a_values.size(), b_values.size(), weights.size(), count});
    const size_t vectorSize = size & ~3;
    
    float32x4_t L_accum = vdupq_n_f32(0.0f);
    float32x4_t a_accum = vdupq_n_f32(0.0f);
    float32x4_t b_accum = vdupq_n_f32(0.0f);
    
    size_t i = 0;
    
    for (; i < vectorSize; i += 4) {
        float32x4_t L_vec = vld1q_f32(&L_values[i]);
        float32x4_t a_vec = vld1q_f32(&a_values[i]);
        float32x4_t b_vec = vld1q_f32(&b_values[i]);
        float32x4_t weight_vec = vld1q_f32(&weights[i]);
        
        L_accum = vmlaq_f32(L_accum, L_vec, weight_vec);
        a_accum = vmlaq_f32(a_accum, a_vec, weight_vec);
        b_accum = vmlaq_f32(b_accum, b_vec, weight_vec);
    }
    
    float32x2_t L_low = vget_low_f32(L_accum);
    float32x2_t L_high = vget_high_f32(L_accum);
    float32x2_t L_sum_pair = vadd_f32(L_low, L_high);
    float L_final = vget_lane_f32(vpadd_f32(L_sum_pair, L_sum_pair), 0);
    
    float32x2_t a_low = vget_low_f32(a_accum);
    float32x2_t a_high = vget_high_f32(a_accum);
    float32x2_t a_sum_pair = vadd_f32(a_low, a_high);
    float a_final = vget_lane_f32(vpadd_f32(a_sum_pair, a_sum_pair), 0);
    
    float32x2_t b_low = vget_low_f32(b_accum);
    float32x2_t b_high = vget_high_f32(b_accum);
    float32x2_t b_sum_pair = vadd_f32(b_low, b_high);
    float b_final = vget_lane_f32(vpadd_f32(b_sum_pair, b_sum_pair), 0);
    
    for (; i < size; ++i) {
        L_final += L_values[i] * weights[i];
        a_final += a_values[i] * weights[i];
        b_final += b_values[i] * weights[i];
    }
    
    if (!result_L.empty()) result_L[0] = L_final;
    if (!result_a.empty()) result_a[0] = a_final;
    if (!result_b.empty()) result_b[0] = b_final;
}

// Utility vector operations
void vectorClamp(std::span<float> data, float min_val, float max_val, size_t count) {
    const size_t size = std::min(data.size(), count);
    const size_t vectorSize = size & ~3;
    
    float32x4_t minVec = vdupq_n_f32(min_val);
    float32x4_t maxVec = vdupq_n_f32(max_val);
    
    size_t i = 0;
    
    for (; i < vectorSize; i += 4) {
        float32x4_t dataVec = vld1q_f32(&data[i]);
        dataVec = vmaxq_f32(vminq_f32(dataVec, maxVec), minVec);
        vst1q_f32(&data[i], dataVec);
    }
    
    for (; i < size; ++i) {
        data[i] = std::clamp(data[i], min_val, max_val);
    }
}

void vectorPow(std::span<float> data, float exponent, size_t count) {
    const size_t size = std::min(data.size(), count);
    const size_t vectorSize = size & ~3;
    
    size_t i = 0;
    
    for (; i < vectorSize; i += 4) {
        float32x4_t dataVec = vld1q_f32(&data[i]);
        dataVec = AccurateMath::accuratePow(dataVec, exponent);
        vst1q_f32(&data[i], dataVec);
    }
    
    for (; i < size; ++i) {
        data[i] = std::pow(data[i], exponent);
    }
}

}

#endif