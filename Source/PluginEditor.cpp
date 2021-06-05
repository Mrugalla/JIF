#include "PluginProcessor.h"
#include "PluginEditor.h"

JIFAudioProcessorEditor::JIFAudioProcessorEditor (JIFAudioProcessor& p)
    : AudioProcessorEditor (&p),
    audioProcessor (p),
    bounds(),
    jif(),
    controlsEditor(nullptr),
    cFont(getCustomFont()),
    fps(1),
    speedValue(420)
{
    setMinimumWidth(MinWidth);
    setMinimumHeight(MinHeight);

    cFont.setExtraKerningFactor(.05f);
    cFont.setHeight(32);

    auto& state = p.apvts.state;
    auto path = state.getProperty("gif", "").toString();
    tryLoad(path);

    setOpaque(true);
    setResizable(true, true);
    const auto width = static_cast<float>(state.getProperty("width", 400));
    const auto height = static_cast<float>(state.getProperty("height", 300));
    setSize(width, height);
}

void JIFAudioProcessorEditor::updateFPS() {
    const auto speed = std::pow(2.f, audioProcessor.speed->load());
    const auto range = static_cast<float>(jif.loopEnd - jif.loopStart);
    fps = juce::jlimit(1.f, 50.f, speed * range);
    startTimer(static_cast<int>(1000.f / fps));
}

void JIFAudioProcessorEditor::paint (juce::Graphics& g) {
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.setFont(cFont);
    jif.paint(g, bounds);
}
void JIFAudioProcessorEditor::resized() {
    bounds = getLocalBounds().toFloat();
    if (controlsEditor != nullptr)
        controlsEditor->setBounds(getLocalBounds());
    auto& state = audioProcessor.apvts.state;
    state.setProperty("width", getWidth(), nullptr);
    state.setProperty("height", getHeight(), nullptr);
    checkComponentBounds(this);
}
void JIFAudioProcessorEditor::timerCallback() {
    if (controlsEditor != nullptr) {
        const auto controlsVisible = isMouseOverOrDragging(true);
        controlsEditor->setVisible(controlsVisible);
        const auto newSpeedValue = audioProcessor.speed->load();
        if (speedValue != newSpeedValue) {
            speedValue = newSpeedValue;
            updateFPS();
        }
    }
    if (!audioProcessor.hasPlayhead.load()) {
        ++jif;
        repaint();
        return;
    }
    if (!audioProcessor.isPlaying.load()) return;
    const auto ppq = audioProcessor.ppq.load();
    const auto phase = audioProcessor.phase->load();
    if(jif.setFrameTo(ppq, phase))
        triggerAsyncUpdate();
}

bool JIFAudioProcessorEditor::tryLoad(const juce::String& path) {
    if (path.endsWith(".gif")) {
        juce::File file(path);
        juce::MemoryBlock block;
        if (file.loadFileAsData(block)) {
            jif.reload(block.getData(), block.getSize());
            auto loaderLambda = [this]() { loadWithFileChooser(); };
            controlsEditor = std::make_unique<ControlsEditor>(audioProcessor, jif, loaderLambda);
            addAndMakeVisible(*controlsEditor);
            controlsEditor->setBounds(getLocalBounds());
            updateFPS();
            for (auto i = path.length() - 1; i > -1; --i)
                if (path[i] == '\\') {
                    auto& state = audioProcessor.apvts.state;
                    state.setProperty("directory", path.substring(0, i), nullptr);
                    break;
                }
            auto& state = audioProcessor.apvts.state;
            const auto lastProperty = state.getProperty("gif", "").toString();
            if (path == lastProperty) {
                jif.loopStart = static_cast<int>(state.getProperty("loopStart", jif.loopStart));
                jif.loopEnd = static_cast<int>(state.getProperty("loopEnd", jif.loopEnd));
            }
            else {
                state.setProperty("gif", path, nullptr);
                state.setProperty("loopStart", jif.loopStart, nullptr);
                state.setProperty("loopEnd", jif.loopEnd, nullptr);
            }
            return true;
        }
    }
    controlsEditor.reset();
    updateFPS();
    return false;
}

void JIFAudioProcessorEditor::mouseUp(const juce::MouseEvent& evt) {
    if (evt.mouseWasDraggedSinceMouseDown()) return;
    if (evt.mods.isRightButtonDown()) return;
    loadWithFileChooser();
}

void JIFAudioProcessorEditor::loadWithFileChooser() {
    const auto& state = audioProcessor.apvts.state;
    const juce::String dialogBoxTitle("Load a JIF!");
    const juce::String initDirectory = state.getProperty("directory", "");
    if (initDirectory != "") {
        juce::File directoryFile(initDirectory);
        juce::FileChooser chooser(dialogBoxTitle, directoryFile);
        if (chooser.browseForFileToOpen())
            tryLoad(chooser.getResult().getFullPathName());
    }
    else {
        juce::FileChooser chooser(dialogBoxTitle);
        if (chooser.browseForFileToOpen())
            tryLoad(chooser.getResult().getFullPathName());
    }
}

void JIFAudioProcessorEditor::handleAsyncUpdate() { repaint(); }