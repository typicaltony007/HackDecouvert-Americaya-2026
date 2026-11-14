#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

//==============================================================================
/**
    Central place for every parameter ID, choice list and the APVTS layout.
    Both the DSP and UI code refer to parameters through these helpers so IDs
    are never duplicated as string literals.
*/
namespace Params
{
    constexpr int numOscillators = 2;
    constexpr int numModSlots    = 5;

    //==========================================================================
    // Choice lists (the index of each entry is what the DSP sees)
    enum class Waveform      { sine = 0, saw, square, triangle, custom };
    enum class ModMode       { envelope = 0, lfo };
    enum class LfoShape      { sine = 0, triangle, saw, square, sampleAndHold };
    enum class ModDest       { none = 0, amp, pitch, oscAPitch, oscBPitch, oscALevel, oscBLevel, pan };
    enum class DistType      { softClip = 0, overdrive, hardClip, tube, foldback };
    enum class FilterType    { lowPass = 0, highPass, bandPass };

    inline const juce::StringArray waveformNames { "Sine", "Saw", "Square", "Triangle", "Custom" };
    inline const juce::StringArray modModeNames  { "Envelope", "LFO" };
    inline const juce::StringArray lfoShapeNames { "Sine", "Triangle", "Saw", "Square", "Sample & Hold" };
    inline const juce::StringArray modDestNames  { "None", "Amp", "Pitch", "Osc A Pitch", "Osc B Pitch",
                                                   "Osc A Level", "Osc B Level", "Pan" };
    inline const juce::StringArray distTypeNames { "Soft Clip", "Overdrive", "Hard Clip", "Tube", "Foldback" };
    inline const juce::StringArray filterTypeNames { "Low-pass", "High-pass", "Band-pass" };

    //==========================================================================
    // Per-oscillator IDs:  osc(0, "wave") -> "oscA_wave"
    inline juce::String osc (int index, const char* name)
    {
        return juce::String ("osc") + (index == 0 ? "A" : "B") + "_" + name;
    }

    // Per-mod-slot IDs:    mod(0, "attack") -> "mod1_attack"
    inline juce::String mod (int index, const char* name)
    {
        return "mod" + juce::String (index + 1) + "_" + name;
    }

    // Global / effects IDs
    namespace ID
    {
        inline constexpr auto masterGain   = "master_gain";

        inline constexpr auto distOn       = "dist_on";
        inline constexpr auto distType     = "dist_type";
        inline constexpr auto distDrive    = "dist_drive";
        inline constexpr auto distOutput   = "dist_output";
        inline constexpr auto distMix      = "dist_mix";

        inline constexpr auto crushOn      = "crush_on";
        inline constexpr auto crushBits    = "crush_bits";
        inline constexpr auto crushRate    = "crush_downsample";
        inline constexpr auto crushMix     = "crush_mix";

        inline constexpr auto eqOn         = "eq_on";
        inline constexpr auto eqLowFreq    = "eq_low_freq";
        inline constexpr auto eqLowGain    = "eq_low_gain";
        inline constexpr auto eqMidFreq    = "eq_mid_freq";
        inline constexpr auto eqMidGain    = "eq_mid_gain";
        inline constexpr auto eqMidQ       = "eq_mid_q";
        inline constexpr auto eqHighFreq   = "eq_high_freq";
        inline constexpr auto eqHighGain   = "eq_high_gain";

        inline constexpr auto delayOn      = "delay_on";
        inline constexpr auto delayTime    = "delay_time";
        inline constexpr auto delayFeedback= "delay_feedback";
        inline constexpr auto delayMix     = "delay_mix";

        inline constexpr auto reverbOn     = "reverb_on";
        inline constexpr auto reverbSize   = "reverb_size";
        inline constexpr auto reverbDamping= "reverb_damping";
        inline constexpr auto reverbWidth  = "reverb_width";
        inline constexpr auto reverbMix    = "reverb_mix";

        inline constexpr auto filterOn        = "filter_on";
        inline constexpr auto filterType      = "filter_type";
        inline constexpr auto filterCutoff    = "filter_cutoff";
        inline constexpr auto filterResonance = "filter_resonance";
        inline constexpr auto filterMix       = "filter_mix";

        inline constexpr auto chorusOn       = "chorus_on";
        inline constexpr auto chorusRate     = "chorus_rate";
        inline constexpr auto chorusDepth    = "chorus_depth";
        inline constexpr auto chorusDelay    = "chorus_delay";
        inline constexpr auto chorusFeedback = "chorus_feedback";
        inline constexpr auto chorusMix      = "chorus_mix";

        inline constexpr auto compOn        = "comp_on";
        inline constexpr auto compThreshold = "comp_threshold";
        inline constexpr auto compRatio     = "comp_ratio";
        inline constexpr auto compAttack    = "comp_attack";
        inline constexpr auto compRelease   = "comp_release";
        inline constexpr auto compMakeup    = "comp_makeup";
    }

    //==========================================================================
    /** The effects that can be placed in the node chain. The index is used
        for the processing order stored in the plugin state. */
    enum class Effect { distortion = 0, bitcrush, filter, eq, chorus, compressor, delay, reverb, count };
    constexpr int numEffects = (int) Effect::count;

    struct EffectInfo
    {
        const char* name;         // shown on the node
        const char* key;          // stable name stored in the saved chain order
        const char* onParamID;    // bypass switch
        const char* description;  // one-line, beginner-friendly explanation
    };

    inline const std::array<EffectInfo, numEffects> effectInfo {{
        { "Distortion", "distortion", ID::distOn,   "Adds grit and crunch" },
        { "Bitcrush",   "bitcrush",   ID::crushOn,  "Lo-fi, retro video-game sound" },
        { "Filter",     "filter",     ID::filterOn, "Makes the tone darker or brighter" },
        { "EQ",         "eq",         ID::eqOn,     "Boosts or cuts bass, mids and treble" },
        { "Chorus",     "chorus",     ID::chorusOn, "Wide, shimmering doubled sound" },
        { "Compressor", "compressor", ID::compOn,   "Evens out loud and quiet parts" },
        { "Delay",      "delay",      ID::delayOn,  "Repeating echoes" },
        { "Reverb",     "reverb",     ID::reverbOn, "Puts the sound in a room or hall" },
    }};

    /** Non-parameter state: the effect chain order, e.g. "filter,delay,reverb". */
    inline const juce::Identifier fxChainProperty { "fxChain" };

    // Non-parameter state stored in the APVTS ValueTree (custom wave formulas)
    inline juce::Identifier customFormulaProperty (int oscIndex)
    {
        return oscIndex == 0 ? juce::Identifier ("customFormulaA") : juce::Identifier ("customFormulaB");
    }

    inline const juce::String defaultCustomFormula = "sin(x) + 0.5*sin(2*x) + 0.25*sin(3*x)";

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
