#pragma once
#include <JuceHeader.h>

namespace blg::ui
{
namespace col
{
    inline const juce::Colour metalTop  { 0xff1a0b0d };   // backdrop, dark oxblood-black
    inline const juce::Colour metalBot  { 0xff070304 };
    inline const juce::Colour plateTop  { 0xff24090c };
    inline const juce::Colour plateBot  { 0xff170406 };
    inline const juce::Colour edgeLight { 0xff5a1b22 };
    inline const juce::Colour edgeDark  { 0xff050102 };
    inline const juce::Colour screen    { 0xff0b0404 };
    inline const juce::Colour white     { 0xffe9e2d2 };   // bone
    inline const juce::Colour text      { 0xffcac2bf };
    inline const juce::Colour textDim   { 0xff8a7a78 };
    inline const juce::Colour accent    { 0xffb2102a };   // blood red (replaces the old blue)
    inline const juce::Colour accentHi  { 0xffe23850 };
    inline const juce::Colour accentLo  { 0xff5c0714 };
}

// The whole UI is designed on a fixed 960 x 600 canvas and scaled with a component transform.
namespace layout
{
    inline constexpr int    baseWidth  = 960;
    inline constexpr int    baseHeight = 600;
    inline constexpr double aspect     = 960.0 / 600.0;

    inline const juce::Rectangle<int> vintagePlate { 24, 84, 580, 344 };
    inline const juce::Rectangle<int> reverbPlate  { 620, 84, 316, 344 };
    inline const juce::Rectangle<int> scopeLeft    { 24, 444, 248, 140 };
    inline const juce::Rectangle<int> scopeRight   { 688, 444, 248, 140 };
    inline const juce::Rectangle<int> browserArea  { 288, 444, 384, 140 };
}

inline juce::Font makeFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

// Drop shadow for a plate, drawn before its background fill.
inline void drawPlateShadow (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.expanded (2.0f, 0.0f).withTrimmedTop (-0.5f).withTrimmedBottom (-4.0f), 12.0f);
}

// Border, title and the dripping-blood accent for a plate, drawn after its background fill.
// Shared so the Vintage and Reverb plates look identical even though Vintage's background is
// now a live animated component (VintageScene) rather than part of BackPanel's static paint.
inline void drawPlateBorder (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (col::edgeDark);
    g.drawRoundedRectangle (r, 10.0f, 1.5f);
    g.setColour (col::edgeLight.withAlpha (0.55f));
    g.drawRoundedRectangle (r.reduced (2.0f), 8.0f, 1.0f);
}

inline void drawPlateFrameAndTitle (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
{
    drawPlateBorder (g, r);

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.setFont (makeFont (17.0f, true));
    g.drawText (title, r.withHeight (34.0f).translated (0.5f, 6.5f), juce::Justification::centred);
    g.setColour (col::white);
    g.drawText (title, r.withHeight (34.0f).translated (0.0f, 6.0f), juce::Justification::centred);

    juce::Path drip;
    drip.startNewSubPath (r.getCentreX() - 18.0f, r.getY() + 38.0f);
    drip.lineTo (r.getCentreX() + 18.0f, r.getY() + 38.0f);
    drip.lineTo (r.getCentreX() + 3.0f, r.getY() + 47.0f);
    drip.quadraticTo (r.getCentreX(), r.getY() + 52.0f, r.getCentreX() - 3.0f, r.getY() + 47.0f);
    drip.closeSubPath();
    g.setColour (col::accent);
    g.fillPath (drip);

    juce::Path tendril;
    tendril.startNewSubPath (r.getCentreX() - 1.0f, r.getY() + 50.0f);
    tendril.quadraticTo (r.getCentreX() - 2.5f, r.getY() + 58.0f, r.getCentreX() - 0.5f, r.getY() + 65.0f);
    g.setColour (col::accent.withAlpha (0.55f));
    g.strokePath (tendril, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

class BlgLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BlgLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, col::metalBot);
        setColour (juce::Label::textColourId, col::text);
        setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ScrollBar::thumbColourId, col::accent.withAlpha (0.75f));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
        const float diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto area = juce::Rectangle<float> (diameter, diameter).withCentre (bounds.getCentre());
        const float R = diameter * 0.5f;
        const auto  c = area.getCentre();
        const float angle = startAngle + sliderPos * (endAngle - startAngle);

        // value ring
        const float ringR = R - 2.5f;
        const float ringW = juce::jmax (3.0f, R * 0.07f);
        const juce::PathStrokeType stroke (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        const juce::PathStrokeType glow   (ringW * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path track;
        track.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
        g.setColour (col::edgeDark);
        g.strokePath (track, stroke);

        juce::Path value;
        value.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startAngle, angle, true);
        g.setColour (col::accent.withAlpha (0.25f));
        g.strokePath (value, glow);
        g.setColour (col::accent);
        g.strokePath (value, stroke);

        // bone knob body
        const auto body = area.reduced (ringW + 5.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (body.translated (0.0f, R * 0.05f).expanded (1.5f));

        g.setGradientFill (juce::ColourGradient (col::white,
                                                 body.getX() + body.getWidth() * 0.3f,
                                                 body.getY() + body.getHeight() * 0.25f,
                                                 juce::Colour (0xffb8ac92),
                                                 body.getRight(), body.getBottom(), true));
        g.fillEllipse (body);
        g.setColour (col::edgeDark);
        g.drawEllipse (body, juce::jmax (1.5f, R * 0.03f));

        // faint hairline cracks across the bone
        juce::Path cracks;
        cracks.startNewSubPath (body.getX() + body.getWidth() * 0.22f, body.getY() + body.getHeight() * 0.70f);
        cracks.lineTo (body.getX() + body.getWidth() * 0.40f, body.getY() + body.getHeight() * 0.56f);
        cracks.lineTo (body.getX() + body.getWidth() * 0.34f, body.getY() + body.getHeight() * 0.40f);
        cracks.startNewSubPath (body.getX() + body.getWidth() * 0.68f, body.getY() + body.getHeight() * 0.28f);
        cracks.lineTo (body.getX() + body.getWidth() * 0.78f, body.getY() + body.getHeight() * 0.42f);
        g.setColour (juce::Colour (0xff8a7a62).withAlpha (0.35f));
        g.strokePath (cracks, juce::PathStrokeType (0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // pointer: a bloody spike instead of a plain line, tipped with a drop of blood.
        // The gradient is centred on the knob's actual centre (c), so it stays correctly
        // aligned (bone near the hub, blood out at the tip) however the spike is rotated.
        const float rb = body.getWidth() * 0.5f;
        const float len = rb * 0.90f, baseW = juce::jmax (1.6f, rb * 0.11f), innerR = rb * 0.26f;

        const float midR = len * 0.80f, tipCut = baseW * 0.32f;

        juce::Path spike;
        spike.startNewSubPath (-baseW, -innerR);
        spike.lineTo (baseW, -innerR);
        spike.lineTo (tipCut, -midR);
        spike.lineTo (0.0f, -len);
        spike.lineTo (-tipCut, -midR);
        spike.closeSubPath();

        const auto xf = juce::AffineTransform::rotation (angle).translated (c.x, c.y);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffcabfa6), c.x, c.y, col::accent, c.x, c.y - rb, true));
        g.fillPath (spike, xf);
        g.setColour (col::edgeDark);
        g.strokePath (spike, juce::PathStrokeType (1.0f), xf);

        // facet shading, so the flat spike reads as a faceted 3D shard: a bright edge
        // catching the light on one side, a dark edge in shadow on the other, and a
        // centre ridge suggesting the two faces meet along the spike's length
        juce::Path faceLit;
        faceLit.startNewSubPath (-baseW * 0.85f, -innerR * 0.9f);
        faceLit.lineTo (-tipCut, -midR);
        faceLit.lineTo (0.0f, -len);
        g.setColour (juce::Colours::white.withAlpha (0.30f));
        g.strokePath (faceLit, juce::PathStrokeType (0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), xf);

        juce::Path faceShadow;
        faceShadow.startNewSubPath (baseW * 0.85f, -innerR * 0.9f);
        faceShadow.lineTo (tipCut, -midR);
        faceShadow.lineTo (0.0f, -len);
        g.setColour (juce::Colours::black.withAlpha (0.40f));
        g.strokePath (faceShadow, juce::PathStrokeType (0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), xf);

        juce::Path ridge;
        ridge.startNewSubPath (0.0f, -innerR);
        ridge.lineTo (0.0f, -len);
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.strokePath (ridge, juce::PathStrokeType (0.7f), xf);

        juce::Path drop;
        const float dropR = baseW * 0.62f;
        drop.addEllipse (-dropR, -len - dropR * 0.3f, dropR * 2.0f, dropR * 2.4f);
        g.setColour (col::accentHi);
        g.fillPath (drop, xf);

        juce::Path glint;
        glint.addEllipse (-dropR * 0.55f, -len - dropR * 0.55f, dropR * 0.5f, dropR * 0.5f);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillPath (glint, xf);

        // thin blood tendrils dripping further down off the spike's tip
        juce::Path tendrils;
        tendrils.startNewSubPath (-dropR * 0.30f, -len - dropR * 0.60f);
        tendrils.quadraticTo (-dropR * 0.55f, -len - dropR * 1.8f, -dropR * 0.15f, -len - dropR * 3.0f);
        tendrils.startNewSubPath (dropR * 0.25f, -len - dropR * 0.70f);
        tendrils.quadraticTo (dropR * 0.42f, -len - dropR * 1.6f, dropR * 0.10f, -len - dropR * 2.4f);
        g.setColour (col::accent.withAlpha (0.65f));
        g.strokePath (tendrils, juce::PathStrokeType (juce::jmax (0.8f, dropR * 0.22f),
                                                       juce::PathStrokeType::curved, juce::PathStrokeType::rounded), xf);
    }

    int getSliderThumbRadius (juce::Slider&) override { return 7; }

    // Bipolar fader: dark groove, blood-red fill running out from the centre mark to the
    // thumb, bone-coloured thumb with a red notch.
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        const bool vertical = style == juce::Slider::LinearVertical;
        const float trackW = 6.0f;
        const auto  area   = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        const auto  centre = area.getCentre();

        const auto groove = vertical ? juce::Rectangle<float> (trackW, area.getHeight() - 6.0f).withCentre (centre)
                                     : juce::Rectangle<float> (area.getWidth() - 6.0f, trackW).withCentre (centre);

        g.setColour (col::edgeDark);
        g.fillRoundedRectangle (groove, 3.0f);
        g.setColour (col::edgeLight.withAlpha (0.5f));
        g.drawRoundedRectangle (groove, 3.0f, 0.8f);

        const float zero = (float) slider.getPositionOfValue (0.0);

        juce::Rectangle<float> fill;
        if (vertical)
            fill = juce::Rectangle<float> (groove.getX(), juce::jmin (zero, sliderPos), groove.getWidth(), std::abs (sliderPos - zero));
        else
            fill = juce::Rectangle<float> (juce::jmin (zero, sliderPos), groove.getY(), std::abs (sliderPos - zero), groove.getHeight());

        g.setColour (col::accent);
        g.fillRoundedRectangle (fill, 2.0f);

        g.setColour (col::textDim.withAlpha (0.8f));
        if (vertical) g.drawHorizontalLine ((int) zero, groove.getX() - 3.0f, groove.getRight() + 3.0f);
        else          g.drawVerticalLine   ((int) zero, groove.getY() - 3.0f, groove.getBottom() + 3.0f);

        const auto thumb = vertical ? juce::Rectangle<float> (16.0f, 8.0f).withCentre ({ centre.x, sliderPos })
                                    : juce::Rectangle<float> (8.0f, 16.0f).withCentre ({ sliderPos, centre.y });

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (thumb.translated (0.0f, 1.0f).expanded (0.5f), 2.5f);
        g.setGradientFill (juce::ColourGradient (col::white, thumb.getX(), thumb.getY(),
                                                 juce::Colour (0xffb8ac92), thumb.getRight(), thumb.getBottom(), false));
        g.fillRoundedRectangle (thumb, 2.5f);
        g.setColour (col::edgeDark);
        g.drawRoundedRectangle (thumb, 2.5f, 1.0f);

        g.setColour (col::accent);
        if (vertical) g.drawHorizontalLine ((int) thumb.getCentreY(), thumb.getX() + 2.5f, thumb.getRight() - 2.5f);
        else          g.drawVerticalLine   ((int) thumb.getCentreX(), thumb.getY() + 2.5f, thumb.getBottom() - 2.5f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (down ? col::accentLo : (highlighted ? juce::Colour (0xff3c3f46) : juce::Colour (0xff2f3238)));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (down ? col::accent : col::edgeLight);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont (makeFont (13.0f, true));
        g.setColour (col::white);
        g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
    }
};
} // namespace blg::ui
