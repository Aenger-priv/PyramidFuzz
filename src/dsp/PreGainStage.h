#pragma once

#include "InputCircuit.h"

namespace pyramidfuzz::dsp
{
class PreGainStage
{
public:
    void prepare(double rate, int, int channels)
    {
        using namespace inputcircuit;
        const auto q4 = preamp();
        filters.resize(static_cast<size_t>(channels));
        for (auto& filter : filters)
            filter.setAnalog(rate, q4.effectiveTransconductance - 1.0 / feedbackResistance,
                -feedbackCapacitance, 0, 1.0 / collectorResistance + 1.0 / feedbackResistance, feedbackCapacitance, 0);
        // Polarity-normalized collector signal: positive moves toward saturation,
        // negative toward the supply. This preserves the engine's sign convention.
        positiveHeadroom = q4.collectorVoltage - q4.emitterVoltage - 0.2;
        negativeHeadroom = supply - q4.collectorVoltage;
        reset();
    }
    void reset() { for (auto& filter : filters) filter.reset(); }
    float processSample(int channel, float sample)
    {
        const double x = filters[static_cast<size_t>(channel)].process(sample);
        const double headroom = x >= 0 ? positiveHeadroom : negativeHeadroom;
        // Monotonic unity-slope soft headroom limit; no quadratic foldback.
        return static_cast<float>(headroom * std::tanh(x / headroom));
    }
private:
    std::vector<inputcircuit::CircuitFilter> filters;
    double positiveHeadroom = 5, negativeHeadroom = 3;
};
}
