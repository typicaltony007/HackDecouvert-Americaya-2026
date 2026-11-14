#pragma once

#include "Controls.h"

//==============================================================================
/** The little "screen" that draws a mod slot's envelope curve or LFO shape. */
class ModDisplay final : public juce::Component,
                         private juce::Timer
{
public:
    ModDisplay (UI::APVTS& apvts, int slotIndex);
    ~ModDisplay() override { stopTimer(); }

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void drawEnvelope (juce::Graphics&, juce::Rectangle<float> area);
    void drawLfo (juce::Graphics&, juce::Rectangle<float> area);

    struct Snapshot
    {
        int mode = 0, dest = 0, shape = 0;
        float amount = 0, attack = 0, decay = 0, sustain = 0, release = 0, rate = 0;
        bool operator== (const Snapshot&) const = default;
    };

    Snapshot read() const;

    std::atomic<float>* mode {}; std::atomic<float>* dest {}; std::atomic<float>* amount {};
    std::atomic<float>* attack {}; std::atomic<float>* decay {}; std::atomic<float>* sustain {};
    std::atomic<float>* release {}; std::atomic<float>* rate {}; std::atomic<float>* shape {};

    Snapshot current;
};

//==============================================================================
/** Contents of one mod tab: display + mode/destination/amount + ADSR or LFO controls. */
class ModSlotPage final : public juce::Component
{
public:
    ModSlotPage (UI::APVTS& apvts, int slotIndex, std::function<void (int, bool)> onModeChanged);

    void resized() override;

private:
    void updateVisibility();

    const int slotIndex;
    std::function<void (int, bool)> modeChanged;

    ModDisplay display;
    UI::Choice modeChoice, destChoice, shapeChoice;
    UI::Knob amountKnob, attackKnob, decayKnob, sustainKnob, releaseKnob, rateKnob;
};

//==============================================================================
/** "MODULATION" section: a tabbed screen holding the five envelope/LFO slots. */
class ModulationPanel final : public UI::Section
{
public:
    explicit ModulationPanel (UI::APVTS& apvts);

    void resized() override;

private:
    static juce::String tabName (int slotIndex, bool isLfo);

    std::vector<std::unique_ptr<ModSlotPage>> pages;   // declared first so pages outlive the tabs
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
};
