#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

static juce::Font getCustomFont() noexcept {
    return juce::Typeface::createSystemTypefaceFor(BinaryData::nel19_ttf, BinaryData::nel19_ttfSize);
}
static juce::Colour getMainColour() {
    juce::Random rand;
    return juce::Colour(0xffff0000).withRotatedHue(rand.nextFloat());
}

#include "JIFViewer.h"
#include "ControlsEditor.h"

struct JIFAudioProcessorEditor :
    public juce::AudioProcessorEditor,
    public juce::ComponentBoundsConstrainer,
    public JIFViewerListener
{
    static constexpr int MinWidth = 253, DefaultWidth = 400;
    static constexpr int MinHeight = 213, DefaultHeight = 300;

    JIFAudioProcessorEditor (JIFAudioProcessor&);
private:
    JIFAudioProcessor& audioProcessor;
    JIFViewer viewer;
    ControlsEditor controls;
    juce::Rectangle<float> bounds;
    bool viewerInForeground;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& evt) override;
    void viewerUpdated() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JIFAudioProcessorEditor)
};
