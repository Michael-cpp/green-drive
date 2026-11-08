// Procedural, physically based rendering of the pedal faceplate.
// Everything is generated from fixed random seeds, so no image files are needed
// and the wear looks identical every time the plugin opens.
#pragma once

#include <cmath>

#include <juce_graphics/juce_graphics.h>

namespace pedal
{
constexpr float rotaryStart = juce::MathConstants<float>::pi * 1.2f;
constexpr float rotaryEnd = juce::MathConstants<float>::pi * 2.8f;

// Point light above the top-left of the pedal, in editor coordinates.
constexpr float lightX = 70.0f, lightY = -40.0f, lightZ = 380.0f;

// Light reaching a point on the faceplate: inverse-square falloff times the
// angle of incidence. 1.0 is directly under the light.
inline float lightAt (float x, float y)
{
    const float dx = x - lightX, dy = y - lightY;
    const float d2 = dx * dx + dy * dy + lightZ * lightZ;
    return (lightZ * lightZ / d2) * (lightZ / std::sqrt (d2));
}

// Angle from a point towards the light, using the knob angle convention.
inline float lightAngle (float x, float y)
{
    return std::atan2 (lightX - x, -(lightY - y));
}

struct Layout
{
    int width = 0, height = 0;
    juce::Rectangle<int> drive, tone, level, calibration, footswitch;
    juce::Rectangle<float> led;
};

juce::Image renderFace (const Layout&, float scale);
} // namespace pedal
