#pragma once

#include "InputCircuit.h"

namespace pyramidfuzz::dsp
{
class InputVoicingStage
{
public:
    void prepare(double rate, int, int channels)
    {
        states.resize(static_cast<size_t>(channels));
        for (auto& state : states)
        {
            prepareFilter(state.hi, rate, inputcircuit::hiResistance);
            prepareFilter(state.lo, rate, inputcircuit::loResistance);
        }
        reset();
    }
    void reset() { for (auto& state : states) { state.hi.reset(); state.lo.reset(); } }
    float processSample(int channel, float sample, float inputGain, float voicingBlend)
    {
        auto& state = states[static_cast<size_t>(channel)];
        const auto input = clampAudio(sample * inputGain);
        // Run both physical endpoint networks continuously for smooth switching.
        return lerp(state.hi.process(input), state.lo.process(input), voicingBlend);
    }
private:
    static void prepareFilter(inputcircuit::CircuitFilter& filter, double rate, double resistance)
    {
        using namespace inputcircuit;
        const auto q4 = preamp();
        const double gc = 1.0 / collectorResistance, gf = 1.0 / feedbackResistance;
        const double sum = q4.baseConductance + gc + q4.effectiveTransconductance;
        const double d0 = q4.baseConductance * gc + gf * sum;
        const double d1 = inputCapacitance * (gc + gf) + d0 * inputCapacitance * resistance + feedbackCapacitance * sum;
        const double d2 = inputCapacitance * feedbackCapacitance * (1.0 + resistance * sum);
        // Q4 base voltage includes feedback-dependent input loading. PreGainStage
        // applies the collector/base transfer at the same oversampled rate.
        filter.setAnalog(rate, 0, inputCapacitance * (gc + gf), inputCapacitance * feedbackCapacitance, d0, d1, d2);
    }
    struct State { inputcircuit::CircuitFilter hi, lo; };
    std::vector<State> states;
};
}
