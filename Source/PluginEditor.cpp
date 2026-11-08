#include "PluginEditor.h"

#include "PedalTexture.h"

namespace
{
juce::Point<float> polar (juce::Point<float> centre, float radius, float angle)
{
    return { centre.x + radius * std::sin (angle), centre.y - radius * std::cos (angle) };
}

juce::Colour grey (float level)
{
    return juce::Colour::greyLevel (juce::jlimit (0.0f, 1.0f, level));
}

// Warm grey of zinc casting.
juce::Colour pewter (float level)
{
    const float v = juce::jlimit (0.0f, 1.0f, level);
    return juce::Colour::fromFloatRGBA (v, v * 0.95f, v * 0.87f, 1.0f);
}

void addCastingGrain (juce::Graphics& g, juce::Rectangle<float> area, float lit, juce::int64 seed, int count)
{
    juce::Random r (seed);
    for (int i = 0; i < count; ++i)
    {
        const float x = area.getX() + r.nextFloat() * area.getWidth();
        const float y = area.getY() + r.nextFloat() * area.getHeight();
        const float size = 0.3f + r.nextFloat() * 0.65f;
        const auto colour = r.nextBool() ? juce::Colours::black : juce::Colours::white;
        g.setColour (colour.withAlpha ((0.05f + r.nextFloat() * 0.24f) * (0.45f + lit * 0.7f)));
        g.fillEllipse (x - size, y - size, size * 2.0f, size * 2.0f);
    }
}
} // namespace

void PedalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float position, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto area = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float size = juce::jmin (area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const float outer = size * 0.5f;
    const float top = outer * 0.76f;
    const float angle = startAngle + position * (endAngle - startAngle);

    // All shading comes from the same light that renders the faceplate.
    const auto world = slider.getBoundsInParent().getCentre().toFloat();
    const float lit = juce::jlimit (0.12f, 1.0f, pedal::lightAt (world.x, world.y) * 1.25f);
    const float lightDir = pedal::lightAngle (world.x, world.y);

    // Black body
    g.setColour (grey (0.018f));
    g.fillEllipse (c.x - outer, c.y - outer, outer * 2.0f, outer * 2.0f);

    // Wide flutes around the side turn with the knob, lit by a fixed light.
    const int flutes = 24;
    const auto twoPi = juce::MathConstants<float>::twoPi;
    for (int i = 0; i < flutes; ++i)
    {
        const float a = angle + twoPi * (float) i / (float) flutes;
        const float facing = juce::jmax (0.0f, std::cos (a - lightDir));
        const float brightness = 0.035f + lit * (0.05f + 0.34f * facing * facing * facing);

        g.setColour (juce::Colours::black);
        g.drawLine ({ polar (c, top, a + twoPi * 0.5f / (float) flutes),
                      polar (c, outer * 0.99f, a + twoPi * 0.5f / (float) flutes) }, size * 0.028f);

        // Rounded crest: a soft body with a narrow highlight along its centre
        g.setColour (grey (brightness * 0.55f));
        g.drawLine ({ polar (c, top * 1.02f, a), polar (c, outer * 0.96f, a) }, size * 0.04f);
        g.setColour (grey (brightness));
        g.drawLine ({ polar (c, top * 1.03f, a), polar (c, outer * 0.92f, a) }, size * 0.014f);
    }

    g.setColour (juce::Colours::black.withAlpha (0.9f));
    g.drawEllipse (c.x - outer, c.y - outer, outer * 2.0f, outer * 2.0f, 1.2f);

    // Dark line where the flat top meets the flutes
    g.setColour (juce::Colours::black);
    g.fillEllipse (c.x - top - 1.2f, c.y - top - 1.2f, (top + 1.2f) * 2.0f, (top + 1.2f) * 2.0f);

    // Flat glossy top
    const auto hot = polar (c, top * 0.5f, lightDir);
    const auto far = polar (c, top * 1.1f, lightDir + juce::MathConstants<float>::pi);
    g.setGradientFill (juce::ColourGradient (grey (0.05f + lit * 0.16f), hot.x, hot.y,
                                             grey (0.012f), far.x, far.y, true));
    g.fillEllipse (c.x - top, c.y - top, top * 2.0f, top * 2.0f);

    // Fine wear on the plastic turns with the knob
    juce::Random wear ((juce::int64) (size * 100.0f));
    for (int i = 0; i < 22; ++i)
    {
        const float a = angle + wear.nextFloat() * twoPi;
        const auto from = polar (c, wear.nextFloat() * top * 0.94f, a);
        const auto to = polar (from, 2.0f + wear.nextFloat() * top * 0.55f, a + 1.2f + wear.nextFloat());
        g.setColour (juce::Colours::white.withAlpha ((0.05f + wear.nextFloat() * 0.16f) * lit));
        g.drawLine ({ from, to }, 0.45f);
    }

    // Reflection of the light: broad bloom plus a tight core
    const float bloom = top * 0.66f;
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.30f * lit), hot.x, hot.y,
                                             juce::Colours::white.withAlpha (0.0f), hot.x + bloom, hot.y, true));
    g.fillEllipse (hot.x - bloom, hot.y - bloom, bloom * 2.0f, bloom * 2.0f);

    const float core = top * 0.2f;
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.65f * lit), hot.x, hot.y,
                                             juce::Colours::white.withAlpha (0.0f), hot.x + core, hot.y, true));
    g.fillEllipse (hot.x - core, hot.y - core, core * 2.0f, core * 2.0f);

    juce::Path rim;
    rim.addCentredArc (c.x, c.y, top, top, 0.0f, lightDir - 1.25f, lightDir + 1.25f, true);
    g.setColour (juce::Colours::white.withAlpha (0.30f * lit));
    g.strokePath (rim, juce::PathStrokeType (0.9f));

    // Short indicator mark at the edge
    const float thickness = juce::jmax (1.6f, size * 0.03f);
    const auto p1 = polar (c, top * 0.58f, angle), p2 = polar (c, top * 0.98f, angle);
    g.setColour (juce::Colour::fromFloatRGBA (0.92f, 0.89f, 0.78f, 1.0f).withMultipliedBrightness (0.45f + lit * 0.62f));
    g.drawLine ({ p1, p2 }, thickness);
}

void Footswitch::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto frame = getLocalBounds().toFloat().reduced (2.0f);
    const auto world = getBoundsInParent().toFloat();
    const float litTop = juce::jlimit (0.08f, 1.0f, pedal::lightAt (world.getCentreX(), world.getY()) * 1.45f);
    const float litBottom = juce::jlimit (0.06f, 1.0f, pedal::lightAt (world.getCentreX(), world.getBottom()) * 1.45f);
    const float hover = highlighted ? 0.03f : 0.0f;

    // Cast frame
    g.setGradientFill (juce::ColourGradient (pewter (0.20f + litTop * 0.55f), frame.getX(), frame.getY(),
                                             pewter (0.09f + litBottom * 0.30f), frame.getRight(), frame.getBottom(), false));
    g.fillRoundedRectangle (frame, 6.0f);
    addCastingGrain (g, frame, litTop, 8081, 900);

    // Worn bright edge along the frame's lower lip
    g.setColour (juce::Colours::white.withAlpha (0.42f * litBottom + 0.12f));
    g.drawLine (frame.getX() + 6.0f, frame.getBottom() - 1.4f, frame.getRight() - 6.0f, frame.getBottom() - 1.4f, 1.3f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (frame, 6.0f, 1.0f);

    // Recess the pad sits in
    const auto well = frame.reduced (11.0f, 10.0f);
    g.setColour (juce::Colour (0xff070707));
    g.fillRoundedRectangle (well.expanded (1.5f), 3.0f);

    // Pad: pressing it moves it down into the frame and it catches less light
    auto pad = well.reduced (1.0f);
    pad = pad.translated (0.0f, down ? 1.0f : -2.0f);
    const float press = down ? 0.72f : 1.0f;
    g.setGradientFill (juce::ColourGradient (pewter ((0.24f + litTop * 0.46f + hover) * press), pad.getX(), pad.getY(),
                                             pewter ((0.12f + litBottom * 0.34f + hover) * press), pad.getRight(), pad.getBottom(), false));
    g.fillRoundedRectangle (pad, 2.5f);
    addCastingGrain (g, pad, litTop, 4558, 1100);

    // Polish from use, across the middle of the pad
    const auto mid = pad.getCentre();
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.13f * press), mid.x, mid.y,
                                             juce::Colours::white.withAlpha (0.0f), mid.x + pad.getWidth() * 0.42f, mid.y, true));
    g.fillEllipse (pad.reduced (4.0f));

    // Machined bevels
    g.setColour (juce::Colours::white.withAlpha (0.55f * litTop + 0.08f));
    g.drawLine (pad.getX() + 3.0f, pad.getY() + 0.8f, pad.getRight() - 3.0f, pad.getY() + 0.8f, 1.2f);
    g.setColour (juce::Colours::white.withAlpha (0.42f * litBottom + 0.14f));
    g.drawLine (pad.getX() + 2.0f, pad.getBottom() - 1.5f, pad.getRight() - 2.0f, pad.getBottom() - 1.5f, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawLine (pad.getRight() - 0.5f, pad.getY() + 2.0f, pad.getRight() - 0.5f, pad.getBottom() - 2.0f, 1.2f);

    // Dirt and small marks
    juce::Random marks (808);
    for (int i = 0; i < 12; ++i)
    {
        const float x1 = pad.getX() + marks.nextFloat() * pad.getWidth() * 0.85f;
        const float y1 = pad.getY() + marks.nextFloat() * pad.getHeight();
        g.setColour (juce::Colours::white.withAlpha ((0.08f + marks.nextFloat() * 0.2f) * (0.4f + litTop)));
        g.drawLine (x1, y1, x1 + 3.0f + marks.nextFloat() * 14.0f, y1 - marks.nextFloat() * 5.0f, 0.55f);
    }
}

GreenDriveEditor::GreenDriveEditor (GreenDriveProcessor& p) : AudioProcessorEditor (p), processor (p)
{
    setUpKnob (drive, "drive", driveAttachment);
    setUpKnob (tone, "tone", toneAttachment);
    setUpKnob (level, "level", levelAttachment);
    setUpKnob (calibration, "calibration", calibrationAttachment);

    footswitch.setTooltip ("Bypass");
    addAndMakeVisible (footswitch);
    bypassAttachment = std::make_unique<ButtonAttachment> (p.params, "bypass", footswitch);

    setSize (340, 560);
    startTimerHz (20);
    timerCallback();
}

GreenDriveEditor::~GreenDriveEditor()
{
    for (auto* s : { &drive, &tone, &level, &calibration })
        s->setLookAndFeel (nullptr);
}

void GreenDriveEditor::setUpKnob (juce::Slider& s, const char* paramId, std::unique_ptr<SliderAttachment>& attachment)
{
    s.setLookAndFeel (&lookAndFeel);
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setRotaryParameters (pedal::rotaryStart, pedal::rotaryEnd, true);
    s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    s.setPopupDisplayEnabled (true, false, this);
    addAndMakeVisible (s);
    attachment = std::make_unique<SliderAttachment> (processor.params, paramId, s);
}

void GreenDriveEditor::timerCallback()
{
    const bool on = *processor.params.getRawParameterValue ("bypass") < 0.5f;
    if (on != ledOn)
    {
        ledOn = on;
        repaint (ledArea.getSmallestIntegerContainer().expanded (22));
    }
}

void GreenDriveEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (face, getLocalBounds().toFloat());

    const auto c = ledArea.getCentre();
    const float radius = ledArea.getWidth() * 0.5f;

    if (ledOn)
    {
        // Glow spilling onto the paint
        const float glow = 26.0f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x88ff2a18), c.x, c.y,
                                                 juce::Colour (0x00ff2a18), c.x + glow, c.y, true));
        g.fillEllipse (c.x - glow, c.y - glow, glow * 2.0f, glow * 2.0f);
    }

    // Domed lens
    g.setGradientFill (juce::ColourGradient (juce::Colour (ledOn ? 0xffffe0d8 : 0xff7a3430), c.x, c.y,
                                             juce::Colour (ledOn ? 0xffb00e06 : 0xff200807), c.x + radius, c.y, true));
    g.fillEllipse (ledArea);

    // Reflection of the light above
    g.setColour (juce::Colours::white.withAlpha (ledOn ? 0.85f : 0.55f));
    g.fillEllipse (c.x - radius * 0.55f, c.y - radius * 0.65f, radius * 0.5f, radius * 0.38f);
}

void GreenDriveEditor::resized()
{
    drive.setBounds (46, 62, 96, 96);
    level.setBounds (198, 62, 96, 96);
    tone.setBounds (136, 128, 68, 68);
    calibration.setBounds (178, 300, 40, 40);
    ledArea = juce::Rectangle<float> (163.0f, 66.0f, 14.0f, 14.0f);
    footswitch.setBounds (88, 362, 164, 128);

    pedal::Layout layout;
    layout.width = getWidth();
    layout.height = getHeight();
    layout.drive = drive.getBounds();
    layout.tone = tone.getBounds();
    layout.level = level.getBounds();
    layout.calibration = calibration.getBounds();
    layout.footswitch = footswitch.getBounds();
    layout.led = ledArea;
    face = pedal::renderFace (layout, 2.0f);
}
