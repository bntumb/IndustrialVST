#pragma once

#include "PluginProcessor.h"

// Dark, industrial look: flat knobs with an orange value arc.
class FoundryLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FoundryLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    static juce::Colour accent() { return juce::Colour (0xffff4a1c); }
    static juce::Colour panel()  { return juce::Colour (0xff1a1a1a); }
};

class FoundryAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit FoundryAudioProcessorEditor (FoundryAudioProcessor&);
    ~FoundryAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    struct Section
    {
        juce::String title;
        std::vector<Knob*> knobs;
        juce::Rectangle<int> bounds;
    };

    Knob* addKnob (const juce::String& paramId, const juce::String& text);
    void  addSelector (juce::ComboBox&, const juce::StringArray& items, const juce::String& paramId,
                       std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>&);
    void  timerCallback() override;

    FoundryAudioProcessor& processor;
    FoundryLookAndFeel look;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Section> sections;

    juce::ComboBox keyBox, scaleBox, rangeBox, shapeBox;
    juce::ToggleButton midiButton { "MIDI chords" };
    juce::Label pitchLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt, rangeAtt, shapeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   midiAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FoundryAudioProcessorEditor)
};
