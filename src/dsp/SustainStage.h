#pragma once

#include "InputCircuit.h"

namespace pyramidfuzz::dsp
{
// Q4's frequency-dependent source impedance -> C4 -> R24/R23 -> C5/R19/Q3.
// Three capacitor states with a 2x2 solve. Q3's midband input resistance remains
// a deliberate approximation pending the separate clipping-stage calibration.
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
        const double input = firstClip().inputResistance();
        gLoad = 1.0 / (firstClipSeriesResistance + input);
        baseDivider = input / (firstClipSeriesResistance + input);
        cachedSustain = cachedVoicing = -1;
        reset();
    }
    void reset() { for (auto& state : states) state = {}; }
    float processSample(int channel, float source, float sustain, float voicing)
    {
        // Exact comparison is intentional: parameters are held between smoother
        // updates, and unchanged values can reuse the factorization.
        if (!juce::exactlyEqual(sustain, cachedSustain) || !juce::exactlyEqual(voicing, cachedVoicing))
            update(sustain, voicing);
        auto& state = states[static_cast<size_t>(channel)];
        const double history4 = -g4 * state.voltage4 - state.current4;
        const double history5 = -g5 * state.voltage5 - state.current5;
        const double historySource = -gSourceCap * state.voltageSource - state.currentSource;
        const double seriesHistory = seriesConductance * (history4 / g4 + historySource / gSourceTotal);
        const double rhs1 = seriesConductance * source + seriesHistory - history5;
        const double rhs2 = history5;
        const double wiper = (rhs1 * diagonal2 + g5 * rhs2) / determinant;
        const double output = (diagonal1 * rhs2 + g5 * rhs1) / determinant;
        const double current4 = seriesConductance * (source - wiper) + seriesHistory;
        state.voltage4 = (current4 - history4) / g4;
        state.current4 = current4;
        state.voltageSource = (current4 - historySource) / gSourceTotal;
        state.currentSource = gSourceCap * state.voltageSource + historySource;
        state.current5 = g5 * (wiper - output) + history5;
        state.voltage5 = wiper - output;
        return static_cast<float>(output * baseDivider);
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
        diagonal1 = seriesConductance + 1.0 / lowerResistance + g5;
        diagonal2 = gLoad + g5;
        determinant = diagonal1 * diagonal2 - g5 * g5;
    }
    struct State { double voltage4 = 0, current4 = 0, voltage5 = 0, current5 = 0, voltageSource = 0, currentSource = 0; };
    std::vector<State> states;
    float cachedSustain = -1, cachedVoicing = -1;
    double g4 = 0, g5 = 0, gLoad = 0, baseDivider = 0, sampleRate = 44100;
    inputcircuit::OperatingPoint::OutputNetwork sourceHi {}, sourceLo {};
    double gSourceCap = 0, gSourceTotal = 1, seriesConductance = 0;
    double diagonal1 = 1, diagonal2 = 1, determinant = 1;
};
}
