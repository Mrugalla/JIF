#pragma once
#include <JuceHeader.h>
#include <functional>

struct Label : public juce::Label {
    Label(const juce::String&& name, float fontSize, juce::Colour txtColour, juce::Justification justfctn) :
        juce::Label(name, name)
    {
        setColour(ColourIds::textColourId, txtColour);
        setJustificationType(justfctn);
        auto cFont = getCustomFont();
        cFont.setHeight(fontSize);
        setFont(cFont);
    }
};

struct Knob :
    public juce::Component
{
    Knob(juce::AudioProcessorValueTreeState& apvts, param::ID id, const juce::Colour col) :
        param(*apvts.getParameter(param::getID(id))),
        attach(param, [this](float) { repaint(); }, nullptr),
        colour(col),
        name(param::getName(id)),
        cFont(getCustomFont()),
        dragStartValue(0.f)
    { attach.sendInitialUpdate(); }
protected:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attach;
    const juce::Colour colour;
    juce::String name;
    juce::Font cFont;
    float dragStartValue;

    void mouseDown(const juce::MouseEvent&) override {
        attach.beginGesture();
        dragStartValue = param.getValue();
    }
    void mouseUp(const juce::MouseEvent& evt) override {
        attach.endGesture();
        if (!evt.mouseWasDraggedSinceMouseDown()) {
            if (!evt.mods.isCtrlDown()) {
                const auto width = static_cast<float>(getWidth());
                const auto x = evt.position.x / width;
                const auto newValue = param.convertFrom0to1(x);
                attach.setValueAsCompleteGesture(newValue);
                repaint();
            }
            else {
                attach.setValueAsCompleteGesture(param.convertFrom0to1(param.getDefaultValue()));
            }
        }
    }
    void mouseDrag(const juce::MouseEvent& evt) override {
        const auto width = static_cast<float>(getWidth());
        const auto dist = static_cast<float>(evt.getDistanceFromDragStartX()) / width;
        const auto speed = evt.mods.isShiftDown() ? .05f : 1.f;
        const auto newValue = juce::jlimit(0.f, 1.f, dragStartValue + dist * speed);
        attach.setValueAsPartOfGesture(param.convertFrom0to1(newValue));
        repaint();
    }
    void mouseWheelMove(const juce::MouseEvent& evt, const juce::MouseWheelDetails& wheel) override {
        if (evt.mods.isLeftButtonDown() || evt.mods.isRightButtonDown())
            return;
        const auto speed = evt.mods.isShiftDown() ? .005f : .025f;
        const auto gestr = wheel.deltaY > 0 ? 1 : -1;
        const auto value = param.getValue();
        const auto interval = param.getNormalisableRange().interval;
        const auto oldValue = param.convertFrom0to1(value);
        const auto newValue = param.convertFrom0to1(value + gestr * speed);
        const auto denormalValue = oldValue != newValue ? newValue : oldValue + gestr;
        attach.setValueAsCompleteGesture(
            juce::jlimit(param.getNormalisableRange().start, param.getNormalisableRange().end, denormalValue)
        );
        repaint();
    }
    void paint(juce::Graphics& g) override {
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        const auto width = static_cast<float>(getWidth());
        const auto height = static_cast<float>(getHeight());
        const auto titleWidth = width * .5f;
        const auto titleHeight = height * .5f;
        const auto titleX = 0.f;
        const auto titleY = 0.f;
        juce::Rectangle<float> titleBounds(titleX, titleY, titleWidth, titleHeight);
        g.setFont(cFont);
        g.setColour(colour);
        g.drawFittedText(name + ":", titleBounds.reduced(2).toNearestInt(), juce::Justification::centredRight, 1);
        const auto valueX = titleWidth;
        const auto valueY = 0.f;
        const auto valueWidth = width - titleWidth;
        const auto valueHeight = height - titleHeight;
        const auto valueStr = param.getCurrentValueAsText();
        juce::Rectangle<float> valueBounds(valueX, valueY, valueWidth, valueHeight);
        g.drawFittedText(valueStr, valueBounds.reduced(2).toNearestInt(), juce::Justification::centredLeft, 1);
        const auto pX = 0.f;
        const auto pY = titleHeight;
        const auto pWidth = width;
        const auto pHeight = height - titleHeight;
        const auto pBounds = juce::Rectangle<float> (pX, pY, pWidth, pHeight).reduced(2);
        g.drawRect(pBounds);
        const auto pValue = param.getValue();
        const auto pValueWidth = 4.f;
        const auto pValueX = pBounds.getX() + pValue * pBounds.getWidth() - pValueWidth * .5f;
        const auto pValueY = pBounds.getY();
        const auto pValueHeight = pBounds.getHeight();
        const auto pValueBounds = juce::Rectangle<float>(pValueX, pValueY, pValueWidth, pValueHeight);
        g.fillRect(pValueBounds);
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Knob)
};

struct PhaseKnob :
    public Knob
{
    PhaseKnob(juce::AudioProcessorValueTreeState& apvts, param::ID id, const juce::Colour col, JIFViewer& v) :
        Knob(apvts, id, col),
        viewer(v)
    {}
protected:
    JIFViewer& viewer;

    void mouseUp(const juce::MouseEvent& evt) override {
        Knob::mouseUp(evt);
        viewer.unfreeze();
    }
    void mouseDrag(const juce::MouseEvent& evt) override {
        Knob::mouseDrag(evt);
        const auto start = viewer.jif.loopStart;
        const auto end = viewer.jif.loopEnd;
        const auto range = static_cast<float>(end - start);
        const auto value = param.getValue();
        const auto imgIdx = (start + static_cast<int>(range * value)) % end;
        viewer.freeze(imgIdx);
    }
};

struct Link : public juce::HyperlinkButton {
    Link(const juce::String& name, const juce::String& path, float fontSize, const juce::Colour col) :
        juce::HyperlinkButton(name, juce::URL(path))
    {
        juce::Font tFont(fontSize);
        setFont(tFont, false);
        setColour(ColourIds::textColourId, col);
    }
};

struct Button :
    public juce::Component
{
    Button(const juce::Image& image, std::function<void()> clickFunc, const juce::Colour col) :
        img(),
        imgHover(image.createCopy()),
        onClickFunc(clickFunc)
    {
        for (auto x = 0; x < imgHover.getWidth(); ++x)
            for (auto y = 0; y < imgHover.getHeight(); ++y)
                if(!imgHover.getPixelAt(x,y).isTransparent())
                    imgHover.setPixelAt(x, y, col);
        img = imgHover.createCopy();
        img.multiplyAllAlphas(.5f);
    }
protected:
    juce::Image img, imgHover;
    std::function<void()> onClickFunc;

    void paint(juce::Graphics& g) override {
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
        if (isMouseOver(false))
            g.drawImage(imgHover, getLocalBounds().toFloat());
        else
            g.drawImage(img, getLocalBounds().toFloat());
    }
    void mouseUp(const juce::MouseEvent&) override { onClickFunc(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Button)
};

struct LoopRangeParam :
    public juce::Component,
    public JIFViewerListener
{
    LoopRangeParam(JIFAudioProcessor& p, JIFViewer& v) :
        juce::Component(),
        processor(p),
        viewer(v),
        jif(v.jif)
    {
    }
protected:
    JIFAudioProcessor& processor;
    JIFViewer& viewer;
    jif::JIF& jif;

    void viewerUpdated() override { repaint(); }
    void paint(juce::Graphics& g) override {
        const auto numImages = static_cast<float>(jif.numImages());
        const auto width = static_cast<float>(getWidth());
        const auto height = static_cast<float>(getHeight());
        const auto blockWidth = width / numImages;
        for (auto i = 0; i < numImages; ++i) {
            const auto blockX = blockWidth * i;
            const auto blockArea = juce::Rectangle<float>(blockX, 0.f, blockWidth, height);
            if (i == jif.readIdx)
                g.setColour(juce::Colours::greenyellow);
            else if(i < jif.loopStart || i >= jif.loopEnd)
                g.setColour(juce::Colours::darkgrey);
            else
                g.setColour(juce::Colours::grey);
            g.fillRect(blockArea);
        }
    }

    void mouseDrag(const juce::MouseEvent& evt) override {
        const auto leftButtonDown = evt.mods.isLeftButtonDown();
        updateLoopCues(evt.position.x, leftButtonDown);
        if (leftButtonDown) viewer.freeze(jif.loopStart);
        else viewer.freeze(jif.loopEnd - 1);
    }
    void mouseUp(const juce::MouseEvent& evt) override {
        updateLoopCues(evt.position.x, evt.mods.isLeftButtonDown());
        viewer.unfreeze();
    }
    void updateLoopCues(float x, bool leftButtonDown) {
        const auto numImagesInt = static_cast<int>(jif.numImages());
        const auto numImages = static_cast<float>(numImagesInt);
        const auto width = static_cast<float>(getWidth());
        if (leftButtonDown)
            jif.loopStart = juce::jlimit(0, jif.loopEnd - 1, static_cast<int>(std::floor(x / width * numImages))); 
        else
            jif.loopEnd = juce::jlimit(jif.loopStart + 1, numImagesInt, static_cast<int>(std::ceil(x / width * numImages)));
        auto& state = processor.apvts.state;
        state.setProperty("loopStart", jif.loopStart, nullptr);
        state.setProperty("loopEnd", jif.loopEnd, nullptr);
    }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopRangeParam)
};

struct ControlsEditor :
    public juce::Component
{
    ControlsEditor(JIFAudioProcessor& p, JIFViewer& v) :
        processor(p),
        viewer(v),
        mainColour(getMainColour()),
        cFont(getCustomFont()),
        titleLabel("JIF*", 42, mainColour, juce::Justification::centred),
        subTitleLabel("by Florian Mrugalla", 12, mainColour, juce::Justification::centredBottom),
        reloadButton(juce::ImageCache::getFromMemory(BinaryData::loadJIF_png, BinaryData::loadJIF_pngSize), [this]() { viewer.tryLoadWithFileChooser(); }, mainColour),
        loopRangeParam(p, viewer),
        speedKnob(processor.apvts, param::ID::Speed, mainColour),
        phaseKnob(processor.apvts, param::ID::Phase, mainColour, viewer),
        discord("Discord", "https://discord.gg/xpTGJJNAZG", 12, mainColour),
        github("Github", "https://github.com/Mrugalla", 12, mainColour),
        paypal("Paypal", "https://www.paypal.com/paypalme/alteoma", 12, mainColour)
    {
       cFont.setExtraKerningFactor(.05f);
        discord.setFont(cFont, false);
        github.setFont(cFont, false);
        paypal.setFont(cFont, false);
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(subTitleLabel);
        addAndMakeVisible(reloadButton);
        addAndMakeVisible(loopRangeParam); viewer.addListener(&loopRangeParam);
        addAndMakeVisible(speedKnob);
        addAndMakeVisible(phaseKnob);
        addAndMakeVisible(discord);
        addAndMakeVisible(github);
        addAndMakeVisible(paypal);
    }
protected:
    JIFAudioProcessor& processor;
    JIFViewer& viewer;
    juce::Colour mainColour;
    juce::Font cFont;
    Label titleLabel, subTitleLabel;
    Button reloadButton;
    LoopRangeParam loopRangeParam;
    Knob speedKnob;
    PhaseKnob phaseKnob;
    Link discord, github, paypal;

    void paint(juce::Graphics& g) override {
        g.setFont(cFont);
        g.fillAll(juce::Colour(0xdd000000));
        const juce::String buildDate(__DATE__);
        g.setColour(juce::Colour(0x77ffffff));
        g.setFont(12);
        g.drawFittedText("Build: " + buildDate, getLocalBounds(), juce::Justification::bottomLeft, 1, 0);
        g.drawFittedText("*pronounced with a hard J", getLocalBounds(), juce::Justification::topRight, 1, 0);
    }
    void resized() override {
        auto thingsCount = 6.f;
        const auto width = static_cast<float>(getWidth());
        const auto height = static_cast<float>(getHeight());
        const auto thingsHeight = height / thingsCount;
        auto x = 0.f;
        auto y = 0.f;
        const auto titlesHeight = thingsHeight * 2;
        titleLabel.setBounds(juce::Rectangle<float>(x, y, width, titlesHeight).toNearestInt());
        subTitleLabel.setBounds(juce::Rectangle<float>(x, y, width, titlesHeight).toNearestInt());
        y += titlesHeight;
        loopRangeParam.setBounds(juce::Rectangle<float>(x, y, width, thingsHeight).toNearestInt());
        y += thingsHeight;
        const auto knobsWidth = width * .5f;
        const auto knobsHeight = thingsHeight * 2;
        speedKnob.setBounds(juce::Rectangle<float>(x, y, knobsWidth, knobsHeight).toNearestInt());
        x += knobsWidth;
        phaseKnob.setBounds(juce::Rectangle<float>(x, y, knobsWidth, knobsHeight).toNearestInt());
        x = 0.f;
        y += knobsHeight;
        const auto buttonsWidth = width / 4.f;
        discord.setBounds(juce::Rectangle<float>(x, y, buttonsWidth, thingsHeight).toNearestInt());
        x += buttonsWidth;
        github.setBounds(juce::Rectangle<float>(x, y, buttonsWidth, thingsHeight).toNearestInt());
        x += buttonsWidth;
        paypal.setBounds(juce::Rectangle<float>(x, y, buttonsWidth, thingsHeight).toNearestInt());
        x += buttonsWidth;
        reloadButton.setBounds(juce::Rectangle<float>(x, y, buttonsWidth, thingsHeight).toNearestInt());
        x = 0.f;
        y += thingsHeight;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlsEditor)
};