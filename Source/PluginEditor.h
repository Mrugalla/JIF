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

#include "JIF.h"
#include "ControlsEditor.h"

struct JIFAudioProcessorEditor :
    public juce::AudioProcessorEditor,
    public juce::ComponentBoundsConstrainer,
    public juce::Timer,
    public juce::AsyncUpdater
{
    static constexpr int MinWidth = 353;
    static constexpr int MinHeight = 313;

    JIFAudioProcessorEditor (JIFAudioProcessor&);

    bool tryLoad(const juce::String& path);
private:
    JIFAudioProcessor& audioProcessor;
    juce::Rectangle<float> bounds;
    jif::JIF jif;
    std::unique_ptr<ControlsEditor> controlsEditor;
    juce::Font cFont;
    float fps, speedValue;

    void updateFPS();

    void loadWithFileChooser();

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    void mouseUp(const juce::MouseEvent& evt) override;

    void handleAsyncUpdate() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JIFAudioProcessorEditor)
};
