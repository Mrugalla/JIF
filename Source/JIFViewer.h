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
        freeze(0);
        juce::CriticalSection mutex;
        juce::ScopedLock lock(mutex);
        const auto state = processor.apvts.state;
        const juce::String dialogBoxTitle("Load a JIF!");
        const juce::String initDirectory = state.getProperty("directory", "");
        bool managedToLoad = false;
        if (initDirectory != "") {
            juce::File directoryFile(initDirectory);
            juce::FileChooser chooser(dialogBoxTitle, directoryFile);
            if (chooser.browseForFileToOpen())
                managedToLoad = tryLoad(chooser.getResult().getFullPathName());
        }
        else {
            juce::FileChooser chooser(dialogBoxTitle);
            if (chooser.browseForFileToOpen())
                managedToLoad = tryLoad(chooser.getResult().getFullPathName());
        }
        if (!managedToLoad) return;
        unfreeze();
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
    void saveWavetable() {
        const auto pathStr = juce::File::getSpecialLocation(
            juce::File::SpecialLocationType::userDesktopDirectory
        ).getFullPathName();
        const int cyclesPerTable = 256, samplesPerCycle = 2048;
        const int numSamples = samplesPerCycle * cyclesPerTable;
        const int numTables = jif.numImages();
        juce::File txtFile(pathStr + "\\FolderInfo.txt");
        juce::FileOutputStream fileStream(txtFile);
        fileStream.setPosition(0);
        fileStream << "[" << juce::String(samplesPerCycle) << "]";
        juce::AudioBuffer<float> wt;
        juce::Image tmpImg(juce::Image::RGB, samplesPerCycle, cyclesPerTable, true);
        const auto bounds = tmpImg.getBounds().toFloat();
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer;
        for (auto j = 0; j < numTables; ++j) {
            jif.setFrameTo(j);
            juce::Graphics g{ tmpImg };
            g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
            jif.paint(g, bounds);
            wt.setSize(1, numSamples, false, true, false);
            auto samples = wt.getWritePointer(0);
            for (auto y = 0; y < cyclesPerTable; ++y) {
                const auto cycleIdx = y * samplesPerCycle;
                for (auto x = 0; x < samplesPerCycle; ++x) {
                    const auto val = tmpImg.getPixelAt(x, y).getBrightness() * 2.f - 1.f;
                    samples[x + cycleIdx] = val;
                }
            }
            const auto filePath = pathStr + "\\wt" + juce::String(j) + ".wav";
            juce::File file(filePath);
            writer.reset(format.createWriterFor(new juce::FileOutputStream(file),
                44100,
                1,
                24,
                {},
                0
            ));
            if (writer != nullptr)
                writer->writeFromAudioSampleBuffer(wt, 0, numSamples);
        }
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