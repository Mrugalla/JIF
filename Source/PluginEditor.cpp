#include "PluginProcessor.h"
#include "PluginEditor.h"

JIFAudioProcessorEditor::JIFAudioProcessorEditor (JIFAudioProcessor& p)
    : AudioProcessorEditor (&p),
    audioProcessor (p),
    viewer(p),
    controls(p, viewer),
    viewerInForeground(true)
{
    addAndMakeVisible(controls);
    addAndMakeVisible(viewer);

    setMinimumWidth(MinWidth);
    setMinimumHeight(MinHeight);

    auto cFont = getCustomFont();
    cFont.setExtraKerningFactor(.05f);
    cFont.setHeight(32);
    viewer.setFont(cFont);

    auto& state = p.apvts.state;
    auto path = state.getProperty("gif", "").toString();
    viewer.tryLoad(path);
    viewer.addListener(this);

    setOpaque(true);
    setResizable(true, true);
    const auto width = static_cast<float>(state.getProperty("width", DefaultWidth));
    const auto height = static_cast<float>(state.getProperty("height", DefaultHeight));
    setSize(width, height);
}

void JIFAudioProcessorEditor::paint (juce::Graphics& g) {
    g.fillAll(juce::Colours::black);
}
void JIFAudioProcessorEditor::resized() {
    viewer.setBounds(getLocalBounds());
    controls.setBounds(getLocalBounds());
    auto& state = audioProcessor.apvts.state;
    state.setProperty("width", getWidth(), nullptr);
    state.setProperty("height", getHeight(), nullptr);
    checkComponentBounds(this);
}

void JIFAudioProcessorEditor::mouseUp(const juce::MouseEvent& evt) {
    if (evt.mouseWasDraggedSinceMouseDown()) return;
    if (evt.mods.isRightButtonDown()) return;
    viewer.tryLoadWithFileChooser();
}

void JIFAudioProcessorEditor::viewerUpdated() {
    const auto viewerInFG = !isMouseOverOrDragging(true);
    if (viewerInForeground != viewerInFG) {
        viewerInForeground = viewerInFG;
        if (viewerInForeground)
            viewer.setBounds(getLocalBounds());
        else {
            const auto newWidth = static_cast<float>(getWidth()) * .2f;
            const auto newHeight = static_cast<float>(getHeight()) * .2f;
            viewer.setBounds(0, 0, static_cast<int>(newWidth), static_cast<int>(newHeight));
        }
    }
}