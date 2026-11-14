#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
    Shared colour palette: deep warm blacks with light beige text, plus three
    warm accents (sand = synth, caramel = effects, clay = modulation).

    Text is tuned for strong contrast against the panels (WCAG contrast ratio):
    text ~16:1, textDim ~12:1, textFaint ~8:1 - all well above the 4.5:1 minimum.
*/
namespace Palette
{
    // Warm blacks (backgrounds)
    inline const juce::Colour background   { 0xff121110 };
    inline const juce::Colour panel        { 0xff1c1a17 };
    inline const juce::Colour knobBody     { 0xff2a2622 };   // raised surfaces: knobs, boxes, buttons
    inline const juce::Colour panelOutline { 0xff3d3730 };   // tracks, separators
    inline const juce::Colour screen       { 0xff0d0c0b };   // displays

    // Beige text
    inline const juce::Colour text         { 0xfffbf6ec };
    inline const juce::Colour textDim      { 0xffe3d8c5 };
    inline const juce::Colour textFaint    { 0xffc3b6a1 };   // hints and grid labels

    // Warm accents
    inline const juce::Colour accent       { 0xffe6d3ae };   // sand     - oscillators
    inline const juce::Colour accentAlt    { 0xffe0a466 };   // caramel  - effects
    inline const juce::Colour modAccent    { 0xffd8957a };   // clay     - modulation
    inline const juce::Colour error        { 0xffff8a78 };
}

//==============================================================================
class SynthLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SynthLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool isMouseOver, bool isMouseDown) override;
    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics&, int w, int h) override;
    int getTabButtonBestWidth (juce::TabBarButton&, int tabDepth) override;
};
