#pragma once

#include "DspUtilities.h"

namespace pyramidfuzz::dsp::inputcircuit
{
// Reduced hybrid-pi model. Bias is solved once at preparation; beta/Vbe are
// explicit device assumptions, not a transistor-level runtime simulation.
constexpr double supply = 9.0, vbe = 0.65, thermalVoltage = 0.02585;
constexpr double collectorResistance = 10000.0, feedbackResistance = 470000.0, baseBiasResistance = 100000.0;
constexpr double feedbackCapacitance = 470e-12, inputCapacitance = 10e-6;
constexpr double hiResistance = 39000.0, loResistance = 390000.0;
constexpr double sustainResistance = 100000.0, sustainBottomResistance = 1000.0;
constexpr double sustainInputCapacitance = 0.47e-6, sustainOutputCapacitance = 0.047e-6;
constexpr double firstClipSeriesResistance = 6200.0;

struct OperatingPoint
{
    double collectorCurrent, emitterVoltage, collectorVoltage;
    double baseConductance, effectiveTransconductance;
    double collectorGain() const
    {
        return (1.0 / feedbackResistance - effectiveTransconductance)
             / (1.0 / collectorResistance + 1.0 / feedbackResistance);
    }
    double inputResistance() const
    {
        return 1.0 / (baseConductance + (1.0 - collectorGain()) / feedbackResistance);
    }
    double outputResistance(double sourceResistance) const
    {
        const double feedback = 1.0 / feedbackResistance;
        return 1.0 / (1.0 / collectorResistance + feedback
            + (effectiveTransconductance - feedback) * feedback
                / (1.0 / sourceResistance + baseConductance + feedback));
    }
    struct OutputNetwork { double series, parallel, capacitance; };
    OutputNetwork outputNetwork(double sourceResistance) const
    {
        // With C1 effectively short in the guitar band, the Q4 output impedance
        // is exactly Rinf + Rdelta/(1+s*Rdelta*C). This retains C10 loading.
        const double a = 1.0 / sourceResistance + baseConductance;
        const double gc = 1.0 / collectorResistance, gf = 1.0 / feedbackResistance;
        const double sum = gc + a + effectiveTransconductance;
        const double denominator = gc * a + gf * sum;
        const double rinf = 1.0 / sum;
        const double rdelta = (a + gf) / denominator - rinf;
        return {rinf, rdelta, feedbackCapacitance * sum / denominator / rdelta};
    }
};

inline OperatingPoint operatingPoint(double beta, double emitterResistance)
{
    const double k = 1.0 + (collectorResistance + feedbackResistance) / baseBiasResistance;
    const double ib = (supply - vbe * k)
        / (collectorResistance * (beta + 1.0) + feedbackResistance + k * (beta + 1.0) * emitterResistance);
    const double ic = beta * ib, ve = (beta + 1.0) * ib * emitterResistance;
    const double vc = supply - collectorResistance * ((beta + 1.0) * ib + (ve + vbe) / baseBiasResistance);
    const double gm = ic / thermalVoltage, rpi = beta / gm;
    return {ic, ve, vc,
        1.0 / baseBiasResistance + 1.0 / (rpi + (beta + 1.0) * emitterResistance),
        gm / (1.0 + emitterResistance * (gm + 1.0 / rpi))};
}
inline OperatingPoint preamp() { return operatingPoint(500.0, 1000.0); }
inline OperatingPoint firstClip() { return operatingPoint(600.0, 100.0); }

// Double state preserves C1's sub-Hz pole at oversampled rates. Analog
// coefficients describe (n0+n1*s+n2*s^2)/(d0+d1*s+d2*s^2).
class CircuitFilter
{
public:
    void setAnalog(double rate, double n0, double n1, double n2, double d0, double d1, double d2)
    {
        const double k = 2.0 * rate;
        if (juce::exactlyEqual(n2, 0.0) && juce::exactlyEqual(d2, 0.0))
        {
            const double norm = d0 + d1 * k;
            b0 = (n0 + n1 * k) / norm; b1 = (n0 - n1 * k) / norm; b2 = 0;
            a1 = (d0 - d1 * k) / norm; a2 = 0;
        }
        else
        {
            const double norm = d0 + d1 * k + d2 * k * k;
            b0 = (n0 + n1 * k + n2 * k * k) / norm;
            b1 = (2.0 * n0 - 2.0 * n2 * k * k) / norm;
            b2 = (n0 - n1 * k + n2 * k * k) / norm;
            a1 = (2.0 * d0 - 2.0 * d2 * k * k) / norm;
            a2 = (d0 - d1 * k + d2 * k * k) / norm;
        }
    }
    void reset() { z1 = z2 = 0; }
    float process(float x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return static_cast<float>(y);
    }
private:
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};
}
