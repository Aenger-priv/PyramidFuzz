#include <JuceHeader.h>
#include "dsp/FuzzEngine.h"
#include <cmath>
#include <iostream>

using namespace pyramidfuzz::dsp;

int main()
{
    int failures = 0;
    const auto check = [&failures](bool condition, const char* message)
    {
        if (! condition) { std::cerr << message << '\n'; ++failures; }
    };

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
