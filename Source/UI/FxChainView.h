#pragma once

#include "Controls.h"
#include "../PluginProcessor.h"

#include <optional>

//==============================================================================
/**
    The effect "node" workflow. Sound flows from upstream to downstream:

        AUDIO IN  ->  [Filter]  ->  [Delay]  ->  [Reverb]  ->  (+)  ->  AUDIO OUT

    - "+" adds an effect to the end of the chain
    - drag a node left/right to change the processing order
    - click a node to edit it; its dot switches it on/off, the x removes it
*/
class FxChainView final : public UI::Section,
                          public juce::TooltipClient,
                          private juce::Timer
{
public:
    explicit FxChainView (SynthAudioProcessor&);
    ~FxChainView() override;

    /** Called when a different node is selected (or when none is left). */
    std::function<void (std::optional<Params::Effect>)> onSelectionChanged;

    std::optional<Params::Effect> getSelected() const noexcept   { return selected; }

    void paint (juce::Graphics&) override;
    void resized() override;
    juce::String getTooltip() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    using Effect = Params::Effect;

    enum class Part { none, audioIn, audioOut, node, power, remove, add };

    struct Hit
    {
        Part part = Part::none;
        int index = -1;

        bool operator== (const Hit& other) const noexcept   { return part == other.part && index == other.index; }
    };

    void timerCallback() override;
    void updateLayout();
    void updateSubtitle();

    Hit findHit (juce::Point<float>) const;
    static juce::Rectangle<float> powerArea (juce::Rectangle<float> node);
    static juce::Rectangle<float> removeArea (juce::Rectangle<float> node);
    std::optional<Effect> hoveredEffect() const;

    void select (std::optional<Effect>);
    void commit (std::vector<Effect> newChain);
    void showAddMenu();
    void addEffect (Effect);
    void removeEffect (Effect);
    bool isEffectOn (Effect) const;
    void toggleEffect (Effect);

    void drawEndpoint (juce::Graphics&, juce::Rectangle<float>, const juce::String& text) const;
    void drawNode (juce::Graphics&, juce::Rectangle<float>, Effect, bool floating) const;
    void drawAddButton (juce::Graphics&) const;

    /** While dragging, the nodes are laid out in the previewed order. */
    const std::vector<Effect>& displayOrder() const noexcept   { return dragging ? dragPreview : chain; }

    SynthAudioProcessor& processor;
    std::array<juce::RangedAudioParameter*, Params::numEffects> onParams {};

    std::vector<Effect> chain;
    std::optional<Effect> selected;

    // Layout of the current display order
    juce::Rectangle<float> inBox, outBox, addBox;
    std::vector<juce::Rectangle<float>> nodeBoxes;
    float nodeWidth = 110.0f;
    float firstNodeX = 0.0f;
    float layoutGap = 28.0f;

    // Drag to reorder
    int pressedIndex = -1;
    bool dragging = false;
    float grabOffset = 0.0f;
    float floatingX = 0.0f;
    std::vector<Effect> dragPreview;

    Hit hover;
    std::array<bool, Params::numEffects> lastOnState {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxChainView)
};
