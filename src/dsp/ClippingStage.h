#pragma once

#include "DspUtilities.h"

namespace pyramidfuzz::dsp
{
class ClippingStageA
{
public:
    void prepare(double newSampleRate, int, int channels)
    {
        sampleRate = newSampleRate;
        preHighPass.resize(static_cast<size_t>(channels));
        preLowPass.resize(static_cast<size_t>(channels));
        postLowPass.resize(static_cast<size_t>(channels));

        for (auto channel = 0; channel < channels; ++channel)
        {
            preHighPass[static_cast<size_t>(channel)].setCoefficients(makeHighPass(sampleRate, 92.0f, 0.72f));
            preLowPass[static_cast<size_t>(channel)].setCoefficients(makeLowPass(sampleRate, 7000.0f, 0.66f));
            postLowPass[static_cast<size_t>(channel)].setCoefficients(makeLowPass(sampleRate, 4200.0f, 0.88f));
        }

        reset();
    }

    void reset()
    {
        for (auto& filter : preHighPass)
            filter.reset();

        for (auto& filter : preLowPass)
            filter.reset();

        for (auto& filter : postLowPass)
            filter.reset();
    }

    float processSample(int channel, float sample, float sustain)
    {
        const auto filtered = preLowPass[static_cast<size_t>(channel)].process(preHighPass[static_cast<size_t>(channel)].process(sample));
        const auto drive = 5.2f + sustain * 12.8f;
        const auto shaped = filtered * drive;

        // This stage is tuned for a thicker, woollier Muff-family core rather than a bright OD-style clip.
        const auto weighted = shaped + 0.26f * shaped * shaped * shaped;
        const auto clipped = softClip(weighted);
        const auto compressed = std::tanh(0.92f * clipped + 0.08f * weighted);

        return clampAudio(postLowPass[static_cast<size_t>(channel)].process(0.94f * compressed));
    }

private:
    double sampleRate { 44100.0 };
    std::vector<Biquad> preHighPass;
    std::vector<Biquad> preLowPass;
    std::vector<Biquad> postLowPass;
};

class ClippingStageB
{
public:
    void prepare(double newSampleRate, int, int channels)
    {
        sampleRate = newSampleRate;
        preHighPass.resize(static_cast<size_t>(channels));
        preLowPass.resize(static_cast<size_t>(channels));
        postLowPass.resize(static_cast<size_t>(channels));
        postHighPass.resize(static_cast<size_t>(channels));

        for (auto channel = 0; channel < channels; ++channel)
        {
            preHighPass[static_cast<size_t>(channel)].setCoefficients(makeHighPass(sampleRate, 72.0f, 0.76f));
            preLowPass[static_cast<size_t>(channel)].setCoefficients(makeLowPass(sampleRate, 6200.0f, 0.72f));
            postLowPass[static_cast<size_t>(channel)].setCoefficients(makeLowPass(sampleRate, 3600.0f, 0.88f));
            // C3 is the output coupling capacitor of this gain stage. The
            // blocker also prevents asymmetrical germanium clipping from
            // depositing DC into the passive tone network.
            postHighPass[static_cast<size_t>(channel)].setCoefficients(makeHighPass(sampleRate, 24.0f, 0.70f));
        }

        reset();
    }

    void reset()
    {
        for (auto& filter : preHighPass)
            filter.reset();

        for (auto& filter : preLowPass)
            filter.reset();

        for (auto& filter : postLowPass)
            filter.reset();

        for (auto& filter : postHighPass)
            filter.reset();
    }

    float processSample(int channel, float sample, float sustain, float modeMorph)
    {
        const auto filtered = preLowPass[static_cast<size_t>(channel)].process(preHighPass[static_cast<size_t>(channel)].process(sample));

        const auto offDrive = filtered * (1.24f + sustain * 1.10f);
        const auto siliconDrive = filtered * (2.6f + sustain * 3.8f);
        const auto germaniumDrive = filtered * (2.2f + sustain * 3.2f);

        // In bypass mode, only the diode feedback is removed. The transistor
        // stage still has finite headroom, but is appreciably more open than a
        // diode-limited path.
        const auto off = 0.92f * std::tanh(0.92f * offDrive);

        // Silicon mode uses a harder knee and a brighter, tighter ceiling.
        const auto silicon = kneeClip(siliconDrive,
                                      0.42f,
                                      6.2f,
                                      0.88f);

        // Germanium mode clips earlier with a softer knee and slight asymmetry for squish.
        const auto germanium = 0.96f * kneeClip(germaniumDrive + 0.035f * germaniumDrive * germaniumDrive,
                                                0.24f,
                                                2.7f,
                                                0.82f);

        float mixed = silicon;
        if (modeMorph < 1.0f)
            mixed = lerp(off, silicon, juce::jlimit(0.0f, 1.0f, modeMorph));
        else
            mixed = lerp(silicon, germanium, juce::jlimit(0.0f, 1.0f, modeMorph - 1.0f));

        const auto postFiltered = postLowPass[static_cast<size_t>(channel)].process(mixed);
        return clampAudio(postHighPass[static_cast<size_t>(channel)].process(postFiltered));
    }

private:
    double sampleRate { 44100.0 };
    std::vector<Biquad> preHighPass;
    std::vector<Biquad> preLowPass;
    std::vector<Biquad> postLowPass;
    std::vector<Biquad> postHighPass;
};
} // namespace pyramidfuzz::dsp
