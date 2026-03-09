#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "dsp/FuzzEngine.h"

namespace pyramidfuzz
{
namespace ParameterIds
{
inline constexpr auto input = "input";
inline constexpr auto sustain = "sustain";
inline constexpr auto tone = "tone";
inline constexpr auto high = "high";
inline constexpr auto volume = "volume";
inline constexpr auto voicing = "voicing";
inline constexpr auto clippingMode = "clippingMode";
inline constexpr auto oversampling = "oversampling";
}
} // namespace pyramidfuzz

class PyramidFuzzAudioProcessor final : public juce::AudioProcessor
{
public:
    PyramidFuzzAudioProcessor();
    ~PyramidFuzzAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept;
    float getInputMeterLevel() const noexcept;
    float getOutputMeterLevel() const noexcept;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void updateMeter(std::atomic<float>& meter, float newPeak) noexcept;

    pyramidfuzz::dsp::FuzzEngine fuzzEngine;
    juce::AudioProcessorValueTreeState valueTreeState;
    int latencySamples { 0 };
    std::atomic<float> inputMeterLevel { 0.0f };
    std::atomic<float> outputMeterLevel { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PyramidFuzzAudioProcessor)
};
