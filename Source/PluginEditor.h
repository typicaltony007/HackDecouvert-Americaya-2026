#pragma once

#include "PluginProcessor.h"
#include "UI/ComputerKeyboard.h"
#include "UI/Controls.h"
#include "UI/EffectsPanel.h"
#include "UI/ModulationPanel.h"
#include "UI/OscillatorPanel.h"
#include "UI/SynthLookAndFeel.h"

#include <juce_audio_utils/juce_audio_utils.h>

//==============================================================================
/** "Oscillators" tab: Osc A and Osc B side by side. */
class OscillatorsPage final : public juce::Component
{
public:
    explicit OscillatorsPage (SynthAudioProcessor& p) : oscA (p, 0), oscB (p, 1)
    {
        addAndMakeVisible (oscA);
        addAndMakeVisible (oscB);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        oscA.setBounds (r.removeFromLeft ((r.getWidth() - 10) / 2));
        r.removeFromLeft (10);
        oscB.setBounds (r);
    }

private:
    OscillatorPanel oscA, oscB;
};

//==============================================================================
/** Header volume control: caption, slider and the current value in dB. */
class VolumeControl final : public juce::Component
{
public:
    explicit VolumeControl (UI::APVTS& apvts)
    {
        caption.setText ("VOLUME", juce::dontSendNotification);
        caption.setFont (juce::FontOptions (13.5f, juce::Font::bold).withKerningFactor (0.08f));
        caption.setColour (juce::Label::textColourId, Palette::textDim);
        caption.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (caption);

        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (slider);
        attachment = std::make_unique<UI::APVTS::SliderAttachment> (apvts, Params::ID::masterGain, slider);

        value.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        value.setColour (juce::Label::textColourId, Palette::text);
        addAndMakeVisible (value);

        slider.onValueChange = [this]
        {
            value.setText (slider.getTextFromValue (slider.getValue()) + " dB", juce::dontSendNotification);
        };
        slider.onValueChange();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        caption.setBounds (r.removeFromLeft (74));
        value.setBounds (r.removeFromRight (76));
        slider.setBounds (r.reduced (8, 0));
    }

private:
    juce::Label caption, value;
    juce::Slider slider;
    std::unique_ptr<UI::APVTS::SliderAttachment> attachment;
};

//==============================================================================
/**
    Layout:
        +--------------------------------------------------------------+
        | SYNTH MATH                                  VOLUME ----o---- |
        +--------------------------------------------------------------+
        | [OSCILLATORS] [EFFECTS]                                      |
        |   Osc A | Osc B      or   AUDIO IN -> nodes -> AUDIO OUT     |
        |                           + the selected effect's controls   |
        +--------------------------------------------------------------+
        | MODULATION: [ENV 1][LFO 2][ENV 3][LFO 4][ENV 5]              |
        +--------------------------------------------------------------+
        | On-screen keyboard (A W S E D F T G Y H U J play C3..B3)     |
        +--------------------------------------------------------------+
*/
class SynthAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SynthAudioProcessorEditor (SynthAudioProcessor&);
    ~SynthAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Clicking anywhere except a text field gives the editor keyboard focus,
        so the computer keyboard plays notes. */
    void mouseDown (const juce::MouseEvent&) override;

private:
    SynthAudioProcessor& processorRef;
    SynthLookAndFeel lookAndFeel;   // must outlive every child component

    // Pages are declared before the tab container so they outlive it.
    OscillatorsPage oscillatorsPage;
    EffectsPanel effectsPanel;
    juce::TabbedComponent mainTabs { juce::TabbedButtonBar::TabsAtTop };
    ModulationPanel modulationPanel;

    VolumeControl volume;
    LabelledKeyboard keyboard;
    ComputerKeyboard computerKeyboard;
    juce::TooltipWindow tooltipWindow { this, 700 };   // hover help (effects, formula, EQ)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthAudioProcessorEditor)
};
