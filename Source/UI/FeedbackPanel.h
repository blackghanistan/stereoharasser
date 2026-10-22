#pragma once
#include "Theme.h"
#include "../PluginProcessor.h"

namespace blg::ui
{
//==============================================================================
/** Static plate for the Feedback panel (bottom-left): shadow, dark fill, border and a
    left-aligned title with a small drip. The controls (Amount knob, Tilt and Shift faders) and
    the SpectrumDisplay are separate components laid over it by MainPanel. */
class FeedbackBackdrop : public juce::Component
{
public:
    FeedbackBackdrop()
    {
        setInterceptsMouseClicks (false, false);
        setBufferedToImage (true);
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

            g.setGradientFill (juce::ColourGradient (col::plateTop, r.getX(), r.getY(),
                                                     col::plateBot, r.getX(), r.getBottom(), false));
            g.fillRect (r);
        }

        drawPlateBorder (g, r);

        g.setFont (makeFont (15.0f, true));
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawText ("Feedback", juce::Rectangle<float> (16.5f, 6.5f, 120.0f, 20.0f), juce::Justification::centredLeft);
        g.setColour (col::white);
        g.drawText ("Feedback", juce::Rectangle<float> (16.0f, 6.0f, 120.0f, 20.0f), juce::Justification::centredLeft);

        juce::Path drip;
        drip.startNewSubPath (16.0f, 26.0f);
        drip.lineTo (40.0f, 26.0f);
        drip.lineTo (30.0f, 31.0f);
        drip.quadraticTo (28.0f, 35.0f, 26.0f, 31.0f);
        drip.closeSubPath();
        g.setColour (col::accent);
        g.fillPath (drip);
    }
};

//==============================================================================
/** Live before/after spectrum graph for the spectral feedback stage, like the two graphs in
    MAIM's Spectral Effects panel: the bone-coloured line is the signal going into the stage,
    the blood-red fill is what comes out of it. Pure display.

    UI-thread only, driven by a juce::Timer. Reads BlgAudioProcessor::spectralInScope /
    spectralOutScope, lock-free ring buffers fed from processBlock either side of the spectral
    feedback stage; the FFT used for drawing runs here on the message thread, never on the
    audio thread. */
class SpectrumDisplay : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumDisplay (BlgAudioProcessor& p) : processor (p)
    {
        setInterceptsMouseClicks (false, false);

        window.resize ((size_t) fftSize);
        for (int n = 0; n < fftSize; ++n)
            window[(size_t) n] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) n / (float) fftSize);

        fftData.assign ((size_t) fftSize * 2, 0.0f);
        smoothIn.fill (0.0f);
        smoothOut.fill (0.0f);

        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();

        g.setColour (col::screen);
        g.fillRoundedRectangle (r, 6.0f);

        const auto plot = r.reduced (4.0f);

        g.setColour (col::edgeLight.withAlpha (0.25f));
        for (int i = 1; i < 4; ++i)
            g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * (float) i * 0.25f), plot.getX(), plot.getRight());

        // after the feedback stage: blood-red fill
        juce::Path after = makePath (plot, smoothOut, true);
        g.setColour (col::accent.withAlpha (0.50f));
        g.fillPath (after);
        g.setColour (col::accentHi.withAlpha (0.90f));
        g.strokePath (makePath (plot, smoothOut, false), juce::PathStrokeType (1.2f));

        // before the feedback stage: thin bone line on top
        g.setColour (col::white.withAlpha (0.60f));
        g.strokePath (makePath (plot, smoothIn, false), juce::PathStrokeType (1.0f));

        g.setColour (col::edgeLight);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    }

private:
    static constexpr int fftOrder = 9;
    static constexpr int fftSize  = 1 << fftOrder;   // 512
    static constexpr int numCols  = 56;

    void timerCallback() override
    {
        analyse (processor.spectralInScope,  smoothIn);
        analyse (processor.spectralOutScope, smoothOut);
        repaint();
    }

    // Reads the latest 512 samples, takes a Hann-windowed FFT of the mono mix, and folds the
    // bins into numCols log-spaced columns, stored as 0..1 heights (-80 dB .. 0 dB) with a quick
    // attack and a slower fall so the graph stays readable.
    void analyse (const blg::dsp::ScopeBuffer& scope, std::array<float, numCols>& smooth)
    {
        if (! scope.readLatest (scopeL, scopeR, fftSize) || (int) scopeL.size() < fftSize)
            return;

        for (int n = 0; n < fftSize; ++n)
            fftData[(size_t) n] = 0.5f * (scopeL[(size_t) n] + scopeR[(size_t) n]) * window[(size_t) n];

        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const float maxBin = (float) (fftSize / 2 - 1);

        for (int c = 0; c < numCols; ++c)
        {
            const int lo = juce::jlimit (1, fftSize / 2 - 1, (int) std::pow (maxBin, (float) c / (float) numCols));
            const int hi = juce::jlimit (lo, fftSize / 2 - 1, (int) std::pow (maxBin, (float) (c + 1) / (float) numCols) - 1);

            float peak = 0.0f;
            for (int b = lo; b <= hi; ++b)
                peak = juce::jmax (peak, fftData[(size_t) b]);

            const float amp    = peak / ((float) fftSize * 0.25f);       // Hann window: peak = A * N / 4
            const float dB     = juce::Decibels::gainToDecibels (amp, -100.0f);
            const float height = juce::jlimit (0.0f, 1.0f, juce::jmap (dB, -80.0f, 0.0f, 0.0f, 1.0f));

            auto& s = smooth[(size_t) c];
            s = height > s ? height : s + (height - s) * 0.25f;
        }
    }

    static juce::Path makePath (const juce::Rectangle<float>& plot, const std::array<float, numCols>& values, bool closed)
    {
        juce::Path p;
        const float stepX = plot.getWidth() / (float) (numCols - 1);

        if (closed)
            p.startNewSubPath (plot.getX(), plot.getBottom());

        for (int c = 0; c < numCols; ++c)
        {
            const float x = plot.getX() + (float) c * stepX;
            const float y = plot.getBottom() - values[(size_t) c] * plot.getHeight();

            if (c == 0 && ! closed) p.startNewSubPath (x, y);
            else                    p.lineTo (x, y);
        }

        if (closed)
        {
            p.lineTo (plot.getRight(), plot.getBottom());
            p.closeSubPath();
        }

        return p;
    }

    BlgAudioProcessor& processor;
    juce::dsp::FFT fft { fftOrder };
    std::vector<float> window, fftData, scopeL, scopeR;
    std::array<float, numCols> smoothIn, smoothOut;
};
} // namespace blg::ui
