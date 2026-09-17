#pragma once

#include "DspUtilities.h"

#include <array>

namespace pyramidfuzz::dsp
{
// Trapezoidal/MNA model of the modified Ram's Head passive tone circuit.
// Nodes: Q2 collector, C9 arm, C8/R8 arm, R25 wiper, and Q4 input.
class ToneStack
{
public:
    void prepare(double newSampleRate, int, int channels)
    {
        sampleRate = newSampleRate;
        states.resize(static_cast<size_t>(channels));
        reset();
    }

    void reset()
    {
        for (auto& state : states)
            state = {};
    }

    float processSample(int channel, float source, float tone, float high)
    {
        auto& state = states[static_cast<size_t>(channel)];
        Matrix matrix {};
        Vector rhs {};

        constexpr auto collector = 0;
        constexpr auto c9Arm = 1;
        constexpr auto c8Arm = 2;
        constexpr auto wiper = 3;
        constexpr auto recoveryInput = 4;

        // Q2 collector is represented by its 10 k collector resistor. C9/R5
        // and C8/R8 are the two passive branches; C3 couples the pot wiper
        // to the recovery transistor's input impedance.
        addResistorToKnownVoltage(matrix, rhs, collector, 10000.0f, source);
        addResistorToGround(matrix, c9Arm, 470000.0f); // R5

        // R8 uses lugs 1 and 2, making it a 0-25 k rheostat. Higher UI values
        // reduce shunt loss and therefore increase the audible high branch.
        const auto r8 = juce::jmax(20.0f, (1.0f - juce::jlimit(0.0f, 1.0f, high)) * 25000.0f);
        addResistorToGround(matrix, c8Arm, r8);
        addResistorToGround(matrix, recoveryInput, 220000.0f);

        constexpr auto potResistance = 250000.0f;
        constexpr auto endStopResistance = 30.0f;
        const auto position = juce::jlimit(0.0f, 1.0f, tone);
        addResistor(matrix, c9Arm, wiper, juce::jmax(endStopResistance, position * potResistance));
        addResistor(matrix, c8Arm, wiper, juce::jmax(endStopResistance, (1.0f - position) * potResistance));

        stampCapacitor(matrix, rhs, collector, c9Arm, 0.01e-6f, state.c9);
        stampCapacitor(matrix, rhs, collector, c8Arm, 0.022e-6f, state.c8);
        stampCapacitor(matrix, rhs, wiper, recoveryInput, 0.047e-6f, state.c3);

        const auto voltages = solve(matrix, rhs);
        updateCapacitor(state.c9, voltages[collector] - voltages[c9Arm], 0.01e-6f);
        updateCapacitor(state.c8, voltages[collector] - voltages[c8Arm], 0.022e-6f);
        updateCapacitor(state.c3, voltages[wiper] - voltages[recoveryInput], 0.047e-6f);
        return clampAudio(voltages[recoveryInput]);
    }

private:
    static constexpr auto nodeCount = 5;
    using Matrix = std::array<std::array<float, nodeCount>, nodeCount>;
    using Vector = std::array<float, nodeCount>;

    struct CapacitorState { float voltage {}; float current {}; };
    struct ChannelState { CapacitorState c9, c8, c3; };

    static void addResistorToGround(Matrix& matrix, int node, float resistance)
    {
        matrix[static_cast<size_t>(node)][static_cast<size_t>(node)] += 1.0f / resistance;
    }

    static void addResistorToKnownVoltage(Matrix& matrix, Vector& rhs, int node, float resistance, float voltage)
    {
        const auto conductance = 1.0f / resistance;
        matrix[static_cast<size_t>(node)][static_cast<size_t>(node)] += conductance;
        rhs[static_cast<size_t>(node)] += conductance * voltage;
    }

    static void addResistor(Matrix& matrix, int first, int second, float resistance)
    {
        const auto conductance = 1.0f / resistance;
        matrix[static_cast<size_t>(first)][static_cast<size_t>(first)] += conductance;
        matrix[static_cast<size_t>(second)][static_cast<size_t>(second)] += conductance;
        matrix[static_cast<size_t>(first)][static_cast<size_t>(second)] -= conductance;
        matrix[static_cast<size_t>(second)][static_cast<size_t>(first)] -= conductance;
    }

    void stampCapacitor(Matrix& matrix, Vector& rhs, int first, int second, float capacitance, const CapacitorState& state) const
    {
        const auto conductance = 2.0f * capacitance * static_cast<float>(sampleRate);
        const auto historyCurrent = -conductance * state.voltage - state.current;
        matrix[static_cast<size_t>(first)][static_cast<size_t>(first)] += conductance;
        matrix[static_cast<size_t>(second)][static_cast<size_t>(second)] += conductance;
        matrix[static_cast<size_t>(first)][static_cast<size_t>(second)] -= conductance;
        matrix[static_cast<size_t>(second)][static_cast<size_t>(first)] -= conductance;
        rhs[static_cast<size_t>(first)] -= historyCurrent;
        rhs[static_cast<size_t>(second)] += historyCurrent;
    }

    void updateCapacitor(CapacitorState& state, float voltage, float capacitance) const
    {
        const auto conductance = 2.0f * capacitance * static_cast<float>(sampleRate);
        const auto historyCurrent = -conductance * state.voltage - state.current;
        state.current = conductance * voltage + historyCurrent;
        state.voltage = voltage;
    }

    static Vector solve(Matrix matrix, Vector rhs)
    {
        for (auto pivot = 0; pivot < nodeCount; ++pivot)
        {
            auto best = pivot;
            for (auto row = pivot + 1; row < nodeCount; ++row)
                if (std::abs(matrix[static_cast<size_t>(row)][static_cast<size_t>(pivot)]) > std::abs(matrix[static_cast<size_t>(best)][static_cast<size_t>(pivot)]))
                    best = row;
            std::swap(matrix[static_cast<size_t>(pivot)], matrix[static_cast<size_t>(best)]);
            std::swap(rhs[static_cast<size_t>(pivot)], rhs[static_cast<size_t>(best)]);

            const auto value = matrix[static_cast<size_t>(pivot)][static_cast<size_t>(pivot)];
            if (std::abs(value) < 1.0e-12f)
                return {};
            for (auto column = pivot; column < nodeCount; ++column)
                matrix[static_cast<size_t>(pivot)][static_cast<size_t>(column)] /= value;
            rhs[static_cast<size_t>(pivot)] /= value;

            for (auto row = 0; row < nodeCount; ++row)
            {
                if (row == pivot)
                    continue;
                const auto factor = matrix[static_cast<size_t>(row)][static_cast<size_t>(pivot)];
                for (auto column = pivot; column < nodeCount; ++column)
                    matrix[static_cast<size_t>(row)][static_cast<size_t>(column)] -= factor * matrix[static_cast<size_t>(pivot)][static_cast<size_t>(column)];
                rhs[static_cast<size_t>(row)] -= factor * rhs[static_cast<size_t>(pivot)];
            }
        }
        return rhs;
    }

    double sampleRate { 44100.0 };
    std::vector<ChannelState> states;
};
} // namespace pyramidfuzz::dsp
