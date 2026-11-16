#pragma once

#include "FeedbackClipper.h"

namespace pyramidfuzz::dsp
{
// Q4's frequency-dependent source impedance -> C4 -> R24/R23 -> C5/R19/Q3.
// Thevenin port into Q3: the feedback clipper supplies its actual base voltage.
// A nominal-load wrapper is retained for independent small-signal regressions.
class SustainStage
{
public:
    void prepare(double rate, int, int channels)
    {
        using namespace inputcircuit;
        states.resize(static_cast<size_t>(channels));
        g4 = 2.0 * rate * sustainInputCapacitance;
        g5 = 2.0 * rate * sustainOutputCapacitance;
        sourceHi = preamp().outputNetwork(hiResistance);
        sourceLo = preamp().outputNetwork(loResistance);
        sampleRate = rate;
        cachedSustain = cachedVoicing = -1;
        reset();
    }
    void reset() { for (auto& state : states) state = {}; }
    SignalPort beginSample(int channel, float source, float sustain, float voicing)
    {
        if (!juce::exactlyEqual(sustain, cachedSustain) || !juce::exactlyEqual(voicing, cachedVoicing))
            update(sustain, voicing);
        auto& state = states[static_cast<size_t>(channel)];
        state.h4 = -g4 * state.voltage4 - state.current4;
        state.h5 = -g5 * state.voltage5 - state.current5;
        state.hs = -gSourceCap * state.voltageSource - state.currentSource;
        state.source = source + state.h4 / g4 + state.hs / gSourceTotal;
        state.wiperOpen = seriesConductance * state.source / wiperConductance;
        state.port = {state.wiperOpen + state.h5 / g5,
                      1.0 / wiperConductance + 1.0 / g5 + inputcircuit::firstClipSeriesResistance};
        return state.port;
    }
    void finishSample(int channel, double base)
    {
        auto& state = states[static_cast<size_t>(channel)];
        const double current5 = (state.port.voltage - base) / state.port.resistance;
        const double wiper = state.wiperOpen - current5 / wiperConductance;
        const double current4 = seriesConductance * (state.source - wiper);
        state.voltage4 = (current4 - state.h4) / g4; state.current4 = current4;
        state.voltageSource = (current4 - state.hs) / gSourceTotal;
        state.currentSource = gSourceCap * state.voltageSource + state.hs;
        state.voltage5 = (current5 - state.h5) / g5; state.current5 = current5;
    }
    float processSample(int channel, float source, float sustain, float voicing)
    {
        const auto port = beginSample(channel, source, sustain, voicing);
        const double inputResistance = inputcircuit::firstClip().inputResistance();
        const double base = port.voltage * inputResistance / (port.resistance + inputResistance);
        finishSample(channel, base);
        return static_cast<float>(base);
    }
private:
    void update(float sustain, float voicing)
    {
        using namespace inputcircuit;
        cachedSustain = sustain; cachedVoicing = voicing;
        const double s = std::clamp(double(sustain), 0.0, 1.0);
        const double v = std::clamp(double(voicing), 0.0, 1.0);
        const double series = sourceHi.series + v * (sourceLo.series - sourceHi.series);
        const double parallel = sourceHi.parallel + v * (sourceLo.parallel - sourceHi.parallel);
        const double cap = sourceHi.capacitance + v * (sourceLo.capacitance - sourceHi.capacitance);
        gSourceCap = 2.0 * sampleRate * cap;
        gSourceTotal = 1.0 / parallel + gSourceCap;
        const double upper = series + (1.0 - s) * sustainResistance;
        const double lowerResistance = sustainBottomResistance + s * sustainResistance;
        seriesConductance = 1.0 / (upper + 1.0 / g4 + 1.0 / gSourceTotal);
        wiperConductance = seriesConductance + 1.0 / lowerResistance;
    }
    struct State { double voltage4 = 0, current4 = 0, voltage5 = 0, current5 = 0, voltageSource = 0, currentSource = 0; double h4 = 0, h5 = 0, hs = 0, source = 0, wiperOpen = 0; SignalPort port {}; };
    std::vector<State> states;
    float cachedSustain = -1, cachedVoicing = -1;
    double g4 = 0, g5 = 0, sampleRate = 44100;
    inputcircuit::OperatingPoint::OutputNetwork sourceHi {}, sourceLo {};
    double gSourceCap = 0, gSourceTotal = 1, seriesConductance = 0;
    double wiperConductance = 1;
};
}
