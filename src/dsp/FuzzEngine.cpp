#include "FuzzEngine.h"

namespace pyramidfuzz::dsp
{
void FuzzEngine::prepare(double newSampleRate, int maximumBlockSize, int channels)
{
    sampleRate = newSampleRate;
    maxBlockSize = std::max(1, maximumBlockSize);
    numChannels = channels;

    oversamplingEngine.prepare(numChannels, maxBlockSize);
    activeOversamplingMode = juce::jlimit(0, 2, currentParameters.oversamplingMode);
    oversamplingEngine.setMode(activeOversamplingMode);
    fadingOutForOversamplingChange = false;
    for (int mode=0; mode<3; ++mode)
        paths[static_cast<size_t>(mode)].prepare(sampleRate * (2 << mode), maxBlockSize * (2 << mode), numChannels);

    resetSmoother(inputGainSmoothed, sampleRate, 0.02, dbToGain(currentParameters.inputDecibels));
    resetSmoother(sustainSmoothed, sampleRate, 0.03, currentParameters.sustain);
    resetSmoother(toneSmoothed, sampleRate, 0.03, currentParameters.tone);
    resetSmoother(highSmoothed, sampleRate, 0.03, currentParameters.high);
    resetSmoother(volumeGainSmoothed, sampleRate, 0.02, dbToGain(currentParameters.volumeDecibels));
    resetSmoother(voicingMorphSmoothed, sampleRate, 0.01, static_cast<float>(currentParameters.voicingMode));
    resetSmoother(clippingModeMorphSmoothed, sampleRate, 0.01, static_cast<float>(currentParameters.clippingMode));
    // Changing oversampling replaces filter state. Briefly mute the wet signal around
    // that change so a quality selection cannot produce a discontinuity at the output.
    resetSmoother(oversamplingTransitionSmoothed, sampleRate, 0.006, 1.0f);

    prepared = true;
    sustainValues.assign(static_cast<size_t>(maxBlockSize), 0.0f);
    inputGainValues.assign(static_cast<size_t>(maxBlockSize), 0.0f);
    voicingValues.assign(static_cast<size_t>(maxBlockSize), 0.0f);
    toneValues.assign(static_cast<size_t>(maxBlockSize), 0.0f);
    highValues.assign(static_cast<size_t>(maxBlockSize), 0.0f);
    volumeValues.assign(static_cast<size_t>(maxBlockSize), 0.0f);
    clippingModeValues.assign(static_cast<size_t>(maxBlockSize), 0.0f);
    oversamplingTransitionValues.assign(static_cast<size_t>(maxBlockSize), 1.0f);
}

void FuzzEngine::reset()
{
    for (auto& path : paths) path.reset();
    oversamplingEngine.reset();
}

void FuzzEngine::updateParameters(const FuzzParameters& newParameters)
{
    const auto valid = [](float x, float lo, float hi, float fallback) { return std::isfinite(x) ? std::clamp(x,lo,hi) : fallback; };
    auto p = newParameters;
    p.inputDecibels=valid(p.inputDecibels,-12,18,currentParameters.inputDecibels);
    p.volumeDecibels=valid(p.volumeDecibels,-30,12,currentParameters.volumeDecibels);
    p.sustain=valid(p.sustain,0,1,currentParameters.sustain);
    p.tone=valid(p.tone,0,1,currentParameters.tone); p.high=valid(p.high,0,1,currentParameters.high);
    p.voicingMode=std::clamp(p.voicingMode,0,1); p.clippingMode=std::clamp(p.clippingMode,0,2);
    p.oversamplingMode=std::clamp(p.oversamplingMode,0,2);
    currentParameters = p;
    if (!prepared) return;

    inputGainSmoothed.setTargetValue(dbToGain(currentParameters.inputDecibels));
    sustainSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, currentParameters.sustain));
    toneSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, currentParameters.tone));
    highSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, currentParameters.high));
    volumeGainSmoothed.setTargetValue(dbToGain(currentParameters.volumeDecibels));
    voicingMorphSmoothed.setTargetValue(static_cast<float>(juce::jlimit(0, 1, currentParameters.voicingMode)));
    clippingModeMorphSmoothed.setTargetValue(static_cast<float>(juce::jlimit(0, 2, currentParameters.clippingMode)));

    const auto requestedOversamplingMode = juce::jlimit(0, 2, currentParameters.oversamplingMode);

    if (requestedOversamplingMode == activeOversamplingMode)
    {
        if (fadingOutForOversamplingChange)
        {
            fadingOutForOversamplingChange = false;
            oversamplingTransitionSmoothed.setTargetValue(1.0f);
        }
    }
    else if (!fadingOutForOversamplingChange)
    {
        fadingOutForOversamplingChange = true;
        oversamplingTransitionSmoothed.setTargetValue(0.0f);
    }
    else if (!oversamplingTransitionSmoothed.isSmoothing())
    {
        activeOversamplingMode = requestedOversamplingMode;
        oversamplingEngine.setMode(activeOversamplingMode);
        paths[static_cast<size_t>(activeOversamplingMode)].reset();
        fadingOutForOversamplingChange = false;
        oversamplingTransitionSmoothed.setTargetValue(1.0f);
    }
}

void FuzzEngine::processBlock(juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    jassert(numSamples <= maxBlockSize);
    if (numSamples == 0 || !prepared) return;

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
        inputGainValues[static_cast<size_t>(sample)] = inputGain;
        voicingValues[static_cast<size_t>(sample)] = voicing;
        toneValues[static_cast<size_t>(sample)] = tone;
        highValues[static_cast<size_t>(sample)] = high;
        volumeValues[static_cast<size_t>(sample)] = volume;
        clippingModeValues[static_cast<size_t>(sample)] = clippingMode;
        oversamplingTransitionValues[static_cast<size_t>(sample)] = oversamplingTransitionSmoothed.getNextValue();
    }

    oversamplingEngine.processBlock(buffer, [this, numSamples](auto& oversampledBlock, int oversamplingFactor)
    {
        const auto oversampledSamples = static_cast<int>(oversampledBlock.getNumSamples());
        const auto oversampledChannels = static_cast<int>(oversampledBlock.getNumChannels());

        for (auto sample = 0; sample < oversampledSamples; ++sample)
        {
            const auto baseIndex = juce::jlimit(0, numSamples - 1, sample / oversamplingFactor);
            const auto sustain = sustainValues[static_cast<size_t>(baseIndex)];
            const auto inputGain = inputGainValues[static_cast<size_t>(baseIndex)];
            const auto voicing = voicingValues[static_cast<size_t>(baseIndex)];
            const auto clippingMode = clippingModeValues[static_cast<size_t>(baseIndex)];
            const auto tone = toneValues[static_cast<size_t>(baseIndex)];
            const auto high = highValues[static_cast<size_t>(baseIndex)];

            for (auto channel = 0; channel < oversampledChannels; ++channel)
            {
                auto* channelData = oversampledBlock.getChannelPointer(static_cast<size_t>(channel));
                auto nonlinearSample = channelData[sample];
                channelData[sample] = paths[static_cast<size_t>(activeOversamplingMode)].process(
                    channel, nonlinearSample, inputGain, voicing, sustain, clippingMode, tone, high);
            }
        }
    });

    for (auto sample = 0; sample < numSamples; ++sample)
    {
        const auto volume = volumeValues[static_cast<size_t>(sample)];
        const auto oversamplingTransition = oversamplingTransitionValues[static_cast<size_t>(sample)];

        for (auto channel = 0; channel < numChannels; ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            channelData[sample] *= volume * OutputStage::voltsToDigital * oversamplingTransition;
        }
    }
}

int FuzzEngine::getLatencySamples() const
{
    return oversamplingEngine.getLatencySamples();
}

} // namespace pyramidfuzz::dsp
