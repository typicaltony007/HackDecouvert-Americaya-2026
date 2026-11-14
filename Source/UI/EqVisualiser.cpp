#include "EqVisualiser.h"

#include <cmath>

using namespace Params;

namespace
{
    // Warm, clearly different band colours: sand / caramel / clay
    const juce::Colour lowColour  { 0xffe3d2b0 };
    const juce::Colour midColour  { 0xffd49a62 };
    const juce::Colour highColour { 0xffc9806b };

    constexpr float handleRadius = 8.0f;
    constexpr float handleHitRadius = 14.0f;
}

//==============================================================================
EqVisualiser::EqVisualiser (SynthAudioProcessor& p)
    : processor (p)
{
    auto& apvts = processor.getAPVTS();

    auto floatParam = [&apvts] (const char* id)
    {
        auto* param = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (id));
        jassert (param != nullptr);   // all EQ band parameters are AudioParameterFloat
        return param;
    };

    bands[low]  = { floatParam (ID::eqLowFreq),  floatParam (ID::eqLowGain),  nullptr,                   lowColour,  "L" };
    bands[mid]  = { floatParam (ID::eqMidFreq),  floatParam (ID::eqMidGain),  floatParam (ID::eqMidQ),  midColour,  "M" };
    bands[high] = { floatParam (ID::eqHighFreq), floatParam (ID::eqHighGain), nullptr,                   highColour, "H" };
    eqOn = apvts.getParameter (ID::eqOn);

    spectrumDb.fill (spectrumFloorDb);
    lastParamValues.fill (-1.0f);

    setTooltip ("Drag a handle to change frequency/gain. Mouse wheel on M changes Q. Double-click resets gain.");
    startTimerHz (30);
}

EqVisualiser::~EqVisualiser()
{
    stopTimer();
}

double EqVisualiser::getSampleRate() const
{
    const double sr = processor.getSampleRate();
    return sr > 0.0 ? sr : 44100.0;
}

//==============================================================================
// Coordinate mapping: log frequency on X, dB on Y
float EqVisualiser::freqToX (double freq) const
{
    const double norm = std::log (freq / minFreq) / std::log (maxFreq / minFreq);
    return plotArea.getX() + (float) norm * plotArea.getWidth();
}

double EqVisualiser::xToFreq (float x) const
{
    const double norm = (double) ((x - plotArea.getX()) / plotArea.getWidth());
    return minFreq * std::pow (maxFreq / minFreq, norm);
}

float EqVisualiser::dbToY (double db) const
{
    return plotArea.getCentreY() - (float) (db / maxDb) * plotArea.getHeight() * 0.5f;
}

double EqVisualiser::yToDb (float y) const
{
    return (double) ((plotArea.getCentreY() - y) / (plotArea.getHeight() * 0.5f)) * maxDb;
}

float EqVisualiser::spectrumDbToY (float db) const
{
    const float norm = juce::jlimit (0.0f, 1.0f, (db - spectrumFloorDb) / -spectrumFloorDb);
    return plotArea.getBottom() - norm * plotArea.getHeight();
}

//==============================================================================
void EqVisualiser::timerCallback()
{
    // Only one effect is shown at a time - don't spend CPU on the FFT while the EQ is hidden.
    if (! isShowing())
        return;

    updateSpectrum();
    updateResponse();
    repaint();
}

void EqVisualiser::updateSpectrum()
{
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    processor.getOutputTap().copyLatest (fftData.data(), fftSize);

    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    // A full-scale sine through a Hann window peaks at roughly fftSize / 4.
    const float normalise = 4.0f / (float) fftSize;
    constexpr float fallPerFrame = 1.5f;   // dB the display falls each frame (smooth release)

    for (size_t i = 0; i < spectrumDb.size(); ++i)
    {
        const float db = juce::Decibels::gainToDecibels (fftData[i] * normalise, spectrumFloorDb);
        spectrumDb[i] = juce::jmax (db, spectrumDb[i] - fallPerFrame);
    }
}

void EqVisualiser::updateResponse()
{
    if (plotArea.getWidth() < 2.0f || plotArea.getHeight() < 2.0f)
        return;

    const double sr = getSampleRate();
    const std::array<float, 8> values { bands[low].freq->get(),  bands[low].gain->get(),
                                        bands[mid].freq->get(),  bands[mid].gain->get(), bands[mid].q->get(),
                                        bands[high].freq->get(), bands[high].gain->get(),
                                        (float) plotArea.getWidth() };

    if (values == lastParamValues && sr == lastSampleRate)
        return;

    lastParamValues = values;
    lastSampleRate = sr;

    // Same filter designs as EffectsChain, so the curve matches what you hear.
    using Coeffs = juce::dsp::IIR::ArrayCoefficients<float>;
    const float nyquistSafe = (float) sr * 0.45f;
    auto gain = [] (float dB) { return juce::Decibels::decibelsToGain (dB); };

    std::array<juce::dsp::IIR::Coefficients<float>, numBands> coeffs;
    coeffs[low]  = Coeffs::makeLowShelf   (sr, juce::jmin (values[0], nyquistSafe), 0.707f,    gain (values[1]));
    coeffs[mid]  = Coeffs::makePeakFilter (sr, juce::jmin (values[2], nyquistSafe), values[4], gain (values[3]));
    coeffs[high] = Coeffs::makeHighShelf  (sr, juce::jmin (values[5], nyquistSafe), 0.707f,    gain (values[6]));

    const auto numPoints = (size_t) juce::jmax (2, (int) plotArea.getWidth());
    totalDb.assign (numPoints, 0.0f);

    for (int b = 0; b < numBands; ++b)
    {
        auto& curve = bandDb[(size_t) b];
        curve.resize (numPoints);

        for (size_t i = 0; i < numPoints; ++i)
        {
            const double freq = xToFreq (plotArea.getX() + (float) i);
            const double mag = freq < sr * 0.5 ? coeffs[(size_t) b].getMagnitudeForFrequency (freq, sr) : 1.0;
            curve[i] = juce::Decibels::gainToDecibels ((float) mag, -100.0f);
            totalDb[i] += curve[i];
        }
    }
}

//==============================================================================
void EqVisualiser::resized()
{
    plotArea = getLocalBounds().toFloat().reduced (32.0f, 12.0f).withTrimmedBottom (8.0f);
    lastParamValues.fill (-1.0f);   // force the curves to be recomputed for the new width
}

void EqVisualiser::paint (juce::Graphics& g)
{
    const bool enabled = eqOn->getValue() >= 0.5f;
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (Palette::screen);
    g.fillRoundedRectangle (bounds, 6.0f);

    //==========================================================================
    // Grid: faint lines, and only a few labels (100 / 1k / 10k, 0 / +-12 dB)
    g.setFont (juce::FontOptions (11.5f));

    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const bool major = (f == 100.0 || f == 1000.0 || f == 10000.0);
        const float x = freqToX (f);
        g.setColour (Palette::panelOutline.withAlpha (major ? 0.8f : 0.35f));
        g.drawVerticalLine ((int) x, plotArea.getY(), plotArea.getBottom());

        if (major)
        {
            g.setColour (Palette::textFaint);
            const auto label = f >= 1000.0 ? juce::String ((int) (f / 1000.0)) + "k" : juce::String ((int) f);
            g.drawText (label, juce::Rectangle<float> (x - 20.0f, plotArea.getBottom() + 2.0f, 40.0f, 13.0f),
                        juce::Justification::centred);
        }
    }

    for (double db : { -12.0, 0.0, 12.0 })
    {
        const float y = dbToY (db);
        g.setColour (Palette::panelOutline.withAlpha (db == 0.0 ? 1.0f : 0.45f));
        g.drawHorizontalLine ((int) y, plotArea.getX(), plotArea.getRight());

        g.setColour (Palette::textFaint);
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db),
                    juce::Rectangle<float> (bounds.getX() + 2.0f, y - 7.0f, 26.0f, 14.0f),
                    juce::Justification::centredRight);
    }

    //==========================================================================
    // Live spectrum (output of the whole synth)
    {
        const double sr = getSampleRate();
        juce::Path spectrum;
        spectrum.startNewSubPath (plotArea.getX(), plotArea.getBottom());

        for (float x = plotArea.getX(); x <= plotArea.getRight(); x += 1.0f)
        {
            // Take the loudest bin in this pixel's frequency range (avoids aliasing at high freqs)
            const double f0 = xToFreq (x), f1 = xToFreq (x + 1.0f);
            const int bin0 = juce::jlimit (0, (int) spectrumDb.size() - 1, (int) (f0 * fftSize / sr));
            const int bin1 = juce::jlimit (bin0, (int) spectrumDb.size() - 1, (int) (f1 * fftSize / sr));

            float db = spectrumFloorDb;
            for (int b = bin0; b <= bin1; ++b)
                db = juce::jmax (db, spectrumDb[(size_t) b]);

            spectrum.lineTo (x, spectrumDbToY (db));
        }

        spectrum.lineTo (plotArea.getRight(), plotArea.getBottom());
        spectrum.closeSubPath();

        g.setColour (Palette::textDim.withAlpha (0.14f));
        g.fillPath (spectrum);
    }

    //==========================================================================
    // EQ curves
    auto makeCurve = [this] (const std::vector<float>& dbValues)
    {
        juce::Path path;
        for (size_t i = 0; i < dbValues.size(); ++i)
        {
            const float x = plotArea.getX() + (float) i;
            const float y = juce::jlimit (plotArea.getY(), plotArea.getBottom(), dbToY (dbValues[i]));
            if (i == 0) path.startNewSubPath (x, y);
            else        path.lineTo (x, y);
        }
        return path;
    };

    const float alpha = enabled ? 1.0f : 0.35f;

    if (! totalDb.empty())
    {
        // Individual bands, filled towards the 0 dB line
        for (int b = 0; b < numBands; ++b)
        {
            auto curve = makeCurve (bandDb[(size_t) b]);
            auto fill = curve;
            fill.lineTo (plotArea.getX() + (float) (bandDb[(size_t) b].size() - 1), dbToY (0.0));
            fill.lineTo (plotArea.getX(), dbToY (0.0));
            fill.closeSubPath();

            const auto c = bands[(size_t) b].colour;
            g.setColour (c.withAlpha (0.12f * alpha));
            g.fillPath (fill);
            g.setColour (c.withAlpha (0.55f * alpha));
            g.strokePath (curve, juce::PathStrokeType (1.0f));
        }

        // Combined response
        g.setColour (Palette::text.withAlpha (alpha));
        g.strokePath (makeCurve (totalDb), juce::PathStrokeType (2.2f, juce::PathStrokeType::curved));
    }

    //==========================================================================
    // Band handles
    for (int b = 0; b < numBands; ++b)
    {
        const auto pos = getHandlePosition (b);
        const auto c = bands[(size_t) b].colour.withAlpha (alpha);
        const bool active = (b == hoveredBand || b == draggingBand);
        const float r = active ? handleRadius + 2.0f : handleRadius;

        g.setColour (c);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pos));
        g.setColour (Palette::screen);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (bands[(size_t) b].name, juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pos),
                    juce::Justification::centred);

        // Readout for the handle being hovered / dragged
        if (active)
        {
            const float freq = bands[(size_t) b].freq->get();
            auto text = (freq >= 1000.0f ? juce::String (freq / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (freq)) + " Hz")
                        + "  " + juce::String (bands[(size_t) b].gain->get(), 1) + " dB";
            if (bands[(size_t) b].q != nullptr)
                text << "  Q " << juce::String (bands[(size_t) b].q->get(), 2);

            g.setColour (Palette::text);
            g.setFont (juce::FontOptions (13.0f));
            g.drawText (text, plotArea.withHeight (16.0f).reduced (4.0f, 0.0f), juce::Justification::centredRight);
        }
    }

    if (! enabled)
    {
        g.setColour (Palette::textDim);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("EQ is switched off", plotArea.withHeight (16.0f).reduced (4.0f, 0.0f), juce::Justification::centredLeft);
    }
}

//==============================================================================
juce::Point<float> EqVisualiser::getHandlePosition (int band) const
{
    const auto& b = bands[(size_t) band];
    return { freqToX (b.freq->get()), dbToY (b.gain->get()) };
}

int EqVisualiser::findHandleAt (juce::Point<float> pos) const
{
    int best = -1;
    float bestDistance = handleHitRadius;

    for (int b = 0; b < numBands; ++b)
    {
        const float d = getHandlePosition (b).getDistanceFrom (pos);
        if (d < bestDistance)
        {
            bestDistance = d;
            best = b;
        }
    }

    return best;
}

void EqVisualiser::setParam (juce::AudioParameterFloat* p, float value)
{
    p->setValueNotifyingHost (p->convertTo0to1 (p->getNormalisableRange().snapToLegalValue (value)));
}

void EqVisualiser::mouseMove (const juce::MouseEvent& e)
{
    const int band = findHandleAt (e.position);
    if (band != hoveredBand)
    {
        hoveredBand = band;
        setMouseCursor (band >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void EqVisualiser::mouseExit (const juce::MouseEvent&)
{
    hoveredBand = -1;
    repaint();
}

void EqVisualiser::mouseDown (const juce::MouseEvent& e)
{
    draggingBand = findHandleAt (e.position);

    if (draggingBand >= 0)
    {
        bands[(size_t) draggingBand].freq->beginChangeGesture();
        bands[(size_t) draggingBand].gain->beginChangeGesture();
    }
}

void EqVisualiser::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingBand < 0)
        return;

    const auto& b = bands[(size_t) draggingBand];
    const float x = juce::jlimit (plotArea.getX(), plotArea.getRight(), e.position.x);
    const float y = juce::jlimit (plotArea.getY(), plotArea.getBottom(), e.position.y);

    setParam (b.freq, (float) xToFreq (x));
    setParam (b.gain, (float) yToDb (y));
    repaint();
}

void EqVisualiser::mouseUp (const juce::MouseEvent&)
{
    if (draggingBand >= 0)
    {
        bands[(size_t) draggingBand].freq->endChangeGesture();
        bands[(size_t) draggingBand].gain->endChangeGesture();
        draggingBand = -1;
    }
}

void EqVisualiser::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int band = findHandleAt (e.position);
    if (band < 0)
        return;

    auto* gainParam = bands[(size_t) band].gain;
    gainParam->beginChangeGesture();
    setParam (gainParam, 0.0f);
    gainParam->endChangeGesture();
}

void EqVisualiser::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int band = findHandleAt (e.position);
    auto* qParam = band >= 0 ? bands[(size_t) band].q : nullptr;

    if (qParam == nullptr)
    {
        Component::mouseWheelMove (e, wheel);
        return;
    }

    // Multiplicative steps feel even across the whole 0.1 .. 10 range
    const float factor = std::pow (2.0f, wheel.deltaY * (wheel.isReversed ? -1.0f : 1.0f));
    qParam->beginChangeGesture();
    setParam (qParam, qParam->get() * factor);
    qParam->endChangeGesture();
}
