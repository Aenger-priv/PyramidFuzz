#pragma once

#include "InputCircuit.h"
#include <array>

namespace pyramidfuzz::dsp
{
// Five-node passive tone network. Double TPT state preserves slow poles at 8x.
// Cholesky factors are reused while Tone/High are unchanged.
class ToneStack
{
public:
    static double sourceResistance()
    {
        // Nominal Q2 source impedance with R12 plus nominal Q3 output resistance.
        // Diode-dependent output impedance remains a block-model approximation.
        const auto q = inputcircuit::firstClip();
        return q.outputResistance(6200.0 + q.outputResistance(30000.0));
    }
    static double loadResistance() { return inputcircuit::operatingPoint(600.0, 2200.0).inputResistance(); }
    void prepare(double rate, int, int channels)
    {
        states.assign(static_cast<size_t>(channels), {});
        conductances = {2 * rate * 10e-9, 2 * rate * 22e-9, 2 * rate * 47e-9};
        cachedTone = cachedHigh = -1;
    }
    void reset() { for (auto& state : states) state = {}; }
    float processSample(int channel, float source, float tone, float high)
    {
        if (!juce::exactlyEqual(tone, cachedTone) || !juce::exactlyEqual(high, cachedHigh)) update(tone, high);
        auto& s = states[static_cast<size_t>(channel)];
        Vector rhs {};
        rhs[0] = source / sourceResistanceValue;
        for (size_t c = 0; c < 3; ++c)
        {
            const double history = -conductances[c] * s[c].voltage - s[c].current;
            rhs[size_t(first[c])] -= history;
            if (second[c] >= 0) rhs[size_t(second[c])] += history;
        }
        // G = L L^T, with positive diagonal guaranteed by passive resistances.
        Vector voltage {};
        for (size_t row = 0; row < 5; ++row)
        {
            double value = rhs[row];
            for (size_t col = 0; col < row; ++col) value -= lower[row][col] * voltage[col];
            voltage[row] = value / lower[row][row];
        }
        for (int row = 4; row >= 0; --row)
        {
            double value = voltage[size_t(row)];
            for (int col = row + 1; col < 5; ++col) value -= lower[size_t(col)][size_t(row)] * voltage[size_t(col)];
            voltage[size_t(row)] = value / lower[size_t(row)][size_t(row)];
        }
        for (size_t c = 0; c < 3; ++c)
        {
            const double v = voltage[size_t(first[c])] - (second[c] < 0 ? 0 : voltage[size_t(second[c])]);
            s[c].current = conductances[c] * (v - s[c].voltage) - s[c].current;
            s[c].voltage = v;
        }
        return static_cast<float>(voltage[4]);
    }
private:
    using Vector = std::array<double, 5>;
    using Matrix = std::array<Vector, 5>;
    struct Capacitor { double voltage = 0, current = 0; };
    static constexpr std::array<int, 3> first {0, 2, 3}, second {1, -1, 4};
    void update(float tone, float high)
    {
        cachedTone = tone; cachedHigh = high;
        Matrix matrix {};
        const auto stamp = [&](int a, int b, double g)
        {
            matrix[size_t(a)][size_t(a)] += g;
            if (b < 0) return;
            matrix[size_t(b)][size_t(b)] += g;
            matrix[size_t(a)][size_t(b)] -= g; matrix[size_t(b)][size_t(a)] -= g;
        };
        sourceResistanceValue = sourceResistance();
        stamp(0, -1, 1.0 / sourceResistanceValue);
        stamp(1, -1, 1.0 / 470000.0); // R5
        stamp(4, -1, 1.0 / loadResistance());
        stamp(0, 2, 1.0 / std::max(20.0, (1 - std::clamp(double(high), 0.0, 1.0)) * 25000));
        const double t = std::clamp(double(tone), 0.0, 1.0);
        stamp(1, 3, 1.0 / std::max(30.0, (1 - t) * 250000));
        stamp(2, 3, 1.0 / std::max(30.0, t * 250000));
        for (size_t c = 0; c < 3; ++c) stamp(first[c], second[c], conductances[c]);
        lower = {};
        for (size_t row = 0; row < 5; ++row)
            for (size_t col = 0; col <= row; ++col)
            {
                double value = matrix[row][col];
                for (size_t k = 0; k < col; ++k) value -= lower[row][k] * lower[col][k];
                lower[row][col] = row == col ? std::sqrt(value) : value / lower[col][col];
            }
    }
    double sourceResistanceValue = 10000;
    float cachedTone = -1, cachedHigh = -1;
    std::array<double, 3> conductances {};
    Matrix lower {};
    std::vector<std::array<Capacitor, 3>> states;
};
}
