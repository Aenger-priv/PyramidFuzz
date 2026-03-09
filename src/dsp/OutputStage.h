#pragma once

#include "DspUtilities.h"

namespace pyramidfuzz::dsp
{
class OutputStage
{
public:
    void prepare(double, int, int)
    {
    }

    void reset()
    {
    }

    float processSample(float sample, float outputGain)
    {
        const auto recovered = sample * 1.10f;
        const auto glued = lerp(recovered, std::tanh(0.88f * recovered), 0.22f);
        return clampAudio(glued * outputGain * 0.72f);
    }
};
} // namespace pyramidfuzz::dsp
