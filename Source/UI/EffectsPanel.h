#pragma once

#include "Controls.h"
#include "FxChainView.h"
#include "../PluginProcessor.h"

#include <optional>

//==============================================================================
/** One effect's controls: a titled section with an on/off switch, an optional
    display on the left (used by the EQ), and a row of knobs / selectors. */
class FxSection final : public UI::Section
{
public:
    FxSection (UI::APVTS& apvts, const juce::String& title, const juce::String& powerParamID, juce::Colour colour);

    FxSection& knob    (const juce::String& paramID, const juce::String& caption);
    FxSection& choice  (const juce::String& paramID, const juce::String& caption);

    /** Adds a display component that is as wide as `widthInControls` knobs. */
    FxSection& display (std::unique_ptr<juce::Component> component, int widthInControls);

    void resized() override;

private:
    UI::APVTS& apvts;
    juce::Colour colour;
    UI::PowerSwitch power;
    std::vector<std::unique_ptr<juce::Component>> controls;

    std::unique_ptr<juce::Component> displayComponent;
    int displayWeight = 0;
};

//==============================================================================
/**
    The "Effects" tab: the node chain on top, and below it the controls of
    whichever node is selected.
*/
class EffectsPanel final : public juce::Component
{
public:
    explicit EffectsPanel (SynthAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FxSection& addSection (Params::Effect);
    void showInspector (std::optional<Params::Effect>);

    SynthAudioProcessor& processor;
    FxChainView chainView;
    std::array<std::unique_ptr<FxSection>, Params::numEffects> sections;

    std::optional<Params::Effect> shown;
    juce::Rectangle<int> inspectorArea;
};
