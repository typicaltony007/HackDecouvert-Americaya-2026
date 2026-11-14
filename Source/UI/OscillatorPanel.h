#pragma once

#include "Controls.h"
#include "../PluginProcessor.h"

//==============================================================================
/** Small screen showing one cycle of the oscillator's current waveform. */
class WaveformDisplay final : public juce::Component
{
public:
    WaveformDisplay (SynthAudioProcessor& p, int oscIndex);

    void setWaveform (Params::Waveform newWaveform);
    void refreshCustomTable();
    void paint (juce::Graphics&) override;

private:
    float valueAt (double phase) const;

    SynthAudioProcessor& processor;
    const int oscIndex;
    Params::Waveform waveform = Params::Waveform::sine;
    std::array<float, CustomWaveTable::tableSize> customCopy {};
};

//==============================================================================
/**
    One oscillator: waveform selector + preview and level / octave / semitone /
    fine knobs. The formula editor only appears when "Custom" is selected.
*/
class OscillatorPanel final : public UI::Section,
                              private juce::ValueTree::Listener,
                              private juce::AsyncUpdater
{
public:
    OscillatorPanel (SynthAudioProcessor& p, int oscIndex);
    ~OscillatorPanel() override;

    void resized() override;

private:
    void waveformChanged();
    void applyFormula();
    void showStatus (const juce::String& message, bool isError);
    bool isCustom();
    void returnFocusToEditor();

    // Keeps the formula text in sync when a preset/session is loaded.
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeRedirected (juce::ValueTree&) override;
    void handleAsyncUpdate() override;

    SynthAudioProcessor& processor;
    const int oscIndex;

    UI::Choice waveChoice;
    UI::Knob levelKnob, octaveKnob, semiKnob, fineKnob;
    WaveformDisplay display;

    juce::Label formulaLabel { {}, "f(x) =" };
    juce::TextEditor formulaEditor;
    juce::TextButton applyButton { "Apply" };
    juce::Label statusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscillatorPanel)
};
