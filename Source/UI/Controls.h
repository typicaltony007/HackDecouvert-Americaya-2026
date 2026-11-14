#pragma once

#include "SynthLookAndFeel.h"
#include "../Parameters.h"

#include <juce_audio_processors/juce_audio_processors.h>

/** Small reusable widgets bound to APVTS parameters. */
namespace UI
{
    using APVTS = juce::AudioProcessorValueTreeState;

    //==========================================================================
    /**
        Rotary knob with its name underneath. While the mouse is over the knob
        (or it is being dragged) the name is replaced by the current value, so
        the panel stays uncluttered. Double-click resets to the default.
    */
    class Knob final : public juce::Component
    {
    public:
        Knob (APVTS& apvts, const juce::String& paramID, const juce::String& captionText,
              juce::Colour colour = Palette::accent)
            : caption (captionText)
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
            addAndMakeVisible (slider);

            label.setJustificationType (juce::Justification::centred);
            label.setFont (juce::FontOptions (13.5f, juce::Font::bold));
            label.setMinimumHorizontalScale (0.7f);
            label.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (label);

            if (auto* param = apvts.getParameter (paramID))
                unit = param->getLabel();

            attachment = std::make_unique<APVTS::SliderAttachment> (apvts, paramID, slider);

            slider.onValueChange = [this] { refreshLabel(); };
            slider.onDragStart   = [this] { dragging = true;  refreshLabel(); };
            slider.onDragEnd     = [this] { dragging = false; refreshLabel(); };
            slider.addMouseListener (this, false);

            refreshLabel();
        }

        ~Knob() override { slider.removeMouseListener (this); }

        void mouseEnter (const juce::MouseEvent&) override { refreshLabel(); }
        void mouseExit  (const juce::MouseEvent&) override { refreshLabel(); }

        void resized() override
        {
            auto r = getLocalBounds();
            label.setBounds (r.removeFromBottom (20));
            slider.setBounds (r);
        }

        juce::Slider& getSlider() noexcept { return slider; }

    private:
        void refreshLabel()
        {
            const bool showValue = dragging || isMouseOver (true);

            if (showValue)
            {
                auto text = slider.getTextFromValue (slider.getValue());
                if (unit.isNotEmpty())
                    text << (unit.startsWithChar (':') ? "" : " ") << unit;

                label.setText (text, juce::dontSendNotification);
            }
            else
            {
                label.setText (caption, juce::dontSendNotification);
            }

            label.setColour (juce::Label::textColourId, showValue ? Palette::text : Palette::textDim);
        }

        juce::String caption, unit;
        juce::Label label;
        juce::Slider slider;
        bool dragging = false;
        std::unique_ptr<APVTS::SliderAttachment> attachment;
    };

    //==========================================================================
    /** Combo box filled with a choice parameter's options, with a caption above. */
    class Choice final : public juce::Component
    {
    public:
        Choice (APVTS& apvts, const juce::String& paramID, const juce::String& caption)
        {
            label.setText (caption, juce::dontSendNotification);
            label.setFont (juce::FontOptions (13.5f, juce::Font::bold));
            label.setColour (juce::Label::textColourId, Palette::textDim);
            label.setBorderSize ({ 0, 2, 0, 0 });
            addAndMakeVisible (label);

            if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramID)))
                box.addItemList (choiceParam->choices, 1);

            addAndMakeVisible (box);
            attachment = std::make_unique<APVTS::ComboBoxAttachment> (apvts, paramID, box);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            label.setBounds (r.removeFromTop (20));
            box.setBounds (r.removeFromTop (28));
        }

        juce::ComboBox& getComboBox() noexcept { return box; }

    private:
        juce::Label label;
        juce::ComboBox box;
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
    };

    //==========================================================================
    /** On/off pill switch bound to a bool parameter. */
    class PowerSwitch final : public juce::Component
    {
    public:
        PowerSwitch (APVTS& apvts, const juce::String& paramID, juce::Colour colour)
        {
            button.setColour (juce::ToggleButton::tickColourId, colour);
            button.setTooltip ("Switch this effect on or off");
            addAndMakeVisible (button);
            attachment = std::make_unique<APVTS::ButtonAttachment> (apvts, paramID, button);
        }

        void resized() override { button.setBounds (getLocalBounds()); }

        juce::ToggleButton& getButton() noexcept { return button; }

    private:
        juce::ToggleButton button;
        std::unique_ptr<APVTS::ButtonAttachment> attachment;
    };

    //==========================================================================
    /**
        Flat rounded panel with a title (in the section's accent colour) and an
        optional dimmer subtitle. Subclasses lay out inside getContentBounds().
    */
    class Section : public juce::Component
    {
    public:
        Section (juce::String sectionTitle, juce::Colour accentColour)
            : title (std::move (sectionTitle)), accent (accentColour) {}

        void setSubtitle (const juce::String& newSubtitle)
        {
            subtitle = newSubtitle;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (Palette::panel);
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

            auto titleArea = getTitleBounds().reduced (14, 0);
            const auto titleFont = juce::FontOptions (13.5f, juce::Font::bold).withKerningFactor (0.08f);

            g.setColour (accent);
            g.setFont (titleFont);
            g.drawText (title, titleArea, juce::Justification::centredLeft);

            if (subtitle.isNotEmpty())
            {
                const int titleWidth = juce::GlyphArrangement::getStringWidthInt (titleFont, title);
                g.setColour (Palette::textFaint);
                g.setFont (juce::FontOptions (13.0f));
                g.drawText (subtitle, titleArea.withTrimmedLeft (titleWidth + 12), juce::Justification::centredLeft);
            }
        }

    protected:
        static constexpr int titleHeight = 32;

        juce::Rectangle<int> getTitleBounds() const   { return getLocalBounds().removeFromTop (titleHeight); }
        juce::Rectangle<int> getContentBounds() const { return getLocalBounds().withTrimmedTop (titleHeight).reduced (12, 6); }

        juce::String title, subtitle;
        juce::Colour accent;
    };

    //==========================================================================
    /** Lays out components left-to-right in equal-width columns. */
    inline void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items, int gap = 8)
    {
        if (items.size() == 0)
            return;

        const int n = (int) items.size();
        const int w = (area.getWidth() - gap * (n - 1)) / n;

        for (auto* c : items)
        {
            c->setBounds (area.removeFromLeft (w));
            area.removeFromLeft (gap);
        }
    }
}
