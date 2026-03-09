#include "PluginEditor.h"

PyramidFuzzAudioProcessorEditor::PyramidFuzzAudioProcessorEditor(PyramidFuzzAudioProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit), audioProcessor(processorToEdit)
{
    setSize(760, 500);

    titleLabel.setText("PYRAMID FUZZ", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(32.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    configureLabel(inputLabel, "Input");
    configureLabel(sustainLabel, "Sustain");
    configureLabel(toneLabel, "Tone");
    configureLabel(highLabel, "High");
    configureLabel(volumeLabel, "Volume");
    configureLabel(voicingLabel, "Voicing");
    configureLabel(clippingModeLabel, "Clipping");
    configureLabel(oversamplingLabel, "Oversampling");

    configureSlider(inputSlider, "Input");
    configureSlider(sustainSlider, "Sustain");
    configureSlider(toneSlider, "Tone");
    configureSlider(highSlider, "High");
    configureSlider(volumeSlider, "Volume");

    configureComboBox(voicingBox, "Voicing");
    voicingBox.addItemList({ "Hi", "Lo" }, 1);
    configureComboBox(clippingModeBox, "Clipping");
    clippingModeBox.addItemList({ "Off", "Silicon", "Germanium" }, 1);
    configureComboBox(oversamplingBox, "Oversampling");
    oversamplingBox.addItemList({ "2x", "4x", "8x" }, 1);

    auto& state = audioProcessor.getValueTreeState();
    inputAttachment = std::make_unique<SliderAttachment>(state, pyramidfuzz::ParameterIds::input, inputSlider);
    sustainAttachment = std::make_unique<SliderAttachment>(state, pyramidfuzz::ParameterIds::sustain, sustainSlider);
    toneAttachment = std::make_unique<SliderAttachment>(state, pyramidfuzz::ParameterIds::tone, toneSlider);
    highAttachment = std::make_unique<SliderAttachment>(state, pyramidfuzz::ParameterIds::high, highSlider);
    volumeAttachment = std::make_unique<SliderAttachment>(state, pyramidfuzz::ParameterIds::volume, volumeSlider);

    voicingAttachment = std::make_unique<ComboAttachment>(state, pyramidfuzz::ParameterIds::voicing, voicingBox);
    clippingModeAttachment = std::make_unique<ComboAttachment>(state, pyramidfuzz::ParameterIds::clippingMode, clippingModeBox);
    oversamplingAttachment = std::make_unique<ComboAttachment>(state, pyramidfuzz::ParameterIds::oversampling, oversamplingBox);

    startTimerHz(30);
}

void PyramidFuzzAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient gradient(juce::Colour::fromRGB(253, 243, 214), bounds.getTopLeft(),
                                  juce::Colour::fromRGB(204, 160, 65), bounds.getBottomRight(), false);
    g.setGradientFill(gradient);
    g.fillAll();

    auto pedalBounds = getLocalBounds().reduced(24);
    g.setColour(juce::Colour::fromRGB(32, 27, 19));
    g.fillRoundedRectangle(pedalBounds.toFloat(), 28.0f);

    g.setColour(juce::Colour::fromRGB(255, 232, 179));
    g.drawRoundedRectangle(pedalBounds.toFloat(), 28.0f, 3.0f);

    auto accentArea = pedalBounds.removeFromBottom(72).reduced(20, 16);
    g.setColour(juce::Colour::fromRGB(191, 127, 45));
    g.fillRoundedRectangle(accentArea.toFloat(), 18.0f);

    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawFittedText("Signal", accentArea.removeFromLeft(84), juce::Justification::centred, 1);

    auto meterArea = accentArea.removeFromLeft(180).reduced(4, 0);
    auto inputMeterArea = meterArea.removeFromLeft(74).toFloat();
    auto outputMeterArea = meterArea.removeFromLeft(74).toFloat();

    drawMeter(g, inputMeterArea, inputMeterDisplay, "In");
    drawMeter(g, outputMeterArea, outputMeterDisplay, "Out");

    g.drawFittedText("Pyramid Fuzz voice tuned for thicker low mids and smoother top end",
                     accentArea,
                     juce::Justification::centredLeft,
                     2);
}

void PyramidFuzzAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(42, 28);
    titleLabel.setBounds(bounds.removeFromTop(54));

    bounds.removeFromTop(12);
    auto comboRow = bounds.removeFromTop(78);
    bounds.removeFromTop(8);
    auto knobRow = bounds.removeFromTop(260);

    const auto comboWidth = comboRow.getWidth() / 3;
    auto voicingArea = comboRow.removeFromLeft(comboWidth).reduced(10);
    auto clippingArea = comboRow.removeFromLeft(comboWidth).reduced(10);
    auto oversamplingArea = comboRow.reduced(10);

    voicingLabel.setBounds(voicingArea.removeFromTop(20));
    voicingBox.setBounds(voicingArea);
    clippingModeLabel.setBounds(clippingArea.removeFromTop(20));
    clippingModeBox.setBounds(clippingArea);
    oversamplingLabel.setBounds(oversamplingArea.removeFromTop(20));
    oversamplingBox.setBounds(oversamplingArea);

    const auto knobWidth = knobRow.getWidth() / 5;
    auto inputArea = knobRow.removeFromLeft(knobWidth).reduced(10);
    auto sustainArea = knobRow.removeFromLeft(knobWidth).reduced(10);
    auto toneArea = knobRow.removeFromLeft(knobWidth).reduced(10);
    auto highArea = knobRow.removeFromLeft(knobWidth).reduced(10);
    auto volumeArea = knobRow.reduced(10);

    inputLabel.setBounds(inputArea.removeFromTop(20));
    inputSlider.setBounds(inputArea);
    sustainLabel.setBounds(sustainArea.removeFromTop(20));
    sustainSlider.setBounds(sustainArea);
    toneLabel.setBounds(toneArea.removeFromTop(20));
    toneSlider.setBounds(toneArea);
    highLabel.setBounds(highArea.removeFromTop(20));
    highSlider.setBounds(highArea);
    volumeLabel.setBounds(volumeArea.removeFromTop(20));
    volumeSlider.setBounds(volumeArea);
}

void PyramidFuzzAudioProcessorEditor::configureSlider(juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 24);
    slider.setName(name);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour::fromRGB(196, 132, 41));
    slider.setColour(juce::Slider::thumbColourId, juce::Colours::black);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha(0.35f));
    slider.setTextValueSuffix({});
    addAndMakeVisible(slider);
}

void PyramidFuzzAudioProcessorEditor::configureComboBox(juce::ComboBox& comboBox, const juce::String& name)
{
    comboBox.setName(name);
    comboBox.setTextWhenNothingSelected(name);
    comboBox.setJustificationType(juce::Justification::centred);
    comboBox.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black.withAlpha(0.35f));
    comboBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    comboBox.setColour(juce::ComboBox::outlineColourId, juce::Colour::fromRGB(255, 232, 179));
    addAndMakeVisible(comboBox);
}

void PyramidFuzzAudioProcessorEditor::configureLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(label);
}

void PyramidFuzzAudioProcessorEditor::timerCallback()
{
    inputMeterDisplay = 0.75f * inputMeterDisplay + 0.25f * audioProcessor.getInputMeterLevel();
    outputMeterDisplay = 0.75f * outputMeterDisplay + 0.25f * audioProcessor.getOutputMeterLevel();
    repaint();
}

void PyramidFuzzAudioProcessorEditor::drawMeter(juce::Graphics& g,
                                                juce::Rectangle<float> bounds,
                                                float level,
                                                const juce::String& label)
{
    auto meterBounds = bounds.reduced(8.0f, 2.0f);
    auto labelArea = meterBounds.removeFromBottom(18.0f);

    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillRoundedRectangle(meterBounds, 8.0f);

    const auto clamped = juce::jlimit(0.0f, 1.0f, level);
    auto fillBounds = meterBounds.reduced(4.0f);
    const auto fillHeight = fillBounds.getHeight() * clamped;
    fillBounds.removeFromTop(fillBounds.getHeight() - fillHeight);

    juce::ColourGradient meterGradient(juce::Colour::fromRGB(84, 180, 97), fillBounds.getBottomLeft(),
                                       juce::Colour::fromRGB(230, 156, 47), fillBounds.getCentre(),
                                       false);
    meterGradient.addColour(1.0, juce::Colour::fromRGB(212, 66, 36));
    g.setGradientFill(meterGradient);
    g.fillRoundedRectangle(fillBounds, 6.0f);

    g.setColour(juce::Colour::fromRGB(255, 232, 179));
    g.drawRoundedRectangle(meterBounds, 8.0f, 1.5f);

    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawFittedText(label, labelArea.toNearestInt(), juce::Justification::centred, 1);
}
