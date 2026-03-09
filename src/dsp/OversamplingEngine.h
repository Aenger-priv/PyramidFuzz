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
                                                                            false);
        oversamplers[1] = std::make_unique<juce::dsp::Oversampling<float>>(static_cast<size_t>(channels),
                                                                            2,
                                                                            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                                                                            true,
                                                                            false);
        oversamplers[2] = std::make_unique<juce::dsp::Oversampling<float>>(static_cast<size_t>(channels),
                                                                            3,
                                                                            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                                                                            true,
                                                                            false);

        for (auto& oversampler : oversamplers)
        {
            oversampler->reset();
            oversampler->initProcessing(static_cast<size_t>(maximumBlockSize));
        }
    }

    void reset()
    {
        for (auto& oversampler : oversamplers)
            if (oversampler != nullptr)
                oversampler->reset();
    }

    void setMode(int newModeIndex)
    {
        modeIndex = juce::jlimit(0, 2, newModeIndex);
    }

    int getMode() const noexcept
    {
        return modeIndex;
    }

    int getFactor() const noexcept
    {
        return 1 << (modeIndex + 1);
    }

    int getLatencySamples() const
    {
        if (oversamplers[static_cast<size_t>(modeIndex)] == nullptr)
            return 0;

        return static_cast<int>(std::ceil(oversamplers[static_cast<size_t>(modeIndex)]->getLatencyInSamples()));
    }

    template <typename ProcessFn>
    void processBlock(juce::AudioBuffer<float>& buffer, ProcessFn&& processFunction)
    {
        auto block = juce::dsp::AudioBlock<float>(buffer);
        auto& oversampler = *oversamplers[static_cast<size_t>(modeIndex)];
        auto oversampledBlock = oversampler.processSamplesUp(block);
        processFunction(oversampledBlock, getFactor());
        oversampler.processSamplesDown(block);
    }

private:
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;
    int modeIndex { 1 };
};
} // namespace pyramidfuzz::dsp
