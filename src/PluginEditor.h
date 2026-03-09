#pragma once

#include "PluginProcessor.h"

class PyramidFuzzAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              private juce::Timer
{
public:
    explicit PyramidFuzzAudioProcessorEditor(PyramidFuzzAudioProcessor&);
    ~PyramidFuzzAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void drawMeter(juce::Graphics& g, juce::Rectangle<float> bounds, float level, const juce::String& label);
    void configureSlider(juce::Slider& slider, const juce::String& name);
    void configureComboBox(juce::ComboBox& comboBox, const juce::String& name);
    void configureLabel(juce::Label& label, const juce::String& text);

    PyramidFuzzAudioProcessor& audioProcessor;

    juce::Label titleLabel;
    juce::Label inputLabel;
    juce::Label sustainLabel;
    juce::Label toneLabel;
    juce::Label highLabel;
    juce::Label volumeLabel;
    juce::Label voicingLabel;
    juce::Label clippingModeLabel;
    juce::Label oversamplingLabel;

    juce::Slider inputSlider;
    juce::Slider sustainSlider;
    juce::Slider toneSlider;
    juce::Slider highSlider;
    juce::Slider volumeSlider;

    juce::ComboBox voicingBox;
    juce::ComboBox clippingModeBox;
    juce::ComboBox oversamplingBox;

    std::unique_ptr<SliderAttachment> inputAttachment;
    std::unique_ptr<SliderAttachment> sustainAttachment;
    std::unique_ptr<SliderAttachment> toneAttachment;
    std::unique_ptr<SliderAttachment> highAttachment;
    std::unique_ptr<SliderAttachment> volumeAttachment;

    std::unique_ptr<ComboAttachment> voicingAttachment;
    std::unique_ptr<ComboAttachment> clippingModeAttachment;
    std::unique_ptr<ComboAttachment> oversamplingAttachment;

    float inputMeterDisplay { 0.0f };
    float outputMeterDisplay { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PyramidFuzzAudioProcessorEditor)
};
