#ifndef SMOOTHING_H
#define SMOOTHING_H

class SpringSmoother {
	struct SpringState {
		float position, velocity, targetPosition;
	};

	SpringState m_channels[3]{};

	float m_stiffness, m_damping, m_mass;
	float m_currentRGB[3]{};
	bool m_rgbCacheDirty;

public:
	explicit SpringSmoother(float stiffness = 8.0f, float damping = 1.0f, float mass = 0.3f);

	void reset(float r, float g, float b);
	void setTargetColour(float r, float g, float b);
	bool update(float deltaTime);
	void getCurrentColour(float& r, float& g, float& b) const;
	void setSmoothingAmount(float smoothingAmount);
	[[nodiscard]] float getSmoothingAmount() const;

private:
	void updateRGBCache() const;
};

#endif
