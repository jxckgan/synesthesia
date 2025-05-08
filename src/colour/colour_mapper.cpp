#include "colour_mapper.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <vector>

void ColourMapper::interpolateCIE(float wavelength, float& X, float& Y, float& Z) {
	// Clamp wavelength to valid CIE 1931 range
	wavelength = std::clamp(wavelength, 380.0f, 825.0f);

	// Calculate index
	const float indexFloat = (wavelength - 380.0f) / 5.0f;
	auto index = static_cast<size_t>(std::floor(indexFloat));

	// Ensure index is within valid range
	if (index >= CIE_TABLE_SIZE - 1) {
		index = CIE_TABLE_SIZE - 2;
	}

	const auto& entry0 = CIE_1931[index];
	const auto& entry1 = CIE_1931[index + 1];

	const float lambda0 = entry0[0];
	const float lambda1 = entry1[0];

	// Calculate interpolation factor
	float t = 0.0f;
	if (lambda1 > lambda0) {
		t = (wavelength - lambda0) / (lambda1 - lambda0);
		t = std::clamp(t, 0.0f, 1.0f);
	}

	X = std::lerp(entry0[1], entry1[1], t);
	Y = std::lerp(entry0[2], entry1[2], t);
	Z = std::lerp(entry0[3], entry1[3], t);
}

void ColourMapper::XYZtoRGB(const float X, const float Y, const float Z, float& r, float& g,
							float& b) {
	// Apply XYZ to linear RGB transformation matrix
	r = 3.2406f * X - 1.5372f * Y - 0.4986f * Z;
	g = -0.9689f * X + 1.8758f * Y + 0.0415f * Z;
	b = 0.0557f * X - 0.2040f * Y + 1.0570f * Z;

	// Apply gamma correction (linearRGB to sRGB)
	auto gammaCorrect = [](const float c) {
		return c <= 0.0031308f ? 12.92f * c : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
	};

	r = std::clamp(gammaCorrect(r), 0.0f, 1.0f);
	g = std::clamp(gammaCorrect(g), 0.0f, 1.0f);
	b = std::clamp(gammaCorrect(b), 0.0f, 1.0f);
}

void ColourMapper::RGBtoXYZ(float r, float g, float b, float& X, float& Y, float& Z) {
	r = std::clamp(r, 0.0f, 1.0f);
	g = std::clamp(g, 0.0f, 1.0f);
	b = std::clamp(b, 0.0f, 1.0f);

	// Convert sRGB to linear RGB
	auto inverseGamma = [](const float c) {
		return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
	};

	const float r_linear = inverseGamma(r);
	const float g_linear = inverseGamma(g);
	const float b_linear = inverseGamma(b);

	// Apply linear RGB to XYZ transformation matrix
	X = 0.4124f * r_linear + 0.3576f * g_linear + 0.1805f * b_linear;
	Y = 0.2126f * r_linear + 0.7152f * g_linear + 0.0722f * b_linear;
	Z = 0.0193f * r_linear + 0.1192f * g_linear + 0.9505f * b_linear;
}

// Convert XYZ to Lab colour space
void ColourMapper::XYZtoLab(const float X, const float Y, const float Z, float& L, float& a,
							float& b) {
	// Normalise by white point
	const float xr = REF_X > 0.0f ? X / REF_X : 0.0f;
	const float yr = REF_Y > 0.0f ? Y / REF_Y : 0.0f;
	const float zr = REF_Z > 0.0f ? Z / REF_Z : 0.0f;

	// Apply nonlinear compression
	auto f = [](const float t) {
		constexpr float epsilon = 0.008856f;  // (6/29)^3
		constexpr float kappa = 903.3f;		  // 116/delta^3, where delta = 6/29
		return t > epsilon ? std::pow(t, 1.0f / 3.0f) : (kappa * t + 16.0f) / 116.0f;
	};

	const float fx = f(xr);
	const float fy = f(yr);
	const float fz = f(zr);

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
	const float fY = (L + 16.0f) / 116.0f;
	const float fX = fY + a / 500.0f;
	const float fZ = fY - b / 200.0f;

	// Inverse nonlinear compression
	auto fInv = [](const float t) {
		constexpr float delta = 6.0f / 29.0f;
		constexpr float delta_squared = delta * delta;
		return t > delta ? std::pow(t, 3.0f) : 3.0f * delta_squared * (t - 4.0f / 29.0f);
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
void ColourMapper::RGBtoLab(const float r, const float g, const float b, float& L, float& a,
							float& b_comp) {
	float X, Y, Z;
	RGBtoXYZ(r, g, b, X, Y, Z);
	XYZtoLab(X, Y, Z, L, a, b_comp);
}

// Convert Lab to RGB colour space
void ColourMapper::LabtoRGB(const float L, const float a, const float b_comp, float& r, float& g,
							float& b) {
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
float ColourMapper::logFrequencyToWavelength(const float freq) {
	if (!std::isfinite(freq) || freq <= 0.001f)
		return MIN_WAVELENGTH;

	// Calculate octave position
	const float MIN_LOG_FREQ = std::log2(MIN_FREQ);
	const float MAX_LOG_FREQ = std::log2(MAX_FREQ);
	const float LOG_FREQ_RANGE = MAX_LOG_FREQ - MIN_LOG_FREQ;

	const float logFreq = std::log2(freq);
	const float normalisedLogFreq = (logFreq - MIN_LOG_FREQ) / LOG_FREQ_RANGE;
	const float t = std::clamp(normalisedLogFreq, 0.0f, 1.0f);

	// Invert wavelength for an intuitive visualisation (bass notes are dark red, etc.)
	return MAX_WAVELENGTH - t * (MAX_WAVELENGTH - MIN_WAVELENGTH);
}

// Calculate spectral characteristics
ColourMapper::SpectralCharacteristics ColourMapper::calculateSpectralCharacteristics(
	const std::vector<float>& spectrum, const float sampleRate) {
	SpectralCharacteristics result{0.5f, 0.0f, 0.0f};

	if (spectrum.empty() || sampleRate <= 0.0f) {
		return result;
	}

	std::vector<float> validValues;
	validValues.reserve(spectrum.size());

	float totalWeight = 0.0f;
	float weightedFreqSum = 0.0f;

	// Collect valid data and calculate basic statistics
	for (size_t i = 0; i < spectrum.size(); ++i) {
		float value = spectrum[i];
		if (value <= 1e-6f || !std::isfinite(value)) {
			continue;
		}

		const float freq = static_cast<float>(i) * sampleRate / (2.0f * (spectrum.size() - 1));
		if (freq < MIN_FREQ || freq > MAX_FREQ) {
			continue;
		}

		validValues.push_back(value);
		totalWeight += value;
		weightedFreqSum += freq * value;
	}

	// Calculate spectral flatness
	if (!validValues.empty() && totalWeight > 0.0f) {
		float logSum = 0.0f;
		for (const float value : validValues) {
			logSum += std::log(value);
		}

		const float geometricMean = std::exp(logSum / validValues.size());

		if (const float arithmeticMean = totalWeight / validValues.size();
			arithmeticMean > 1e-10f) {
			result.flatness = geometricMean / arithmeticMean;
		}

		// Calculate spectral centroid
		result.centroid = weightedFreqSum / totalWeight;

		// Calculate spectral spread
		float spreadSum = 0.0f;
		for (size_t i = 0; i < spectrum.size(); ++i) {
			const float value = spectrum[i];
			if (value <= 1e-6f || !std::isfinite(value)) {
				continue;
			}

			const float freq = static_cast<float>(i) * sampleRate / (2.0f * (spectrum.size() - 1));
			if (freq < MIN_FREQ || freq > MAX_FREQ) {
				continue;
			}

			const float diff = freq - result.centroid;
			spreadSum += value * diff * diff;
		}

		if (totalWeight > 0.0f) {
			result.spread = std::sqrt(spreadSum / totalWeight);
			result.normalisedSpread = std::min(result.spread / 5000.0f, 1.0f);
		}
	}

	return result;
}

ColourMapper::ColourResult ColourMapper::frequenciesToColour(
	const std::vector<float>& frequencies, const std::vector<float>& magnitudes,
	const std::vector<float>& spectralEnvelope, float sampleRate, float gamma) {
	// Default dark result for no input
	ColourResult result{0.1f, 0.1f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

	// Validate inputs
	bool hasPeaks = !frequencies.empty() && !magnitudes.empty();
	bool hasEnvelope = !spectralEnvelope.empty() && sampleRate > 0.0f;

	if (!hasPeaks && !hasEnvelope) {
		return result;
	}

	// Calculate colour from dominant frequencies if available
	ColourResult peakResult{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	bool hasPeakColour = false;

	if (hasPeaks) {
		size_t count = std::min(frequencies.size(), magnitudes.size());
		std::vector weights(count, 0.0f);

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

		// Calculate colour if we have valid peaks
		if (validCount > 0 && maxValidMagnitude > 0.0f) {
			float maxFrequency = 0.0f;
			float maxWeight = 0.0f;
			float totalWeight = 0.0f;
			for (size_t i = 0; i < count; ++i) {
				if (!std::isfinite(frequencies[i]) || !std::isfinite(magnitudes[i]) ||
					frequencies[i] <= 0.0f || magnitudes[i] < 0.0f) {
					continue;
				}

				float freq = frequencies[i];
				float mag = magnitudes[i];

				weights[i] = mag;
				totalWeight += mag;

				if (mag > maxWeight) {
					maxWeight = mag;
					maxFrequency = freq;
				}
			}

			if (totalWeight > 0.0f) {
				float L_blend = 0.0f;
				float a_blend = 0.0f;
				float b_blend = 0.0f;
				float dominantWavelength = logFrequencyToWavelength(maxFrequency);

				// Convert each frequency to a colour and accumulate in Lab space
				for (size_t i = 0; i < count; ++i) {
					if (weights[i] <= 0.0f)
						continue;

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
				}

				// Convert blended Lab back to RGB
				LabtoRGB(L_blend, a_blend, b_blend, peakResult.r, peakResult.g, peakResult.b);

				peakResult.dominantWavelength = dominantWavelength;
				peakResult.dominantFrequency = maxFrequency;
				peakResult.L = L_blend;
				peakResult.a = a_blend;
				peakResult.b_comp = b_blend;

				hasPeakColour = true;
			}
		}
	}

	// Calculate colour from spectral envelope if available
	ColourResult envelopeResult{0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	bool hasEnvelopeColour = false;

	if (hasEnvelope) {
		const size_t binCount = spectralEnvelope.size();

		// Create frequency and weight arrays for the spectral envelope
		std::vector<float> envelopeFrequencies;
		std::vector<float> envelopeWeights;
		envelopeFrequencies.reserve(binCount);
		envelopeWeights.reserve(binCount);

		// Calculate weighted centroid to find dominant frequency
		float totalEnvelopeWeight = 0.0f;
		float maxEnvelopeWeight = 0.0f;
		float dominantEnvelopeFreq = 0.0f;

		// Collect valid data and calculate basic statistics
		for (size_t i = 0; i < binCount; ++i) {
			float freq;
			freq = static_cast<float>(i) * sampleRate / (2.0f * (binCount - 1));

			// Only process bins within our frequency range of interest
			if (freq < MIN_FREQ || freq > MAX_FREQ || !std::isfinite(freq)) {
				continue;
			}

			float weight = spectralEnvelope[i];
			if (!std::isfinite(weight) || weight <= 0.0f) {
				continue;
			}

			// Use raw magnitudes without perceptual weighting
			float perceptualWeight = weight;

			// Store frequency and its weighted magnitude
			envelopeFrequencies.push_back(freq);
			envelopeWeights.push_back(perceptualWeight);

			// Update statistics
			totalEnvelopeWeight += perceptualWeight;

			if (perceptualWeight > maxEnvelopeWeight) {
				maxEnvelopeWeight = perceptualWeight;
				dominantEnvelopeFreq = freq;
			}
		}

		if (!envelopeFrequencies.empty() && totalEnvelopeWeight > 0.0f) {
			// Calculate spectral characteristics using our new helper function
			SpectralCharacteristics spectralStats =
				calculateSpectralCharacteristics(spectralEnvelope, sampleRate);

			float spectralCentroid = spectralStats.centroid;
			float spectralFlatness = spectralStats.flatness;
			float normalisedSpread = spectralStats.normalisedSpread;

			// Normalise weights
			for (float& envelopeWeight : envelopeWeights) {
				envelopeWeight /= totalEnvelopeWeight;
			}

			float L_envelope = 0.0f;
			float a_envelope = 0.0f;
			float b_envelope = 0.0f;
			float dominantWavelength = logFrequencyToWavelength(dominantEnvelopeFreq);

			for (size_t i = 0; i < envelopeFrequencies.size(); ++i) {
				float weight = envelopeWeights[i];
				if (weight <= 0.0f)
					continue;

				// Convert frequency to wavelength
				float wavelength = logFrequencyToWavelength(envelopeFrequencies[i]);

				// Get colour in RGB
				float r, g, b;
				wavelengthToRGBCIE(wavelength, r, g, b);

				// Convert to Lab for perceptual blending
				float L, a, b_comp;
				RGBtoLab(r, g, b, L, a, b_comp);

				// Enhance saturation based on spectral characteristics
				float saturationFactor =
					(1.0f - spectralFlatness) * (1.0f - 0.5f * normalisedSpread);
				float saturationBoost = 1.0f + saturationFactor;

				a *= saturationBoost;
				b_comp *= saturationBoost;

				// Adjust brightness based on spectral characteristics
				float centroidFactor = std::clamp(
					std::log2(spectralCentroid / MIN_FREQ) / std::log2(MAX_FREQ / MIN_FREQ), 0.0f,
					1.0f);

				// Spread also affects the brightness curve
				float contrastBoost = 1.0f + normalisedSpread * 0.5f;
				float brightnessAdjust = centroidFactor * contrastBoost;

				// Apply brightness adjustment
				L = std::lerp(L, std::min(L * 1.2f, 100.0f), brightnessAdjust);

				// Weighted contribution to final colour
				L_envelope += L * weight;
				a_envelope += a * weight;
				b_envelope += b_comp * weight;
			}

			// Convert blended Lab back to RGB
			LabtoRGB(L_envelope, a_envelope, b_envelope, envelopeResult.r, envelopeResult.g,
					 envelopeResult.b);

			envelopeResult.dominantWavelength = dominantWavelength;
			envelopeResult.dominantFrequency = dominantEnvelopeFreq;
			envelopeResult.L = L_envelope;
			envelopeResult.a = a_envelope;
			envelopeResult.b_comp = b_envelope;

			hasEnvelopeColour = true;
		}
	}

	// Blend results based on spectral characteristics
	if (hasPeakColour && hasEnvelopeColour) {
		float blendFactor = 0.5f;

		if (hasEnvelope) {
			// Calculate spectral characteristics for blending factor using our helper function
			SpectralCharacteristics spectralStats =
				calculateSpectralCharacteristics(spectralEnvelope, sampleRate);

			float spectralFlatness = spectralStats.flatness;
			float spectralSpread = spectralStats.normalisedSpread;

			// Determine blending based on spectral characteristics
			float tonalFactor = 1.0f - spectralFlatness;
			float spreadFactor = spectralSpread;

			blendFactor = 0.7f - 0.5f * tonalFactor + 0.3f * spreadFactor;
			blendFactor = std::clamp(blendFactor, 0.1f, 0.9f);
		}

		// Blend in Lab space
		result.L = std::lerp(peakResult.L, envelopeResult.L, blendFactor);
		result.a = std::lerp(peakResult.a, envelopeResult.a, blendFactor);
		result.b_comp = std::lerp(peakResult.b_comp, envelopeResult.b_comp, blendFactor);

		// Convert back to RGB
		LabtoRGB(result.L, result.a, result.b_comp, result.r, result.g, result.b);

		// Choose dominant frequency based on which method likely provides better data
		if (blendFactor < 0.5f) {
			result.dominantFrequency = peakResult.dominantFrequency;
			result.dominantWavelength = peakResult.dominantWavelength;
		} else {
			result.dominantFrequency = envelopeResult.dominantFrequency;
			result.dominantWavelength = envelopeResult.dominantWavelength;
		}
	} else if (hasPeakColour) {
		result = peakResult;
	} else if (hasEnvelopeColour) {
		result = envelopeResult;
	}

	gamma = std::clamp(gamma, 0.1f, 5.0f);

	// Calculate colour intensity from linear RGB values before gamma correction
	result.colourIntensity = 0.2126f * result.r + 0.7152f * result.g + 0.0722f * result.b;

	// Apply gamma correction to final output
	result.r = std::pow(std::clamp(result.r, 0.0f, 1.0f), gamma);
	result.g = std::pow(std::clamp(result.g, 0.0f, 1.0f), gamma);
	result.b = std::pow(std::clamp(result.b, 0.0f, 1.0f), gamma);

	return result;
}