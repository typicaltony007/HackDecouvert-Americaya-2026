#include "ModulationPanel.h"

using namespace Params;

//==============================================================================
ModDisplay::ModDisplay (UI::APVTS& apvts, int slot)
{
    auto get = [&] (const char* name) { return apvts.getRawParameterValue (Params::mod (slot, name)); };

    mode    = get ("mode");    dest  = get ("dest");    amount  = get ("amount");
    attack  = get ("attack");  decay = get ("decay");   sustain = get ("sustain");
    release = get ("release"); rate  = get ("rate");    shape   = get ("shape");

    current = read();
    startTimerHz (20);
}

ModDisplay::Snapshot ModDisplay::read() const
{
    Snapshot s;
    s.mode    = juce::roundToInt (mode->load());
    s.dest    = juce::roundToInt (dest->load());
    s.shape   = juce::roundToInt (shape->load());
    s.amount  = amount->load();
    s.attack  = attack->load();
    s.decay   = decay->load();
    s.sustain = sustain->load();
    s.release = release->load();
    s.rate    = rate->load();
    return s;
}

void ModDisplay::timerCallback()
{
    const auto latest = read();

    if (! (latest == current))
    {
        current = latest;
        repaint();
    }
}

void ModDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::screen);
    g.fillRoundedRectangle (r, 6.0f);

    // Header line: what this slot is and where it goes
    auto header = r.reduced (10.0f, 6.0f).removeFromTop (16.0f);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.setColour (Palette::textDim);
    g.drawText (current.mode == (int) ModMode::envelope ? juce::String ("Envelope")
                                                         : "LFO  " + juce::String (current.rate, 2) + " Hz",
                header, juce::Justification::centredLeft);

    const bool routed = current.dest != (int) ModDest::none;
    g.setFont (juce::FontOptions (13.0f));
    g.setColour (routed ? Palette::text : Palette::textFaint);
    g.drawText (routed ? "to " + modDestNames[current.dest] : juce::String ("not connected"),
                header, juce::Justification::centredRight);

    // Plot with a single faint centre line
    auto plot = r.reduced (12.0f, 10.0f).withTrimmedTop (20.0f);
    g.setColour (Palette::panelOutline.withAlpha (0.7f));
    g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());

    if (current.mode == (int) ModMode::envelope)
        drawEnvelope (g, plot);
    else
        drawLfo (g, plot);
}

void ModDisplay::drawEnvelope (juce::Graphics& g, juce::Rectangle<float> area)
{
    // sqrt-scaled segment widths keep short and long times both readable
    const float a = std::sqrt (current.attack), d = std::sqrt (current.decay), rel = std::sqrt (current.release);
    const float hold = std::sqrt (400.0f);
    const float total = a + d + hold + rel;
    const float w = area.getWidth() / total;

    const float x0 = area.getX();
    const float x1 = x0 + a * w;
    const float x2 = x1 + d * w;
    const float x3 = x2 + hold * w;
    const float x4 = area.getRight();
    const float top = area.getY(), bottom = area.getBottom();
    const float sustainY = bottom - current.sustain * area.getHeight();

    juce::Path p;
    p.startNewSubPath (x0, bottom);
    p.lineTo (x1, top);
    p.quadraticTo (x1 + (x2 - x1) * 0.2f, sustainY, x2, sustainY);
    p.lineTo (x3, sustainY);
    p.quadraticTo (x3 + (x4 - x3) * 0.2f, bottom, x4, bottom);

    juce::Path fill (p);
    fill.lineTo (x0, bottom);
    fill.closeSubPath();

    g.setColour (Palette::modAccent.withAlpha (0.15f));
    g.fillPath (fill);
    g.setColour (Palette::modAccent);
    g.strokePath (p, juce::PathStrokeType (2.0f));

    using Pt = juce::Point<float>;
    for (auto pt : { Pt (x1, top), Pt (x2, sustainY), Pt (x3, sustainY) })
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (pt));
}

void ModDisplay::drawLfo (juce::Graphics& g, juce::Rectangle<float> area)
{
    constexpr int cycles = 2;
    const int numPoints = juce::jmax (2, (int) area.getWidth());
    juce::Random random (42);   // fixed seed -> stable sample & hold preview
    float held = random.nextFloat() * 2.0f - 1.0f;
    int lastStep = 0;

    juce::Path p;

    for (int i = 0; i < numPoints; ++i)
    {
        const double t = (double) i / (double) (numPoints - 1) * cycles;
        const double phase = t - std::floor (t);
        float v = 0.0f;

        switch ((LfoShape) current.shape)
        {
            case LfoShape::sine:          v = (float) std::sin (juce::MathConstants<double>::twoPi * phase); break;
            case LfoShape::triangle:      v = (float) (1.0 - 4.0 * std::abs (phase - 0.5)); break;
            case LfoShape::saw:           v = (float) (2.0 * phase - 1.0); break;
            case LfoShape::square:        v = phase < 0.5 ? 1.0f : -1.0f; break;
            case LfoShape::sampleAndHold:
            {
                const int step = (int) (t * 4.0);   // a few steps per drawn cycle
                if (step != lastStep) { held = random.nextFloat() * 2.0f - 1.0f; lastStep = step; }
                v = held;
                break;
            }
        }

        const float x = area.getX() + (float) i;
        const float y = area.getCentreY() - v * area.getHeight() * 0.5f;

        if (i == 0) p.startNewSubPath (x, y);
        else        p.lineTo (x, y);
    }

    g.setColour (Palette::modAccent);
    g.strokePath (p, juce::PathStrokeType (2.0f));
}

//==============================================================================
ModSlotPage::ModSlotPage (UI::APVTS& apvts, int slot, std::function<void (int, bool)> onModeChanged)
    : slotIndex (slot),
      modeChanged (std::move (onModeChanged)),
      display     (apvts, slot),
      modeChoice  (apvts, mod (slot, "mode"),    "Mode"),
      destChoice  (apvts, mod (slot, "dest"),    "Destination"),
      shapeChoice (apvts, mod (slot, "shape"),   "LFO Shape"),
      amountKnob  (apvts, mod (slot, "amount"),  "Amount",  Palette::modAccent),
      attackKnob  (apvts, mod (slot, "attack"),  "Attack",  Palette::modAccent),
      decayKnob   (apvts, mod (slot, "decay"),   "Decay",   Palette::modAccent),
      sustainKnob (apvts, mod (slot, "sustain"), "Sustain", Palette::modAccent),
      releaseKnob (apvts, mod (slot, "release"), "Release", Palette::modAccent),
      rateKnob    (apvts, mod (slot, "rate"),    "Rate",    Palette::modAccent)
{
    for (auto* c : std::initializer_list<juce::Component*> { &display, &modeChoice, &destChoice, &shapeChoice,
                                                             &amountKnob, &attackKnob, &decayKnob, &sustainKnob,
                                                             &releaseKnob, &rateKnob })
        addAndMakeVisible (c);

    modeChoice.getComboBox().onChange = [this] { updateVisibility(); };
    updateVisibility();
}

void ModSlotPage::updateVisibility()
{
    const bool isLfo = modeChoice.getComboBox().getSelectedId() - 1 == (int) ModMode::lfo;

    for (auto* c : std::initializer_list<juce::Component*> { &attackKnob, &decayKnob, &sustainKnob, &releaseKnob })
        c->setVisible (! isLfo);

    rateKnob.setVisible (isLfo);
    shapeChoice.setVisible (isLfo);

    if (modeChanged)
        modeChanged (slotIndex, isLfo);

    resized();
}

void ModSlotPage::resized()
{
    auto r = getLocalBounds().reduced (4, 8);

    display.setBounds (r.removeFromLeft (juce::jmin (300, r.getWidth() * 2 / 5)));
    r.removeFromLeft (16);

    // Column of selectors
    auto selectors = r.removeFromLeft (150);
    modeChoice.setBounds (selectors.removeFromTop (48));
    selectors.removeFromTop (8);
    destChoice.setBounds (selectors.removeFromTop (48));
    r.removeFromLeft (16);

    // Knobs: amount + (ADSR | rate & shape)
    if (rateKnob.isVisible())
    {
        auto shapeArea = r.removeFromRight (150);
        shapeChoice.setBounds (shapeArea.withSizeKeepingCentre (shapeArea.getWidth(), 48));
        r.removeFromRight (16);
        UI::layoutRow (r, { &amountKnob, &rateKnob });
    }
    else
    {
        UI::layoutRow (r, { &amountKnob, &attackKnob, &decayKnob, &sustainKnob, &releaseKnob });
    }
}

//==============================================================================
ModulationPanel::ModulationPanel (UI::APVTS& apvts)
    : UI::Section ("MODULATION", Palette::modAccent)
{
    setSubtitle ("envelopes and LFOs change the sound over time");

    tabs.setTabBarDepth (30);
    tabs.setOutline (0);
    addAndMakeVisible (tabs);

    for (int i = 0; i < numModSlots; ++i)
    {
        auto onModeChanged = [this] (int slot, bool isLfo)
        {
            if (slot < tabs.getNumTabs())
                tabs.setTabName (slot, tabName (slot, isLfo));
        };

        auto& page = pages.emplace_back (std::make_unique<ModSlotPage> (apvts, i, onModeChanged));

        const bool isLfo = juce::roundToInt (apvts.getRawParameterValue (mod (i, "mode"))->load()) == (int) ModMode::lfo;
        tabs.addTab (tabName (i, isLfo), Palette::modAccent, page.get(), false);
    }
}

juce::String ModulationPanel::tabName (int slotIndex, bool isLfo)
{
    return juce::String (isLfo ? "LFO " : "ENV ") + juce::String (slotIndex + 1);
}

void ModulationPanel::resized()
{
    tabs.setBounds (getContentBounds());
}
