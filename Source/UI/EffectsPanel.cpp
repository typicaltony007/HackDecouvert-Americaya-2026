#include "EffectsPanel.h"
#include "EqVisualiser.h"

using namespace Params;

//==============================================================================
FxSection::FxSection (UI::APVTS& state, const juce::String& sectionTitle,
                      const juce::String& powerParamID, juce::Colour c)
    : UI::Section (sectionTitle, c), apvts (state), colour (c), power (state, powerParamID, c)
{
    addAndMakeVisible (power);
}

FxSection& FxSection::knob (const juce::String& paramID, const juce::String& caption)
{
    auto& k = controls.emplace_back (std::make_unique<UI::Knob> (apvts, paramID, caption, colour));
    addAndMakeVisible (*k);
    return *this;
}

FxSection& FxSection::choice (const juce::String& paramID, const juce::String& caption)
{
    auto& c = controls.emplace_back (std::make_unique<UI::Choice> (apvts, paramID, caption));
    addAndMakeVisible (*c);
    return *this;
}

FxSection& FxSection::display (std::unique_ptr<juce::Component> component, int widthInControls)
{
    displayComponent = std::move (component);
    displayWeight = juce::jmax (1, widthInControls);
    addAndMakeVisible (*displayComponent);
    return *this;
}

void FxSection::resized()
{
    power.setBounds (getTitleBounds().removeFromRight (64).reduced (12, 7));

    auto r = getContentBounds();
    constexpr int gap = 12;
    constexpr int maxColumnWidth = 120;
    constexpr int maxKnobHeight = 150;

    // Optional display (the EQ visualiser) takes the left part of the section.
    if (displayComponent != nullptr)
    {
        const int units = (int) controls.size() + displayWeight;
        displayComponent->setBounds (r.removeFromLeft (r.getWidth() * displayWeight / juce::jmax (1, units)));
        r.removeFromLeft (gap * 2);
    }

    if (controls.empty())
        return;

    const int n = (int) controls.size();
    const int w = juce::jmax (1, juce::jmin (maxColumnWidth, (r.getWidth() - gap * (n - 1)) / n));

    // A short row of controls is centred instead of being stretched across the panel.
    if (displayComponent == nullptr)
        r = r.withSizeKeepingCentre (w * n + gap * (n - 1), r.getHeight());

    for (auto& c : controls)
    {
        auto column = r.removeFromLeft (w);
        r.removeFromLeft (gap);

        if (dynamic_cast<UI::Choice*> (c.get()) != nullptr)
            c->setBounds (column.withSizeKeepingCentre (column.getWidth(), 48));
        else
            c->setBounds (column.withSizeKeepingCentre (column.getWidth(), juce::jmin (column.getHeight(), maxKnobHeight)));
    }
}

//==============================================================================
EffectsPanel::EffectsPanel (SynthAudioProcessor& p)
    : processor (p), chainView (p)
{
    addSection (Effect::distortion)
        .choice (ID::distType,   "Type")
        .knob   (ID::distDrive,  "Drive")
        .knob   (ID::distOutput, "Output")
        .knob   (ID::distMix,    "Mix");

    addSection (Effect::bitcrush)
        .knob (ID::crushBits, "Bits")
        .knob (ID::crushRate, "Rate")
        .knob (ID::crushMix,  "Mix");

    addSection (Effect::filter)
        .choice (ID::filterType,      "Type")
        .knob   (ID::filterCutoff,    "Cutoff")
        .knob   (ID::filterResonance, "Resonance")
        .knob   (ID::filterMix,       "Mix");

    addSection (Effect::eq)
        .display (std::make_unique<EqVisualiser> (p), 6)
        .knob (ID::eqLowFreq,  "Low Freq")
        .knob (ID::eqLowGain,  "Low Gain")
        .knob (ID::eqMidFreq,  "Mid Freq")
        .knob (ID::eqMidGain,  "Mid Gain")
        .knob (ID::eqMidQ,     "Mid Q")
        .knob (ID::eqHighFreq, "High Freq")
        .knob (ID::eqHighGain, "High Gain");

    addSection (Effect::chorus)
        .knob (ID::chorusRate,     "Rate")
        .knob (ID::chorusDepth,    "Depth")
        .knob (ID::chorusDelay,    "Delay")
        .knob (ID::chorusFeedback, "Feedback")
        .knob (ID::chorusMix,      "Mix");

    addSection (Effect::compressor)
        .knob (ID::compThreshold, "Threshold")
        .knob (ID::compRatio,     "Ratio")
        .knob (ID::compAttack,    "Attack")
        .knob (ID::compRelease,   "Release")
        .knob (ID::compMakeup,    "Makeup");

    addSection (Effect::delay)
        .knob (ID::delayTime,     "Time")
        .knob (ID::delayFeedback, "Feedback")
        .knob (ID::delayMix,      "Mix");

    addSection (Effect::reverb)
        .knob (ID::reverbSize,    "Size")
        .knob (ID::reverbDamping, "Damping")
        .knob (ID::reverbWidth,   "Width")
        .knob (ID::reverbMix,     "Mix");

    addAndMakeVisible (chainView);
    chainView.onSelectionChanged = [this] (std::optional<Effect> effect) { showInspector (effect); };
    showInspector (chainView.getSelected());
}

FxSection& EffectsPanel::addSection (Effect effect)
{
    const auto& info = effectInfo[(size_t) effect];
    auto& section = sections[(size_t) effect];

    section = std::make_unique<FxSection> (processor.getAPVTS(), juce::String (info.name).toUpperCase(),
                                           info.onParamID, Palette::accentAlt);
    section->setSubtitle (info.description);
    addChildComponent (*section);
    return *section;
}

void EffectsPanel::showInspector (std::optional<Effect> effect)
{
    shown = effect;

    for (int i = 0; i < numEffects; ++i)
        sections[(size_t) i]->setVisible (effect.has_value() && (int) *effect == i);

    repaint();
}

void EffectsPanel::paint (juce::Graphics& g)
{
    if (shown.has_value())
        return;

    // Empty state: explain what to do
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (inspectorArea.toFloat(), 8.0f);

    g.setColour (Palette::textDim);
    g.setFont (juce::FontOptions (16.0f));
    g.drawFittedText ("Effects change how your sound feels.\n"
                      "Click + above to add one, then click it to change its settings here.",
                      inspectorArea.reduced (24), juce::Justification::centred, 2);
}

void EffectsPanel::resized()
{
    auto r = getLocalBounds();
    chainView.setBounds (r.removeFromTop (118));
    r.removeFromTop (8);

    inspectorArea = r;

    for (auto& section : sections)
        section->setBounds (r);
}
