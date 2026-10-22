#pragma once
#include "Theme.h"
#include "../PluginProcessor.h"

namespace blg::ui
{
/** The plugin's actual output waveform, drawn crisply over a dark, slowly churning/bubbling
    blood backdrop. Pure display: no controls.

    UI-thread only, driven by a juce::Timer. Reads BlgAudioProcessor::outputScope, a lock-free
    ring buffer fed from processBlock; this component never touches audio data directly. */
class OutputWaveScene : public juce::Component, private juce::Timer
{
public:
    explicit OutputWaveScene (BlgAudioProcessor& p) : processor (p)
    {
        setInterceptsMouseClicks (false, false);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();

        drawPlateShadow (g, r);

        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clipPath;
            clipPath.addRoundedRectangle (r, 10.0f);
            g.reduceClipRegion (clipPath);

            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0a0203), r.getX(), r.getY(),
                                                     juce::Colour (0xff050102), r.getX(), r.getBottom(), false));
            g.fillRect (r);

            drawChurn (g, r);
            drawWaveform (g, r);
        }

        drawPlateFrameAndTitle (g, r, "Output");
    }

private:
    void timerCallback() override
    {
        phase += 1.0f;
        repaint();
    }

    // dark, slow-moving churn at the base plus a handful of rising, popping bubbles -- kept
    // behind the waveform and low-contrast so the real signal trace stays the clear focus
    void drawChurn (juce::Graphics& g, const juce::Rectangle<float>& r) const
    {
        struct Layer { float freq, amp, speed, yFrac; juce::Colour colour; float alpha; };
        const Layer layers[] =
        {
            { 0.035f, 3.0f,  0.30f, 0.82f, juce::Colour (0xff170406), 0.55f },
            { 0.048f, 4.0f, -0.45f, 0.92f, juce::Colour (0xff22070a), 0.45f },
        };

        for (auto& L : layers)
        {
            const float y0 = r.getY() + r.getHeight() * L.yFrac;
            juce::Path wave;
            wave.startNewSubPath (r.getX(), r.getBottom() + 2.0f);
            wave.lineTo (r.getX(), y0);

            for (float x = 0.0f; x <= r.getWidth() + 5.0f; x += 5.0f)
            {
                const float y = y0 + std::sin ((x + phase * L.speed) * L.freq) * L.amp;
                wave.lineTo (r.getX() + x, y);
            }

            wave.lineTo (r.getRight(), r.getBottom() + 2.0f);
            wave.closeSubPath();
            g.setColour (L.colour.withAlpha (L.alpha));
            g.fillPath (wave);
        }

        constexpr int N = 7;
        const float seeds[N]  = { 0.05f, 0.42f, 0.63f, 0.18f, 0.77f, 0.30f, 0.90f };
        const float xFracs[N] = { 0.10f, 0.25f, 0.40f, 0.55f, 0.68f, 0.82f, 0.94f };
        const float speeds[N] = { 0.006f, 0.005f, 0.007f, 0.0045f, 0.0065f, 0.0055f, 0.006f };

        for (int i = 0; i < N; ++i)
        {
            const float t     = std::fmod (phase * speeds[i] + seeds[i], 1.0f);
            const float x     = r.getX() + r.getWidth() * xFracs[i];
            const float baseY = r.getBottom() - 4.0f;
            const float topY  = r.getY() + r.getHeight() * 0.60f;
            const float y     = baseY - t * (baseY - topY);
            const float size  = 1.1f + 2.0f * (1.0f - t);
            const float alpha = (1.0f - t) * 0.45f;

            g.setColour (col::accentLo.withAlpha (alpha));
            g.drawEllipse (x - size, y - size, size * 2.0f, size * 2.0f, 0.8f);
        }
    }

    // the real output signal, mixed to mono and drawn as a crisp line on top of everything else
    void drawWaveform (juce::Graphics& g, const juce::Rectangle<float>& r)
    {
        if (! processor.outputScope.readLatest (scopeL, scopeR, numPoints) || scopeL.empty())
            return;

        const float midY  = r.getY() + r.getHeight() * 0.5f;
        const float halfH = r.getHeight() * 0.40f;
        const float stepX = r.getWidth() / (float) juce::jmax (1, (int) scopeL.size() - 1);

        juce::Path trace;
        for (size_t i = 0; i < scopeL.size(); ++i)
        {
            const float sample = (scopeL[i] + scopeR[i]) * 0.5f;
            const float x = r.getX() + (float) i * stepX;
            const float y = midY - juce::jlimit (-1.0f, 1.0f, sample) * halfH;

            if (i == 0) trace.startNewSubPath (x, y);
            else        trace.lineTo (x, y);
        }

        g.setColour (col::accent.withAlpha (0.30f));
        g.strokePath (trace, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col::white);
        g.strokePath (trace, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    BlgAudioProcessor& processor;
    std::vector<float> scopeL, scopeR;
    float phase = 0.0f;
    static constexpr int numPoints = 380;
};
} // namespace blg::ui
