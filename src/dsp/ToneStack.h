#pragma once

#include "DspUtilities.h"

namespace pyramidfuzz::dsp
{
class ToneStack
{
public:
    void prepare(double newSampleRate, int, int channels)
    {
        sampleRate = newSampleRate;
        darkLowPass.resize(static_cast<size_t>(channels));
        brightHighPass.resize(static_cast<size_t>(channels));
        brightLowPass.resize(static_cast<size_t>(channels));
        scoopPeak.resize(static_cast<size_t>(channels));

        for (auto channel = 0; channel < channels; ++channel)
        {
            darkLowPass[static_cast<size_t>(channel)].setCoefficients(makeLowPass(sampleRate, 720.0f, 0.76f));
            brightHighPass[static_cast<size_t>(channel)].setCoefficients(makeHighPass(sampleRate, 760.0f, 0.72f));
            brightLowPass[static_cast<size_t>(channel)].setCoefficients(makeLowPass(sampleRate, 3300.0f, 0.84f));
            scoopPeak[static_cast<size_t>(channel)].setCoefficients(makePeak(sampleRate, 920.0f, 0.82f, -8.5f));
        }

        reset();
    }

    void reset()
    {
        for (auto& filter : darkLowPass)
            filter.reset();

        for (auto& filter : brightHighPass)
            filter.reset();

        for (auto& filter : brightLowPass)
            filter.reset();

        for (auto& filter : scoopPeak)
            filter.reset();
    }

    float processSample(int channel, float sample, float tone)
    {
        const auto dark = darkLowPass[static_cast<size_t>(channel)].process(sample);
        const auto bright = brightLowPass[static_cast<size_t>(channel)].process(brightHighPass[static_cast<size_t>(channel)].process(sample));
        const auto blended = lerp(dark * 1.18f, bright * 0.92f, tone);

        const auto centerFactor = juce::jmax(0.0f, 1.0f - std::abs(2.0f * tone - 1.0f));
        const auto scooped = scoopPeak[static_cast<size_t>(channel)].process(blended);
        return clampAudio(lerp(blended, scooped, 0.82f * centerFactor));
    }

private:
    double sampleRate { 44100.0 };
    std::vector<Biquad> darkLowPass;
    std::vector<Biquad> brightHighPass;
    std::vector<Biquad> brightLowPass;
    std::vector<Biquad> scoopPeak;
};
} // namespace pyramidfuzz::dsp
