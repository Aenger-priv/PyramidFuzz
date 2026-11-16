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
    // The source includes a large background around the circular knob.
    : knobImage(imageToUse.getClippedImage({ 424, 128, 688, 684 }))
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
                                                                             juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                         static_cast<float>(width), static_cast<float>(height));
    const auto diameter = juce::jmax(0.0f, juce::jmin(bounds.getWidth(), bounds.getHeight()) - 16.0f);
    const auto imageBounds = juce::Rectangle<float>(diameter, diameter).withCentre(bounds.getCentre());

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

        // The shadow above is translucent; restore full opacity for the knob.
        g.setOpacity(1.0f);
        g.saveState();
        juce::Path knobClip;
        knobClip.addEllipse(imageBounds);
        g.reduceClipRegion(knobClip);
        g.drawImageTransformed(knobImage, transform, false);
        g.restoreState();
    }
    else
    {
        g.setColour(juce::Colour::fromRGB(64, 53, 36));
        g.fillEllipse(imageBounds);
    }

    g.setColour(juce::Colour::fromRGB(246, 216, 142).withAlpha(0.95f));
    g.drawEllipse(imageBounds, 1.2f);

    if (slider.hasKeyboardFocus(true))
    {
        g.setColour(juce::Colours::white.withAlpha(.9f));
        g.drawEllipse(imageBounds.expanded(7.0f),1.5f);
    }
    auto arcBounds = imageBounds.expanded(3.0f);
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
      panelArtwork(loadPngFromBinaryData(BinaryData::pyramidfuzzpanel_png, BinaryData::pyramidfuzzpanel_pngSize)),
      tooltips(this,700)
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
    configureLabel(clippingModeLabel, "Diode clipping");
    configureLabel(oversamplingLabel, "Oversampling");

    configureSlider(inputSlider, "Input");
    configureSlider(sustainSlider, "Sustain");
    configureSlider(toneSlider, "Tone");
    configureSlider(highSlider, "High");
    configureSlider(volumeSlider, "Volume");

    inputSlider.setLookAndFeel(&imageKnobLookAndFeel);
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

    inputSlider.setTextValueSuffix(" dB"); volumeSlider.setTextValueSuffix(" dB");
    toneSlider.setTextValueSuffix("%"); highSlider.setTextValueSuffix("%");
    inputSlider.setTooltip("Input trim in dB. Double-click to reset. Shift-drag for fine adjustment.");
    sustainSlider.setTooltip("Drive into the fuzz stages, from 0 to 10. Lower settings clean up; higher settings add sustain.");
    toneSlider.setTooltip("Dark to bright. Type a percentage, or double-click to restore 34%.");
    highSlider.setTooltip("Extends the tone network's upper response. Interacts with Tone; double-click restores 22%.");
    volumeSlider.setTooltip("Output level in dB. Match loudness when comparing diode modes; red Out means 0 dBFS was reached.");
    voicingBox.setTooltip("Hi gives more input drive. Lo is cleaner and quieter. Their levels intentionally differ.");
    clippingModeBox.setTooltip("Off opens only the second diode branch; the fuzz remains active. Silicon is firmer; germanium is softer and quieter.");
    oversamplingBox.setTooltip("4x is the playing default. 8x reduces digital fizz at higher CPU cost; use a larger buffer at 96 kHz. Quality changes briefly fade the signal.");
    voicingBox.setExplicitFocusOrder(1); clippingModeBox.setExplicitFocusOrder(2); oversamplingBox.setExplicitFocusOrder(3);
    inputSlider.setExplicitFocusOrder(4); sustainSlider.setExplicitFocusOrder(5); toneSlider.setExplicitFocusOrder(6);
    highSlider.setExplicitFocusOrder(7); volumeSlider.setExplicitFocusOrder(8);
    for(auto* label : {&modeHelp,&gestureHelp,&keyboardHelp})
    {
        label->setFont(juce::FontOptions(12.5f));
        label->setColour(juce::Label::textColourId,juce::Colour::fromRGB(255,240,206));
        label->setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(*label);
    }
    gestureHelp.setText("Double-click a knob to reset. Shift-drag for fine control.",juce::dontSendNotification);
    keyboardHelp.setText("Tab / arrows to adjust. Click a value to type.",juce::dontSendNotification);
    modeHelp.setTooltip("Clipping Off is not effect bypass. Use your host's plug-in bypass to compare the complete effect.");
    resized(); timerCallback();
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

    auto accentArea = pedalBounds.removeFromBottom(96).reduced(20, 12);
    g.setColour(juce::Colour::fromRGBA(28, 19, 12, 225));
    g.fillRoundedRectangle(accentArea.toFloat(), 18.0f);
    g.setColour(juce::Colour::fromRGB(229, 192, 120).withAlpha(0.8f));
    g.drawRoundedRectangle(accentArea.toFloat(), 18.0f, 1.4f);

    g.setColour(juce::Colour::fromRGB(247, 223, 169));
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawFittedText("Signal", accentArea.removeFromLeft(64), juce::Justification::centred, 1);

    auto meterArea = accentArea.removeFromLeft(200).reduced(4, 0);
    auto inputMeterArea = meterArea.removeFromLeft(92).toFloat();
    auto outputMeterArea = meterArea.removeFromLeft(92).toFloat();

    drawMeter(g, inputMeterArea, inputMeterDisplay, inputOverload, "In");
    drawMeter(g, outputMeterArea, outputMeterDisplay, outputOverload, "Out");


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

    const auto knobWidth = knobRow.getWidth() / 5;
    auto inputArea = knobRow.removeFromLeft(knobWidth).reduced(8);
    auto sustainArea = knobRow.removeFromLeft(knobWidth).reduced(8);
    auto toneArea = knobRow.removeFromLeft(knobWidth).reduced(8);
    auto highArea = knobRow.removeFromLeft(knobWidth).reduced(8);
    auto volumeArea = knobRow.reduced(8);

    inputLabel.setBounds(inputArea.removeFromTop(20));
    inputSlider.setBounds(inputArea.removeFromTop(150));
    sustainLabel.setBounds(sustainArea.removeFromTop(20));
    sustainSlider.setBounds(sustainArea.removeFromTop(150));
    toneLabel.setBounds(toneArea.removeFromTop(20));
    toneSlider.setBounds(toneArea.removeFromTop(150));
    highLabel.setBounds(highArea.removeFromTop(20));
    highSlider.setBounds(highArea.removeFromTop(150));
    volumeLabel.setBounds(volumeArea.removeFromTop(20));
    volumeSlider.setBounds(volumeArea.removeFromTop(150));
    auto help = getLocalBounds().reduced(24).removeFromBottom(96).reduced(20,12);
    help.removeFromLeft(264);
    modeHelp.setBounds(help.removeFromTop(23));
    gestureHelp.setBounds(help.removeFromTop(23));
    keyboardHelp.setBounds(help.removeFromTop(23));

}

void PyramidFuzzAudioProcessorEditor::configureSlider(juce::Slider& slider, const juce::String& name)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 84, 24);
    slider.setName(name);
    slider.setWantsKeyboardFocus(true);
    slider.setMouseDragSensitivity(180);
    slider.setVelocityModeParameters(.25,1,0,true,juce::ModifierKeys::shiftModifier);
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
    comboBox.setWantsKeyboardFocus(true);
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
    label.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colour::fromRGB(255, 250, 240));
    addAndMakeVisible(label);
}

void PyramidFuzzAudioProcessorEditor::timerCallback()
{
    inputMeterDisplay = audioProcessor.getInputMeterLevel();
    outputMeterDisplay = audioProcessor.getOutputMeterLevel();
    inputOverload = audioProcessor.getInputOverload(); outputOverload = audioProcessor.getOutputOverload();
    const juce::String help[] = {"Diodes Off: amplifier and first fuzz stage stay active.",
                               "Silicon: tighter, firmer clipping.",
                               "Germanium: softer, lower-threshold clipping."};
    modeHelp.setText(help[juce::jlimit(0,2,clippingModeBox.getSelectedId()-1)],juce::dontSendNotification);
    repaint();
}

void PyramidFuzzAudioProcessorEditor::drawMeter(juce::Graphics& g,
                                                juce::Rectangle<float> bounds,
                                                float level, bool overload,
                                                const juce::String& label)
{
    auto area = bounds.reduced(4,2);
    auto title = area.removeFromTop(19);
    g.setColour(juce::Colour::fromRGB(255,240,206));
    g.setFont(juce::FontOptions(13.0f,juce::Font::bold));
    g.drawText(label,title,juce::Justification::centred);
    g.setColour(overload?juce::Colour::fromRGB(255,70,45):juce::Colour::fromRGB(65,54,37));
    g.fillEllipse(title.getRight()-7,title.getY()+5,6,6);
    area.removeFromTop(4);
    const auto bar = area.removeFromTop(12);
    g.setColour(juce::Colours::black.withAlpha(.8f));g.fillRoundedRectangle(bar,3);
    const float db = juce::Decibels::gainToDecibels(level,-90.0f);
    const float position = juce::jlimit(0.0f,1.0f,(db+60.0f)/60.0f);
    auto fill = bar.reduced(1);fill.setWidth(fill.getWidth()*position);
    g.setColour(overload?juce::Colour::fromRGB(255,70,45):(db>-6?juce::Colour::fromRGB(239,183,65):juce::Colour::fromRGB(104,204,134)));
    g.fillRoundedRectangle(fill,2);
    g.setColour(juce::Colour::fromRGB(229,192,120));g.drawRoundedRectangle(bar,3,1);
    const float mark=bar.getX()+bar.getWidth()*.7f; // -18 dBFS
    g.drawVerticalLine(juce::roundToInt(mark),bar.getY(),bar.getBottom());
    area.removeFromTop(5);
    g.setColour(juce::Colour::fromRGB(255,240,206));g.setFont(juce::FontOptions(11.5f));
    g.drawText(level<1e-5f?"-inf dBFS":juce::String(db,1)+" dBFS",area,juce::Justification::centredTop);
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
