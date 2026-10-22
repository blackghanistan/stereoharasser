#pragma once
#include "Theme.h"

namespace blg::ui
{
/** Animated background for the Vintage plate: a near-black, shimmering, occasionally
    splashing blood sea. Deliberately kept dark and low-contrast so it stays out of the way
    of the knobs and text that sit on top of it. Pure UI-thread animation (juce::Timer); no
    audio data is involved, so it stays real-time safe. Draws its own plate frame and title
    (shared with BackPanel's Reverb plate via Theme.h) so it looks identical to a static plate. */
class VintageScene : public juce::Component, private juce::Timer
{
public:
    VintageScene() { startTimerHz (30); }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();

        drawPlateShadow (g, r);

        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clipPath;
            clipPath.addRoundedRectangle (r, 10.0f);
            g.reduceClipRegion (clipPath);

            // near-black base the sea sits on
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0a0203), r.getX(), r.getY(),
                                                     juce::Colour (0xff050102), r.getX(), r.getBottom(), false));
            g.fillRect (r);

            drawSea (g, r);
            drawSplashes (g, r);
        }

        drawPlateFrameAndTitle (g, r, "Vintage");
    }

private:
    void timerCallback() override
    {
        phase += 1.0f;
        repaint();
    }

    // layered swells, kept almost black with only a faint blood-red undertone and a few
    // low-alpha shimmer glints, so the surface reads as dark texture rather than a bright shape
    void drawSea (juce::Graphics& g, const juce::Rectangle<float>& r) const
    {
        struct Layer { float freq, amp, speed, yFrac; juce::Colour colour; float alpha; };
        const Layer layers[] =
        {
            { 0.028f, 4.0f,  0.35f, 0.30f, juce::Colour (0xff170406), 0.55f },
            { 0.038f, 5.0f, -0.55f, 0.55f, juce::Colour (0xff22070a), 0.45f },
            { 0.050f, 3.0f,  0.80f, 0.75f, col::accentLo,             0.18f },
        };

        for (auto& L : layers)
        {
            const float y0 = r.getY() + r.getHeight() * L.yFrac;
            juce::Path wave;
            wave.startNewSubPath (r.getX(), r.getBottom() + 2.0f);
            wave.lineTo (r.getX(), y0);

            for (float x = 0.0f; x <= r.getWidth() + 5.0f; x += 5.0f)
            {
                const float y = y0 + std::sin ((x + phase * L.speed) * L.freq) * L.amp
                                    + std::sin ((x + phase * L.speed * 1.5f) * L.freq * 2.3f) * L.amp * 0.3f;
                wave.lineTo (r.getX() + x, y);
            }

            wave.lineTo (r.getRight(), r.getBottom() + 2.0f);
            wave.closeSubPath();
            g.setColour (L.colour.withAlpha (L.alpha));
            g.fillPath (wave);
        }

        // faint shimmer, subtle enough to stay almost invisible until it catches the eye
        for (int i = 0; i < 10; ++i)
        {
            const float seed = (float) i * 0.53f;
            const float tx   = std::fmod (phase * 0.005f + seed, 1.0f);
            const float x    = r.getX() + tx * r.getWidth();
            const float y    = r.getY() + r.getHeight() * 0.55f
                                + std::sin ((tx * r.getWidth() + phase * 0.7f) * 0.07f) * 5.0f;
            const float tw   = 0.5f + 0.5f * std::sin (phase * 0.09f + seed * 5.0f);
            g.setColour (col::accentLo.withAlpha (0.22f * tw));
            g.fillEllipse (x - 1.0f, y - 1.0f, 2.0f, 2.0f);
        }
    }

    // brief, sparse splashes that pop up from the surface and fall back — small and dim
    // enough not to distract from the controls sitting above this layer
    void drawSplashes (juce::Graphics& g, const juce::Rectangle<float>& r) const
    {
        constexpr int N = 6;
        const float seeds[N]  = { 0.10f, 0.47f, 0.68f, 0.23f, 0.81f, 0.35f };
        const float xFracs[N] = { 0.12f, 0.30f, 0.46f, 0.62f, 0.78f, 0.92f };
        const float speeds[N] = { 0.011f, 0.009f, 0.013f, 0.010f, 0.012f, 0.0085f };

        for (int i = 0; i < N; ++i)
        {
            const float t = std::fmod (phase * speeds[i] + seeds[i], 1.0f);
            if (t > 0.5f)
                continue;   // only the "rise and fall" half of the loop is drawn; the rest is a pause

            const float arcShape = std::sin (t * juce::MathConstants<float>::twoPi);
            const float x        = r.getX() + r.getWidth() * xFracs[i];
            const float baseY    = r.getY() + r.getHeight() * 0.62f;
            const float y        = baseY - arcShape * 15.0f;
            const float alpha    = juce::jmax (0.0f, (1.0f - t * 2.0f) * 0.45f);

            g.setColour (col::accentLo.withAlpha (alpha));
            g.fillEllipse (x - 1.3f, y - 1.3f, 2.6f, 2.6f);
            g.setColour (col::accent.withAlpha (alpha * 0.6f));
            g.fillEllipse (x - 0.6f, y + 2.5f, 1.2f, 1.2f);
        }
    }

    float phase = 0.0f;
};
} // namespace blg::ui
