#include "PluginEditor.h"

static juce::String noteName (float midi)
{
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int n = (int) std::lround (midi);
    return juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
}

// ------------------------------------------------------------------ look and feel
FoundryLookAndFeel::FoundryLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,       juce::Colour (0xffd8d8d8));
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,               juce::Colour (0xff9a9a9a));
    setColour (juce::ComboBox::backgroundColourId,      juce::Colour (0xff262626));
    setColour (juce::ComboBox::textColourId,            juce::Colour (0xfff0f0f0));
    setColour (juce::ComboBox::outlineColourId,         juce::Colour (0xff3a3a3a));
    setColour (juce::ComboBox::arrowColourId,           accent());
    setColour (juce::PopupMenu::backgroundColourId,     juce::Colour (0xff202020));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent());
    setColour (juce::ToggleButton::textColourId,        juce::Colour (0xffd8d8d8));
    setColour (juce::ToggleButton::tickColourId,        accent());
}

void FoundryLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float pos, float startAngle, float endAngle, juce::Slider&)
{
    auto area   = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
    const float radius = std::min (area.getWidth(), area.getHeight()) / 2.0f;
    const auto  c      = area.getCentre();
    const float angle  = startAngle + pos * (endAngle - startAngle);

    g.setColour (juce::Colour (0xff111111));
    g.fillEllipse (c.x - radius + 4, c.y - radius + 4, (radius - 4) * 2, (radius - 4) * 2);
    g.setColour (juce::Colour (0xff2c2c2c));
    g.drawEllipse (c.x - radius + 4, c.y - radius + 4, (radius - 4) * 2, (radius - 4) * 2, 1.5f);

    juce::Path track;
    track.addCentredArc (c.x, c.y, radius - 1, radius - 1, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff333333));
    g.strokePath (track, juce::PathStrokeType (3.0f));

    juce::Path value;
    value.addCentredArc (c.x, c.y, radius - 1, radius - 1, 0.0f, startAngle, angle, true);
    g.setColour (accent());
    g.strokePath (value, juce::PathStrokeType (3.0f));

    const float inner = radius * 0.25f, outer = radius - 8.0f;
    g.setColour (juce::Colour (0xfff0f0f0));
    g.drawLine (c.x + inner * std::sin (angle), c.y - inner * std::cos (angle),
                c.x + outer * std::sin (angle), c.y - outer * std::cos (angle), 2.2f);
}

// ------------------------------------------------------------------ editor
FoundryAudioProcessorEditor::FoundryAudioProcessorEditor (FoundryAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&look);

    addSelector (keyBox,   { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, "key", keyAtt);
    addSelector (scaleBox, { "Major", "Minor", "Dorian", "Mixolydian" }, "scale", scaleAtt);
    addSelector (rangeBox, { "Low voice (40 ms)", "Mid voice (26 ms)", "High voice (17 ms)" }, "range", rangeAtt);
    addSelector (shapeBox, { "Hard clip", "Wavefold", "Fuzz" }, "shape", shapeAtt);
    addAndMakeVisible (midiButton);
    midiAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "midi", midiButton);

    pitchLabel.setJustificationType (juce::Justification::centredRight);
    pitchLabel.setColour (juce::Label::textColourId, FoundryLookAndFeel::accent());
    pitchLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    addAndMakeVisible (pitchLabel);

    Section tune { "TUNE", {}, {} };
    tune.knobs = { addKnob ("retune", "Retune"), addKnob ("tune", "Tune"), addKnob ("sub", "Sub"),
                   addKnob ("stack", "Stack"),   addKnob ("width", "Width") };
    Section grit { "GRIT", {}, {} };
    grit.knobs = { addKnob ("drive", "Drive"),   addKnob ("bits", "Bits"),     addKnob ("rate", "Rate"),
                   addKnob ("ring", "Ring"),     addKnob ("ringHz", "Ring Hz"),addKnob ("lowcut", "Low Cut"),
                   addKnob ("bright", "Bright"), addKnob ("gritMix", "Mix") };
    Section finish { "FINISH", {}, {} };
    finish.knobs = { addKnob ("slam", "Slam"), addKnob ("slap", "Slap"), addKnob ("room", "Room"),
                     addKnob ("out", "Output") };
    sections = { tune, grit, finish };

    setSize (880, 520);
    startTimerHz (20);
}

FoundryAudioProcessorEditor::~FoundryAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void FoundryAudioProcessorEditor::addSelector (juce::ComboBox& box, const juce::StringArray& items,
                                               const juce::String& paramId,
                                               std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att)
{
    box.addItemList (items, 1);
    addAndMakeVisible (box);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, paramId, box);
}

FoundryAudioProcessorEditor::Knob* FoundryAudioProcessorEditor::addKnob (const juce::String& paramId,
                                                                          const juce::String& text)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    k->label.setText (text.toUpperCase(), juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, paramId, k->slider);
    addAndMakeVisible (k->slider);
    addAndMakeVisible (k->label);

    auto* raw = k.get();
    knobs.push_back (std::move (k));
    return raw;
}

void FoundryAudioProcessorEditor::timerCallback()
{
    const float in = processor.getInputMidi(), lead = processor.getLeadMidi();
    pitchLabel.setText (in < 0.0f ? juce::String ("--")
                                  : noteName (in) + "  >  " + noteName (lead),
                        juce::dontSendNotification);
}

void FoundryAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0e0e0e));

    g.setColour (FoundryLookAndFeel::accent());
    g.fillRect (0, 0, getWidth(), 4);

    g.setColour (juce::Colour (0xfff0f0f0));
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("FOUNDRY", 18, 14, 240, 34, juce::Justification::centredLeft);

    for (const auto& s : sections)
    {
        g.setColour (FoundryLookAndFeel::panel());
        g.fillRoundedRectangle (s.bounds.toFloat(), 6.0f);
        g.setColour (FoundryLookAndFeel::accent());
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (s.title, s.bounds.getX() + 12, s.bounds.getY() + 6, 200, 16, juce::Justification::centredLeft);
    }
}

void FoundryAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (14, 0);

    auto head = area.removeFromTop (54).withTrimmedTop (10);
    head.removeFromLeft (240);
    pitchLabel.setBounds (head.removeFromRight (150));
    midiButton.setBounds (head.removeFromRight (120).withSizeKeepingCentre (110, 28));
    shapeBox.setBounds (head.removeFromRight (120).reduced (4, 6));
    rangeBox.setBounds (head.removeFromRight (160).reduced (4, 6));
    scaleBox.setBounds (head.removeFromRight (110).reduced (4, 6));
    keyBox.setBounds   (head.removeFromRight (70).reduced (4, 6));

    area.removeFromTop (6);
    const int rowH = (area.getHeight() - 12 - 2 * 10) / 3;

    for (auto& s : sections)
    {
        s.bounds = area.removeFromTop (rowH);
        area.removeFromTop (10);

        auto inner = s.bounds.reduced (8, 0).withTrimmedTop (22);
        const int cols = 8;                                  // all sections share one column width
        const int cw   = inner.getWidth() / cols;
        const int n    = (int) s.knobs.size();
        auto row = inner.withSizeKeepingCentre (cw * n, inner.getHeight());

        for (auto* k : s.knobs)
        {
            auto cell = row.removeFromLeft (cw).reduced (2, 0);
            k->label.setBounds (cell.removeFromTop (16));
            k->slider.setBounds (cell);
        }
    }
}
