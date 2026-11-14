#include "SynthLookAndFeel.h"

#include <juce_audio_utils/juce_audio_utils.h>

SynthLookAndFeel::SynthLookAndFeel()
{
    // Base scheme so every stock widget (menus, alerts, scrollbars...) picks up the warm palette.
    setColourScheme (LookAndFeel_V4::ColourScheme (Palette::background,    // windowBackground
                                                   Palette::knobBody,      // widgetBackground
                                                   Palette::panel,         // menuBackground
                                                   Palette::panelOutline,  // outline
                                                   Palette::text,          // defaultText
                                                   Palette::accent,        // defaultFill
                                                   Palette::background,    // highlightedText
                                                   Palette::accent,        // highlightedFill
                                                   Palette::text));        // menuText

   #if JUCE_WINDOWS
    setDefaultSansSerifTypefaceName ("Segoe UI");   // cleaner and easier to read than the default
   #endif

    setColour (juce::ResizableWindow::backgroundColourId, Palette::background);

    setColour (juce::Slider::textBoxTextColourId,       Palette::text);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId,  Palette::accent);
    setColour (juce::Slider::trackColourId,             Palette::accent);
    setColour (juce::Slider::backgroundColourId,        Palette::panelOutline);
    setColour (juce::Slider::thumbColourId,             Palette::text);

    setColour (juce::Label::textColourId, Palette::text);

    setColour (juce::ComboBox::backgroundColourId, Palette::knobBody);
    setColour (juce::ComboBox::outlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::ComboBox::textColourId,       Palette::text);
    setColour (juce::ComboBox::arrowColourId,      Palette::textDim);

    setColour (juce::PopupMenu::backgroundColourId,            Palette::panel);
    setColour (juce::PopupMenu::textColourId,                  Palette::text);
    setColour (juce::PopupMenu::headerTextColourId,            Palette::textDim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::accent);
    setColour (juce::PopupMenu::highlightedTextColourId,       Palette::background);

    setColour (juce::TextEditor::backgroundColourId,     Palette::screen);
    setColour (juce::TextEditor::textColourId,           Palette::text);
    setColour (juce::TextEditor::highlightColourId,      Palette::accent.withAlpha (0.35f));
    setColour (juce::TextEditor::outlineColourId,        Palette::panelOutline);
    setColour (juce::TextEditor::focusedOutlineColourId, Palette::accent);
    setColour (juce::CaretComponent::caretColourId,      Palette::text);

    setColour (juce::TextButton::buttonColourId,  Palette::knobBody);
    setColour (juce::TextButton::buttonOnColourId, Palette::accent);
    setColour (juce::TextButton::textColourOffId, Palette::text);
    setColour (juce::TextButton::textColourOnId,  Palette::background);

    setColour (juce::TabbedComponent::backgroundColourId, Palette::background);
    setColour (juce::TabbedComponent::outlineColourId,    juce::Colours::transparentBlack);

    setColour (juce::TooltipWindow::backgroundColourId, Palette::knobBody);
    setColour (juce::TooltipWindow::textColourId,       Palette::text);
    setColour (juce::TooltipWindow::outlineColourId,    Palette::panelOutline);

    // On-screen keyboard: beige keys, warm-black sharps
    setColour (juce::MidiKeyboardComponent::whiteNoteColourId,           juce::Colour (0xffefe7d8));
    setColour (juce::MidiKeyboardComponent::blackNoteColourId,           juce::Colour (0xff1a1815));
    setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId,    juce::Colour (0xffb3a690));
    setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId,      Palette::accentAlt.withAlpha (0.8f));
    setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Palette::accent.withAlpha (0.4f));
    setColour (juce::MidiKeyboardComponent::textLabelColourId,           juce::Colour (0xff4f463c));
    setColour (juce::MidiKeyboardComponent::shadowColourId,              juce::Colours::transparentBlack);
}

//==============================================================================
void SynthLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float pos, float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre  = bounds.getCentre();
    const float trackWidth = juce::jlimit (2.5f, 4.0f, radius * 0.09f);
    const float arcRadius  = radius - trackWidth * 0.5f;
    const auto fill = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const juce::PathStrokeType stroke { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded };

    // Track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (Palette::panelOutline);
    g.strokePath (track, stroke);

    // Value arc (bipolar parameters fill from the centre)
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float zeroAngle = bipolar ? startAngle + (float) slider.valueToProportionOfLength (0.0) * (endAngle - startAngle)
                                    : startAngle;
    const float valueAngle = startAngle + pos * (endAngle - startAngle);

    juce::Path valueArc;
    valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                            juce::jmin (zeroAngle, valueAngle), juce::jmax (zeroAngle, valueAngle), true);
    g.setColour (slider.isEnabled() ? fill : fill.withSaturation (0.0f).withAlpha (0.4f));
    g.strokePath (valueArc, stroke);

    // Body + pointer
    const float bodyRadius = arcRadius - trackWidth - 4.0f;
    g.setColour (slider.isMouseOverOrDragging() ? Palette::knobBody.brighter (0.12f) : Palette::knobBody);
    g.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);

    const auto tip  = centre.getPointOnCircumference (bodyRadius * 0.82f, valueAngle);
    const auto base = centre.getPointOnCircumference (bodyRadius * 0.30f, valueAngle);
    g.setColour (Palette::text);
    g.drawLine ({ base, tip }, 2.5f);
}

void SynthLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float minSliderPos, float maxSliderPos,
                                         juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    // Thin track, filled up to the thumb, round beige thumb
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float cy = bounds.getCentreY();
    const auto track = juce::Rectangle<float> (bounds.getX(), cy - 2.0f, bounds.getWidth(), 4.0f);

    g.setColour (slider.findColour (juce::Slider::backgroundColourId));
    g.fillRoundedRectangle (track, 2.0f);

    g.setColour (slider.findColour (juce::Slider::trackColourId));
    g.fillRoundedRectangle (track.withRight (sliderPos), 2.0f);

    const float thumb = slider.isMouseOverOrDragging() ? 15.0f : 13.0f;
    g.setColour (slider.findColour (juce::Slider::thumbColourId));
    g.fillEllipse (juce::Rectangle<float> (thumb, thumb).withCentre ({ sliderPos, cy }));
}

void SynthLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    // A small "power" pill switch
    auto bounds = button.getLocalBounds().toFloat().reduced (2.0f);
    const float h = juce::jmin (bounds.getHeight(), 18.0f);
    auto pill = juce::Rectangle<float> (h * 1.8f, h).withCentre ({ bounds.getX() + h * 0.9f, bounds.getCentreY() });

    const bool on = button.getToggleState();
    const auto onColour = button.findColour (juce::ToggleButton::tickColourId);

    g.setColour (on ? onColour : Palette::panelOutline.brighter (highlighted ? 0.25f : 0.0f));
    g.fillRoundedRectangle (pill, h * 0.5f);

    const float knob = h - 4.0f;
    const float knobX = on ? pill.getRight() - knob - 2.0f : pill.getX() + 2.0f;
    g.setColour (on ? Palette::background : Palette::textDim);
    g.fillEllipse (knobX, pill.getY() + 2.0f, knob, knob);

    if (button.getButtonText().isNotEmpty())
    {
        g.setColour (Palette::text);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (button.getButtonText(), bounds.withLeft (pill.getRight() + 6.0f), juce::Justification::centredLeft);
    }
}

//==============================================================================
void SynthLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat();
    g.setColour (box.isMouseOver (true) ? Palette::knobBody.brighter (0.1f) : box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 5.0f);

    // Chevron
    juce::Path arrow;
    const float ax = (float) width - 14.0f, ay = (float) height * 0.5f;
    arrow.startNewSubPath (ax - 4.0f, ay - 2.0f);
    arrow.lineTo (ax, ay + 2.0f);
    arrow.lineTo (ax + 4.0f, ay - 2.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font SynthLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::FontOptions (14.5f);
}

juce::Font SynthLookAndFeel::getPopupMenuFont()
{
    return juce::FontOptions (15.0f);
}

void SynthLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                             bool highlighted, bool down)
{
    auto colour = backgroundColour;
    if (down)             colour = colour.brighter (0.2f);
    else if (highlighted) colour = colour.brighter (0.1f);

    g.setColour (colour);
    g.fillRoundedRectangle (button.getLocalBounds().toFloat(), 5.0f);
}

juce::Font SynthLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return juce::FontOptions (14.0f, juce::Font::bold);
}

//==============================================================================
void SynthLookAndFeel::drawTabButton (juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (2.0f, 1.0f);
    const bool front = button.isFrontTab();
    const auto accent = button.getTabBackgroundColour();

    if (front || isMouseOver)
    {
        g.setColour (front ? Palette::panel : Palette::panel.withAlpha (0.5f));
        g.fillRoundedRectangle (bounds, 5.0f);
    }

    if (front)
    {
        g.setColour (accent);
        g.fillRoundedRectangle (bounds.removeFromBottom (3.0f).reduced (8.0f, 0.0f), 1.5f);
    }

    g.setColour (front ? Palette::text : Palette::textDim);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold).withKerningFactor (0.06f));
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
}

void SynthLookAndFeel::drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics&, int, int)
{
    // Intentionally empty: no outline behind the tabs.
}

int SynthLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& button, int)
{
    return juce::GlyphArrangement::getStringWidthInt (juce::FontOptions (14.0f, juce::Font::bold).withKerningFactor (0.06f),
                                                      button.getButtonText()) + 36;
}
