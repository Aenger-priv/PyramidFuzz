#pragma once

#include "DspUtilities.h"

namespace pyramidfuzz::dsp
{
class InputVoicingStage
{
public:
    void prepare(double newSampleRate, int, int channels)
    {
        sampleRate = newSampleRate;
        hiHighPass.resize(static_cast<size_t>(channels));
        hiLowPass.resize(static_cast<size_t>(channels));
        loHighPass.resize(static_cast<size_t>(channels));
        loLowPass.resize(static_cast<size_t>(channels));

        for (auto channel = 0; channel < channels; ++channel)
        {
            hiHighPass[static_cast<size_t>(channel)].setCoefficients(makeHighPass(sampleRate, 120.0f, 0.69f));
            hiLowPass[static_cast<size_t>(channel)].setCoefficients(makeLowPass(sampleRate, 5400.0f, 0.78f));
            loHighPass[static_cast<size_t>(channel)].setCoefficients(makeHighPass(sampleRate, 52.0f, 0.66f));
            loLowPass[static_cast<size_t>(channel)].setCoefficients(makeLowPass(sampleRate, 4600.0f, 0.80f));
        }

        reset();
    }

    void reset()
    {
        for (auto& filter : hiHighPass)
            filter.reset();

        for (auto& filter : hiLowPass)
            filter.reset();

        for (auto& filter : loHighPass)
            filter.reset();

        for (auto& filter : loLowPass)
            filter.reset();
    }

    float processSample(int channel, float sample, float inputGain, float voicingBlend)
    {
        auto hi = hiLowPass[static_cast<size_t>(channel)].process(hiHighPass[static_cast<size_t>(channel)].process(sample));
        auto lo = loLowPass[static_cast<size_t>(channel)].process(loHighPass[static_cast<size_t>(channel)].process(sample));

        hi *= 1.03f;
        lo *= 1.18f;

        const auto voiced = lerp(hi, lo, voicingBlend);
        return clampAudio(voiced * inputGain);
    }

private:
    double sampleRate { 44100.0 };
    std::vector<Biquad> hiHighPass;
    std::vector<Biquad> hiLowPass;
    std::vector<Biquad> loHighPass;
    std::vector<Biquad> loLowPass;
};
} // namespace pyramidfuzz::dsp
