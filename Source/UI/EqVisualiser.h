#pragma once

#include "Controls.h"
#include "../PluginProcessor.h"

#include <juce_dsp/juce_dsp.h>

//==============================================================================
/**
    Frequency display for the 3-band EQ:
      - live spectrum of the plugin output (background)
      - each band's individual response (faint) and the combined EQ curve
      - draggable handles: drag to change a band's frequency and gain,
        mouse-wheel on a handle to change the mid band's Q,
        double-click a handle to reset its gain to 0 dB.
*/
class EqVisualiser final : public juce::Component,
                           public juce::SettableTooltipClient,
                           private juce::Timer
{
public:
    explicit EqVisualiser (SynthAudioProcessor&);
    ~EqVisualiser() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    enum Band { low = 0, mid, high, numBands };

    struct BandParams
    {
        juce::AudioParameterFloat* freq = nullptr;
        juce::AudioParameterFloat* gain = nullptr;
        juce::AudioParameterFloat* q    = nullptr;   // mid band only
        juce::Colour colour;
        const char* name = "";
    };

    void timerCallback() override;
    void updateSpectrum();
    void updateResponse();
    double getSampleRate() const;

    // Coordinate mapping (inside plotArea)
    float freqToX (double freq) const;
    double xToFreq (float x) const;
    float dbToY (double db) const;
    double yToDb (float y) const;
    float spectrumDbToY (float db) const;

    juce::Point<float> getHandlePosition (int band) const;
    int findHandleAt (juce::Point<float> pos) const;
    static void setParam (juce::AudioParameterFloat* p, float value);

    SynthAudioProcessor& processor;
    std::array<BandParams, numBands> bands;
    juce::RangedAudioParameter* eqOn = nullptr;

    juce::Rectangle<float> plotArea;

    // Filter response, one value per pixel column
    std::array<std::vector<float>, numBands> bandDb;
    std::vector<float> totalDb;
    std::array<float, 8> lastParamValues {};
    double lastSampleRate = 0.0;

    // Spectrum analyser
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize / 2> spectrumDb {};

    int hoveredBand = -1;
    int draggingBand = -1;

    static constexpr double minFreq = 20.0, maxFreq = 20000.0;
    static constexpr double maxDb = 20.0;                         // response curve range: +/- maxDb
    static constexpr float spectrumFloorDb = -90.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqVisualiser)
};
