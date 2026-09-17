#include "PluginEditor.h"

#include <JuceHeader.h>

namespace
{
juce::Image loadPngFromBinaryData(const void* data, int dataSize)
{
    juce::MemoryInputStream stream(data, static_cast<size_t>(dataSize), false);
    return juce::PNGImageFormat::loadFrom(stream);
}
}

PyramidFuzzAudioProcessorEditor::ImageKnobLookAndFeel::ImageKnobLookAndFeel(juce::Image imageToUse)
    : knobImage(std::move(imageToUse))
{
}

void PyramidFuzzAudioProcessorEditor::ImageKnobLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                                                             int x,
                                                                             int y,
                                                                             int width,
                                                                             int height,
                                                                             float sliderPosProportional,
                                                                             float rotaryStartAngle,
                                                                             float rotaryEndAngle,
                                                                             juce::Slider&)
{
    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                         static_cast<float>(width), static_cast<float>(height));
    auto imageBounds = bounds.reduced(8.0f, 8.0f);

    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillEllipse(imageBounds.translated(0.0f, 7.0f));

    if (knobImage.isValid())
    {
        const auto angle = juce::jmap(sliderPosProportional, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);
        const auto knobWidth = static_cast<float>(knobImage.getWidth());
        const auto knobHeight = static_cast<float>(knobImage.getHeight());
        const auto scale = juce::jmin(imageBounds.getWidth() / knobWidth,
                                      imageBounds.getHeight() / knobHeight);

        auto transform = juce::AffineTransform::scale(scale)
                             .translated(-0.5f * knobWidth * scale,
                                         -0.5f * knobHeight * scale)
                             .rotated(angle)
                             .translated(imageBounds.getCentreX(), imageBounds.getCentreY());

        g.drawImageTransformed(knobImage, transform, false);
    }
    else
    {
        g.setColour(juce::Colour::fromRGB(64, 53, 36));
        g.fillEllipse(imageBounds);
    }

    g.setColour(juce::Colour::fromRGB(246, 216, 142).withAlpha(0.95f));
    g.drawEllipse(imageBounds, 1.2f);

    auto arcBounds = imageBounds.expanded(6.0f);
    const auto currentAngle = juce::jmap(sliderPosProportional, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);

    juce::Path backgroundArc;
    backgroundArc.addCentredArc(arcBounds.getCentreX(), arcBounds.getCentreY(),
                                arcBounds.getWidth() * 0.5f, arcBounds.getHeight() * 0.5f,
                                0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.strokePath(backgroundArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc(arcBounds.getCentreX(), arcBounds.getCentreY(),
                           arcBounds.getWidth() * 0.5f, arcBounds.getHeight() * 0.5f,
                           0.0f, rotaryStartAngle, currentAngle, true);

    juce::ColourGradient glow(juce::Colour::fromRGB(255, 236, 172), arcBounds.getBottomLeft(),
                              juce::Colour::fromRGB(200, 114, 30), arcBounds.getTopRight(),
                              false);
    g.setGradientFill(glow);
    g.strokePath(valueArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

PyramidFuzzAudioProcessorEditor::PyramidFuzzAudioProcessorEditor(PyramidFuzzAudioProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit),
      audioProcessor(processorToEdit),
      imageKnobLookAndFeel(loadPngFromBinaryData(BinaryData::knob_png, BinaryData::knob_pngSize)),
      panelArtwork(loadPngFromBinaryData(BinaryData::pyramidfuzzpanel_png, BinaryData::pyramidfuzzpanel_pngSize))
{
    setSize(760, 500);

    titleLabel.setText("PYRAMID FUZZ", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(32.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(255, 237, 194));
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

    sustainSlider.setLookAndFeel(&imageKnobLookAndFeel);
    toneSlider.setLookAndFeel(&imageKnobLookAndFeel);
    highSlider.setLookAndFeel(&imageKnobLookAndFeel);
    volumeSlider.setLookAndFeel(&imageKnobLookAndFeel);

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

    juce::ColourGradient gradient(juce::Colour::fromRGB(20, 15, 11), bounds.getTopLeft(),
                                  juce::Colour::fromRGB(88, 60, 24), bounds.getBottomRight(), false);
    g.setGradientFill(gradient);
    g.fillAll();

    auto pedalBounds = getLocalBounds().reduced(24);
    drawBackgroundPanel(g, pedalBounds);

    auto accentArea = pedalBounds.removeFromBottom(72).reduced(20, 16);
    g.setColour(juce::Colour::fromRGBA(28, 19, 12, 225));
    g.fillRoundedRectangle(accentArea.toFloat(), 18.0f);
    g.setColour(juce::Colour::fromRGB(229, 192, 120).withAlpha(0.8f));
    g.drawRoundedRectangle(accentArea.toFloat(), 18.0f, 1.4f);

    g.setColour(juce::Colour::fromRGB(247, 223, 169));
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawFittedText("Signal", accentArea.removeFromLeft(84), juce::Justification::centred, 1);

    auto meterArea = accentArea.removeFromLeft(180).reduced(4, 0);
    auto inputMeterArea = meterArea.removeFromLeft(74).toFloat();
    auto outputMeterArea = meterArea.removeFromLeft(74).toFloat();

    drawMeter(g, inputMeterArea, inputMeterDisplay, "In");
    drawMeter(g, outputMeterArea, outputMeterDisplay, "Out");

    g.setColour(juce::Colour::fromRGB(244, 223, 178));
    g.drawFittedText("Esoteric Ram's Head-derived doom fuzz with switchable clipping",
                     accentArea,
                     juce::Justification::centredLeft,
                     2);
}

void PyramidFuzzAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(42, 28);
    titleLabel.setBounds(bounds.removeFromTop(60));

    bounds.removeFromTop(10);
    auto comboRow = bounds.removeFromTop(74);
    bounds.removeFromTop(12);
    auto knobRow = bounds.removeFromTop(286);

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

    auto inputArea = knobRow.removeFromLeft(78).reduced(8, 18);
    auto mainKnobArea = knobRow.reduced(8, 0);
    const auto knobWidth = mainKnobArea.getWidth() / 4;

    auto sustainArea = mainKnobArea.removeFromLeft(knobWidth).reduced(8);
    auto toneArea = mainKnobArea.removeFromLeft(knobWidth).reduced(8);
    auto highArea = mainKnobArea.removeFromLeft(knobWidth).reduced(8);
    auto volumeArea = mainKnobArea.reduced(8);

    inputLabel.setBounds(inputArea.removeFromTop(18));
    inputSlider.setBounds(inputArea.removeFromTop(188));

    sustainLabel.setBounds(sustainArea.removeFromTop(20));
    sustainSlider.setBounds(sustainArea.removeFromTop(212));
    toneLabel.setBounds(toneArea.removeFromTop(20));
    toneSlider.setBounds(toneArea.removeFromTop(212));
    highLabel.setBounds(highArea.removeFromTop(20));
    highSlider.setBounds(highArea.removeFromTop(212));
    volumeLabel.setBounds(volumeArea.removeFromTop(20));
    volumeSlider.setBounds(volumeArea.removeFromTop(212));
}

void PyramidFuzzAudioProcessorEditor::configureSlider(juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 84, 24);
    slider.setName(name);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour::fromRGB(196, 132, 41));
    slider.setColour(juce::Slider::thumbColourId, juce::Colours::black);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour::fromRGB(252, 237, 201));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha(0.55f));
    slider.setTextValueSuffix({});
    addAndMakeVisible(slider);
}

void PyramidFuzzAudioProcessorEditor::configureComboBox(juce::ComboBox& comboBox, const juce::String& name)
{
    comboBox.setName(name);
    comboBox.setTextWhenNothingSelected(name);
    comboBox.setJustificationType(juce::Justification::centred);
    comboBox.setColour(juce::ComboBox::backgroundColourId, juce::Colours::black.withAlpha(0.48f));
    comboBox.setColour(juce::ComboBox::textColourId, juce::Colour::fromRGB(255, 240, 206));
    comboBox.setColour(juce::ComboBox::outlineColourId, juce::Colour::fromRGB(255, 221, 151));
    addAndMakeVisible(comboBox);
}

void PyramidFuzzAudioProcessorEditor::configureLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour::fromRGB(255, 230, 176));
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

    g.setColour(juce::Colour::fromRGB(255, 225, 158));
    g.drawRoundedRectangle(meterBounds, 8.0f, 1.5f);

    g.setColour(juce::Colour::fromRGB(250, 225, 167));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawFittedText(label, labelArea.toNearestInt(), juce::Justification::centred, 1);
}

void PyramidFuzzAudioProcessorEditor::drawBackgroundPanel(juce::Graphics& g, juce::Rectangle<int> pedalBounds)
{
    auto panel = pedalBounds.toFloat();

    g.setColour(juce::Colour::fromRGB(18, 13, 10));
    g.fillRoundedRectangle(panel, 28.0f);

    juce::Path clip;
    clip.addRoundedRectangle(panel, 28.0f);
    g.saveState();
    g.reduceClipRegion(clip);

    if (panelArtwork.isValid())
    {
        g.drawImageWithin(panelArtwork,
                          pedalBounds.getX(),
                          pedalBounds.getY(),
                          pedalBounds.getWidth(),
                          pedalBounds.getHeight(),
                          juce::RectanglePlacement::fillDestination);
    }

    juce::ColourGradient shadow(juce::Colours::transparentBlack, panel.getCentreX(), panel.getY() + 60.0f,
                                juce::Colours::black.withAlpha(0.58f), panel.getCentreX(), panel.getBottom(),
                                false);
    shadow.addColour(0.35, juce::Colours::black.withAlpha(0.14f));
    g.setGradientFill(shadow);
    g.fillRoundedRectangle(panel, 28.0f);
    g.restoreState();

    g.setColour(juce::Colour::fromRGB(252, 230, 172).withAlpha(0.9f));
    g.drawRoundedRectangle(panel.reduced(1.5f), 28.0f, 2.5f);

    g.setColour(juce::Colours::black.withAlpha(0.24f));
    g.drawRoundedRectangle(panel.reduced(10.0f), 22.0f, 1.0f);
}
