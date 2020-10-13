/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ChorusAudioProcessor::ChorusAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{

	addParameter(delayInSeconds = new juce::AudioParameterFloat("delayInSeconds", "Delay", 0.0f, 1.0f, 0.1f));  // [2]
	addParameter(alpha = new juce::AudioParameterFloat("alpha", "Alpha", 0.0f, 1.0f, 0.7f));
	addParameter(period = new juce::AudioParameterFloat("period", "Period", 0.0f, 5.0f, 2.0f));
	addParameter(depth = new juce::AudioParameterFloat("depth", "Depth", 0.0f, 1, .1));
	visualizer = new juce::AudioVisualiserComponent(1);

	

}

ChorusAudioProcessor::~ChorusAudioProcessor()
{
}

//==============================================================================
const juce::String ChorusAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ChorusAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ChorusAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ChorusAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ChorusAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ChorusAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ChorusAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ChorusAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String ChorusAudioProcessor::getProgramName (int index)
{
    return {};
}

void ChorusAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void ChorusAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

	delayBuffer.setSize(getTotalNumInputChannels(), 2 * sampleRate);
	echo.setSize(getTotalNumInputChannels(), samplesPerBlock);

	secondsPerBlock = samplesPerBlock / sampleRate;
	
	time = 0;
	visualizer->setSamplesPerBlock(512);
	visualizer->setRepaintRate(10);

}

void ChorusAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ChorusAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
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

void ChorusAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.

    
    //Retrieve our user-inputted parameters
	auto alphaCopy = alpha->get();
	auto delayCopy = delayInSeconds->get();
	auto periodCopy = period->get();
	double depthCopy = depth->get() / 100.0;
	double mdelay;

	
	int bufferLength = buffer.getNumSamples();
	int delayBufferLength = delayBuffer.getNumSamples();
	int sampleRate = getSampleRate();

	//modulate the delay according to time
	//delayCopy += depthCopy * sin((2 * juce::MathConstants<double>::pi / periodCopy) * secondsPerBlock * time);
	//time++;
	
	

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {

		auto* bufferData = buffer.getWritePointer(channel);
		auto* delayBufferData = delayBuffer.getWritePointer(channel);

		//copy echo from beginning of delay buffer
		echo.makeCopyOf(delayBuffer);

		//move delayBuffer down
		delayBuffer.copyFrom(channel, 0, delayBufferData + bufferLength, delayBufferLength - bufferLength);

		//put dampened echo at end of delay buffer
		delayBuffer.copyFromWithRamp(channel, delayBufferLength - bufferLength, echo.getReadPointer(channel) + delayBufferLength - (int)(delayCopy * getSampleRate()), bufferLength, alphaCopy, alphaCopy);

		//add new data to end of delay buffer
		delayBuffer.addFrom(channel, delayBufferLength - bufferLength, bufferData, bufferLength);

		//read data from beginning of delay buffer
		for (int j = 0; j < bufferLength; j++)
		{
			mdelay = delayCopy + depthCopy * sin((2 * juce::MathConstants<double>::pi / periodCopy) / sampleRate * time);
			if (mdelay * sampleRate > bufferLength)
				buffer.addSample(channel, j, delayBuffer.getSample(channel, delayBufferLength - (int)(mdelay * sampleRate) + j));
			time++;
			

		}

		time %= (int)(periodCopy * sampleRate);
    }

	visualizer->pushBuffer(buffer);
	
	
}

//==============================================================================
bool ChorusAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* ChorusAudioProcessor::createEditor()
{
    return new ChorusAudioProcessorEditor (*this);
}

//==============================================================================
void ChorusAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void ChorusAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChorusAudioProcessor();
}
