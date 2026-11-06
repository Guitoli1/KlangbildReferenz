#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "LoudnessDsp.h"
#include "EQDsp.h"

class ReferenzAudioProcessor : public juce::AudioProcessor
{
public:
    ReferenzAudioProcessor();
    ~ReferenzAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static constexpr int numSlots = 3;

    // geladene Referenz (unveränderlich nach dem Laden, wird als Ganzes ausgetauscht)
    struct RefData
    {
        juce::String name;
        juce::File file;
        juce::AudioBuffer<float> original;    // in der Datei-Samplerate
        double originalRate = 44100.0;
        juce::AudioBuffer<float> audio;       // in der Host-Samplerate
        double rate = 48000.0;
        double lufs = -200.0;                 // integrierte Lautheit der ganzen Datei
        std::vector<float> overview;          // Spitzenwerte für die Wellenform-Anzeige
    };

    // Laden im Hintergrund (Editor) bzw. sofort (Tests)
    void loadFileAsync (int slot, const juce::File& file);
    bool loadFileNow (int slot, const juce::File& file);
    void clearSlot (int slot);
    std::shared_ptr<const RefData> getSlot (int slot) const;
    juce::String getLoadError() const { const juce::SpinLock::ScopedLockType l (lock); return lastError; }
    std::atomic<int> loading { 0 };

    // Startpunkt setzen: Position (Sekunden) in der Referenz, die zur aktuellen Song-Position passen soll
    void alignTo (int slot, double refSeconds);

    juce::AudioProcessorValueTreeState apvts;

    // Anzeigen
    std::atomic<float> mixLufs { -200 }, refLufs { -200 }, matchGainDb { 0 }, refPosition { 0 }, refMix { 0 };
    std::atomic<bool> playing { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    std::shared_ptr<RefData> readFile (const juce::File& file, double targetRate, juce::String& error);
    static void resampleInto (RefData& d, double targetRate);

    mutable juce::SpinLock lock;
    std::array<std::shared_ptr<RefData>, numSlots> slots;
    std::array<std::atomic<double>, numSlots> offsets;      // Referenz-Sekunden − Song-Sekunden
    juce::String lastError;
    juce::ThreadPool pool { 1 };
    juce::AudioFormatManager formats;

    loudness::Meter mixMeter;
    double sr = 48000.0;
    std::atomic<double> freePos { 0.0 };  // Position ohne Song-Synchronisation (Sekunden)
    std::atomic<double> lastSongTime { 0.0 };
    bool wasPlaying = false;
    int shownBand = 0;

    juce::SmoothedValue<float> smRef, smGain, smBandFade;
    std::array<std::array<eq::BiquadState, 4>, 2> bandState;
    eq::Coeffs lp150, hp150, lp5k, hp5k;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReferenzAudioProcessor)
};
