#pragma once

#include "Parameters.h"
#include "DSP/EffectsChain.h"
#include "DSP/SpectrumTap.h"
#include "DSP/SynthVoice.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

//==============================================================================
/**
    Signal flow:
        MIDI -> Synthesiser (8 voices x [Osc A + Osc B], 5 mod sources/voice)   = "audio in"
             -> effect node chain, in the order the user arranged it
             -> Master gain -> Stereo out                                        = "audio out"
*/
class SynthAudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int numVoices = 8;

    SynthAudioProcessor();
    ~SynthAudioProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                         { return true; }

    const juce::String getName() const override             { return JucePlugin_Name; }
    bool acceptsMidi() const override                       { return true; }
    bool producesMidi() const override                      { return false; }
    bool isMidiEffect() const override                      { return false; }
    double getTailLengthSeconds() const override            { return 2.0; }

    int getNumPrograms() override                           { return 1; }
    int getCurrentProgram() override                        { return 0; }
    void setCurrentProgram (int) override                   {}
    const juce::String getProgramName (int) override        { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept  { return apvts; }
    juce::MidiKeyboardState& getKeyboardState() noexcept    { return keyboardState; }
    const SpectrumTap& getOutputTap() const noexcept        { return outputTap; }

    /** Message thread only. Rebuilds an oscillator's custom wavetable from a formula
        and stores the formula in the plugin state. Returns an error message, or
        an empty string on success. */
    juce::String setCustomFormula (int oscIndex, const juce::String& formula);
    juce::String getCustomFormula (int oscIndex) const;
    const CustomWaveTable& getCustomTable (int oscIndex) const { return customTables[(size_t) oscIndex]; }

    /** Message thread only. The effect node chain, from upstream (audio in) to
        downstream (audio out). It is saved with the plugin state. */
    std::vector<Params::Effect> getFxChain() const;
    void setFxChain (const std::vector<Params::Effect>& chain);

private:
    void updateVoiceParams();
    void rebuildCustomTablesFromState();
    void publishFxChainFromState();

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;   // on-screen + computer keyboard notes

    CustomTables customTables;
    VoiceParams voiceParams;
    juce::Synthesiser synth;
    EffectsChain effects;

    // Effect order shared with the audio thread. Written by the message thread:
    // the count is set to 0 first, then the entries, then the new count.
    std::array<std::atomic<int>, Params::numEffects> fxOrder {};
    std::atomic<int> fxCount { 0 };

    juce::SmoothedValue<float> masterGain;
    SpectrumTap outputTap;   // feeds the EQ visualiser's spectrum analyser

    // Cached raw parameter pointers (lock-free reads on the audio thread)
    struct OscRaw  { std::atomic<float>* wave {}; std::atomic<float>* level {}; std::atomic<float>* octave {};
                     std::atomic<float>* semi {}; std::atomic<float>* fine {}; };
    struct ModRaw  { std::atomic<float>* mode {}; std::atomic<float>* dest {}; std::atomic<float>* amount {};
                     std::atomic<float>* attack {}; std::atomic<float>* decay {}; std::atomic<float>* sustain {};
                     std::atomic<float>* release {}; std::atomic<float>* rate {}; std::atomic<float>* shape {}; };

    std::array<OscRaw, Params::numOscillators> oscRaw;
    std::array<ModRaw, Params::numModSlots> modRaw;
    std::atomic<float>* masterGainRaw = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthAudioProcessor)
};
