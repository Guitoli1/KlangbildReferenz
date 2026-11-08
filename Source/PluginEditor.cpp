#include "PluginEditor.h"

using namespace vintage;

namespace
{
juce::String u8 (const char* t) { return juce::String (juce::CharPointer_UTF8 (t)); }

const juce::Colour lacquer   { 0xff3f4247 };   // Graphit (Mastering)
const juce::Colour silk      { 0xfff2eee4 };
const juce::Colour silkDim   { 0xc8f2eee4 };
const juce::Colour amber     { 0xffffb347 };

juce::String fmtDb (float v, int decimals, bool sign = false)
{
    if (v <= -199.0f) return u8 ("\xe2\x80\x93\xe2\x80\x93,\xe2\x80\x93");
    juce::String s = juce::String (std::abs (v), decimals).replaceCharacter ('.', ',');
    if (v < -0.049f) return u8 ("\xe2\x88\x92") + s;
    if (sign && v > 0.049f) return "+" + s;
    return s;
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> b)
{
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (b.translated (2.0f, 3.0f), 8.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2a28), 0, b.getY(), juce::Colour (0xff121110), 0, b.getBottom(), false));
    g.fillRoundedRectangle (b, 8.0f);
    const auto in = b.reduced (6.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1410), 0, in.getY(), juce::Colour (0xff0a0806), 0, in.getBottom(), false));
    g.fillRoundedRectangle (in, 5.0f);
}
}

//==============================================================================
void RackLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider& slider)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = b.getCentre();
    const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const float knobR = juce::jmax (16.0f, R - 27.0f);
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Skala im Siebdruck
    const auto steps = juce::StringArray::fromTokens (slider.getProperties()["stepLabels"].toString(), "|", "");
    g.setColour (silk);
    if (steps.size() > 1)
    {
        for (int i = 0; i < steps.size(); ++i)
        {
            const float a = startAngle + (float) i / (float) (steps.size() - 1) * (endAngle - startAngle);
            const auto p1 = c.getPointOnCircumference (knobR + 4.0f, a), p2 = c.getPointOnCircumference (knobR + 11.0f, a);
            g.drawLine ({ p1, p2 }, 1.6f);
            if (steps[i].isNotEmpty())
            {
                g.setFont (labelFont (steps[i].length() > 3 ? 10.5f : 12.0f, 0.03f));
                const auto tp = c.getPointOnCircumference (knobR + (steps[i].length() > 3 ? 22.0f : 19.0f), a);
                g.drawText (steps[i], juce::Rectangle<float> (56.0f, 16.0f).withCentre (tp), juce::Justification::centred);
            }
        }
    }
    else
    {
        for (int i = 0; i <= 20; ++i)
        {
            const float a = startAngle + (float) i / 20.0f * (endAngle - startAngle);
            const bool major = i % 2 == 0;
            g.drawLine ({ c.getPointOnCircumference (knobR + 4.0f, a), c.getPointOnCircumference (knobR + (major ? 11.0f : 7.0f), a) }, major ? 1.6f : 1.0f);
            if (major)
            {
                g.setFont (labelFont (12.0f, 0.03f));
                g.drawText (juce::String (i / 2), juce::Rectangle<float> (24.0f, 14.0f).withCentre (c.getPointOnCircumference (knobR + 19.0f, a)), juce::Justification::centred);
            }
        }
    }

    // Schatten
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (c.translated (2.5f, 4.0f)));
    // Rock (geriffelt)
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3836), c.x, c.y - knobR, juce::Colour (0xff0c0b0a), c.x, c.y + knobR, false));
    g.fillEllipse (juce::Rectangle<float> (knobR * 2.0f, knobR * 2.0f).withCentre (c));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    for (int k = 0; k < 48; ++k)
    {
        const float a = (float) k / 48.0f * juce::MathConstants<float>::twoPi;
        g.drawLine ({ c.getPointOnCircumference (knobR * 0.86f, a), c.getPointOnCircumference (knobR - 0.5f, a) }, 1.2f);
    }
    // Kappe
    const float capR = knobR * 0.74f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4744), c.x - capR * 0.5f, c.y - capR, juce::Colour (0xff161514), c.x + capR * 0.4f, c.y + capR, false));
    g.fillEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c));
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.fillEllipse (juce::Rectangle<float> (capR * 1.1f, capR * 0.55f).withCentre (c.translated (-capR * 0.2f, -capR * 0.5f)));
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (c), 1.0f);
    // weiße Zeigerlinie über Kappe und Rock
    g.setColour (juce::Colour (0xfff5f1e6));
    g.drawLine ({ c.getPointOnCircumference (capR * 0.25f, angle), c.getPointOnCircumference (knobR - 1.5f, angle) }, 3.0f);
}

//==============================================================================
void ReferenzAudioProcessorEditor::Wave::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1410), 0, b.getY(), juce::Colour (0xff0a0806), 0, b.getBottom(), false));
    g.fillRoundedRectangle (b, 5.0f);

    const auto d = proc.getSlot (slot);
    if (d == nullptr || d->overview.empty())
    {
        g.setColour (amber.withAlpha (0.6f));
        g.setFont (lnf.labelFont (13.0f, 0.15f));
        g.drawText (proc.loading.load() > 0 ? u8 ("WIRD GELADEN \xe2\x80\xa6") : u8 ("KEIN REFERENZSONG \xc2\xb7 LADEN \xe2\x80\xa6 DR\xc3\x9c" "CKEN"),
                    b, juce::Justification::centred);
        return;
    }

    const auto plot = b.reduced (8.0f, 10.0f);
    const float mid = plot.getCentreY(), h = plot.getHeight() * 0.5f;
    const int cols = (int) d->overview.size();
    const float pos = proc.refPosition.load();
    juce::Path played, rest;
    for (int c = 0; c < cols; ++c)
    {
        const float x = plot.getX() + plot.getWidth() * (float) c / (float) cols;
        const float a = juce::jmin (1.0f, d->overview[(size_t) c]) * h;
        auto& p = (float) c / (float) cols <= pos ? played : rest;
        p.addRectangle (x, mid - a, juce::jmax (1.0f, plot.getWidth() / cols), 2.0f * a);
    }
    g.setColour (amber.withAlpha (0.85f)); g.fillPath (played);
    g.setColour (amber.withAlpha (0.35f)); g.fillPath (rest);
    g.setColour (amber.withAlpha (0.15f));
    g.drawHorizontalLine ((int) mid, plot.getX(), plot.getRight());

    // Zeitmarken (Minuten)
    const double dur = d->audio.getNumSamples() / d->rate;
    g.setFont (lnf.labelFontRegular (10.0f));
    for (int s = 0; s <= (int) dur; s += 30)
    {
        const float x = plot.getX() + plot.getWidth() * (float) (s / dur);
        g.setColour (amber.withAlpha (0.25f));
        g.drawVerticalLine ((int) x, plot.getBottom() - 6.0f, plot.getBottom());
        if (s % 60 == 0)
        {
            g.setColour (amber.withAlpha (0.55f));
            g.drawText (juce::String (s / 60) + ":00", juce::Rectangle<float> (x + 2.0f, plot.getBottom() - 14.0f, 40.0f, 12.0f), juce::Justification::centredLeft);
        }
    }

    // Abspielposition
    const float px = plot.getX() + plot.getWidth() * pos;
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.drawLine (px, plot.getY() - 4.0f, px, plot.getBottom() + 4.0f, 1.5f);
    g.setColour (amber);
    g.setFont (lnf.labelFont (11.0f, 0.12f));
    g.drawText (d->name, b.reduced (10.0f, 6.0f), juce::Justification::topLeft);
    const int secs = (int) (pos * dur);
    g.drawText (juce::String::formatted ("%d:%02d / %d:%02d", secs / 60, secs % 60, (int) dur / 60, (int) dur % 60), b.reduced (10.0f, 6.0f), juce::Justification::topRight);
}

void ReferenzAudioProcessorEditor::Wave::mouseDown (const juce::MouseEvent& e)
{
    const auto d = proc.getSlot (slot);
    if (d == nullptr) return;
    const auto plot = getLocalBounds().toFloat().reduced (8.0f, 10.0f);
    const double frac = juce::jlimit (0.0, 1.0, (double) ((e.position.x - plot.getX()) / plot.getWidth()));
    proc.alignTo (slot, frac * d->audio.getNumSamples() / d->rate);
    repaint();
}

void ReferenzAudioProcessorEditor::Readout::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    drawPanel (g, b);
    const auto in = b.reduced (16.0f, 10.0f);
    const float w = in.getWidth() / 3.0f;
    struct Field { const char* title; juce::String value; const char* unit; };
    const Field f[] = {
        { "MIX", fmtDb (mix, 1), "LUFS" },
        { "REFERENZ", fmtDb (ref, 1), "LUFS" },
        { match ? "ANGLEICHUNG" : "ANGLEICHUNG AUS", ref > -199.0f ? fmtDb (gain, 1, true) : fmtDb (-200, 1), "dB" },
    };
    for (int i = 0; i < 3; ++i)
    {
        const auto r = in.withX (in.getX() + w * (float) i).withWidth (w).reduced (6.0f, 0.0f);
        g.setColour (logo.withAlpha (0.55f));
        g.setFont (lnf.labelFont (10.5f, 0.22f));
        g.drawText (u8 (f[i].title), r.withHeight (14.0f), juce::Justification::centredLeft);
        const auto vr = r.withTrimmedTop (16.0f);
        const auto big = lnf.labelFont (34.0f, 0.02f);
        g.setFont (big);
        g.setColour (amber.withAlpha (0.18f));
        g.drawText (f[i].value, vr.translated (0.0f, 1.0f).expanded (1.0f), juce::Justification::centredLeft);
        g.setColour (amber);
        g.drawText (f[i].value, vr, juce::Justification::centredLeft);
        const float vw = (float) juce::GlyphArrangement::getStringWidthInt (big, f[i].value);
        g.setColour (amber.withAlpha (0.7f));
        g.setFont (lnf.labelFont (11.0f, 0.15f));
        g.drawText (f[i].unit, vr.withTrimmedLeft (vw + 6.0f).withTrimmedBottom (6.0f), juce::Justification::bottomLeft);
        if (i < 2)
        {
            g.setColour (gold.withAlpha (0.22f));
            g.drawVerticalLine ((int) r.getRight() + 6, r.getY() + 4.0f, r.getBottom() - 4.0f);
        }
    }
}

//==============================================================================
ReferenzAudioProcessorEditor::ReferenzAudioProcessorEditor (ReferenzAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    addKnob ("band", "VERGLEICH", u8 ("VOLL|TIEFEN|MITTEN|H\xc3\x96HEN"));
    addKnob ("trim", "REFERENZ-PEGEL (DB)", u8 ("\xe2\x88\x92" "12|\xe2\x88\x92" "6|0|+6|+12"));
    addToggle ("match", "EIN", "AUS", lampGreen);
    addToggle ("sync", "EIN", "AUS", lampAmber);

    mixButton.setButtonText ("MIX");
    refButton.setButtonText ("REFERENZ");
    for (auto* b : { &mixButton, &refButton })
    {
        b->setClickingTogglesState (false);
        addAndMakeVisible (*b);
    }
    mixButton.onClick = [this] { setParam ("ref", 0.0f); };
    refButton.onClick = [this] { setParam ("ref", 1.0f); };

    const char* letters[] = { "A", "B", "C" };
    for (int s = 0; s < ReferenzAudioProcessor::numSlots; ++s)
    {
        slotButtons[(size_t) s].setButtonText (letters[s]);
        slotButtons[(size_t) s].onClick = [this, s] { setParam ("slot", (float) s); };
        addAndMakeVisible (slotButtons[(size_t) s]);
    }
    loadButton.setButtonText (u8 ("LADEN \xe2\x80\xa6"));
    loadButton.getProperties().set ("noLamp", true);
    loadButton.onClick = [this] { chooseFile(); };
    clearButton.setButtonText ("LEEREN");
    clearButton.getProperties().set ("noLamp", true);
    clearButton.onClick = [this] { proc.clearSlot ((int) proc.apvts.getRawParameterValue ("slot")->load()); };
    addAndMakeVisible (loadButton);
    addAndMakeVisible (clearButton);

    addAndMakeVisible (wave);
    readout.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (readout);

    setSize (rackW, rackH);
    startTimerHz (30);
}

ReferenzAudioProcessorEditor::~ReferenzAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ReferenzAudioProcessorEditor::setParam (const char* id, float value)
{
    if (auto* prm = proc.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        prm->endChangeGesture();
    }
}

void ReferenzAudioProcessorEditor::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Referenzsong laden", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.aif;*.aiff;*.mp3;*.m4a;*.flac;*.caf");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f.existsAsFile())
            proc.loadFileAsync ((int) proc.apvts.getRawParameterValue ("slot")->load(), f);
    });
}

juce::Slider& ReferenzAudioProcessorEditor::addKnob (const juce::String& paramID, const juce::String& name, const juce::String& steps)
{
    auto k = std::make_unique<Knob>();
    k->name = name;
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    k->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k->slider.setMouseDragSensitivity (paramID == "band" ? 120 : 240);
    k->slider.getProperties().set ("stepLabels", steps);
    addAndMakeVisible (k->slider);
    k->attachment = std::make_unique<SliderAttachment> (proc.apvts, paramID, k->slider);
    if (auto* rp = proc.apvts.getParameter (paramID))
        k->slider.setDoubleClickReturnValue (true, rp->convertFrom0to1 (rp->getDefaultValue()));
    knobs.push_back (std::move (k));
    return knobs.back()->slider;
}

juce::ToggleButton& ReferenzAudioProcessorEditor::addToggle (const juce::String& paramID, const juce::String& onText, const juce::String& offText, juce::Colour lamp)
{
    auto b = std::make_unique<juce::ToggleButton>();
    b->setButtonText ("x");
    b->getProperties().set ("onText", onText);
    b->getProperties().set ("offText", offText);
    b->getProperties().set ("lamp", (int) lamp.getARGB());
    b->setColour (juce::ToggleButton::textColourId, silk);
    addAndMakeVisible (*b);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (proc.apvts, paramID, *b));
    toggles.push_back (std::move (b));
    return *toggles.back();
}

void ReferenzAudioProcessorEditor::timerCallback()
{
    const bool refOn = proc.apvts.getRawParameterValue ("ref")->load() > 0.5f;
    mixButton.setToggleState (! refOn, juce::dontSendNotification);
    refButton.setToggleState (refOn, juce::dontSendNotification);
    const int slot = (int) proc.apvts.getRawParameterValue ("slot")->load();
    const char* letters[] = { "A", "B", "C" };
    for (int s = 0; s < ReferenzAudioProcessor::numSlots; ++s)
    {
        auto& b = slotButtons[(size_t) s];
        b.setToggleState (s == slot, juce::dontSendNotification);
        const auto d = proc.getSlot (s);
        const juce::String label = juce::String (letters[s]) + (d != nullptr ? u8 (" \xc2\xb7 ") + d->name.substring (0, 14).toUpperCase() : u8 (" \xc2\xb7 LEER"));
        if (b.getButtonText() != label) b.setButtonText (label);
    }
    wave.slot = slot;
    wave.repaint();

    readout.mix = proc.mixLufs.load();
    readout.ref = proc.refLufs.load();
    readout.gain = proc.matchGainDb.load() + proc.apvts.getRawParameterValue ("trim")->load();
    readout.match = proc.apvts.getRawParameterValue ("match")->load() > 0.5f;
    readout.repaint();

    const auto err = proc.getLoadError();
    if (err != status) { status = err; repaint (statusArea); }
}

//==============================================================================
void ReferenzAudioProcessorEditor::renderPanel()
{
    panelImage = juce::Image (juce::Image::ARGB, rackW, rackH, true);
    juce::Graphics g (panelImage);
    g.setGradientFill (juce::ColourGradient (lacquer.brighter (0.12f), 0, 0, lacquer.darker (0.18f), 0, (float) rackH, false));
    g.fillRect (0, 0, rackW, rackH);
    juce::Random rnd (13);
    for (int i = 0; i < 12000; ++i)
    {
        const float r = 1.5f + rnd.nextFloat() * 4.0f;
        const float x = rnd.nextFloat() * rackW, y = rnd.nextFloat() * rackH;
        g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.012f + rnd.nextFloat() * 0.02f));
        g.fillEllipse (x - r, y - r, 2 * r, 2 * r);
    }
    {
        juce::Image::BitmapData data (panelImage, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < rackH; ++y)
            for (int x = 0; x < rackW; ++x)
            {
                auto c = data.getPixelColour (x, y);
                const int n = rnd.nextInt (9) - 4;
                data.setPixelColour (x, y, juce::Colour ((juce::uint8) juce::jlimit (0, 255, c.getRed() + n),
                                                         (juce::uint8) juce::jlimit (0, 255, c.getGreen() + n),
                                                         (juce::uint8) juce::jlimit (0, 255, c.getBlue() + n)));
            }
    }
    g.setColour (juce::Colours::white.withAlpha (0.25f)); g.drawHorizontalLine (0, 0.0f, (float) rackW);
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.drawHorizontalLine (rackH - 1, 0.0f, (float) rackW);

    // Rack-Ohren mit Langlöchern
    for (float ex : { 0.0f, (float) rackW - 56.0f })
    {
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawVerticalLine ((int) (ex < 1.0f ? 56.0f : ex), 0.0f, (float) rackH);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawVerticalLine ((int) (ex < 1.0f ? 57.0f : ex + 1.0f), 0.0f, (float) rackH);
        for (float sy : { 44.0f, (float) rackH * 0.5f, (float) rackH - 44.0f })
        {
            const auto slot = juce::Rectangle<float> (ex + 14.0f, sy - 8.0f, 28.0f, 16.0f);
            g.setColour (juce::Colour (0xff141210));
            g.fillRoundedRectangle (slot, 8.0f);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawRoundedRectangle (slot.translated (0.0f, 1.0f), 8.0f, 1.0f);
            VintageLookAndFeel::drawScrew (g, slot.getCentre().translated (ex < 1.0f ? 3.0f : -3.0f, 0.0f), 6.5f, sy * 0.01f + ex * 0.003f);
        }
    }

    const juce::String titles[] = { u8 ("ABH\xc3\x96REN"), "REFERENZSONG", "OPTIONEN" };
    for (size_t i = 0; i < sections.size(); ++i)
    {
        const auto r = sections[i].toFloat();
        g.setColour (silkDim);
        juce::Path box; box.addRoundedRectangle (r, 5.0f);
        g.strokePath (box, juce::PathStrokeType (1.2f));
        const auto f = lnf.labelFont (13.0f, 0.3f);
        const float tw = (float) juce::GlyphArrangement::getStringWidthInt (f, titles[i]) + 20.0f;
        const auto tr = juce::Rectangle<float> (tw, 16.0f).withCentre ({ r.getCentreX(), r.getY() });
        g.setColour (lacquer);
        g.fillRect (tr);
        g.setColour (silk);
        g.setFont (f);
        g.drawText (titles[i], tr, juce::Justification::centred);
    }

    // Rahmen um die Wellenform
    const auto wb = waveBezel.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (wb.translated (2.0f, 3.0f), 9.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2a28), 0, wb.getY(), juce::Colour (0xff121110), 0, wb.getBottom(), false));
    g.fillRoundedRectangle (wb, 9.0f);

    for (auto& k : knobs)
    {
        const auto sb = k->slider.getBounds().toFloat();
        g.setColour (silk);
        g.setFont (lnf.labelFont (12.0f, 0.16f));
        g.drawText (k->name, juce::Rectangle<float> (sb.getX() - 30.0f, sb.getBottom() - 8.0f, sb.getWidth() + 60.0f, 16.0f), juce::Justification::centred);
    }
    for (auto& [r, t] : toggleLabels)
    {
        g.setColour (silk);
        g.setFont (lnf.labelFont (11.0f, 0.22f));
        g.drawFittedText (t, r, juce::Justification::centredLeft, 2);
    }
    g.setColour (silkDim);
    g.setFont (lnf.labelFont (10.0f, 0.15f));
    g.drawText (u8 ("KLICK IN DIE WELLENFORM: DIESE STELLE PASST ZUR AKTUELLEN SONG-POSITION"),
                juce::Rectangle<float> (wb.getX(), wb.getBottom() + 6.0f, wb.getWidth(), 14.0f), juce::Justification::centred);
    g.drawText (u8 ("PLATZ"), slotButtons[0].getBounds().toFloat().withY ((float) slotButtons[0].getY() - 16.0f).withHeight (14.0f), juce::Justification::centredLeft);

    // Typenschild
    const auto np = nameplate.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (np.translated (1.5f, 2.5f), 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2522), 0, np.getY(), juce::Colour (0xff121010), 0, np.getBottom(), false));
    g.fillRoundedRectangle (np, 4.0f);
    g.setColour (gold.withAlpha (0.7f));
    g.drawRoundedRectangle (np.reduced (3.0f), 3.0f, 1.0f);
    g.setFont (lnf.scriptFont (34.0f));
    g.setColour (logo);
    g.drawText ("Klangbild", np.withTrimmedLeft (12.0f).withHeight (44.0f).translated (0.0f, 2.0f), juce::Justification::centredLeft);
    g.setColour (gold);
    g.setFont (lnf.labelFont (17.0f, 0.45f));
    g.drawText ("REFERENZ", np.withTrimmedLeft (16.0f).withTrimmedTop (44.0f).withHeight (20.0f), juce::Justification::centredLeft);
    g.setColour (logo.withAlpha (0.6f));
    g.setFont (lnf.labelFont (9.0f, 0.2f));
    g.drawText ("A/B-VERGLEICH", np.withTrimmedLeft (16.0f).withTrimmedTop (66.0f).withHeight (14.0f), juce::Justification::centredLeft);

    g.setColour (silkDim);
    g.setFont (lnf.labelFont (10.0f, 0.3f));
    g.drawText (u8 ("TYP KB-AB  \xc2\xb7  NR. 0615"), tagArea.toFloat(), juce::Justification::centredRight);
}

void ReferenzAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (panelImage, 0, 0);
    if (status.isNotEmpty())
    {
        g.setColour (lampRed);
        g.setFont (lnf.labelFont (11.0f, 0.1f));
        g.drawText (status, statusArea.toFloat(), juce::Justification::centredLeft);
    }
}

void ReferenzAudioProcessorEditor::resized()
{
    const int top = 30, bottom = rackH - 34;
    sections[0] = { 72, top, 270, bottom - top };
    sections[1] = { 354, top, 676, bottom - top };
    const int colX = 1042, colW = rackW - 56 - 16 - colX;
    sections[2] = { colX, top + 110, colW, bottom - top - 110 };

    mixButton.setBounds (sections[0].getX() + 25, top + 40, sections[0].getWidth() - 50, 64);
    refButton.setBounds (sections[0].getX() + 25, top + 120, sections[0].getWidth() - 50, 64);
    knobs[0]->slider.setBounds (sections[0].getCentreX() - 80, top + 230, 160, 160);

    const int sx = sections[1].getX() + 16;
    for (int s = 0; s < ReferenzAudioProcessor::numSlots; ++s)
        slotButtons[(size_t) s].setBounds (sx + s * 160, top + 36, 152, 34);
    loadButton.setBounds (sx + 3 * 160 + 4, top + 36, 84, 34);
    clearButton.setBounds (sx + 3 * 160 + 94, top + 36, 72, 34);
    waveBezel = { sx, top + 86, sections[1].getWidth() - 32, 172 };
    wave.setBounds (waveBezel.reduced (8));
    readout.setBounds (sx, top + 300, 470, 104);
    knobs[1]->slider.setBounds (sx + 490, top + 280, 150, 150);
    statusArea = { sx, top + 412, 470, 16 };

    nameplate = { colX, top - 8, colW, 88 };
    toggleLabels.clear();
    toggles[0]->setBounds (colX + 14, top + 156, 130, 40);
    toggleLabels.push_back ({ { colX + 14, top + 124, colW - 20, 30 }, "LAUTHEIT ANGLEICHEN" });
    toggles[1]->setBounds (colX + 14, top + 256, 130, 40);
    toggleLabels.push_back ({ { colX + 14, top + 224, colW - 20, 30 }, "MIT SONG MITLAUFEN" });
    tagArea = { colX - 60, rackH - 28, colW + 60, 16 };

    renderPanel();
}
