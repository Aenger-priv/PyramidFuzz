#pragma once

#include "InputCircuit.h"

namespace pyramidfuzz::dsp
{
class OutputStage
{
public:
    // Chosen output-interface convention: 3 V peak = 1 digital unit. No per-mode
    // normalization. Input still uses the existing 1 input unit = 1 model volt.
    static constexpr float voltsToDigital = 1.0f / 3.0f;
    void prepare(double rate, int, int channels)
    {
        using namespace inputcircuit;
        const auto q = operatingPoint(600.0, 2200.0); // Q1, R4
        gain = -q.collectorGain();
        // Fourth inversion: positive normalized Q1 output moves physically
        // toward the supply, rather than toward transistor saturation.
        positiveHeadroom = supply - q.collectorVoltage;
        negativeHeadroom = q.collectorVoltage - q.emitterVoltage - 0.2;
        coupling.resize(static_cast<size_t>(channels));
        // Approximate tone-wiper source impedance for Q1 output loading.
        // This nominal 50 kOhm source does not track Tone/High or frequency.
        const double resistance = q.outputResistance(50000.0) + 100000.0;
        for (auto& filter : coupling)
            filter.setAnalog(rate, 0, 100000.0 * 10e-6, 0, 1, resistance * 10e-6, 0); // C2, R26
        reset();
    }
    void reset() { for (auto& filter : coupling) filter.reset(); }
    float processSample(int channel, float sample)
    {
        const double raw = gain * sample;
        const double headroom = raw >= 0 ? positiveHeadroom : negativeHeadroom;
        const float recovered = static_cast<float>(headroom * std::tanh(raw / headroom));
        return coupling[static_cast<size_t>(channel)].process(recovered);
    }
private:
    std::vector<inputcircuit::CircuitFilter> coupling;
    double gain = 1, positiveHeadroom = 6, negativeHeadroom = 2;
};
}
