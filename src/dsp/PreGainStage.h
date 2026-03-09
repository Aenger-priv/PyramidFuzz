#pragma once

#include "DspUtilities.h"

namespace pyramidfuzz::dsp
{
class PreGainStage
{
public:
    void prepare(double newSampleRate, int, int channels)
    {
        sampleRate = newSampleRate;
        couplingHighPass.resize(static_cast<size_t>(channels));
        inputLowPass.resize(static_cast<size_t>(channels));
        outputLowPass.resize(static_cast<size_t>(channels));

        for (auto channel = 0; channel < channels; ++channel)
        {
            couplingHighPass[static_cast<size_t>(channel)].setCoefficients(makeHighPass(sampleRate, 85.0f, 0.74f));
            inputLowPass[static_cast<size_t>(channel)].setCoefficients(makeLowPass(sampleRate, 6400.0f, 0.68f));
            outputLowPass[static_cast<size_t>(channel)].setCoefficients(makeLowPass(sampleRate, 5200.0f, 0.86f));
        }

        reset();
    }

    void reset()
    {
        for (auto& filter : couplingHighPass)
            filter.reset();

        for (auto& filter : inputLowPass)
            filter.reset();

        for (auto& filter : outputLowPass)
            filter.reset();
    }

    float processSample(int channel, float sample, float sustain)
    {
        const auto filtered = inputLowPass[static_cast<size_t>(channel)].process(couplingHighPass[static_cast<size_t>(channel)].process(sample));
        const auto drive = 2.0f + sustain * 2.6f;
        const auto saturated = asymSoftClip(filtered * drive, 0.13f);
        const auto blended = lerp(filtered * 0.82f, saturated, 0.78f);
        return clampAudio(outputLowPass[static_cast<size_t>(channel)].process(blended * 0.96f));
    }

private:
    double sampleRate { 44100.0 };
    std::vector<Biquad> couplingHighPass;
    std::vector<Biquad> inputLowPass;
    std::vector<Biquad> outputLowPass;
};
} // namespace pyramidfuzz::dsp
