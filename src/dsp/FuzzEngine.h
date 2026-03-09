#pragma once

#include "ClippingStage.h"
#include "HighControlFilter.h"
#include "InputVoicingStage.h"
#include "OutputStage.h"
#include "OversamplingEngine.h"
#include "PreGainStage.h"
#include "ToneStack.h"

namespace pyramidfuzz::dsp
{
struct FuzzParameters
{
    float inputDecibels { 0.0f };
    float sustain { 0.5f };
    float tone { 0.5f };
    float high { 0.5f };
    float volumeDecibels { -6.0f };
    int voicingMode { 0 };
    int clippingMode { 1 };
    int oversamplingMode { 1 };
};

class FuzzEngine
{
public:
    void prepare(double newSampleRate, int maximumBlockSize, int channels);
    void reset();
    void updateParameters(const FuzzParameters& newParameters);
    void processBlock(juce::AudioBuffer<float>& buffer);

    int getLatencySamples() const;

private:
    void updateNonlinearSampleRate();
    void ensureScratchCapacity(int numSamples);

    double sampleRate { 44100.0 };
    int maxBlockSize { 0 };
    int numChannels { 0 };
    int activeOversamplingMode { 1 };

    FuzzParameters currentParameters;

    InputVoicingStage inputVoicingStage;
    PreGainStage preGainStage;
    ClippingStageA clippingStageA;
    ClippingStageB clippingStageB;
    ToneStack toneStack;
    HighControlFilter highControlFilter;
    OutputStage outputStage;
    OversamplingEngine oversamplingEngine;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputGainSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> sustainSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> toneSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> highSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> volumeGainSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> voicingMorphSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> clippingModeMorphSmoothed;

    std::vector<float> sustainValues;
    std::vector<float> toneValues;
    std::vector<float> highValues;
    std::vector<float> volumeValues;
    std::vector<float> clippingModeValues;
};
} // namespace pyramidfuzz::dsp
