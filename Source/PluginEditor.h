#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "VintageUI.h"

class RackLookAndFeel : public VintageLookAndFeel
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
};

class ReferenzAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ReferenzAudioProcessorEditor (ReferenzAudioProcessor&);
    ~ReferenzAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob { juce::Slider slider; juce::String name; std::unique_ptr<SliderAttachment> attachment; };
    juce::Slider& addKnob (const juce::String& paramID, const juce::String& name, const juce::String& steps);
    juce::ToggleButton& addToggle (const juce::String& paramID, const juce::String& onText, const juce::String& offText, juce::Colour lamp);
    void setParam (const char* id, float value);
    void chooseFile();
    void timerCallback() override;
    void renderPanel();

    // Wellenform der Referenz: Klick legt fest, welche Stelle zur aktuellen Song-Position passt
    struct Wave : public juce::Component
    {
        Wave (VintageLookAndFeel& l, ReferenzAudioProcessor& p) : lnf (l), proc (p) {}
        VintageLookAndFeel& lnf; ReferenzAudioProcessor& proc;
        int slot = 0;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
    };

    // Leuchtanzeige: Lautheit Mix / Referenz / Angleichung
    struct Readout : public juce::Component
    {
        explicit Readout (VintageLookAndFeel& l) : lnf (l) {}
        VintageLookAndFeel& lnf;
        float mix = -200, ref = -200, gain = 0; bool match = true;
        void paint (juce::Graphics&) override;
    };

    ReferenzAudioProcessor& proc;
    RackLookAndFeel lnf;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    juce::TextButton mixButton, refButton, loadButton, clearButton;
    std::array<juce::TextButton, ReferenzAudioProcessor::numSlots> slotButtons;
    std::unique_ptr<juce::FileChooser> chooser;
    Wave wave { lnf, proc };
    Readout readout { lnf };
    juce::String status;

    static constexpr int rackW = 1280, rackH = 560;
    juce::Rectangle<int> nameplate, tagArea, waveBezel, statusArea;
    std::array<juce::Rectangle<int>, 3> sections;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> toggleLabels;
    juce::Image panelImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReferenzAudioProcessorEditor)
};
