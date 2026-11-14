#include "PluginEditor.h"

namespace
{
    constexpr int editorWidth    = 1000;
    constexpr int editorHeight   = 800;
    constexpr int headerHeight   = 58;
    constexpr int modHeight      = 222;
    constexpr int keyboardHeight = 84;
    constexpr int margin         = 10;

    bool isInsideTextEditor (juce::Component* c)
    {
        for (; c != nullptr; c = c->getParentComponent())
            if (dynamic_cast<juce::TextEditor*> (c) != nullptr)
                return true;

        return false;
    }
}

//==============================================================================
SynthAudioProcessorEditor::SynthAudioProcessorEditor (SynthAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      oscillatorsPage (p),
      effectsPanel (p),
      modulationPanel (p.getAPVTS()),
      volume (p.getAPVTS()),
      keyboard (p.getKeyboardState()),
      computerKeyboard (p.getKeyboardState())
{
    juce::ignoreUnused (processorRef);
    setLookAndFeel (&lookAndFeel);

    mainTabs.setTabBarDepth (34);
    mainTabs.setOutline (0);
    mainTabs.addTab ("OSCILLATORS", Palette::accent,    &oscillatorsPage, false);
    mainTabs.addTab ("EFFECTS",     Palette::accentAlt, &effectsPanel,    false);
    addAndMakeVisible (mainTabs);

    addAndMakeVisible (modulationPanel);
    addAndMakeVisible (volume);
    addAndMakeVisible (keyboard);

    // Computer-keyboard playing: the editor takes keyboard focus, and key events
    // from any focused child (except text fields) bubble up to the key listener.
    setWantsKeyboardFocus (true);
    addKeyListener (&computerKeyboard);
    addMouseListener (this, true);

    setResizable (true, true);
    setResizeLimits (900, 720, 1600, 1150);
    setSize (editorWidth, editorHeight);

    // Take focus once the window is on screen, so the keys work without clicking first.
    juce::Timer::callAfterDelay (250, [safeThis = juce::Component::SafePointer<SynthAudioProcessorEditor> (this)]
    {
        if (safeThis != nullptr && safeThis->isShowing())
            safeThis->grabKeyboardFocus();
    });
}

SynthAudioProcessorEditor::~SynthAudioProcessorEditor()
{
    removeMouseListener (this);
    removeKeyListener (&computerKeyboard);
    computerKeyboard.releaseAll();
    setLookAndFeel (nullptr);
}

void SynthAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! isInsideTextEditor (e.originalComponent))
        grabKeyboardFocus();
}

//==============================================================================
void SynthAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (margin + 6, 0);

    const auto titleFont = juce::FontOptions (25.0f, juce::Font::bold).withKerningFactor (0.04f);
    g.setFont (titleFont);
    g.setColour (Palette::text);
    g.drawText ("SYNTH", header, juce::Justification::centredLeft);

    const int titleWidth = juce::GlyphArrangement::getStringWidthInt (titleFont, "SYNTH");
    g.setColour (Palette::accent);
    g.drawText ("MATH", header.withTrimmedLeft (titleWidth + 2), juce::Justification::centredLeft);

    g.setColour (Palette::panelOutline);
    g.drawHorizontalLine (headerHeight - 1, (float) margin, (float) (getWidth() - margin));
}

void SynthAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();

    auto header = r.removeFromTop (headerHeight).reduced (margin + 6, 0);
    volume.setBounds (header.removeFromRight (340).withSizeKeepingCentre (340, 30));

    r.reduce (margin, 0);
    r.removeFromTop (margin);
    r.removeFromBottom (margin);

    keyboard.setBounds (r.removeFromBottom (keyboardHeight));
    r.removeFromBottom (margin);

    modulationPanel.setBounds (r.removeFromBottom (modHeight));
    r.removeFromBottom (margin);

    mainTabs.setBounds (r);
}
