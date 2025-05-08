#include "colour_mapper.h"
#include "smoothing.h"

#include <algorithm>
#include <cmath>

SpringSmoother::SpringSmoother(const float stiffness, const float damping, const float mass)
	: m_stiffness(stiffness), m_damping(damping), m_mass(mass), m_rgbCacheDirty(true) {
	// Initialise all Lab channels to default values
	for (int i = 0; i < 3; ++i) {
		m_channels[i].position = i == 0 ? 50.0f : 0.0f;
		m_channels[i].velocity = 0.0f;
		m_channels[i].targetPosition = m_channels[i].position;
	}

	// Initialise RGB cache
	m_currentRGB[0] = m_currentRGB[1] = m_currentRGB[2] = 0.5f;
}

void SpringSmoother::reset(const float r, const float g, const float b) {
	// Convert RGB to Lab for internal representation
	float L, a, b_comp;
	ColourMapper::RGBtoLab(r, g, b, L, a, b_comp);

	// Reset the spring system to match the given colour
	m_channels[0].position = m_channels[0].targetPosition = L;
	m_channels[1].position = m_channels[1].targetPosition = a;
	m_channels[2].position = m_channels[2].targetPosition = b_comp;

	// Reset velocities
	for (auto& m_channel : m_channels) {
		m_channel.velocity = 0.0f;
	}

	// Store RGB directly to avoid immediate recalculation
	m_currentRGB[0] = r;
	m_currentRGB[1] = g;
	m_currentRGB[2] = b;
	m_rgbCacheDirty = false;
}

void SpringSmoother::setTargetColour(const float r, const float g, const float b) {
	// Convert RGB to Lab space for internal target representation
	float L, a, b_comp;
	ColourMapper::RGBtoLab(r, g, b, L, a, b_comp);

	// Set new target positions for each Lab channel
	m_channels[0].targetPosition = L;
	m_channels[1].targetPosition = a;
	m_channels[2].targetPosition = b_comp;
}

bool SpringSmoother::update(float deltaTime) {
	// Apply spring physics to each Lab channel
	bool significantMovement = false;

	// Limit max delta time to prevent instability
	deltaTime = std::min(deltaTime, 0.05f);

	for (int i = 0; i < 3; ++i) {
		const float displacement = m_channels[i].position - m_channels[i].targetPosition;
		const float springForce = -m_stiffness * displacement;
		const float dampingForce = -m_damping * m_channels[i].velocity;
		const float acceleration = (springForce + dampingForce) / m_mass;

		const float previousVelocity = m_channels[i].velocity;
		m_channels[i].velocity += acceleration * deltaTime;

		const float previousPosition = m_channels[i].position;
		m_channels[i].position += m_channels[i].velocity * deltaTime;

		// Apply constraints appropriate to each Lab component
		if (i == 0) {
			m_channels[i].position = std::clamp(m_channels[i].position, 0.0f, 100.0f);
		} else {
			m_channels[i].position = std::clamp(m_channels[i].position, -128.0f, 127.0f);
		}

		// Check if significant change occurred
		const float positionDelta = std::abs(m_channels[i].position - previousPosition);
		const float velocityDelta = std::abs(m_channels[i].velocity - previousVelocity);

		if (constexpr float MIN_DELTA = 0.0001f;
			positionDelta > MIN_DELTA || velocityDelta > MIN_DELTA) {
			significantMovement = true;
		}
	}

	// Mark RGB cache as dirty if we moved
	if (significantMovement) {
		m_rgbCacheDirty = true;
	}

	return significantMovement;
}

void SpringSmoother::updateRGBCache() const {
	// Create temporary non-const variables for RGB output
	float r, g, b;

	// Convert current Lab values to RGB
	ColourMapper::LabtoRGB(m_channels[0].position,	// L
						   m_channels[1].position,	// a
						   m_channels[2].position,	// b
						   r,						// r (temporary)
						   g,						// g (temporary)
						   b						// b (temporary)
	);

	// Ensure RGB values stay in valid range and assign to mutable cache
	r = std::clamp(r, 0.0f, 1.0f);
	g = std::clamp(g, 0.0f, 1.0f);
	b = std::clamp(b, 0.0f, 1.0f);

	const_cast<SpringSmoother*>(this)->m_currentRGB[0] = std::clamp(r, 0.0f, 1.0f);
	const_cast<SpringSmoother*>(this)->m_currentRGB[1] = std::clamp(g, 0.0f, 1.0f);
	const_cast<SpringSmoother*>(this)->m_currentRGB[2] = std::clamp(b, 0.0f, 1.0f);

	// Mark cache as clean
	const_cast<SpringSmoother*>(this)->m_rgbCacheDirty = false;
}

void SpringSmoother::getCurrentColour(float& r, float& g, float& b) const {
	// Update the RGB cache if necessary
	if (m_rgbCacheDirty) {
		updateRGBCache();
	}

	// Return the current smoothed RGB values
	r = m_currentRGB[0];
	g = m_currentRGB[1];
	b = m_currentRGB[2];
}

void SpringSmoother::setSmoothingAmount(float smoothingAmount) {
	// Clamp input to valid range
	smoothingAmount = std::clamp(smoothingAmount, 0.0f, 1.0f);

	// Map 0-1 to appropriate spring stiffness range
	constexpr float MIN_STIFFNESS = 8.0f;
	constexpr float MAX_STIFFNESS = 120.0f;

	m_stiffness = MIN_STIFFNESS * pow(MAX_STIFFNESS / MIN_STIFFNESS, smoothingAmount);

	// Adjust damping to maintain critical damping ratio
	m_damping = 2.0f * sqrt(m_stiffness * m_mass);

	m_damping *= 0.5f;
}

float SpringSmoother::getSmoothingAmount() const {
	// Convert from spring parameters back to 0-1 range
	constexpr float MIN_STIFFNESS = 8.0f;
	constexpr float MAX_STIFFNESS = 120.0f;

	const float logMin = log(MIN_STIFFNESS);
	const float logMax = log(MAX_STIFFNESS);
	const float logCurrent = log(m_stiffness);

	return std::clamp((logCurrent - logMin) / (logMax - logMin), 0.0f, 1.0f);
}
