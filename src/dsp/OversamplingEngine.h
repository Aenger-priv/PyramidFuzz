#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>

namespace pyramidfuzz::dsp
{
class OversamplingEngine
{
public:
    void prepare(int channels, int maximumBlockSize)
    {
        oversamplers[0] = std::make_unique<juce::dsp::Oversampling<float>>(static_cast<size_t>(channels),
                                                                            1,
                                                                            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                                                                            true,
                                                                            true);
        oversamplers[1] = std::make_unique<juce::dsp::Oversampling<float>>(static_cast<size_t>(channels),
                                                                            2,
                                                                            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                                                                            true,
                                                                            true);
        oversamplers[2] = std::make_unique<juce::dsp::Oversampling<float>>(static_cast<size_t>(channels),
                                                                            3,
                                                                            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                                                                            true,
                                                                            true);

        maximumLatency = 0;
        for (auto& oversampler : oversamplers)
        {
            oversampler->reset();
            oversampler->initProcessing(static_cast<size_t>(maximumBlockSize));
            maximumLatency = std::max(maximumLatency, juce::roundToInt(oversampler->getLatencyInSamples()));
        }
        padding.setSize(channels, maximumLatency + 1);
        padding.clear();
        paddingPosition = 0;
    }

    void reset()
    {
        for (auto& oversampler : oversamplers)
            if (oversampler != nullptr)
                oversampler->reset();
        padding.clear();
        paddingPosition = 0;
    }

    void setMode(int newModeIndex)
    {
        const auto next = juce::jlimit(0, 2, newModeIndex);
        if (next == modeIndex) return;
        modeIndex = next;
        if (auto& sampler = oversamplers[static_cast<size_t>(modeIndex)]; sampler != nullptr) sampler->reset();
        padding.clear(); paddingPosition = 0;
    }

    int getMode() const noexcept
    {
        return modeIndex;
    }

    int getFactor() const noexcept
    {
        return 1 << (modeIndex + 1);
    }

    int getLatencySamples() const noexcept { return maximumLatency; }
    int getNativeLatencySamples(int mode) const
    {
        const auto& sampler = oversamplers[static_cast<size_t>(juce::jlimit(0,2,mode))];
        return sampler == nullptr ? 0 : juce::roundToInt(sampler->getLatencyInSamples());
    }

    template <typename ProcessFn>
    void processBlock(juce::AudioBuffer<float>& buffer, ProcessFn&& processFunction)
    {
        auto block = juce::dsp::AudioBlock<float>(buffer);
        auto& oversampler = *oversamplers[static_cast<size_t>(modeIndex)];
        auto oversampledBlock = oversampler.processSamplesUp(block);
        processFunction(oversampledBlock, getFactor());
        oversampler.processSamplesDown(block);
        const int delay = maximumLatency - getNativeLatencySamples(modeIndex);
        const int ringSize = padding.getNumSamples();
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const int read = (paddingPosition + ringSize - delay) % ringSize;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto* ring = padding.getWritePointer(ch);
                ring[paddingPosition] = buffer.getSample(ch,i);
                buffer.setSample(ch,i,ring[read]);
            }
            paddingPosition = (paddingPosition + 1) % ringSize;
        }
    }

private:
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;
    int modeIndex { 1 };
    int maximumLatency = 0, paddingPosition = 0;
    juce::AudioBuffer<float> padding;
};
} // namespace pyramidfuzz::dsp
