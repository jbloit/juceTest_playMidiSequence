/*
  ==============================================================================

    SynthSource.h
    Created: 23 Jun 2020 10:49:30am
    Author:  Julien Bloit

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

//==============================================================================
/** Our demo synth sound is just a basic sine wave.. */
struct SineWaveSound : public SynthesiserSound
{
    SineWaveSound() {}

    bool appliesToNote (int /*midiNoteNumber*/) override    { return true; }
    bool appliesToChannel (int /*midiChannel*/) override    { return true; }
};

//==============================================================================
/** Our demo synth voice just plays a sine wave.. */
struct SineWaveVoice  : public SynthesiserVoice
{
    SineWaveVoice() {}

    bool canPlaySound (SynthesiserSound* sound) override
    {
        return dynamic_cast<SineWaveSound*> (sound) != nullptr;
    }

    void startNote (int midiNoteNumber, float velocity,
                    SynthesiserSound*, int /*currentPitchWheelPosition*/) override
    {
        currentAngle = 0.0;
        level = velocity * 0.15;
        tailOff = 0.0;

        auto cyclesPerSecond = MidiMessage::getMidiNoteInHertz (midiNoteNumber);
        auto cyclesPerSample = cyclesPerSecond / getSampleRate();

        angleDelta = cyclesPerSample * MathConstants<double>::twoPi;
    }

    void stopNote (float /*velocity*/, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            // start a tail-off by setting this flag. The render callback will pick up on
            // this and do a fade out, calling clearCurrentNote() when it's finished.

            if (tailOff == 0.0) // we only need to begin a tail-off if it's not already doing so - the
                tailOff = 1.0;  // stopNote method could be called more than once.
        }
        else
        {
            // we're being told to stop playing immediately, so reset everything..
            clearCurrentNote();
            angleDelta = 0.0;
        }
    }

    void pitchWheelMoved (int /*newValue*/) override                              {}
    void controllerMoved (int /*controllerNumber*/, int /*newValue*/) override    {}

    void renderNextBlock (AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
    {
        if (angleDelta != 0.0)
        {
            if (tailOff > 0.0)
            {
                while (--numSamples >= 0)
                {
                    auto currentSample = (float) (std::sin (currentAngle) * level * tailOff);

                    for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                        outputBuffer.addSample (i, startSample, currentSample);

                    currentAngle += angleDelta;
                    ++startSample;

                    tailOff *= 0.99;

                    if (tailOff <= 0.005)
                    {
                        clearCurrentNote();

                        angleDelta = 0.0;
                        break;
                    }
                }
            }
            else
            {
                while (--numSamples >= 0)
                {
                    auto currentSample = (float) (std::sin (currentAngle) * level);

                    for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                        outputBuffer.addSample (i, startSample, currentSample);

                    currentAngle += angleDelta;
                    ++startSample;
                }
            }
        }
    }

    using SynthesiserVoice::renderNextBlock;

private:
    double currentAngle = 0.0, angleDelta = 0.0, level = 0.0, tailOff = 0.0;
};


//==============================================================================
// This is an audio source that streams the output of our demo synth.
struct SynthSource  : public AudioSource
{
    SynthSource ()
    {
        // Add some voices to our synth, to play the sounds..
        for (auto i = 0; i < 4; ++i)
        {
            synth.addVoice (new SineWaveVoice());   // These voices will play our custom sine-wave sounds..

        }

        
        // ..and add a sound for them to play...
        setUsingSineWaveSound();
        
        
    }

    void setUsingSineWaveSound()
    {
        synth.clearSounds();
        synth.addSound (new SineWaveSound());
    }

    void prepareToPlay (int /*samplesPerBlockExpected*/, double newSampleRate) override
    {
        
        sampleRate = newSampleRate;
        synth.setCurrentPlaybackSampleRate (sampleRate);
        initMidiSequence();
    }

    void releaseResources() override {}

    void getNextAudioBlock (const AudioSourceChannelInfo& bufferToFill) override
    {
        // the synth always adds its output to the audio buffer, so we have to clear it
        // first..
        bufferToFill.clearActiveBufferRegion();


        auto numSamples = bufferToFill.numSamples;
        midiBuffer.clear();

        int nextEventIndex  = midiFile.getTrack(0)->getNextIndexAtTime(samplePosition/sampleRate);
        double nextEventTime = midiFile.getTrack(0)->getEventTime(nextEventIndex);
        nextEventTime = nextEventTime * sampleRate;
        
        if (nextEventTime > samplePosition && nextEventTime <= samplePosition + numSamples )
        {
            midiBuffer.addEvent(midiFile.getTrack(0)->getEventPointer(nextEventIndex)->message, 0);
        }

        
        // and now get the synth to process the midi events and generate its output.
        synth.renderNextBlock (*bufferToFill.buffer, midiBuffer, 0, bufferToFill.numSamples);
        
        
        samplePosition += numSamples;
    }
    
    void initMidiSequence()
    {
        std::unique_ptr<MemoryInputStream> inputStream;
        inputStream.reset(new MemoryInputStream(BinaryData::pianoPhase_mid, BinaryData::pianoPhase_midSize, false));
        
        midiFile.readFrom(*inputStream.get());
        
        midiFile.convertTimestampTicksToSeconds();
        
        DBG("Found N events " << midiFile.getTrack(0)->getNumEvents());
        
        sequence = MidiMessageSequence{*midiFile.getTrack(0)};
        
        for (int i=0; i<midiFile.getTrack(0)->getNumEvents(); i++)
        {
            DBG("event time stamp " << String(midiFile.getTrack(0)->getEventTime(i)));
        }
    }
    
    //==============================================================================
    // this collects real-time midi messages from the midi input device, and
    // turns them into blocks that we can process in our audio callback
    MidiMessageCollector midiCollector;

    // the synth itself!
    Synthesiser synth;
    double sampleRate;
    MidiMessageSequence sequence;
    MidiBuffer midiBuffer;
    
    MidiFile midiFile;
    
    double samplePosition = 0;
};
