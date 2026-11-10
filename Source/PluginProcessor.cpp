#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ScaledEditor.h"

namespace
{
using APF = juce::AudioParameterFloat;
using Attr = juce::AudioParameterFloatAttributes;
constexpr double maxSeconds = 15.0 * 60.0;   // längere Dateien werden gekürzt
}

juce::AudioProcessorValueTreeState::ParameterLayout ReferenzAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "ref", 1 }, "Referenz hören", false));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "slot", 1 }, "Referenz-Platz", juce::StringArray { "A", "B", "C" }, 0));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "match", 1 }, "Lautheit angleichen", true));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "sync", 1 }, "Mit Song mitlaufen", true));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "band", 1 }, "Vergleich",
                                                         juce::StringArray { "Voll", "Tiefen", "Mitten", juce::String (juce::CharPointer_UTF8 ("H\xc3\xb6hen")) }, 0));
    l.add (std::make_unique<APF> (juce::ParameterID { "trim", 1 }, "Referenz-Pegel", juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                  Attr().withStringFromValueFunction ([] (float v, int) { return (v > 0.04f ? "+" : "") + juce::String (v, 1).replaceCharacter ('.', ',') + " dB"; })));
    return l;
}

ReferenzAudioProcessor::ReferenzAudioProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "KlangbildReferenz", createLayout())
{
    formats.registerBasicFormats();
    for (auto& o : offsets) o.store (0.0);
}

ReferenzAudioProcessor::~ReferenzAudioProcessor()
{
    pool.removeAllJobs (true, 5000);
}

bool ReferenzAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
void ReferenzAudioProcessor::resampleInto (RefData& d, double targetRate)
{
    d.rate = targetRate;
    const double ratio = d.originalRate / targetRate;
    const int inLen = d.original.getNumSamples();
    const int outLen = (int) std::floor ((double) inLen / ratio);
    d.audio.setSize (2, juce::jmax (1, outLen));
    for (int c = 0; c < 2; ++c)
    {
        if (juce::exactlyEqual (ratio, 1.0)) { d.audio.copyFrom (c, 0, d.original, c, 0, juce::jmin (inLen, outLen)); continue; }
        juce::LagrangeInterpolator interp;
        interp.process (ratio, d.original.getReadPointer (c), d.audio.getWritePointer (c), outLen, inLen, 0);
    }
}

std::shared_ptr<ReferenzAudioProcessor::RefData> ReferenzAudioProcessor::readFile (const juce::File& file, double targetRate, juce::String& error)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr) { error = "Datei kann nicht gelesen werden: " + file.getFileName(); return nullptr; }
    const auto len = (int) juce::jmin ((juce::int64) (maxSeconds * reader->sampleRate), reader->lengthInSamples);
    if (len <= 0) { error = "Datei ist leer: " + file.getFileName(); return nullptr; }

    auto d = std::make_shared<RefData>();
    d->name = file.getFileNameWithoutExtension();
    d->file = file;
    d->originalRate = reader->sampleRate;
    d->original.setSize (2, len);
    reader->read (&d->original, 0, len, 0, true, true);
    if (reader->numChannels == 1)
        d->original.copyFrom (1, 0, d->original, 0, 0, len);
    resampleInto (*d, targetRate);

    // integrierte Lautheit der ganzen Datei (BS.1770, mit Gating)
    loudness::Meter m;
    m.prepare (targetRate, 2);
    const float* l = d->audio.getReadPointer (0);
    const float* r = d->audio.getReadPointer (1);
    for (int i = 0; i < d->audio.getNumSamples(); ++i)
        m.process (l[i], r[i], true);
    d->lufs = m.integrated;

    // Übersicht für die Wellenform (1200 Spalten)
    constexpr int cols = 1200;
    d->overview.assign (cols, 0.0f);
    const int n = d->audio.getNumSamples();
    for (int c = 0; c < cols; ++c)
    {
        const int a = (int) ((juce::int64) n * c / cols), b = (int) ((juce::int64) n * (c + 1) / cols);
        float pk = 0;
        for (int i = a; i < b; ++i) pk = juce::jmax (pk, std::abs (l[i]), std::abs (r[i]));
        d->overview[(size_t) c] = pk;
    }
    return d;
}

bool ReferenzAudioProcessor::loadFileNow (int slot, const juce::File& file)
{
    juce::String err;
    auto d = readFile (file, sr, err);
    const juce::SpinLock::ScopedLockType l (lock);
    if (d == nullptr) { lastError = err; return false; }
    slots[(size_t) slot] = d;
    lastError.clear();
    return true;
}

void ReferenzAudioProcessor::loadFileAsync (int slot, const juce::File& file)
{
    ++loading;
    pool.addJob ([this, slot, file]
    {
        loadFileNow (slot, file);
        --loading;
    });
}

void ReferenzAudioProcessor::clearSlot (int slot)
{
    const juce::SpinLock::ScopedLockType l (lock);
    slots[(size_t) slot].reset();
    offsets[(size_t) slot].store (0.0);
}

std::shared_ptr<const ReferenzAudioProcessor::RefData> ReferenzAudioProcessor::getSlot (int slot) const
{
    const juce::SpinLock::ScopedLockType l (lock);
    return slots[(size_t) slot];
}

void ReferenzAudioProcessor::alignTo (int slot, double refSeconds)
{
    // beides setzen: mit Song-Position (Logic) gilt der Versatz, ohne (frei / Standalone) die freie Position
    offsets[(size_t) slot].store (refSeconds - lastSongTime.load());
    freePos = refSeconds;
}

//==============================================================================
void ReferenzAudioProcessor::prepareToPlay (double sampleRate, int)
{
    const bool rateChanged = ! juce::exactlyEqual (sampleRate, sr);
    sr = sampleRate;
    mixMeter.prepare (sr, 2);
    lp150 = eq::makeLowPass (sr, 150.0, 0.70710678);  hp150 = eq::makeHighPass (sr, 150.0, 0.70710678);
    lp5k  = eq::makeLowPass (sr, 5000.0, 0.70710678); hp5k  = eq::makeHighPass (sr, 5000.0, 0.70710678);
    for (auto& ch : bandState) for (auto& s : ch) s.reset();
    shownBand = (int) apvts.getRawParameterValue ("band")->load();
    smRef.reset (sr, 0.03);      smRef.setCurrentAndTargetValue (apvts.getRawParameterValue ("ref")->load() > 0.5f ? 1.0f : 0.0f);
    smGain.reset (sr, 0.5);      smGain.setCurrentAndTargetValue (1.0f);
    smBandFade.reset (sr, 0.008); smBandFade.setCurrentAndTargetValue (1.0f);

    // geladene Referenzen an die neue Samplerate anpassen (im Hintergrund)
    if (rateChanged)
        for (int s = 0; s < numSlots; ++s)
            if (auto d = getSlot (s))
            {
                ++loading;
                pool.addJob ([this, s, d, rate = sr]
                {
                    auto copy = std::make_shared<RefData> (*d);
                    resampleInto (*copy, rate);
                    { const juce::SpinLock::ScopedLockType l (lock); if (slots[(size_t) s] == d) slots[(size_t) s] = copy; }
                    --loading;
                });
            }
}

void ReferenzAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2)
        return;

    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const int slot = juce::jlimit (0, numSlots - 1, (int) v ("slot"));
    const bool sync = v ("sync") > 0.5f;

    // Song-Position und Transport
    bool isPlaying = true; double songTime = lastSongTime.load(); bool hasTime = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            isPlaying = pos->getIsPlaying();
            if (auto t = pos->getTimeInSeconds()) { songTime = *t; hasTime = true; }
        }
    if (isPlaying && ! wasPlaying && hasTime && songTime <= 0.05)
        mixMeter.resetIntegrated();          // Wiedergabe ab Songanfang: Mix-Lautheit neu messen
    wasPlaying = isPlaying;
    lastSongTime.store (songTime);
    playing.store (isPlaying);

    std::shared_ptr<RefData> ref;
    {
        const juce::SpinLock::ScopedTryLockType l (lock);
        if (l.isLocked()) ref = slots[(size_t) slot];
    }

    // Lautheitsangleich: Referenz auf die Lautheit des Mixes bringen
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    for (int i = 0; i < n; ++i)
        mixMeter.process (L[i], R[i], isPlaying);
    const double mixL = mixMeter.stepsGated >= 30 && mixMeter.integrated > -199.0 ? mixMeter.integrated : mixMeter.shortTerm;
    mixLufs.store ((float) mixL);
    refLufs.store (ref != nullptr ? (float) ref->lufs : -200.0f);
    float matchDb = matchGainDb.load();
    if (v ("match") < 0.5f) matchDb = 0.0f;
    else if (ref != nullptr && ref->lufs > -199.0 && mixL > -70.0)
        matchDb = (float) juce::jlimit (-30.0, 12.0, mixL - ref->lufs);
    matchGainDb.store (matchDb);
    smGain.setTargetValue (juce::Decibels::decibelsToGain (matchDb + v ("trim")));
    smRef.setTargetValue (v ("ref") > 0.5f && ref != nullptr ? 1.0f : 0.0f);

    // Bandauswahl wechseln: kurz ausblenden
    const int wantBand = (int) v ("band");
    if (wantBand != shownBand)
    {
        if (! smBandFade.isSmoothing() && smBandFade.getCurrentValue() <= 0.0f)
        {
            shownBand = wantBand;
            for (auto& ch : bandState) for (auto& s : ch) s.reset();
            smBandFade.setTargetValue (1.0f);
        }
        else smBandFade.setTargetValue (0.0f);
    }
    else smBandFade.setTargetValue (1.0f);

    // Referenz-Position
    const int len = ref != nullptr ? ref->audio.getNumSamples() : 0;
    double pos = 0.0;
    if (ref != nullptr)
        pos = sync && hasTime ? (songTime + offsets[(size_t) slot].load()) : freePos.load();
    const float* rl = ref != nullptr ? ref->audio.getReadPointer (0) : nullptr;
    const float* rr = ref != nullptr ? ref->audio.getReadPointer (1) : nullptr;
    const double rate = ref != nullptr ? ref->rate : sr;
    juce::int64 idx = (juce::int64) std::floor (pos * rate);

    for (int i = 0; i < n; ++i)
    {
        float al = L[i], ar = R[i];
        const float x = smRef.getNextValue();
        const float g = smGain.getNextValue();
        if (rl != nullptr && x > 0.0f)
        {
            float bl = 0, br = 0;
            if (isPlaying)
            {
                const auto k = (int) (((idx + i) % len + len) % len);
                bl = rl[k] * g; br = rr[k] * g;
            }
            al += (bl - al) * x;
            ar += (br - ar) * x;
        }

        // Vergleich nur eines Frequenzbereichs (für Mix und Referenz gleich)
        if (shownBand != 0)
        {
            float* io[2] = { &al, &ar };
            for (int c = 0; c < 2; ++c)
            {
                auto& st = bandState[(size_t) c];
                float y = *io[c];
                if (shownBand == 1)      { y = st[1].process (lp150, st[0].process (lp150, y)); }
                else if (shownBand == 2) { y = st[1].process (hp150, st[0].process (hp150, y)); y = st[3].process (lp5k, st[2].process (lp5k, y)); }
                else                     { y = st[1].process (hp5k, st[0].process (hp5k, y)); }
                *io[c] = y;
            }
        }
        const float f = smBandFade.getNextValue();
        L[i] = al * f; R[i] = ar * f;
    }

    if (ref != nullptr && isPlaying && ! (sync && hasTime))
        freePos.store (freePos.load() + n / sr);
    if (ref != nullptr && len > 0)
    {
        const double p = std::fmod ((double) (idx + n) / rate, (double) len / rate);
        refPosition.store ((float) ((p < 0 ? p + (double) len / rate : p) / ((double) len / rate)));
    }
    refMix.store (smRef.getCurrentValue());
}

//==============================================================================
void ReferenzAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    for (int s = 0; s < numSlots; ++s)
    {
        const auto d = getSlot (s);
        state.setProperty ("file" + juce::String (s), d != nullptr ? d->file.getFullPathName() : juce::String(), nullptr);
        state.setProperty ("offset" + juce::String (s), offsets[(size_t) s].load(), nullptr);
    }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ReferenzAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    auto tree = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (tree);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->convertTo0to1 (rp->convertFrom0to1 (rp->getValue())));
    for (int s = 0; s < numSlots; ++s)
    {
        offsets[(size_t) s].store ((double) tree.getProperty ("offset" + juce::String (s), 0.0));
        const juce::File f (tree.getProperty ("file" + juce::String (s), "").toString());
        if (f.existsAsFile()) loadFileAsync (s, f);
    }
}

juce::AudioProcessorEditor* ReferenzAudioProcessor::createEditor() { return new kb::ScaledEditor (*this, new ReferenzAudioProcessorEditor (*this)); }

#ifndef KB_EMBEDDED
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReferenzAudioProcessor(); }
#endif
