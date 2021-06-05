#include "PluginProcessor.h"
#include "PluginEditor.h"

JIFAudioProcessor::JIFAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
    ppq(0),
    isPlaying(false),
    hasPlayhead(false),
    posInfo(),
    apvts(*this, nullptr, "params", param::createParameters()),
    speed(apvts.getRawParameterValue(param::getID(param::ID::Speed))),
    phase(apvts.getRawParameterValue(param::getID(param::ID::Phase)))
#endif
{

}

JIFAudioProcessor::~JIFAudioProcessor()
{
}

//==============================================================================
const juce::String JIFAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool JIFAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool JIFAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool JIFAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double JIFAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int JIFAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int JIFAudioProcessor::getCurrentProgram()
{
    return 0;
}

void JIFAudioProcessor::setCurrentProgram (int)
{
}

const juce::String JIFAudioProcessor::getProgramName (int)
{
    return {};
}

void JIFAudioProcessor::changeProgramName (int, const juce::String&)
{
}

//==============================================================================
void JIFAudioProcessor::prepareToPlay (double, int)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void JIFAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool JIFAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void JIFAudioProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) {
    auto playHead = getPlayHead();
    if (playHead) {
        hasPlayhead.store(true);
        playHead->getCurrentPosition(posInfo);
        isPlaying.store(posInfo.isPlaying);

        const auto speedValue = speed->load();
        const auto speedConv = std::pow(2.f, speedValue) * .25f;
        const auto curPPQ = static_cast<float>(posInfo.ppqPosition) * speedConv;
        const auto ppqFrac = curPPQ - std::floor(curPPQ);
        ppq.store(ppqFrac);
    }
    else
        hasPlayhead.store(false);
}

//==============================================================================
bool JIFAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* JIFAudioProcessor::createEditor()
{
    return new JIFAudioProcessorEditor (*this);
}

//==============================================================================
void JIFAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // SAFE GIF

    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void JIFAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

    // LOAD GIF
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JIFAudioProcessor();
}
