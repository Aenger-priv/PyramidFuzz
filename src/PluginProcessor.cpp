#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdlib>

PyramidFuzzAudioProcessor::PyramidFuzzAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      valueTreeState(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

void PyramidFuzzAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const auto channels = juce::jmax(getTotalNumInputChannels(), 1);
    preparedBlockSize = juce::jmax(1, samplesPerBlock);
    fuzzEngine.updateParameters(readParameters());
    fuzzEngine.prepare(sampleRate, preparedBlockSize, channels);
    latencySamples = fuzzEngine.getLatencySamples();
    setLatencySamples(latencySamples);
    inputMeter.prepare(sampleRate); outputMeter.prepare(sampleRate);
    dryRing.setSize(channels,latencySamples+1); dryRing.clear();
    dryScratch.setSize(channels,preparedBlockSize); dryScratch.clear();
    dryPosition=0; hasProcessed=false;
    wetMix.reset(sampleRate,.005); wetMix.setCurrentAndTargetValue(1);
}

void PyramidFuzzAudioProcessor::releaseResources()
{
    fuzzEngine.reset();
    inputMeter.reset(); outputMeter.reset();
    dryRing.clear(); dryScratch.clear(); dryPosition=0; hasProcessed=false;
}

bool PyramidFuzzAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainInput = layouts.getMainInputChannelSet();
    const auto& mainOutput = layouts.getMainOutputChannelSet();

    const auto inputIsMonoOrStereo = mainInput == juce::AudioChannelSet::mono()
                                  || mainInput == juce::AudioChannelSet::stereo();
    const auto outputIsMonoOrStereo = mainOutput == juce::AudioChannelSet::mono()
                                   || mainOutput == juce::AudioChannelSet::stereo();

    if (! inputIsMonoOrStereo || ! outputIsMonoOrStereo)
        return false;

    // The engine intentionally supports the useful mono-to-stereo expansion,
    // but not stereo-to-mono: the latter would otherwise silently drop right.
    return mainInput == mainOutput
        || (mainInput == juce::AudioChannelSet::mono()
            && mainOutput == juce::AudioChannelSet::stereo());
}

void PyramidFuzzAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    processAudio(buffer,midiMessages,false);
}

void PyramidFuzzAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    processAudio(buffer,midiMessages,true);
}

void PyramidFuzzAudioProcessor::processAudio(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, bool bypassed)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const auto numInputChannels = getTotalNumInputChannels();
    const auto numOutputChannels = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();
    if (numSamples == 0) return;
    if (!hasProcessed) { wetMix.setCurrentAndTargetValue(bypassed?0.0f:1.0f); hasProcessed=true; }
    wetMix.setTargetValue(bypassed?0.0f:1.0f);
    float inputPeak = 0.0f;

    for (auto channel = 0; channel < numInputChannels; ++channel)
        inputPeak = juce::jmax(inputPeak, buffer.getMagnitude(channel, 0, numSamples));

    for (auto channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear(channel, 0, numSamples);

    fuzzEngine.updateParameters(readParameters());
    // Hosts can occasionally supply blocks larger than their preparation hint.
    // Process bounded views so the engine's preallocated scratch remains safe.
    for (int offset = 0; offset < numSamples; offset += preparedBlockSize)
    {
        const auto length = juce::jmin(preparedBlockSize, numSamples - offset);
        juce::AudioBuffer<float> chunk(buffer.getArrayOfWritePointers(), numInputChannels, offset, length);
        // Keep an aligned dry history even while wet. No resize/allocation here.
        const int ringSize=dryRing.getNumSamples();
        for(int i=0;i<length;++i)
        {
            const int read=(dryPosition+ringSize-latencySamples)%ringSize;
            for(int ch=0;ch<numInputChannels;++ch)
            {
                auto* ring=dryRing.getWritePointer(ch);
                const float raw=chunk.getSample(ch,i);
                ring[dryPosition]=std::isfinite(raw)?raw:0.0f;
                dryScratch.setSample(ch,i,ring[read]);
            }
            dryPosition=(dryPosition+1)%ringSize;
        }
        // Keep wet state warm through host bypass, including quality changes.
        fuzzEngine.processBlock(chunk);
        for(int i=0;i<length;++i)
        {
            const float mix=wetMix.getNextValue();
            if(mix>=1.0f) continue; // Preserve active-path samples exactly.
            for(int ch=0;ch<numInputChannels;++ch)
            {
                const float dry=dryScratch.getSample(ch,i), wet=chunk.getSample(ch,i);
                chunk.setSample(ch,i,mix<=0.0f?dry:dry+mix*(wet-dry));
            }
        }
    }

    // A mono input needs one circuit path; expand the processed result afterward.
    if (numInputChannels == 1 && numOutputChannels > 1)
        for (auto channel = 1; channel < numOutputChannels; ++channel)
            buffer.copyFrom(channel, 0, buffer, 0, 0, numSamples);

    float outputPeak = 0.0f;
    for (auto channel = 0; channel < numOutputChannels; ++channel)
        outputPeak = juce::jmax(outputPeak, buffer.getMagnitude(channel, 0, numSamples));

    inputMeter.push(inputPeak,numSamples);
    outputMeter.push(outputPeak,numSamples);

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
    return 10.0;
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
    if (data == nullptr || sizeInBytes <= 0 || sizeInBytes > 1024 * 1024) return;
    const std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr || !xmlState->hasTagName("PARAMETERS")) return;
    const auto state = juce::ValueTree::fromXml(*xmlState);
    if (!state.isValid()) return;
    juce::StringArray seen;
    for (const auto& child : state)
    {
        if (!child.hasType("PARAM") || !child.hasProperty("id") || !child.hasProperty("value")) return;
        const auto id = child["id"].toString();
        const auto* parameter = valueTreeState.getParameter(id);
        if (parameter == nullptr || seen.contains(id)) return;
        seen.add(id);
        const auto text = child["value"].toString().trim();
        const auto* start = text.toRawUTF8();
        char* end = nullptr;
        const double value = std::strtod(start, &end);
        const auto range = parameter->getNormalisableRange();
        if (end == start || *end != 0 || !std::isfinite(value) || value < range.start || value > range.end) return;
    }
    if (seen.size() != getParameters().size()) return;
    valueTreeState.replaceState(state);
}

juce::AudioProcessorValueTreeState& PyramidFuzzAudioProcessor::getValueTreeState() noexcept
{
    return valueTreeState;
}

float PyramidFuzzAudioProcessor::getInputMeterLevel() const noexcept
{
    return inputMeter.level();
}

float PyramidFuzzAudioProcessor::getOutputMeterLevel() const noexcept
{
    return outputMeter.level();
}

juce::AudioProcessorValueTreeState::ParameterLayout PyramidFuzzAudioProcessor::createParameterLayout()
{
    using ChoiceParameter = juce::AudioParameterChoice;
    using FloatParameter = juce::AudioParameterFloat;

    const auto decibels = juce::AudioParameterFloatAttributes().withLabel("dB")
        .withStringFromValueFunction([](float x,int) { return (x>0?"+":"") + juce::String(x,1); })
        .withValueFromStringFunction([](const juce::String& text) { return text.getFloatValue(); });
    const auto percent = juce::AudioParameterFloatAttributes().withLabel("%")
        .withStringFromValueFunction([](float x,int) { return juce::String(x*100.0f,1); })
        .withValueFromStringFunction([](const juce::String& text) { return text.getFloatValue()/100.0f; });
    const auto amount = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction([](float x,int) { return juce::String(x,1); });
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    parameters.push_back(std::make_unique<FloatParameter>(pyramidfuzz::ParameterIds::input,
                                                          "Input",
                                                          juce::NormalisableRange<float>(-12.0f, 18.0f, 0.01f),
                                                          0.0f,
                                                          decibels));
    parameters.push_back(std::make_unique<FloatParameter>(pyramidfuzz::ParameterIds::sustain,
                                                          "Sustain",
                                                          juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f),
                                                          6.8f, amount));
    parameters.push_back(std::make_unique<FloatParameter>(pyramidfuzz::ParameterIds::tone,
                                                          "Tone",
                                                          juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                          0.34f, percent));
    parameters.push_back(std::make_unique<FloatParameter>(pyramidfuzz::ParameterIds::high,
                                                          "High",
                                                          juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                                                          0.22f, percent));
    parameters.push_back(std::make_unique<FloatParameter>(pyramidfuzz::ParameterIds::volume,
                                                          "Volume",
                                                          juce::NormalisableRange<float>(-30.0f, 12.0f, 0.01f),
                                                          -11.0f,
                                                          decibels));
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

pyramidfuzz::dsp::FuzzParameters PyramidFuzzAudioProcessor::readParameters() const
{
    pyramidfuzz::dsp::FuzzParameters parameters;
    parameters.inputDecibels = *valueTreeState.getRawParameterValue(pyramidfuzz::ParameterIds::input);
    parameters.sustain = *valueTreeState.getRawParameterValue(pyramidfuzz::ParameterIds::sustain) / 10.0f;
    parameters.tone = *valueTreeState.getRawParameterValue(pyramidfuzz::ParameterIds::tone);
    parameters.high = *valueTreeState.getRawParameterValue(pyramidfuzz::ParameterIds::high);
    parameters.volumeDecibels = *valueTreeState.getRawParameterValue(pyramidfuzz::ParameterIds::volume);
    parameters.voicingMode = static_cast<int>(*valueTreeState.getRawParameterValue(pyramidfuzz::ParameterIds::voicing));
    parameters.clippingMode = static_cast<int>(*valueTreeState.getRawParameterValue(pyramidfuzz::ParameterIds::clippingMode));
    parameters.oversamplingMode = static_cast<int>(*valueTreeState.getRawParameterValue(pyramidfuzz::ParameterIds::oversampling));
    return parameters;
}
