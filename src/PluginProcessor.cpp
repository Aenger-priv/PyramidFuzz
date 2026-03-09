#include "PluginProcessor.h"
#include "PluginEditor.h"

PyramidFuzzAudioProcessor::PyramidFuzzAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      valueTreeState(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

void PyramidFuzzAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const auto channels = juce::jmax(getTotalNumOutputChannels(), 1);
    fuzzEngine.prepare(sampleRate, samplesPerBlock, channels);
    latencySamples = fuzzEngine.getLatencySamples();
    setLatencySamples(latencySamples);
}

void PyramidFuzzAudioProcessor::releaseResources()
{
    fuzzEngine.reset();
}

bool PyramidFuzzAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainInput = layouts.getMainInputChannelSet();
    const auto& mainOutput = layouts.getMainOutputChannelSet();

    const auto inputIsMonoOrStereo = mainInput == juce::AudioChannelSet::mono()
                                  || mainInput == juce::AudioChannelSet::stereo();
    const auto outputIsMonoOrStereo = mainOutput == juce::AudioChannelSet::mono()
                                   || mainOutput == juce::AudioChannelSet::stereo();

    return inputIsMonoOrStereo && outputIsMonoOrStereo;
}

void PyramidFuzzAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const auto numInputChannels = getTotalNumInputChannels();
    const auto numOutputChannels = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();
    float inputPeak = 0.0f;

    for (auto channel = 0; channel < numInputChannels; ++channel)
        inputPeak = juce::jmax(inputPeak, buffer.getMagnitude(channel, 0, numSamples));

    if (numInputChannels == 1 && numOutputChannels > 1)
    {
        for (auto channel = 1; channel < numOutputChannels; ++channel)
            buffer.copyFrom(channel, 0, buffer, 0, 0, numSamples);
    }
    else
    {
        for (auto channel = numInputChannels; channel < numOutputChannels; ++channel)
            buffer.clear(channel, 0, numSamples);
    }

    pyramidfuzz::dsp::FuzzParameters parameters;
    parameters.inputDecibels = *valueTreeState.getRawParameterValue(pyramidfuzz::ParameterIds::input);
    parameters.sustain = *valueTreeState.getRawParameterValue(pyramidfuzz::ParameterIds::sustain) / 10.0f;
    parameters.tone = *valueTreeState.getRawParameterValue(pyramidfuzz::ParameterIds::tone);
    parameters.high = *valueTreeState.getRawParameterValue(pyramidfuzz::ParameterIds::high);
    parameters.volumeDecibels = *valueTreeState.getRawParameterValue(pyramidfuzz::ParameterIds::volume);
    parameters.voicingMode = static_cast<int>(*valueTreeState.getRawParameterValue(pyramidfuzz::ParameterIds::voicing));
    parameters.clippingMode = static_cast<int>(*valueTreeState.getRawParameterValue(pyramidfuzz::ParameterIds::clippingMode));
    parameters.oversamplingMode = static_cast<int>(*valueTreeState.getRawParameterValue(pyramidfuzz::ParameterIds::oversampling));

    fuzzEngine.updateParameters(parameters);
    fuzzEngine.processBlock(buffer);

    float outputPeak = 0.0f;
    for (auto channel = 0; channel < numOutputChannels; ++channel)
        outputPeak = juce::jmax(outputPeak, buffer.getMagnitude(channel, 0, numSamples));

    updateMeter(inputMeterLevel, inputPeak);
    updateMeter(outputMeterLevel, outputPeak);

    const auto reportedLatency = fuzzEngine.getLatencySamples();
    if (reportedLatency != latencySamples)
    {
        latencySamples = reportedLatency;
        setLatencySamples(latencySamples);
    }
}

juce::AudioProcessorEditor* PyramidFuzzAudioProcessor::createEditor()
{
    return new PyramidFuzzAudioProcessorEditor(*this);
}

bool PyramidFuzzAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String PyramidFuzzAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PyramidFuzzAudioProcessor::acceptsMidi() const
{
    return false;
}

bool PyramidFuzzAudioProcessor::producesMidi() const
{
    return false;
}

bool PyramidFuzzAudioProcessor::isMidiEffect() const
{
    return false;
}

double PyramidFuzzAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PyramidFuzzAudioProcessor::getNumPrograms()
{
    return 1;
}

int PyramidFuzzAudioProcessor::getCurrentProgram()
{
    return 0;
}

void PyramidFuzzAudioProcessor::setCurrentProgram(int)
{
}

const juce::String PyramidFuzzAudioProcessor::getProgramName(int)
{
    return {};
}

void PyramidFuzzAudioProcessor::changeProgramName(int, const juce::String&)
{
}

void PyramidFuzzAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (const auto state = valueTreeState.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        copyXmlToBinary(*xml, destData);
    }
}

void PyramidFuzzAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState == nullptr)
        return;

    if (const auto state = juce::ValueTree::fromXml(*xmlState); state.isValid())
        valueTreeState.replaceState(state);
}

juce::AudioProcessorValueTreeState& PyramidFuzzAudioProcessor::getValueTreeState() noexcept
{
    return valueTreeState;
}

float PyramidFuzzAudioProcessor::getInputMeterLevel() const noexcept
{
    return inputMeterLevel.load(std::memory_order_relaxed);
}

float PyramidFuzzAudioProcessor::getOutputMeterLevel() const noexcept
{
    return outputMeterLevel.load(std::memory_order_relaxed);
}

juce::AudioProcessorValueTreeState::ParameterLayout PyramidFuzzAudioProcessor::createParameterLayout()
{
    using ChoiceParameter = juce::AudioParameterChoice;
    using FloatParameter = juce::AudioParameterFloat;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    parameters.push_back(std::make_unique<FloatParameter>(pyramidfuzz::ParameterIds::input,
                                                          "Input",
                                                          juce::NormalisableRange<float>(-12.0f, 18.0f, 0.01f),
                                                          0.0f,
                                                          " dB"));
    parameters.push_back(std::make_unique<FloatParameter>(pyramidfuzz::ParameterIds::sustain,
                                                          "Sustain",
                                                          juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f),
                                                          6.8f));
    parameters.push_back(std::make_unique<FloatParameter>(pyramidfuzz::ParameterIds::tone,
                                                          "Tone",
                                                          juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                          0.34f));
    parameters.push_back(std::make_unique<FloatParameter>(pyramidfuzz::ParameterIds::high,
                                                          "High",
                                                          juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                          0.22f));
    parameters.push_back(std::make_unique<FloatParameter>(pyramidfuzz::ParameterIds::volume,
                                                          "Volume",
                                                          juce::NormalisableRange<float>(-30.0f, 12.0f, 0.01f),
                                                          -11.0f,
                                                          " dB"));
    parameters.push_back(std::make_unique<ChoiceParameter>(pyramidfuzz::ParameterIds::voicing,
                                                           "Voicing",
                                                           juce::StringArray { "Hi", "Lo" },
                                                           1));
    parameters.push_back(std::make_unique<ChoiceParameter>(pyramidfuzz::ParameterIds::clippingMode,
                                                           "Clipping",
                                                           juce::StringArray { "Off", "Silicon", "Germanium" },
                                                           1));
    parameters.push_back(std::make_unique<ChoiceParameter>(pyramidfuzz::ParameterIds::oversampling,
                                                           "Oversampling",
                                                           juce::StringArray { "2x", "4x", "8x" },
                                                           1));

    return { parameters.begin(), parameters.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PyramidFuzzAudioProcessor();
}

void PyramidFuzzAudioProcessor::updateMeter(std::atomic<float>& meter, float newPeak) noexcept
{
    const auto previous = meter.load(std::memory_order_relaxed);
    const auto next = newPeak > previous ? newPeak : previous * 0.9f;
    meter.store(juce::jlimit(0.0f, 1.2f, next), std::memory_order_relaxed);
}
