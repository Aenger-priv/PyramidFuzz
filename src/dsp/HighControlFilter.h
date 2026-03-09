#pragma once

#include "DspUtilities.h"

namespace pyramidfuzz::dsp
{
class HighControlFilter
{
public:
    void prepare(double newSampleRate, int, int channels)
    {
        sampleRate = newSampleRate;
        highShelf.resize(static_cast<size_t>(channels));
        presencePeak.resize(static_cast<size_t>(channels));

        for (auto channel = 0; channel < channels; ++channel)
        {
            highShelf[static_cast<size_t>(channel)].setCoefficients(makeHighShelf(sampleRate, 2400.0f, 0.8f, 3.4f));
            presencePeak[static_cast<size_t>(channel)].setCoefficients(makePeak(sampleRate, 1750.0f, 0.9f, 1.8f));
        }

        reset();
    }

    void reset()
    {
        for (auto& filter : highShelf)
            filter.reset();

        for (auto& filter : presencePeak)
            filter.reset();
    }

    float processSample(int channel, float sample, float highAmount)
    {
        const auto contoured = presencePeak[static_cast<size_t>(channel)].process(highShelf[static_cast<size_t>(channel)].process(sample));
        return clampAudio(lerp(sample, contoured, highAmount * 0.72f));
    }

private:
    double sampleRate { 44100.0 };
    std::vector<Biquad> highShelf;
    std::vector<Biquad> presencePeak;
};
} // namespace pyramidfuzz::dsp
