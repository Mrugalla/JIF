#pragma once
#include <JuceHeader.h>
#include "JIF.h"

struct JIFViewerListener {
    virtual void viewerUpdated() = 0;
};

struct JIFViewer :
    public juce::Component,
    public juce::Timer,
    public juce::AsyncUpdater
{
    JIFViewer(JIFAudioProcessor& p) :
        jif(),
        processor(p),
        cFont(),
        bounds(0,0,0,0),
        fps(0), speedValue(420)
    { setOpaque(true); }
    void freeze(const int imageIdx) {
        stopTimer();
        if(jif.setFrameTo(imageIdx))
            triggerRepaint();
    }
    void unfreeze() { updateFPS(); }
    void addListener(JIFViewerListener* c) { listeners.push_back(c); }
    void setFont(const juce::Font& f) noexcept { cFont = f; }
    void tryLoadWithFileChooser() {
        const auto& state = processor.apvts.state;
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
    bool tryLoad(const juce::String& path) {
        if (path.endsWith(".gif")) {
            juce::File file(path);
            juce::MemoryBlock block;
            if (file.loadFileAsData(block)) {
                jif.reload(block.getData(), block.getSize());
                updateFPS();
                auto& state = processor.apvts.state;
                for (auto i = path.length() - 1; i > -1; --i)
                    if (path[i] == '\\') {
                        state.setProperty("directory", path.substring(0, i), nullptr);
                        break;
                    }
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
        updateFPS();
        return false;
    }
    jif::JIF jif;
protected:
    JIFAudioProcessor& processor;
    std::vector<JIFViewerListener*> listeners;
    juce::Font cFont;
    juce::Rectangle<float> bounds;
    float fps, speedValue;

    void timerCallback() override {
        const auto newSpeedValue = processor.speed->load();
        if (speedValue != newSpeedValue) {
                speedValue = newSpeedValue;
                const auto speed = convertSpeed(speedValue);
                const auto range = static_cast<float>(jif.loopEnd - jif.loopStart);
                updateFPS(speed, range);
            }
        if (!processor.hasPlayhead.load()) {
            ++jif;
            return triggerRepaint();
        }
        if (!processor.isPlaying.load()) return updateListeners();
        const auto ppq = processor.ppq.load();
        const auto phase = processor.phase->load();
        if (jif.setFrameTo(ppq, phase))
            triggerRepaint();
    }
    void triggerRepaint() {
        handleAsyncUpdate();
        updateListeners();
    }
    void updateListeners() {
        for (auto comp : listeners)
            comp->viewerUpdated();
    }
    void handleAsyncUpdate() override { repaint(); }
    void paint(juce::Graphics& g) override {
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        g.setFont(cFont);
        jif.paint(g, bounds);
    }
    void resized() override { bounds = getLocalBounds().toFloat(); }

    inline float convertSpeed(const float s) const noexcept { return std::pow(2.f, s); }
    void updateFPS(const float speed, const float range) noexcept {
        fps = juce::jlimit(1.f, 50.f, speed * range);
        startTimer(static_cast<int>(1000.f / fps));
    }
    void updateFPS() noexcept {
        const auto speed = convertSpeed(speedValue);
        const auto range = static_cast<float>(jif.loopEnd - jif.loopStart);
        updateFPS(speed, range);
    }

    void mouseUp(const juce::MouseEvent& evt) override {
        if (evt.mouseWasDraggedSinceMouseDown() || evt.mods.isRightButtonDown()) return;
        tryLoadWithFileChooser();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JIFViewer)
};

/* to do
*
* if(!isPlaying) still be able to open controls editor
* 
* frame drop less
*   alternate thread?
*
* reverse button?
*   erfordert loopup table jif
*/