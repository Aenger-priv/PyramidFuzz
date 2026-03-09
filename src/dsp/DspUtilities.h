#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <vector>

namespace pyramidfuzz::dsp
{
struct BiquadCoefficients
{
    float b0 { 1.0f };
    float b1 { 0.0f };
    float b2 { 0.0f };
    float a1 { 0.0f };
    float a2 { 0.0f };
};

class Biquad
{
public:
    void setCoefficients(const BiquadCoefficients& newCoefficients)
    {
        coefficients = newCoefficients;
    }

    void reset()
    {
        z1 = 0.0f;
        z2 = 0.0f;
    }

    float process(float inputSample)
    {
        const auto output = coefficients.b0 * inputSample + z1;
        z1 = coefficients.b1 * inputSample - coefficients.a1 * output + z2;
        z2 = coefficients.b2 * inputSample - coefficients.a2 * output;
        return juce::jlimit(-8.0f, 8.0f, output);
    }

private:
    BiquadCoefficients coefficients;
    float z1 { 0.0f };
    float z2 { 0.0f };
};

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

inline float softClip(float sample)
{
    const auto x2 = sample * sample;
    return sample * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float asymSoftClip(float sample, float asymmetry)
{
    const auto skewed = sample + asymmetry * sample * sample;
    return std::tanh(skewed);
}

inline float kneeClip(float sample, float threshold, float softness, float ceiling)
{
    const auto sign = sample >= 0.0f ? 1.0f : -1.0f;
    const auto absolute = std::abs(sample);

    if (absolute <= threshold)
        return sample;

    const auto excess = absolute - threshold;
    const auto compressed = threshold + (ceiling - threshold) * (1.0f - std::exp(-softness * excess));
    return sign * juce::jmin(compressed, ceiling);
}

inline BiquadCoefficients makeLowPass(double sampleRate, float frequency, float q = 0.7071f)
{
    const auto omega = 2.0 * juce::MathConstants<double>::pi * juce::jlimit(10.0f, static_cast<float>(0.45 * sampleRate), frequency) / sampleRate;
    const auto sinOmega = std::sin(omega);
    const auto cosOmega = std::cos(omega);
    const auto alpha = sinOmega / (2.0 * q);

    const auto b0 = (1.0 - cosOmega) * 0.5;
    const auto b1 = 1.0 - cosOmega;
    const auto b2 = (1.0 - cosOmega) * 0.5;
    const auto a0 = 1.0 + alpha;
    const auto a1 = -2.0 * cosOmega;
    const auto a2 = 1.0 - alpha;

    return { static_cast<float>(b0 / a0), static_cast<float>(b1 / a0), static_cast<float>(b2 / a0),
             static_cast<float>(a1 / a0), static_cast<float>(a2 / a0) };
}

inline BiquadCoefficients makeHighPass(double sampleRate, float frequency, float q = 0.7071f)
{
    const auto omega = 2.0 * juce::MathConstants<double>::pi * juce::jlimit(10.0f, static_cast<float>(0.45 * sampleRate), frequency) / sampleRate;
    const auto sinOmega = std::sin(omega);
    const auto cosOmega = std::cos(omega);
    const auto alpha = sinOmega / (2.0 * q);

    const auto b0 = (1.0 + cosOmega) * 0.5;
    const auto b1 = -(1.0 + cosOmega);
    const auto b2 = (1.0 + cosOmega) * 0.5;
    const auto a0 = 1.0 + alpha;
    const auto a1 = -2.0 * cosOmega;
    const auto a2 = 1.0 - alpha;

    return { static_cast<float>(b0 / a0), static_cast<float>(b1 / a0), static_cast<float>(b2 / a0),
             static_cast<float>(a1 / a0), static_cast<float>(a2 / a0) };
}

inline BiquadCoefficients makePeak(double sampleRate, float frequency, float q, float gainDecibels)
{
    const auto gain = std::pow(10.0, gainDecibels / 40.0f);
    const auto omega = 2.0 * juce::MathConstants<double>::pi * juce::jlimit(10.0f, static_cast<float>(0.45 * sampleRate), frequency) / sampleRate;
    const auto sinOmega = std::sin(omega);
    const auto cosOmega = std::cos(omega);
    const auto alpha = sinOmega / (2.0 * q);

    const auto b0 = 1.0 + alpha * gain;
    const auto b1 = -2.0 * cosOmega;
    const auto b2 = 1.0 - alpha * gain;
    const auto a0 = 1.0 + alpha / gain;
    const auto a1 = -2.0 * cosOmega;
    const auto a2 = 1.0 - alpha / gain;

    return { static_cast<float>(b0 / a0), static_cast<float>(b1 / a0), static_cast<float>(b2 / a0),
             static_cast<float>(a1 / a0), static_cast<float>(a2 / a0) };
}

inline BiquadCoefficients makeHighShelf(double sampleRate, float frequency, float slope, float gainDecibels)
{
    const auto gain = std::pow(10.0, gainDecibels / 40.0f);
    const auto omega = 2.0 * juce::MathConstants<double>::pi * juce::jlimit(10.0f, static_cast<float>(0.45 * sampleRate), frequency) / sampleRate;
    const auto sinOmega = std::sin(omega);
    const auto cosOmega = std::cos(omega);
    const auto sqrtGain = std::sqrt(gain);
    const auto alpha = sinOmega / 2.0 * std::sqrt((gain + 1.0 / gain) * (1.0 / slope - 1.0) + 2.0);

    const auto b0 = gain * ((gain + 1.0) + (gain - 1.0) * cosOmega + 2.0 * sqrtGain * alpha);
    const auto b1 = -2.0 * gain * ((gain - 1.0) + (gain + 1.0) * cosOmega);
    const auto b2 = gain * ((gain + 1.0) + (gain - 1.0) * cosOmega - 2.0 * sqrtGain * alpha);
    const auto a0 = (gain + 1.0) - (gain - 1.0) * cosOmega + 2.0 * sqrtGain * alpha;
    const auto a1 = 2.0 * ((gain - 1.0) - (gain + 1.0) * cosOmega);
    const auto a2 = (gain + 1.0) - (gain - 1.0) * cosOmega - 2.0 * sqrtGain * alpha;

    return { static_cast<float>(b0 / a0), static_cast<float>(b1 / a0), static_cast<float>(b2 / a0),
             static_cast<float>(a1 / a0), static_cast<float>(a2 / a0) };
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
