#include <JuceHeader.h>
#include "dsp/FuzzEngine.h"
#include <cmath>
#include <iostream>

using namespace pyramidfuzz::dsp;

namespace
{
double toneGain(double rate, float tone, float high, double frequency)
{
    ToneStack stack;
    stack.prepare(rate, 128, 1);
    double real = 0, imaginary = 0;
    const int settle = int(rate / 2);
    const int window = int(rate / 4);
    for (int i = 0; i < settle + window; ++i)
    {
        const double phase = 2 * juce::MathConstants<double>::pi * frequency * i / rate;
        const float output = stack.processSample(0, float(0.1 * std::sin(phase)), tone, high);
        if (i >= settle)
        {
            real += output * std::sin(phase);
            imaginary += output * std::cos(phase);
        }
    }
    return 2 * std::hypot(real, imaginary) / window / 0.1;
}
}

int main()
{
    int failures = 0;
    const auto check = [&failures](bool condition, const char* message)
    {
        if (! condition) { std::cerr << message << '\n'; ++failures; }
    };

    // The fixed preamp must remain monotonic for hot inputs; its old quadratic
    // skew could fold a sufficiently negative input toward the positive rail.
    for (double rate : {88200.0, 192000.0, 768000.0})
    {
        float previous = -10;
        for (float input : {-10.0f, -1.0f, -.1f, 0.0f, .1f, 1.0f, 10.0f})
        {
            PreGainStage pre;
            pre.prepare(rate, 128, 1);
            float output = 0;
            for (int i = 0; i < int(.01 * rate); ++i) output = pre.processSample(0, input);
            check(std::isfinite(output) && std::abs(output) < 9, "preamp must respect finite supply headroom");
            check(output > previous, "fixed preamp must be monotonic without foldback");
            previous = output;
        }
    }

    // Behavioral regressions complement the independent full AC grid check.
    // They test direction and useful signal flow, not copied circuit constants.
    for (double rate : {44100.0, 48000.0, 96000.0})
    {
        for (float high : {0.0f, 0.5f, 1.0f})
        {
            const auto darkTilt = toneGain(rate, 0, high, 5000) / toneGain(rate, 0, high, 100);
            const auto brightTilt = toneGain(rate, 1, high, 5000) / toneGain(rate, 1, high, 100);
            check(brightTilt > darkTilt * 1.2, "Tone must sweep from dark to bright");
        }
        check(toneGain(rate, 0, 1, 5000) > 2 * toneGain(rate, 0, 0, 5000), "High must extend the low-pass arm's upper response");
        check(toneGain(rate, 1, 1, 1000) > 0.1, "Tone/High maximum must not collapse into near silence");
    }

    for (const auto threshold : { 0.24f, 0.42f })
    {
        constexpr float step = 0.0001f;
        const auto left = kneeClip(threshold - step, threshold, 0.88f);
        const auto right = kneeClip(threshold + step, threshold, 0.88f);
        check(std::abs((right - left) / (2.0f * step) - 1.0f) < 0.01f, "clipping knee must be smooth");
        check(std::abs(kneeClip(100.0f, threshold, 0.88f)) <= 0.88001f, "clipping must be bounded");
    }

    for (const auto rate : { 44100.0, 48000.0, 96000.0 })
        for (int quality = 0; quality < 3; ++quality)
            for (int mode = 0; mode < 3; ++mode)
            {
                FuzzEngine engine;
                FuzzParameters parameters;
                parameters.oversamplingMode = quality;
                parameters.clippingMode = mode;
                engine.updateParameters(parameters);
                engine.prepare(rate, 128, 2);
                juce::AudioBuffer<float> buffer(2, 128);
                double energy = 0.0;
                for (int block = 0; block < 40; ++block)
                {
                    for (int i = 0; i < 128; ++i)
                    {
                        const auto sample = 0.15f * std::sin(static_cast<float>(2.0 * juce::MathConstants<double>::pi * 110.0 * (block * 128 + i) / rate));
                        buffer.setSample(0, i, sample);
                        buffer.setSample(1, i, sample);
                    }
                    engine.processBlock(buffer);
                    for (int i = 0; i < 128; ++i)
                    {
                        const auto left = buffer.getSample(0, i);
                        check(std::isfinite(left) && std::abs(left) <= 8.0f, "output must be finite and bounded");
                        check(std::abs(left - buffer.getSample(1, i)) < 1.0e-6f, "identical channels must match");
                        if (block >= 20) energy += left * left;
                    }
                }
                check(energy > 1.0e-6, "signal must pass through the engine");
            }
    return failures == 0 ? 0 : 1;
}
