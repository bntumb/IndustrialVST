#include "PluginProcessor.h"
#include "PluginEditor.h"

FoundryAudioProcessor::FoundryAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout FoundryAudioProcessor::createLayout()
{
    using namespace juce;
    using Range = NormalisableRange<float>;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto flt = [&l] (const char* id, const char* name, Range r, float def)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def));
    };

    // --- tuner
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key",
            StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale",
            StringArray { "Major", "Minor", "Dorian", "Mixolydian" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "range", 1 }, "Voice Range",
            StringArray { "Low voice", "Mid voice", "High voice" }, 1));
    l.add (std::make_unique<AudioParameterBool>   (ParameterID { "midi", 1 }, "MIDI Chord Mode", false));
    flt ("retune", "Retune (ms)", Range (1.0f, 100.0f, 0.1f, 0.4f), 1.0f);
    flt ("tune",   "Tune",        Range (0.0f, 1.0f, 0.001f), 1.0f);
    flt ("sub",    "Sub (oct down)", Range (0.0f, 1.0f, 0.001f), 0.5f);
    flt ("stack",  "Stack (fifth up)", Range (0.0f, 1.0f, 0.001f), 0.0f);
    flt ("width",  "Width",       Range (0.0f, 1.0f, 0.001f), 0.4f);

    // --- grit
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "shape", 1 }, "Shape",
            StringArray { "Hard clip", "Wavefold", "Fuzz" }, 1));
    flt ("drive",  "Drive (dB)",  Range (0.0f, 40.0f, 0.1f), 16.0f);
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "bits", 1 }, "Bits", 3, 16, 11));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "rate", 1 }, "Rate Div", 1, 32, 3));
    flt ("ring",   "Ring Mod",    Range (0.0f, 1.0f, 0.001f), 0.2f);
    flt ("ringHz", "Ring Hz",     Range (20.0f, 2000.0f, 1.0f, 0.35f), 140.0f);
    flt ("lowcut", "Low Cut (Hz)",Range (20.0f, 500.0f, 1.0f, 0.4f), 90.0f);
    flt ("bright", "Bright (dB)", Range (-12.0f, 12.0f, 0.1f), 4.0f);
    flt ("gritMix","Grit Mix",    Range (0.0f, 1.0f, 0.001f), 0.85f);

    // --- finish
    flt ("slam",   "Slam",        Range (0.0f, 1.0f, 0.001f), 0.6f);
    flt ("slap",   "Slap",        Range (0.0f, 1.0f, 0.001f), 0.15f);
    flt ("room",   "Room",        Range (0.0f, 1.0f, 0.001f), 0.12f);
    flt ("out",    "Output (dB)", Range (-24.0f, 6.0f, 0.1f), -3.0f);
    return l;
}

bool FoundryAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void FoundryAudioProcessor::prepareToPlay (double sampleRate, int)
{
    chain.prepare (sampleRate);
    lastLatency = chain.latencySamples();
    setLatencySamples (lastLatency);
    heldNotes.fill (false);
}

void FoundryAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
            heldNotes[(size_t) msg.getNoteNumber()] = true;
        else if (msg.isNoteOff())
            heldNotes[(size_t) msg.getNoteNumber()] = false;
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            heldNotes.fill (false);
    }

    auto val = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    FoundryChain::Params p;
    auto& t = p.tuner;
    t.key         = (int) val ("key");
    t.scale       = (int) val ("scale");
    t.range       = (int) val ("range");
    t.midiMode    = val ("midi") > 0.5f;
    t.glideMs     = val ("retune");
    t.tune        = val ("tune");
    t.leadLevel   = 1.0f;
    t.interval[0] = -7;  t.level[0] = val ("sub");      // octave below
    t.interval[1] =  4;  t.level[1] = val ("stack");    // fifth above
    t.interval[2] =  0;  t.level[2] = 0.0f;
    t.interval[3] =  0;  t.level[3] = 0.0f;
    t.spreadCents = 0.0f;
    t.width       = val ("width");

    p.shape    = (int) val ("shape");
    p.driveDb  = val ("drive");
    p.bits     = val ("bits");
    p.rate     = (int) val ("rate");
    p.ring     = val ("ring");
    p.ringHz   = val ("ringHz");
    p.lowcutHz = val ("lowcut");
    p.brightDb = val ("bright");
    p.gritMix  = val ("gritMix");
    p.slam     = val ("slam");
    p.slap     = val ("slap");
    p.room     = val ("room");
    p.outDb    = val ("out");

    const int numIn      = getTotalNumInputChannels();
    const int numSamples = buffer.getNumSamples();

    const float* inL = buffer.getReadPointer (0);
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : inL;
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getWritePointer (1);

    std::array<float, FoundryChain::chunk> mono;

    for (int offset = 0; offset < numSamples; offset += FoundryChain::chunk)
    {
        const int n = std::min (FoundryChain::chunk, numSamples - offset);

        for (int i = 0; i < n; ++i)
            mono[(size_t) i] = 0.5f * (inL[offset + i] + inR[offset + i]);

        // inL/inR may alias outL/outR; the whole chunk was read into `mono` above.
        chain.process (mono.data(), outL + offset, outR + offset, n, p, heldNotes.data());
    }

    if (chain.latencySamples() != lastLatency)                       // voice range changed
    {
        lastLatency = chain.latencySamples();
        setLatencySamples (lastLatency);
    }
}

juce::AudioProcessorEditor* FoundryAudioProcessor::createEditor()
{
    return new FoundryAudioProcessorEditor (*this);
}

void FoundryAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FoundryAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FoundryAudioProcessor();
}
