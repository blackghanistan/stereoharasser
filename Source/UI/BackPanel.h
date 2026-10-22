#pragma once
#include "Theme.h"
#include "BinaryData.h"

namespace blg::ui
{
/** Static artwork: a dark, grungy horror-finish backdrop, the Reverb plate (with its own
    background photo under a blood-tinted overlay) and the preset screen. Rendered once into
    a cached image. The Vintage plate is drawn separately by the live, animated VintageScene
    component (its background needs to move; see Source/UI/VintageScene.h), which sits on top
    of this layer at the same position. */
class BackPanel : public juce::Component
{
public:
    BackPanel()
    {
        setOpaque (true);
        setBufferedToImage (true);
        setInterceptsMouseClicks (false, false);

        reverbImage = juce::ImageCache::getFromMemory (BinaryData::reverb_bg_jpg, BinaryData::reverb_bg_jpgSize);
    }

    void paint (juce::Graphics& g) override
    {
        const float W = (float) layout::baseWidth, H = (float) layout::baseHeight;

        // dark backdrop with a vignette, roughed up with grime streaks and dripping blood
        g.setGradientFill (juce::ColourGradient (col::metalTop, 0.0f, 0.0f, col::metalBot, 0.0f, H, false));
        g.fillAll();
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, W * 0.5f, H * 0.4f,
                                                 juce::Colours::black.withAlpha (0.65f), 0.0f, 0.0f, true));
        g.fillAll();

        drawGrime (g, W, H);
        drawDrips (g, W, H);

        g.setColour (col::edgeLight.withAlpha (0.4f));
        g.drawRect (getLocalBounds(), 1);

        // the Reverb plate (the Vintage plate is drawn by the separate VintageScene component)
        drawPlate (g, layout::reverbPlate, "Reverb", reverbImage);

        drawScreen (g, layout::browserArea);

        // wordmark (the logo tile that used to sit to its right has been removed, so this
        // now has the top-right corner to itself)
        g.setColour (col::text);
        g.setFont (makeFont (13.0f, true));
        g.drawText ("blackghanistan sound solutions", juce::Rectangle<int> (600, 16, 312, 20), juce::Justification::centredRight);
        g.setColour (col::textDim);
        g.setFont (makeFont (11.0f));
        g.drawText ("stereo width + reverb", juce::Rectangle<int> (600, 36, 312, 16), juce::Justification::centredRight);
    }

private:
    juce::Image reverbImage;

    static void drawGrime (juce::Graphics& g, float W, float H)
    {
        juce::Random rng (11);
        for (int i = 0; i < 180; ++i)
        {
            const float x = rng.nextFloat() * W, y = rng.nextFloat() * H;
            const float len = 20.0f + rng.nextFloat() * 120.0f;
            const float a = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            g.setColour (juce::Colours::black.withAlpha (0.05f + 0.07f * rng.nextFloat()));
            g.drawLine (x, y, x + len * std::cos (a), y + len * std::sin (a), 1.0f);
        }
    }

    // blood streaks on the backdrop: a soft wobbling strand with a pooled drop at the foot,
    // plus a scatter of small flecks, for a more convincing dripped-blood look
    static void drawDrips (juce::Graphics& g, float W, float H)
    {
        juce::Random rng (5);

        for (int i = 0; i < 20; ++i)
        {
            const float x = rng.nextFloat() * W, y = rng.nextFloat() * H;
            const float r = 1.0f + rng.nextFloat() * 3.0f;
            g.setColour (col::accentLo.withAlpha (0.14f + 0.18f * rng.nextFloat()));
            g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
        }

        for (int i = 0; i < 10; ++i)
        {
            const float x0   = rng.nextFloat() * W;
            const float y0   = rng.nextFloat() * H * 0.55f;
            const float len  = 28.0f + rng.nextFloat() * 110.0f;
            const float wob  = 1.5f + rng.nextFloat() * 3.0f;
            const float seed = rng.nextFloat() * 6.28f;

            juce::Path strand;
            strand.startNewSubPath (x0, y0);
            float xEnd = x0;
            for (float dy = 6.0f; dy <= len; dy += 6.0f)
            {
                xEnd = x0 + std::sin (dy * 0.10f + seed) * wob;
                strand.lineTo (xEnd, y0 + dy);
            }

            const float alpha = 0.14f + 0.14f * rng.nextFloat();
            g.setColour (col::accentLo.withAlpha (alpha));
            g.strokePath (strand, juce::PathStrokeType (1.3f + rng.nextFloat() * 1.3f,
                                                         juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            const float headR = 1.3f + rng.nextFloat() * 1.5f;
            g.setColour (col::accentLo.withAlpha (alpha * 1.4f));
            g.fillEllipse (xEnd - headR, y0 + len - headR, headR * 2.0f, headR * 2.0f);
        }
    }

    static void drawPlate (juce::Graphics& g, juce::Rectangle<int> rc, const juce::String& title,
                           const juce::Image& backgroundImage)
    {
        const auto r = rc.toFloat();
        drawPlateShadow (g, r);

        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clipPath;
            clipPath.addRoundedRectangle (r, 10.0f);
            g.reduceClipRegion (clipPath);

            if (backgroundImage.isValid())
                g.drawImage (backgroundImage, r,
                            juce::RectanglePlacement (juce::RectanglePlacement::centred
                                                       | juce::RectanglePlacement::fillDestination));
            else
            {
                g.setGradientFill (juce::ColourGradient (col::plateTop, r.getX(), r.getY(),
                                                         col::plateBot, r.getX(), r.getBottom(), false));
                g.fillRect (r);
            }

            // blood-tinted overlay so the photo reads as part of the theme and the title stays legible
            g.setGradientFill (juce::ColourGradient (col::plateTop.withAlpha (0.55f), r.getX(), r.getY(),
                                                     col::plateBot.withAlpha (0.80f), r.getX(), r.getBottom(), false));
            g.fillRect (r);
            g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, r.getCentreX(), r.getY() + 6.0f,
                                                     juce::Colours::black.withAlpha (0.6f), r.getX(), r.getY(), true));
            g.fillRect (r);
        }

        drawPlateFrameAndTitle (g, r, title);
    }

    static void drawScreen (juce::Graphics& g, juce::Rectangle<int> rc)
    {
        const auto r = rc.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (r.expanded (2.0f, 0.0f).withTrimmedBottom (-4.0f), 12.0f);
        g.setColour (col::screen);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (col::edgeLight);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    }
};
} // namespace blg::ui
