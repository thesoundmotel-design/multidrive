#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
const juce::Colour plateBg    (0xff14100f);
const juce::Colour motelRed   (0xffd2142b);
const juce::Colour motelYel   (0xffffe63a);
const juce::Colour motelOra   (0xfff7931e);
const juce::Colour knobBody   (0xff251c1b);
const juce::Colour cream      (0xfff6edd0);
const juce::Colour divider    (0xff3a2a2a);
}

void MotelLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (10.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();

    // scale ticks
    g.setColour (juce::Colour (0xff888780));
    for (float t : { 0.0f, 0.5f, 1.0f })
    {
        const float a = startAngle + t * (endAngle - startAngle);
        juce::Line<float> tick (c.getPointOnCircumference (r + 5.0f, a), c.getPointOnCircumference (r + 11.0f, a));
        g.drawLine (tick, 1.5f);
    }

    g.setColour (knobBody);
    g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
    g.setColour (motelOra);
    g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 4.0f);

    const float a = startAngle + pos * (endAngle - startAngle);
    juce::Path pointer;
    pointer.addRoundedRectangle (-3.0f, -r + 9.0f, 6.0f, r * 0.55f, 2.0f);
    g.setColour (motelYel);
    g.fillPath (pointer, juce::AffineTransform::rotation (a).translated (c.x, c.y));
}

MotelVoxEditor::MotelVoxEditor (MotelVoxAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    logo = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

    setupKnob (comp,    compL,    "COMPRESSION");
    setupKnob (gate,    gateL,    "GATE");
    setupKnob (release, releaseL, "RELEASE");
    setupKnob (gain,    gainL,    "GAIN");

    compA    = std::make_unique<Attach> (proc.apvts, "compression", comp);
    gateA    = std::make_unique<Attach> (proc.apvts, "gate",        gate);
    releaseA = std::make_unique<Attach> (proc.apvts, "release",     release);
    gainA    = std::make_unique<Attach> (proc.apvts, "gain",        gain);

    // readouts with units (set after the attachments, which install their own text functions)
    comp.textFromValueFunction    = [] (double v) { return juce::String (v, 1) + " dB"; };
    gain.textFromValueFunction    = [] (double v) { return juce::String (v, 1) + " dB"; };
    release.textFromValueFunction = [] (double v) { return juce::String ((int) std::round (v)) + " ms"; };
    gate.textFromValueFunction    = [] (double v) { return v <= -78.95 ? juce::String ("Off") : juce::String (v, 1) + " dB"; };
    comp.updateText(); gain.updateText(); release.updateText(); gate.updateText();

    setSize (760, 440);
    startTimerHz (30);
}

MotelVoxEditor::~MotelVoxEditor() { setLookAndFeel (nullptr); }

void MotelVoxEditor::setupKnob (juce::Slider& s, juce::Label& l, const juce::String& name)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    s.setColour (juce::Slider::textBoxTextColourId, cream);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (s);

    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setColour (juce::Label::textColourId, motelYel);
    l.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    addAndMakeVisible (l);
}

void MotelVoxEditor::timerCallback()
{
    const float gr = proc.grDb.load();
    // quick attack, slow fall so the meter is readable
    shownGr = gr < shownGr ? gr : shownGr + (gr - shownGr) * 0.15f;
    repaint (meterArea.expanded (4));
}

void MotelVoxEditor::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (plateBg);
    g.fillRoundedRectangle (area, 14.0f);
    g.setColour (motelRed);
    g.drawRoundedRectangle (area, 14.0f, 3.0f);

    // header
    if (logo.isValid())
        g.drawImage (logo, juce::Rectangle<float> (24.0f, 18.0f, 200.0f, 200.0f * (float) logo.getHeight() / (float) logo.getWidth()),
                     juce::RectanglePlacement::centred);

    g.setColour (motelRed);
    g.setFont (juce::FontOptions (44.0f, juce::Font::bold));
    g.drawText ("MOTEL VOX", 254, 42, 460, 52, juce::Justification::centredLeft);   // shadow
    g.setColour (motelYel);
    g.drawText ("MOTEL VOX", 251, 39, 460, 52, juce::Justification::centredLeft);
    g.setColour (motelOra);
    g.setFont (juce::FontOptions (15.0f));
    g.drawText ("VOCAL COMPRESSOR + GATE", 253, 96, 460, 22, juce::Justification::centredLeft);

    g.setColour (divider);
    g.drawLine (24.0f, 228.0f, (float) getWidth() - 24.0f, 228.0f, 2.0f);
    g.drawLine (24.0f, (float) getHeight() - 52.0f, (float) getWidth() - 24.0f, (float) getHeight() - 52.0f, 2.0f);

    // gain-reduction meter: 12 segments, 18 dB full scale
    g.setColour (motelYel);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("GR", meterArea.getX() - 8, meterArea.getY() - 24, meterArea.getWidth() + 16, 18, juce::Justification::centred);
    const int segs = 12;
    const int lit = juce::jlimit (0, segs, (int) std::round (juce::jlimit (0.0f, 18.0f, -shownGr) / 18.0f * (float) segs));
    const float segH = (float) meterArea.getHeight() / (float) segs;
    for (int i = 0; i < segs; ++i)
    {
        auto r = juce::Rectangle<float> ((float) meterArea.getX(), (float) meterArea.getY() + segH * (float) i,
                                         (float) meterArea.getWidth(), segH - 4.0f);
        g.setColour (i < lit ? (i < 2 ? motelRed : motelOra) : juce::Colour (0xff2f2323));
        g.fillRoundedRectangle (r, 2.0f);
    }
    g.setColour (juce::Colour (0xff9b8a78));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("0",   meterArea.getRight() + 6, meterArea.getY() - 2, 30, 14, juce::Justification::left);
    g.drawText ("-6",  meterArea.getRight() + 6, meterArea.getY() + (int) (segH * 4) - 2, 30, 14, juce::Justification::left);
    g.drawText ("-12", meterArea.getRight() + 6, meterArea.getY() + (int) (segH * 8) - 2, 30, 14, juce::Justification::left);

    g.setColour (juce::Colour (0xff9b8a78));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("The Sound Motel", 28, getHeight() - 44, 300, 28, juce::Justification::centredLeft);
    g.drawText (juce::String ("Latency: ") + juce::String (proc.getLatencySamples()) + " samples",
                getWidth() - 328, getHeight() - 44, 300, 28, juce::Justification::centredRight);
}

void MotelVoxEditor::resized()
{
    const int knobW = 130, knobH = 130, top = 262;
    const int gap = 38;
    const int totalW = 4 * knobW + 3 * gap;
    const int startX = 28 + (getWidth() - 56 - 90 - totalW) / 2;

    juce::Slider* sl[] = { &comp, &gate, &release, &gain };
    juce::Label*  lb[] = { &compL, &gateL, &releaseL, &gainL };
    for (int i = 0; i < 4; ++i)
    {
        const int x = startX + i * (knobW + gap);
        lb[i]->setBounds (x, top - 30, knobW, 20);
        sl[i]->setBounds (x, top - 6, knobW, knobH + 28);
    }
    meterArea = { getWidth() - 110, 268, 34, 150 };
}
