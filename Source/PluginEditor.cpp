#include "PluginEditor.h"

namespace Palette
{
    const juce::Colour backdrop   { 0xff121014 };
    const juce::Colour paintTop   { 0xff3b2a63 };   // deep violet metal-flake
    const juce::Colour paintBot   { 0xff150f2b };
    const juce::Colour cream      { 0xfff1e6cf };
    const juce::Colour creamDim   { 0xb0f1e6cf };
    const juce::Colour accent     { 0xff6fe3d0 };   // sea-glass teal
    const juce::Colour ledRed     { 0xffff3b2f };
}

static juce::Font makeFont (float height, bool bold = false, bool serif = false)
{
    const auto name = serif ? juce::Font::getDefaultSerifFontName()
                            : juce::Font::getDefaultSansSerifFontName();
    return juce::Font (juce::FontOptions (name, height, bold ? juce::Font::bold : juce::Font::plain));
}

//==============================================================================
PedalLookAndFeel::PedalLookAndFeel()
{
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xee1a1426));
    setColour (juce::BubbleComponent::outlineColourId, Palette::accent.withAlpha (0.6f));
    setColour (juce::TooltipWindow::textColourId, Palette::cream);
}

void PedalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float startAngle, float endAngle,
                                         juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float outerR = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    // Scale dots printed on the enclosure
    const float dotR = outerR - 2.0f;
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
        const float s = (i % 5 == 0) ? 2.4f : 1.5f;
        const juce::Point<float> p (c.x + dotR * std::sin (a), c.y - dotR * std::cos (a));
        g.setColour (Palette::creamDim);
        g.fillEllipse (p.x - s, p.y - s, s * 2.0f, s * 2.0f);
    }

    const float knobR = outerR * 0.74f;

    // Drop shadow
    {
        juce::Path shadowShape;
        shadowShape.addEllipse (c.x - knobR, c.y - knobR, knobR * 2.0f, knobR * 2.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 10, { 2, 5 }).drawForPath (g, shadowShape);
    }

    // Knurled skirt (rotates with the knob)
    {
        juce::Path knurl;
        const int teeth = 36;
        for (int k = 0; k < teeth * 2; ++k)
        {
            const float a = angle + (float) k * juce::MathConstants<float>::pi / (float) teeth;
            const float r = (k % 2 == 0) ? knobR : knobR * 0.95f;
            const juce::Point<float> p (c.x + r * std::sin (a), c.y - r * std::cos (a));
            if (k == 0) knurl.startNewSubPath (p); else knurl.lineTo (p);
        }
        knurl.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3e), c.x, c.y - knobR,
                                                 juce::Colour (0xff0b0b0c), c.x, c.y + knobR, false));
        g.fillPath (knurl);
    }

    // Top cap
    const float capR = knobR * 0.78f;
    {
        juce::ColourGradient cap (juce::Colour (0xff4a4a50), c.x - capR * 0.4f, c.y - capR * 0.6f,
                                  juce::Colour (0xff101012), c.x + capR * 0.5f, c.y + capR, true);
        g.setGradientFill (cap);
        g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);

        // brushed-metal insert
        const float insR = capR * 0.55f;
        juce::ColourGradient ins (juce::Colour (0xffe8e8ea), c.x - insR, c.y - insR,
                                  juce::Colour (0xff7d7d84), c.x + insR, c.y + insR, false);
        g.setGradientFill (ins);
        g.fillEllipse (c.x - insR, c.y - insR, insR * 2.0f, insR * 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawEllipse (c.x - insR, c.y - insR, insR * 2.0f, insR * 2.0f, 1.0f);
    }

    // Pointer line
    {
        const juce::Point<float> p1 (c.x + capR * 0.62f * std::sin (angle), c.y - capR * 0.62f * std::cos (angle));
        const juce::Point<float> p2 (c.x + knobR * 0.97f * std::sin (angle), c.y - knobR * 0.97f * std::cos (angle));
        g.setColour (Palette::cream);
        g.drawLine ({ p1, p2 }, 3.0f);
    }
}

//==============================================================================
void Footswitch::paintButton (juce::Graphics& g, bool isHighlighted, bool isDown)
{
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    const auto c = b.getCentre();
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;

    // Hex nut
    juce::Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const float a = juce::MathConstants<float>::pi / 3.0f * (float) i + juce::MathConstants<float>::pi / 6.0f;
        const juce::Point<float> p (c.x + r * std::cos (a), c.y + r * std::sin (a));
        if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
    }
    hex.closeSubPath();
    juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 8, { 1, 4 }).drawForPath (g, hex);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8d8dc), c.x, c.y - r,
                                             juce::Colour (0xff5d5d64), c.x, c.y + r, false));
    g.fillPath (hex);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.strokePath (hex, juce::PathStrokeType (1.0f));

    // Plunger cap
    const float capR = r * 0.62f;
    const float press = isDown ? 1.5f : 0.0f;
    const auto cc = c.translated (0.0f, press);
    juce::ColourGradient cap (juce::Colours::white, cc.x - capR * 0.4f, cc.y - capR * 0.5f,
                              juce::Colour (0xff6e6e76), cc.x + capR * 0.6f, cc.y + capR, true);
    g.setGradientFill (cap);
    g.fillEllipse (cc.x - capR, cc.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (isHighlighted ? 0.25f : 0.45f));
    g.drawEllipse (cc.x - capR, cc.y - capR, capR * 2.0f, capR * 2.0f, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.drawEllipse (cc.x - capR * 0.7f, cc.y - capR * 0.7f, capR * 1.4f, capR * 1.4f, 1.0f);
}

//==============================================================================
UndertowAudioProcessorEditor::UndertowAudioProcessorEditor (UndertowAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    const char* ids[6]    = { ParamIDs::length, ParamIDs::diffuse, ParamIDs::dampen,
                              ParamIDs::drag,   ParamIDs::reflect, ParamIDs::mix };
    const char* labels[6] = { "LENGTH", "DIFFUSE", "DAMPEN", "DRAG", "REFLECT", "MIX" };

    for (int i = 0; i < 6; ++i)
    {
        auto& k = knobs[(size_t) i];
        k.label = labels[i];
        k.slider.setLookAndFeel (&lookAndFeel);
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                      juce::MathConstants<float>::pi * 2.75f, true);
        k.slider.setPopupDisplayEnabled (true, false, this);
        k.slider.setMouseDragSensitivity (220);
        addAndMakeVisible (k.slider);

        k.attachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, ids[i], k.slider);

        // Double-click a knob to return it to its default
        if (auto* param = audioProcessor.apvts.getParameter (ids[i]))
            k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    }

    addAndMakeVisible (footswitch);
    footswitchAttachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, ParamIDs::engaged, footswitch);

    ledOn = audioProcessor.apvts.getRawParameterValue (ParamIDs::engaged)->load() > 0.5f;

    setSize (380, 600);
    startTimerHz (30);
}

UndertowAudioProcessorEditor::~UndertowAudioProcessorEditor()
{
    stopTimer();
    for (auto& k : knobs)
        k.slider.setLookAndFeel (nullptr);
}

void UndertowAudioProcessorEditor::timerCallback()
{
    const bool on = audioProcessor.apvts.getRawParameterValue (ParamIDs::engaged)->load() > 0.5f;
    if (on != ledOn)
    {
        ledOn = on;
        repaint (ledBounds.expanded (14.0f).toNearestInt());
    }
}

void UndertowAudioProcessorEditor::resized()
{
    const float pedalX = 18.0f, pedalW = (float) getWidth() - 36.0f;
    const int knobSize = 92;
    const float colW = pedalW / 3.0f;

    for (int i = 0; i < 6; ++i)
    {
        const int row = i / 3, col = i % 3;
        const float cx = pedalX + colW * ((float) col + 0.5f);
        const float cy = 118.0f + (float) row * 132.0f;
        knobs[(size_t) i].slider.setBounds (juce::roundToInt (cx - knobSize * 0.5f),
                                            juce::roundToInt (cy - knobSize * 0.5f), knobSize, knobSize);
    }

    ledBounds = { (float) getWidth() * 0.5f - 7.0f, 448.0f, 14.0f, 14.0f };
    footswitch.setBounds (getWidth() / 2 - 36, 482, 72, 72);
}

void UndertowAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::backdrop);

    const auto pedal = getLocalBounds().toFloat().reduced (18.0f, 22.0f);
    drawEnclosure (g, pedal);

    // Knob labels
    g.setFont (makeFont (12.5f, true).withExtraKerningFactor (0.18f));
    for (auto& k : knobs)
    {
        const auto kb = k.slider.getBounds().toFloat();
        g.setColour (Palette::cream);
        g.drawText (k.label, juce::Rectangle<float> (kb.getX() - 10.0f, kb.getBottom() - 2.0f, kb.getWidth() + 20.0f, 18.0f),
                    juce::Justification::centred, false);
    }

    drawArtwork (g, { pedal.getX() + 20.0f, 365.0f, pedal.getWidth() - 40.0f, 72.0f });
    drawLed (g);
}

void UndertowAudioProcessorEditor::drawEnclosure (juce::Graphics& g, juce::Rectangle<float> r)
{
    // Top-mounted jacks
    g.setColour (juce::Colour (0xff2a2a2e));
    for (float jx : { r.getX() + 50.0f, r.getCentreX() - 14.0f, r.getRight() - 78.0f })
        g.fillRoundedRectangle (jx, r.getY() - 12.0f, 28.0f, 16.0f, 3.0f);
    g.setColour (juce::Colour (0xffb9b9c0));
    for (float jx : { r.getX() + 56.0f, r.getCentreX() - 8.0f, r.getRight() - 72.0f })
        g.fillRoundedRectangle (jx, r.getY() - 16.0f, 16.0f, 8.0f, 2.0f);

    juce::Path body;
    body.addRoundedRectangle (r, 16.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 20, { 0, 8 }).drawForPath (g, body);

    g.setGradientFill (juce::ColourGradient (Palette::paintTop, r.getX(), r.getY(),
                                             Palette::paintBot, r.getRight(), r.getBottom(), false));
    g.fillPath (body);

    // Metal-flake sparkle
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (body);
        juce::Random rng (20261121);
        for (int i = 0; i < 900; ++i)
        {
            const float px = r.getX() + rng.nextFloat() * r.getWidth();
            const float py = r.getY() + rng.nextFloat() * r.getHeight();
            g.setColour (juce::Colours::white.withAlpha (0.03f + 0.09f * rng.nextFloat()));
            g.fillRect (px, py, 1.0f, 1.0f);
        }

        // Soft sheen across the top
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), r.getX(), r.getY(),
                                                 juce::Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.35f, false));
        g.fillRect (r);
    }

    // Bevel edge
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (r.reduced (1.0f), 15.0f, 1.5f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (r, 16.0f, 1.0f);

    // Corner screws
    for (auto p : { r.getTopLeft().translated (14, 14), r.getTopRight().translated (-14, 14),
                    r.getBottomLeft().translated (14, -14), r.getBottomRight().translated (-14, -14) })
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd0d0d6), p.x - 4, p.y - 4,
                                                 juce::Colour (0xff55555c), p.x + 4, p.y + 4, false));
        g.fillEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawLine (p.x - 3.0f, p.y, p.x + 3.0f, p.y, 1.2f);
        g.drawLine (p.x, p.y - 3.0f, p.x, p.y + 3.0f, 1.2f);
    }

    // Small print: IN / OUT / 9V
    g.setFont (makeFont (9.5f, true).withExtraKerningFactor (0.15f));
    g.setColour (Palette::creamDim);
    g.drawText ("OUT", juce::Rectangle<float> (r.getX() + 40.0f, r.getY() + 8.0f, 48.0f, 12.0f), juce::Justification::centred);
    g.drawText ("9V", juce::Rectangle<float> (r.getCentreX() - 24.0f, r.getY() + 8.0f, 48.0f, 12.0f), juce::Justification::centred);
    g.drawText ("IN", juce::Rectangle<float> (r.getRight() - 88.0f, r.getY() + 8.0f, 48.0f, 12.0f), juce::Justification::centred);
}

void UndertowAudioProcessorEditor::drawArtwork (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto c = area.getCentre();

    // Whirlpool: concentric, offset rings fading outwards
    for (int i = 0; i < 9; ++i)
    {
        const float rr = 14.0f + (float) i * 15.0f;
        const float off = (float) i * 2.2f;
        g.setColour (Palette::accent.withAlpha (0.42f - (float) i * 0.044f));
        g.drawEllipse (c.x - rr + off, c.y - rr * 0.3f - off * 0.3f, rr * 2.0f, rr * 0.6f, 1.2f);
    }

    // Title with a soft glow
    const auto titleArea = area.withTrimmedBottom (20.0f);
    g.setFont (makeFont (40.0f, true, true).withExtraKerningFactor (0.12f));
    g.setColour (Palette::accent.withAlpha (0.25f));
    for (auto d : { juce::Point<float> (-1, 0), juce::Point<float> (1, 0), juce::Point<float> (0, -1), juce::Point<float> (0, 1) })
        g.drawText ("UNDERTOW", titleArea.translated (d.x * 1.5f, d.y * 1.5f), juce::Justification::centred, false);
    g.setColour (Palette::cream);
    g.drawText ("UNDERTOW", titleArea, juce::Justification::centred, false);

    g.setFont (makeFont (11.0f, false).withExtraKerningFactor (0.35f));
    g.setColour (Palette::creamDim);
    g.drawText ("CAVERN  REVERB", area.withTrimmedTop (area.getHeight() - 20.0f), juce::Justification::centred, false);
}

void UndertowAudioProcessorEditor::drawLed (juce::Graphics& g)
{
    const auto b = ledBounds;
    if (ledOn)
    {
        for (int i = 4; i > 0; --i)
        {
            const float e = (float) i * 3.0f;
            g.setColour (Palette::ledRed.withAlpha (0.08f));
            g.fillEllipse (b.expanded (e));
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffd0c0), b.getCentreX() - 2, b.getCentreY() - 2,
                                                 Palette::ledRed, b.getRight(), b.getBottom(), true));
    }
    else
    {
        g.setColour (juce::Colour (0xff4a1512));
    }
    g.fillEllipse (b);

    // chrome bezel
    g.setColour (juce::Colour (0xffb0b0b8));
    g.drawEllipse (b.expanded (1.5f), 1.5f);
}
