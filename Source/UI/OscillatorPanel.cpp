#include "OscillatorPanel.h"

using Params::Waveform;

//==============================================================================
WaveformDisplay::WaveformDisplay (SynthAudioProcessor& p, int index)
    : processor (p), oscIndex (index)
{
    refreshCustomTable();
}

void WaveformDisplay::setWaveform (Waveform newWaveform)
{
    waveform = newWaveform;
    repaint();
}

void WaveformDisplay::refreshCustomTable()
{
    processor.getCustomTable (oscIndex).copyTable (customCopy);
    repaint();
}

float WaveformDisplay::valueAt (double phase) const
{
    switch (waveform)
    {
        case Waveform::sine:     return (float) std::sin (juce::MathConstants<double>::twoPi * phase);
        case Waveform::saw:      return (float) (2.0 * phase - 1.0);
        case Waveform::square:   return phase < 0.5 ? 1.0f : -1.0f;
        case Waveform::triangle: return (float) (1.0 - 4.0 * std::abs (std::fmod (phase + 0.25, 1.0) - 0.5));
        case Waveform::custom:
        {
            const auto i = juce::jlimit<size_t> (0, customCopy.size() - 1, (size_t) (phase * (double) customCopy.size()));
            return customCopy[i];
        }
    }

    return 0.0f;
}

void WaveformDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (r, 6.0f);

    r = r.reduced (8.0f, 10.0f);
    g.setColour (Palette::panelOutline);
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

    juce::Path path;
    const int numPoints = juce::jmax (2, (int) r.getWidth());

    for (int i = 0; i < numPoints; ++i)
    {
        const double phase = (double) i / (double) (numPoints - 1) * 0.9999;
        const float x = r.getX() + (float) i;
        const float y = r.getCentreY() - valueAt (phase) * r.getHeight() * 0.5f;

        if (i == 0) path.startNewSubPath (x, y);
        else        path.lineTo (x, y);
    }

    g.setColour (Palette::accent);
    g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
OscillatorPanel::OscillatorPanel (SynthAudioProcessor& p, int index)
    : UI::Section (index == 0 ? "OSCILLATOR A" : "OSCILLATOR B", Palette::accent),
      processor (p),
      oscIndex (index),
      waveChoice (p.getAPVTS(), Params::osc (index, "wave"),   "Waveform"),
      levelKnob  (p.getAPVTS(), Params::osc (index, "level"),  "Level"),
      octaveKnob (p.getAPVTS(), Params::osc (index, "octave"), "Octave"),
      semiKnob   (p.getAPVTS(), Params::osc (index, "semi"),   "Semitone"),
      fineKnob   (p.getAPVTS(), Params::osc (index, "fine"),   "Fine Tune"),
      display    (p, index)
{
    for (auto* c : std::initializer_list<juce::Component*> { &waveChoice, &levelKnob, &octaveKnob, &semiKnob,
                                                             &fineKnob, &display })
        addAndMakeVisible (c);

    // The formula row is added but only shown for the "Custom" waveform.
    for (auto* c : std::initializer_list<juce::Component*> { &formulaLabel, &formulaEditor, &applyButton, &statusLabel })
        addChildComponent (c);

    waveChoice.getComboBox().onChange = [this] { waveformChanged(); };

    formulaLabel.setFont (juce::FontOptions (14.0f, juce::Font::italic));
    formulaLabel.setColour (juce::Label::textColourId, Palette::textDim);

    formulaEditor.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::plain));
    formulaEditor.setIndents (8, 5);
    formulaEditor.setText (processor.getCustomFormula (oscIndex), false);
    formulaEditor.setTooltip ("Draw your own wave with maths. x goes from 0 to 2*pi over one cycle.\n"
                              "Functions: sin cos tan abs min max pow sqrt exp log tanh sign floor frac "
                              "saw square tri noise. Constants: pi e");
    // Enter applies the formula; Enter/Escape also hand the keys back to note playing.
    formulaEditor.onReturnKey = [this] { applyFormula(); returnFocusToEditor(); };
    formulaEditor.onEscapeKey = [this] { returnFocusToEditor(); };

    applyButton.onClick = [this] { applyFormula(); };

    statusLabel.setFont (juce::FontOptions (13.0f));
    showStatus ("Try sin(x) + 0.3*sin(5*x), then press Enter", false);

    waveformChanged();
    processor.getAPVTS().state.addListener (this);
}

OscillatorPanel::~OscillatorPanel()
{
    processor.getAPVTS().state.removeListener (this);
    cancelPendingUpdate();
}

void OscillatorPanel::returnFocusToEditor()
{
    if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
        editor->grabKeyboardFocus();
}

bool OscillatorPanel::isCustom()
{
    return waveChoice.getComboBox().getSelectedId() - 1 == (int) Waveform::custom;
}

void OscillatorPanel::waveformChanged()
{
    display.setWaveform ((Waveform) (waveChoice.getComboBox().getSelectedId() - 1));

    const bool custom = isCustom();
    for (auto* c : std::initializer_list<juce::Component*> { &formulaLabel, &formulaEditor, &applyButton, &statusLabel })
        c->setVisible (custom);

    resized();
}

void OscillatorPanel::applyFormula()
{
    const auto error = processor.setCustomFormula (oscIndex, formulaEditor.getText());

    if (error.isEmpty())
    {
        showStatus ("Formula applied", false);
        display.refreshCustomTable();
    }
    else
    {
        showStatus (error, true);
    }
}

void OscillatorPanel::showStatus (const juce::String& message, bool isError)
{
    statusLabel.setText (message, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, isError ? Palette::error : Palette::textDim);
}

void OscillatorPanel::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& property)
{
    if (property == Params::customFormulaProperty (oscIndex))
        triggerAsyncUpdate();
}

void OscillatorPanel::valueTreeRedirected (juce::ValueTree&)
{
    triggerAsyncUpdate();
}

void OscillatorPanel::handleAsyncUpdate()
{
    const auto formula = processor.getCustomFormula (oscIndex);

    if (! formulaEditor.hasKeyboardFocus (true) && formulaEditor.getText() != formula)
        formulaEditor.setText (formula, false);

    display.refreshCustomTable();
}

void OscillatorPanel::resized()
{
    auto r = getContentBounds();

    // Bottom (Custom only): formula editor + status line
    if (isCustom())
    {
        statusLabel.setBounds (r.removeFromBottom (20));

        auto formulaRow = r.removeFromBottom (30);
        formulaLabel.setBounds (formulaRow.removeFromLeft (52));
        applyButton.setBounds (formulaRow.removeFromRight (70));
        formulaRow.removeFromRight (8);
        formulaEditor.setBounds (formulaRow);

        r.removeFromBottom (8);
    }

    // Waveform selector + preview on the left, knobs on the right
    auto left = r.removeFromLeft (juce::jmin (170, r.getWidth() / 3));
    waveChoice.setBounds (left.removeFromTop (48));
    left.removeFromTop (6);
    display.setBounds (left);

    r.removeFromLeft (12);
    UI::layoutRow (r, { &levelKnob, &octaveKnob, &semiKnob, &fineKnob });
}
