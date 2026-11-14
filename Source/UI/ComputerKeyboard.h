#pragma once

#include "SynthLookAndFeel.h"

#include <juce_audio_utils/juce_audio_utils.h>

//==============================================================================
/** Which computer key plays which note. */
namespace ComputerKeys
{
    /** "A" plays C3 = MIDI note 60 (middle C), matching the "C3" label on the
        on-screen keyboard. Change this to move the whole row up or down. */
    inline constexpr int baseNote = 60;

    /** C  C# D  D# E  F  F# G  G# A  A# B */
    inline constexpr char keys[] = "awsedftgyhuj";
    inline constexpr int numKeys = 12;

    /** The (upper-case) computer key for a MIDI note, or an empty string. */
    inline juce::String letterForNote (int midiNote)
    {
        const int index = midiNote - baseNote;

        if (! juce::isPositiveAndBelow (index, numKeys))
            return {};

        return juce::String::charToString ((juce::juce_wchar) keys[index]).toUpperCase();
    }
}

//==============================================================================
/**
    Plays notes from the computer keyboard (A W S E D F T G Y H U J = C3..B3)
    into a MidiKeyboardState.

    Attach it to the editor with addKeyListener(). Key events bubble up from
    the focused component, so notes play whenever something in the editor
    has focus - except a text field, which keeps the keys for typing.
*/
class ComputerKeyboard final : public juce::KeyListener,
                               private juce::Timer
{
public:
    explicit ComputerKeyboard (juce::MidiKeyboardState& stateToUse)
        : state (stateToUse)
    {
        // Safety net: releases notes whose key was let go while the window had no focus.
        startTimerHz (30);
    }

    ~ComputerKeyboard() override
    {
        stopTimer();
        releaseAll();
    }

    bool keyPressed (const juce::KeyPress& key, juce::Component*) override
    {
        // Swallow plain note keys so the host doesn't also react to them.
        for (int i = 0; i < ComputerKeys::numKeys; ++i)
            if (key == juce::KeyPress (ComputerKeys::keys[i]))
                return true;

        return false;
    }

    bool keyStateChanged (bool isKeyDown, juce::Component*) override
    {
        // Notes only start on key-down events that reached us (i.e. a focused text
        // field didn't use them). Key-ups only ever release notes.
        const bool changed = update (isKeyDown);
        return changed || (isKeyDown && anyNoteKeyHeld());
    }

    void releaseAll()
    {
        for (int i = 0; i < ComputerKeys::numKeys; ++i)
        {
            if (held[(size_t) i])
            {
                held[(size_t) i] = false;
                state.noteOff (midiChannel, ComputerKeys::baseNote + i, 0.0f);
            }
        }
    }

private:
    void timerCallback() override   { update (false); }

    static bool shortcutModifierDown()
    {
        const auto mods = juce::ModifierKeys::getCurrentModifiersRealtime();
        return mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown();
    }

    bool anyNoteKeyHeld() const
    {
        for (auto h : held)
            if (h)
                return true;

        return false;
    }

    bool update (bool allowNoteOn)
    {
        const bool blockNoteOn = ! allowNoteOn || shortcutModifierDown();
        bool changed = false;

        for (int i = 0; i < ComputerKeys::numKeys; ++i)
        {
            const bool down = juce::KeyPress::isKeyCurrentlyDown ((int) ComputerKeys::keys[i]);
            auto& isHeld = held[(size_t) i];
            const int note = ComputerKeys::baseNote + i;

            if (down && ! isHeld && ! blockNoteOn)
            {
                isHeld = true;
                state.noteOn (midiChannel, note, velocity);
                changed = true;
            }
            else if (! down && isHeld)
            {
                isHeld = false;
                state.noteOff (midiChannel, note, 0.0f);
                changed = true;
            }
        }

        return changed;
    }

    static constexpr int midiChannel = 1;
    static constexpr float velocity = 0.8f;

    juce::MidiKeyboardState& state;
    std::array<bool, ComputerKeys::numKeys> held {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ComputerKeyboard)
};

//==============================================================================
/**
    On-screen keyboard that fits its whole range to the available width and
    labels the keys you can play from the computer keyboard (A, W, S, ...).
*/
class LabelledKeyboard final : public juce::MidiKeyboardComponent
{
public:
    explicit LabelledKeyboard (juce::MidiKeyboardState& stateToUse)
        : juce::MidiKeyboardComponent (stateToUse, horizontalKeyboard)
    {
        clearKeyMappings();               // computer keys are handled by ComputerKeyboard
        setWantsKeyboardFocus (false);
        setScrollButtonsVisible (false);
        setAvailableRange (36, 96);       // C2 .. C7
        setOctaveForMiddleC (3);          // MIDI 60 is labelled "C3"
        setBlackNoteLengthProportion (0.6f);
    }

    void resized() override
    {
        int numWhiteKeys = 0;

        for (int note = getRangeStart(); note <= getRangeEnd(); ++note)
            if (! juce::MidiMessage::isMidiNoteBlack (note))
                ++numWhiteKeys;

        if (getWidth() > 0)   // setKeyWidth() asserts on zero (e.g. before the first layout)
            setKeyWidth ((float) getWidth() / (float) juce::jmax (1, numWhiteKeys));

        juce::MidiKeyboardComponent::resized();
    }

    void drawWhiteNote (int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour) override
    {
        juce::MidiKeyboardComponent::drawWhiteNote (midiNoteNumber, g, area, isDown, isOver, lineColour, textColour);

        const auto letter = ComputerKeys::letterForNote (midiNoteNumber);

        if (letter.isNotEmpty())
        {
            // Just above the "C3" note-name label, below where the black keys end
            g.setColour (juce::Colour (0xff2e2822));
            g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
            g.drawText (letter, area.withTrimmedBottom (16.0f).removeFromBottom (18.0f), juce::Justification::centred);
        }
    }

    void drawBlackNote (int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                        bool isDown, bool isOver, juce::Colour noteFillColour) override
    {
        juce::MidiKeyboardComponent::drawBlackNote (midiNoteNumber, g, area, isDown, isOver, noteFillColour);

        const auto letter = ComputerKeys::letterForNote (midiNoteNumber);

        if (letter.isNotEmpty())
        {
            g.setColour (Palette::text);
            g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
            g.drawText (letter, area.removeFromBottom (18.0f), juce::Justification::centred);
        }
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabelledKeyboard)
};
