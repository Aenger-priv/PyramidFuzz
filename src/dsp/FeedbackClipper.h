#pragma once

#include "InputCircuit.h"

namespace pyramidfuzz::dsp
{
struct SignalPort { double voltage, resistance; };

// Nominal junction law, with a C1 tangent continuation at large currents. The
// continuation bounds exp() and approximates finite series resistance. These
// are reference curves, not measured specimens or temperature-dependent SPICE.
struct DiodeLaw
{
    double saturationCurrent, ideality, seriesResistance;
    struct Value { double current, slope; };
    Value forward(double voltage, int count) const
    {
        const double k = count * ideality * inputcircuit::thermalVoltage;
        const double r = count * seriesResistance;
        const double transition = k * std::log1p(k / (r * saturationCurrent));
        if (voltage > transition)
        {
            const double slope = (saturationCurrent + k / r) / k;
            return {k / r + slope * (voltage - transition), slope};
        }
        const double e = std::exp(voltage / k);
        return {saturationCurrent * std::expm1(voltage / k), saturationCurrent * e / k};
    }
    Value pair(double voltage, int positiveCount = 1, int negativeCount = 1) const
    {
        const auto a = forward(voltage, positiveCount), b = forward(-voltage, negativeCount);
        return {a.current - b.current, a.slope + b.slope};
    }
};
inline constexpr DiodeLaw silicon914 {2.52e-9, 1.752, 2.0};
inline constexpr DiodeLaw silicon4001 {14e-9, 1.984, 0.05};
inline constexpr DiodeLaw germanium34 {0.2e-6, 1.35, 40.0};

// Reduced common-emitter voltage amplifier, with Rf//Cf feedback and a series
// 47 nF capacitor/diode branch. Bias/gm are prepared, not solved transistor by
// transistor. At each sample one monotone scalar equation closes the feedback.
class FeedbackClipper
{
public:
    struct Result
    {
        double output = 0, base = 0, diodeVoltage = 0, diodeCurrent = 0;
        double residual = 0;
        int iterations = 0;
    };
    void prepare(double rate, int channels, bool selectable)
    {
        states.assign(static_cast<size_t>(channels), {});
        secondStage = selectable;
        const auto q = inputcircuit::firstClip();
        baseConductance = q.baseConductance;
        openGain = q.effectiveTransconductance * inputcircuit::collectorResistance;
        positiveHeadroom = q.collectorVoltage - q.emitterVoltage - 0.2;
        negativeHeadroom = inputcircuit::supply - q.collectorVoltage;
        // Positive-gain engine coordinates alternate relative to physical nodes:
        // Q3 base is polarity-reversed after Q4, whereas Q2 base is not. The
        // symmetric Q3 diode pair is invariant, but its rail limits must swap.
        if (!selectable) std::swap(positiveHeadroom, negativeHeadroom);
        gFeedbackCap = 2.0 * rate * inputcircuit::feedbackCapacitance;
        gDiodeCap = 2.0 * rate * 0.047e-6;
    }
    void reset() { for (auto& s : states) s = {}; }
    Result process(int channel, SignalPort source, float mode = 1)
    {
        auto& state = states[static_cast<size_t>(channel)];
        const double hc = -gFeedbackCap * state.feedbackVoltage - state.feedbackCurrent;
        const double hd = -gDiodeCap * state.capVoltage - state.capCurrent;
        const double gl = 1.0 / inputcircuit::feedbackResistance + gFeedbackCap;
        const double resistance = 1.0 / (1.0 / source.resistance + baseConductance);
        const double baseSource = source.voltage * resistance / source.resistance;
        const double m = std::clamp(double(mode), 0.0, 2.0);
        const double siliconWeight = secondStage ? 1.0 - std::abs(m - 1.0) : 1.0;
        const double germaniumWeight = secondStage ? std::max(0.0, m - 1.0) : 0.0;
        struct Evaluation { Result result; double derivative, difference; };
        const auto evaluate = [&](double v) -> Evaluation
        {
            const auto si = siliconWeight > 0 ? (secondStage ? silicon4001 : silicon914).pair(v) : DiodeLaw::Value {0, 0};
            // D5 conducts base -> collector; D6+D7 conduct in reverse.
            const auto ge = germaniumWeight > 0 ? germanium34.pair(v, 1, 2) : DiodeLaw::Value {0, 0};
            const double current = siliconWeight * si.current + germaniumWeight * ge.current;
            const double slope = siliconWeight * si.slope + germaniumWeight * ge.slope;
            const double difference = v + (current - hd) / gDiodeCap;
            const double feedback = gl * difference + hc + current;
            const double b = baseSource - resistance * feedback;
            const double raw = openGain * b - inputcircuit::collectorResistance * feedback;
            const double headroom = raw >= 0 ? positiveHeadroom : negativeHeadroom;
            const double t = std::tanh(raw / headroom);
            const double y = headroom * t;
            const double dDifference = 1.0 + slope / gDiodeCap;
            const double dFeedback = gl * dDifference + slope;
            const double derivative = -(resistance + (1.0 - t * t)
                * (openGain * resistance + inputcircuit::collectorResistance)) * dFeedback - dDifference;
            return {{y, b, v, current, b + y - difference, 0}, derivative, difference};
        };
        // Bracket plus safeguarded Newton: bounded work even after a large input
        // or mode jump. Exp is bounded by DiodeLaw's monotone continuation.
        double lo = -1, hi = 1;
        for (int i = 0; i < 8 && evaluate(lo).result.residual < 0; ++i) lo *= 2;
        for (int i = 0; i < 8 && evaluate(hi).result.residual > 0; ++i) hi *= 2;
        double v = std::clamp(state.diodeVoltage, lo, hi);
        auto value = evaluate(v);
        int iterations = 0;
        for (; iterations < 48; ++iterations)
        {
            if (std::abs(value.result.residual) < 1e-10) break;
            if (value.result.residual > 0) lo = v; else hi = v;
            double next = v - value.result.residual / value.derivative;
            if (iterations >= 12 || next <= lo || next >= hi) next = 0.5 * (lo + hi);
            v = next; value = evaluate(v);
        }
        state.feedbackCurrent = gFeedbackCap * value.difference + hc;
        state.feedbackVoltage = value.difference;
        state.capVoltage = (value.result.diodeCurrent - hd) / gDiodeCap;
        state.capCurrent = value.result.diodeCurrent;
        state.diodeVoltage = v;
        value.result.iterations = iterations;
        return value.result;
    }
private:
    struct State { double feedbackVoltage = 0, feedbackCurrent = 0, capVoltage = 0, capCurrent = 0, diodeVoltage = 0; };
    std::vector<State> states;
    bool secondStage = false;
    double baseConductance = 0, openGain = 1, positiveHeadroom = 4, negativeHeadroom = 4;
    double gFeedbackCap = 0, gDiodeCap = 1;
};

// Series coupling plus the upstream stage's nominal linear output impedance.
// Does not solve diode-dependent source impedance or bidirectional stage loading.
class InterstageCoupling
{
public:
    void prepare(double rate, int channels)
    {
        states.assign(static_cast<size_t>(channels), {});
        const auto network = inputcircuit::firstClip().outputNetwork(30000.0);
        series = network.series + 6200.0; // R12
        gCoupling = 2.0 * rate * 0.047e-6; // C13
        gSourceCap = 2.0 * rate * network.capacitance;
        gSource = gSourceCap + 1.0 / network.parallel;
        resistance = series + 1.0 / gCoupling + 1.0 / gSource;
    }
    void reset() { for (auto& s : states) s = {}; }
    SignalPort begin(int channel, double source)
    {
        auto& s = states[static_cast<size_t>(channel)];
        s.hc = -gCoupling * s.vc - s.ic;
        s.hs = -gSourceCap * s.vs - s.is;
        s.port = {source + s.hc / gCoupling + s.hs / gSource, resistance};
        return s.port;
    }
    void finish(int channel, double base)
    {
        auto& s = states[static_cast<size_t>(channel)];
        const double current = (s.port.voltage - base) / s.port.resistance;
        s.vc = (current - s.hc) / gCoupling; s.ic = current;
        s.vs = (current - s.hs) / gSource; s.is = gSourceCap * s.vs + s.hs;
    }
private:
    struct State { double vc = 0, ic = 0, vs = 0, is = 0, hc = 0, hs = 0; SignalPort port {}; };
    std::vector<State> states;
    double series = 0, gCoupling = 1, gSourceCap = 0, gSource = 1, resistance = 1;
};
}
