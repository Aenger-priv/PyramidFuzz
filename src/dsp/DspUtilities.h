#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace pyramidfuzz::dsp
{
inline float dbToGain(float decibels)
{
    return juce::Decibels::decibelsToGain(decibels, -120.0f);
}

inline float clampAudio(float sample)
{
    if (! std::isfinite(sample))
        return 0.0f;

    return juce::jlimit(-8.0f, 8.0f, sample);
}

inline float lerp(float a, float b, float amount)
{
    return a + amount * (b - a);
}

template <typename T>
inline void resetSmoother(juce::SmoothedValue<T, juce::ValueSmoothingTypes::Linear>& smoother,
                          double sampleRate,
                          double seconds,
                          T initialValue)
{
    smoother.reset(sampleRate, seconds);
    smoother.setCurrentAndTargetValue(initialValue);
}
} // namespace pyramidfuzz::dsp
