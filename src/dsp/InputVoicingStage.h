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
            // C1 is 10 uF. The 39 k / 390 k input-resistor selection changes
            // the effective input loading as well as the coupling corner.
            hiHighPass[static_cast<size_t>(channel)].setCoefficients(makeHighPass(sampleRate, 120.0f, 0.69f));
            hiLowPass[static_cast<size_t>(channel)].setCoefficients(makeLowPass(sampleRate, 5600.0f, 0.78f));
            loHighPass[static_cast<size_t>(channel)].setCoefficients(makeHighPass(sampleRate, 48.0f, 0.66f));
            loLowPass[static_cast<size_t>(channel)].setCoefficients(makeLowPass(sampleRate, 5000.0f, 0.80f));
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

        // The 390 k path preserves more bass after C1 but attenuates the
        // transistor input more than the 39 k path. Keep that level difference
        // audible; the Input control is available to compensate it.
        hi *= 0.94f;
        lo *= 0.67f;

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
