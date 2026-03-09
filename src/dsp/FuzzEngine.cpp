#include "FuzzEngine.h"

namespace pyramidfuzz::dsp
{
void FuzzEngine::prepare(double newSampleRate, int maximumBlockSize, int channels)
{
    sampleRate = newSampleRate;
    maxBlockSize = maximumBlockSize;
    numChannels = channels;

    inputVoicingStage.prepare(sampleRate, maxBlockSize, numChannels);
    toneStack.prepare(sampleRate, maxBlockSize, numChannels);
    highControlFilter.prepare(sampleRate, maxBlockSize, numChannels);
    outputStage.prepare(sampleRate, maxBlockSize, numChannels);
    oversamplingEngine.prepare(numChannels, maxBlockSize);
    oversamplingEngine.setMode(activeOversamplingMode);
    updateNonlinearSampleRate();

    resetSmoother(inputGainSmoothed, sampleRate, 0.02, dbToGain(currentParameters.inputDecibels));
    resetSmoother(sustainSmoothed, sampleRate, 0.03, currentParameters.sustain);
    resetSmoother(toneSmoothed, sampleRate, 0.03, currentParameters.tone);
    resetSmoother(highSmoothed, sampleRate, 0.03, currentParameters.high);
    resetSmoother(volumeGainSmoothed, sampleRate, 0.02, dbToGain(currentParameters.volumeDecibels));
    resetSmoother(voicingMorphSmoothed, sampleRate, 0.01, static_cast<float>(currentParameters.voicingMode));
    resetSmoother(clippingModeMorphSmoothed, sampleRate, 0.01, static_cast<float>(currentParameters.clippingMode));

    sustainValues.assign(static_cast<size_t>(maxBlockSize), 0.0f);
    toneValues.assign(static_cast<size_t>(maxBlockSize), 0.0f);
    highValues.assign(static_cast<size_t>(maxBlockSize), 0.0f);
    volumeValues.assign(static_cast<size_t>(maxBlockSize), 0.0f);
    clippingModeValues.assign(static_cast<size_t>(maxBlockSize), 0.0f);
}

void FuzzEngine::reset()
{
    inputVoicingStage.reset();
    preGainStage.reset();
    clippingStageA.reset();
    clippingStageB.reset();
    toneStack.reset();
    highControlFilter.reset();
    outputStage.reset();
    oversamplingEngine.reset();
}

void FuzzEngine::updateParameters(const FuzzParameters& newParameters)
{
    currentParameters = newParameters;

    inputGainSmoothed.setTargetValue(dbToGain(currentParameters.inputDecibels));
    sustainSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, currentParameters.sustain));
    toneSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, currentParameters.tone));
    highSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, currentParameters.high));
    volumeGainSmoothed.setTargetValue(dbToGain(currentParameters.volumeDecibels));
    voicingMorphSmoothed.setTargetValue(static_cast<float>(juce::jlimit(0, 1, currentParameters.voicingMode)));
    clippingModeMorphSmoothed.setTargetValue(static_cast<float>(juce::jlimit(0, 2, currentParameters.clippingMode)));

    if (currentParameters.oversamplingMode != activeOversamplingMode)
    {
        activeOversamplingMode = juce::jlimit(0, 2, currentParameters.oversamplingMode);
        oversamplingEngine.setMode(activeOversamplingMode);
        updateNonlinearSampleRate();
        oversamplingEngine.reset();
    }
}

void FuzzEngine::processBlock(juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    jassert(numSamples <= maxBlockSize);

    for (auto sample = 0; sample < numSamples; ++sample)
    {
        const auto inputGain = inputGainSmoothed.getNextValue();
        const auto voicing = voicingMorphSmoothed.getNextValue();
        const auto sustain = sustainSmoothed.getNextValue();
        const auto tone = toneSmoothed.getNextValue();
        const auto high = highSmoothed.getNextValue();
        const auto volume = volumeGainSmoothed.getNextValue();
        const auto clippingMode = clippingModeMorphSmoothed.getNextValue();

        sustainValues[static_cast<size_t>(sample)] = sustain;
        toneValues[static_cast<size_t>(sample)] = tone;
        highValues[static_cast<size_t>(sample)] = high;
        volumeValues[static_cast<size_t>(sample)] = volume;
        clippingModeValues[static_cast<size_t>(sample)] = clippingMode;

        for (auto channel = 0; channel < numChannels; ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            channelData[sample] = inputVoicingStage.processSample(channel, channelData[sample], inputGain, voicing);
        }
    }

    oversamplingEngine.processBlock(buffer, [this, numSamples](auto& oversampledBlock, int oversamplingFactor)
    {
        const auto oversampledSamples = static_cast<int>(oversampledBlock.getNumSamples());
        const auto oversampledChannels = static_cast<int>(oversampledBlock.getNumChannels());

        for (auto sample = 0; sample < oversampledSamples; ++sample)
        {
            const auto baseIndex = juce::jlimit(0, numSamples - 1, sample / oversamplingFactor);
            const auto sustain = sustainValues[static_cast<size_t>(baseIndex)];
            const auto clippingMode = clippingModeValues[static_cast<size_t>(baseIndex)];

            for (auto channel = 0; channel < oversampledChannels; ++channel)
            {
                auto* channelData = oversampledBlock.getChannelPointer(static_cast<size_t>(channel));
                auto nonlinearSample = channelData[sample];
                nonlinearSample = preGainStage.processSample(channel, nonlinearSample, sustain);
                nonlinearSample = clippingStageA.processSample(channel, nonlinearSample, sustain);
                nonlinearSample = clippingStageB.processSample(channel, nonlinearSample, sustain, clippingMode);
                channelData[sample] = clampAudio(nonlinearSample);
            }
        }
    });

    for (auto sample = 0; sample < numSamples; ++sample)
    {
        const auto tone = toneValues[static_cast<size_t>(sample)];
        const auto high = highValues[static_cast<size_t>(sample)];
        const auto volume = volumeValues[static_cast<size_t>(sample)];

        for (auto channel = 0; channel < numChannels; ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            auto processedSample = channelData[sample];
            processedSample = toneStack.processSample(channel, processedSample, tone);
            processedSample = highControlFilter.processSample(channel, processedSample, high);
            channelData[sample] = outputStage.processSample(processedSample, volume);
        }
    }
}

int FuzzEngine::getLatencySamples() const
{
    return oversamplingEngine.getLatencySamples();
}

void FuzzEngine::updateNonlinearSampleRate()
{
    const auto oversampledRate = sampleRate * oversamplingEngine.getFactor();
    const auto oversampledBlockSize = maxBlockSize * oversamplingEngine.getFactor();

    preGainStage.prepare(oversampledRate, oversampledBlockSize, numChannels);
    clippingStageA.prepare(oversampledRate, oversampledBlockSize, numChannels);
    clippingStageB.prepare(oversampledRate, oversampledBlockSize, numChannels);
}

void FuzzEngine::ensureScratchCapacity(int numSamples)
{
    jassert(numSamples <= maxBlockSize);
    juce::ignoreUnused(numSamples);
}
} // namespace pyramidfuzz::dsp
