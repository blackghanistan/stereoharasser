#include "PluginEditor.h"

using namespace blg::ui;

namespace
{
constexpr int minWidth = 640;
constexpr int maxWidth = 1920;

// The interface was scaled up 20% from the original 960 px design width; this is the
// size the plugin now opens at by default (a saved window size still overrides it).
constexpr int defaultWidth = (int) (blg::ui::layout::baseWidth * 1.2);
}

BlgAudioProcessorEditor::BlgAudioProcessorEditor (BlgAudioProcessor& p)
    : AudioProcessorEditor (&p), blgProcessor (p), panel (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (panel);

    setResizable (true, true);
    setResizeLimits (minWidth, (int) std::round (minWidth / layout::aspect),
                     maxWidth, (int) std::round (maxWidth / layout::aspect));
    getConstrainer()->setFixedAspectRatio (layout::aspect);

    const int savedWidth = juce::jlimit (minWidth, maxWidth,
                                         (int) blgProcessor.apvts.state.getProperty ("uiWidth", defaultWidth));
    setSize (savedWidth, (int) std::round (savedWidth / layout::aspect));
}

BlgAudioProcessorEditor::~BlgAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void BlgAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::metalBot);
}

void BlgAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) layout::baseWidth;

    panel.setBounds (0, 0, layout::baseWidth, layout::baseHeight);
    panel.setTransform (juce::AffineTransform::scale (scale));

    blgProcessor.apvts.state.setProperty ("uiWidth", getWidth(), nullptr);
}
