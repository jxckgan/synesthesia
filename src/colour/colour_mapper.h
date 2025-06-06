#pragma once

#include <array>
#include <vector>

class ColourMapper {
public:
	struct ColourResult {
		float r;
		float g;
		float b;
		float dominantWavelength;
		float dominantFrequency;
		float colourIntensity;
		float L;
		float a;
		float b_comp;
	};

	struct SpectralCharacteristics {
		float flatness;
		float centroid;
		float spread;
		float normalisedSpread = 0.0f;
	};

	static constexpr float MIN_WAVELENGTH = 380.0f;
	static constexpr float MAX_WAVELENGTH = 750.0f;
	static constexpr float MIN_FREQ = 20.0f;
	static constexpr float MAX_FREQ = 20000.0f;
	static constexpr float SPEED_OF_SOUND = 343.0f;
	static constexpr size_t CIE_TABLE_SIZE = 90;

	static ColourResult frequenciesToColour(const std::vector<float>& frequencies,
											const std::vector<float>& magnitudes,
											const std::vector<float>& spectralEnvelope = {},
											float sampleRate = 44100.0f, float gamma = 1.0f);

	static float logFrequencyToWavelength(float freq);
	static void RGBtoLab(float r, float g, float b, float& L, float& a, float& b_comp);
	static void LabtoRGB(float L, float a, float b_comp, float& r, float& g, float& b);

	static SpectralCharacteristics calculateSpectralCharacteristics(
		const std::vector<float>& spectrum, float sampleRate);

private:
	static void XYZtoRGB(float X, float Y, float Z, float& r, float& g, float& b);
	static void RGBtoXYZ(float r, float g, float b, float& X, float& Y, float& Z);
	static void XYZtoLab(float X, float Y, float Z, float& L, float& a, float& b);
	static void LabtoXYZ(float L, float a, float b, float& X, float& Y, float& Z);
	static void wavelengthToRGBCIE(float wavelength, float& r, float& g, float& b);
	static void interpolateCIE(float wavelength, float& X, float& Y, float& Z);

	static constexpr float REF_X = 0.95047f;
	static constexpr float REF_Y = 1.0f;
	static constexpr float REF_Z = 1.08883f;
	static constexpr std::array<std::array<float, 4>, CIE_TABLE_SIZE> CIE_1931 = {
		{{380.0f, 2.689900e-003f, 2.000000e-004f, 1.226000e-002f},
		 {385.0f, 5.310500e-003f, 3.955600e-004f, 2.422200e-002f},
		 {390.0f, 1.078100e-002f, 8.000000e-004f, 4.925000e-002f},
		 {395.0f, 2.079200e-002f, 1.545700e-003f, 9.513500e-002f},
		 {400.0f, 3.798100e-002f, 2.800000e-003f, 1.740900e-001f},
		 {405.0f, 6.315700e-002f, 4.656200e-003f, 2.901300e-001f},
		 {410.0f, 9.994100e-002f, 7.400000e-003f, 4.605300e-001f},
		 {415.0f, 1.582400e-001f, 1.177900e-002f, 7.316600e-001f},
		 {420.0f, 2.294800e-001f, 1.750000e-002f, 1.065800e+000f},
		 {425.0f, 2.810800e-001f, 2.267800e-002f, 1.314600e+000f},
		 {430.0f, 3.109500e-001f, 2.730000e-002f, 1.467200e+000f},
		 {435.0f, 3.307200e-001f, 3.258400e-002f, 1.579600e+000f},
		 {440.0f, 3.333600e-001f, 3.790000e-002f, 1.616600e+000f},
		 {445.0f, 3.167200e-001f, 4.239100e-002f, 1.568200e+000f},
		 {450.0f, 2.888200e-001f, 4.680000e-002f, 1.471700e+000f},
		 {455.0f, 2.596900e-001f, 5.212200e-002f, 1.374000e+000f},
		 {460.0f, 2.327600e-001f, 6.000000e-002f, 1.291700e+000f},
		 {465.0f, 2.099900e-001f, 7.294200e-002f, 1.235600e+000f},
		 {470.0f, 1.747600e-001f, 9.098000e-002f, 1.113800e+000f},
		 {475.0f, 1.328700e-001f, 1.128400e-001f, 9.422000e-001f},
		 {480.0f, 9.194400e-002f, 1.390200e-001f, 7.559600e-001f},
		 {485.0f, 5.698500e-002f, 1.698700e-001f, 5.864000e-001f},
		 {490.0f, 3.173100e-002f, 2.080200e-001f, 4.466900e-001f},
		 {495.0f, 1.461300e-002f, 2.580800e-001f, 3.411600e-001f},
		 {500.0f, 4.849100e-003f, 3.230000e-001f, 2.643700e-001f},
		 {505.0f, 2.321500e-003f, 4.054000e-001f, 2.059400e-001f},
		 {510.0f, 9.289900e-003f, 5.030000e-001f, 1.544500e-001f},
		 {515.0f, 2.927800e-002f, 6.081100e-001f, 1.091800e-001f},
		 {520.0f, 6.379100e-002f, 7.100000e-001f, 7.658500e-002f},
		 {525.0f, 1.108100e-001f, 7.951000e-001f, 5.622700e-002f},
		 {530.0f, 1.669200e-001f, 8.620000e-001f, 4.136600e-002f},
		 {535.0f, 2.276800e-001f, 9.150500e-001f, 2.935300e-002f},
		 {540.0f, 2.926900e-001f, 9.540000e-001f, 2.004200e-002f},
		 {545.0f, 3.622500e-001f, 9.800400e-001f, 1.331200e-002f},
		 {550.0f, 4.363500e-001f, 9.949500e-001f, 8.782300e-003f},
		 {555.0f, 5.151300e-001f, 1.000100e+000f, 5.857300e-003f},
		 {560.0f, 5.974800e-001f, 9.950000e-001f, 4.049300e-003f},
		 {565.0f, 6.812100e-001f, 9.787500e-001f, 2.921700e-003f},
		 {570.0f, 7.642500e-001f, 9.520000e-001f, 2.277100e-003f},
		 {575.0f, 8.439400e-001f, 9.155800e-001f, 1.970600e-003f},
		 {580.0f, 9.163500e-001f, 8.700000e-001f, 1.806600e-003f},
		 {585.0f, 9.770300e-001f, 8.162300e-001f, 1.544900e-003f},
		 {590.0f, 1.023000e+000f, 7.570000e-001f, 1.234800e-003f},
		 {595.0f, 1.051300e+000f, 6.948300e-001f, 1.117700e-003f},
		 {600.0f, 1.055000e+000f, 6.310000e-001f, 9.056400e-004f},
		 {605.0f, 1.036200e+000f, 5.665400e-001f, 6.946700e-004f},
		 {610.0f, 9.923900e-001f, 5.030000e-001f, 4.288500e-004f},
		 {615.0f, 9.286100e-001f, 4.417200e-001f, 3.181700e-004f},
		 {620.0f, 8.434600e-001f, 3.810000e-001f, 2.559800e-004f},
		 {625.0f, 7.398300e-001f, 3.205200e-001f, 1.567900e-004f},
		 {630.0f, 6.328900e-001f, 2.650000e-001f, 9.769400e-005f},
		 {635.0f, 5.335100e-001f, 2.170200e-001f, 6.894400e-005f},
		 {640.0f, 4.406200e-001f, 1.750000e-001f, 5.116500e-005f},
		 {645.0f, 3.545300e-001f, 1.381200e-001f, 3.601600e-005f},
		 {650.0f, 2.786200e-001f, 1.070000e-001f, 2.423800e-005f},
		 {655.0f, 2.148500e-001f, 8.165200e-002f, 1.691500e-005f},
		 {660.0f, 1.616100e-001f, 6.100000e-002f, 1.190600e-005f},
		 {665.0f, 1.182000e-001f, 4.432700e-002f, 8.148900e-006f},
		 {670.0f, 8.575300e-002f, 3.200000e-002f, 5.600600e-006f},
		 {675.0f, 6.307700e-002f, 2.345400e-002f, 3.954400e-006f},
		 {680.0f, 4.583400e-002f, 1.700000e-002f, 2.791200e-006f},
		 {685.0f, 3.205700e-002f, 1.187200e-002f, 1.917600e-006f},
		 {690.0f, 2.218700e-002f, 8.210000e-003f, 1.313500e-006f},
		 {695.0f, 1.561200e-002f, 5.772300e-003f, 9.151900e-007f},
		 {700.0f, 1.109800e-002f, 4.102000e-003f, 6.476700e-007f},
		 {705.0f, 7.923300e-003f, 2.929100e-003f, 4.635200e-007f},
		 {710.0f, 5.653100e-003f, 2.091000e-003f, 3.330400e-007f},
		 {715.0f, 4.003900e-003f, 1.482200e-003f, 2.382300e-007f},
		 {720.0f, 2.825300e-003f, 1.047000e-003f, 1.702600e-007f},
		 {725.0f, 1.994700e-003f, 7.401500e-004f, 1.220700e-007f},
		 {730.0f, 1.399400e-003f, 5.200000e-004f, 8.710700e-008f},
		 {735.0f, 9.698000e-004f, 3.609300e-004f, 6.145500e-008f},
		 {740.0f, 6.684700e-004f, 2.492000e-004f, 4.316200e-008f},
		 {745.0f, 4.614100e-004f, 1.723100e-004f, 3.037900e-008f},
		 {750.0f, 3.207300e-004f, 1.200000e-004f, 2.155400e-008f},
		 {755.0f, 2.257300e-004f, 8.462000e-005f, 1.549300e-008f},
		 {760.0f, 1.597300e-004f, 6.000000e-005f, 1.120400e-008f},
		 {765.0f, 1.127500e-004f, 4.244600e-005f, 8.087300e-009f},
		 {770.0f, 7.951300e-005f, 3.000000e-005f, 5.834000e-009f},
		 {775.0f, 5.608700e-005f, 2.121000e-005f, 4.211000e-009f},
		 {780.0f, 3.954100e-005f, 1.498900e-005f, 3.038300e-009f},
		 {785.0f, 2.785200e-005f, 1.058400e-005f, 2.190700e-009f},
		 {790.0f, 1.959700e-005f, 7.465600e-006f, 1.577800e-009f},
		 {795.0f, 1.377000e-005f, 5.259200e-006f, 1.134800e-009f},
		 {800.0f, 9.670000e-006f, 3.702800e-006f, 8.156500e-010f},
		 {805.0f, 6.791800e-006f, 2.607600e-006f, 5.862600e-010f},
		 {810.0f, 4.770600e-006f, 1.836500e-006f, 4.213800e-010f},
		 {815.0f, 3.355000e-006f, 1.295000e-006f, 3.031900e-010f},
		 {820.0f, 2.353400e-006f, 9.109200e-007f, 2.175300e-010f},
		 {825.0f, 1.637700e-006f, 6.356400e-007f, 1.547600e-010f}}};
};
