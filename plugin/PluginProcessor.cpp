// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{

constexpr int kMidiChannel = 1;
constexpr int kTicksPerQuarter = 480;
constexpr int kTicksPerStep = kTicksPerQuarter / 4;

juce::ValueTree progressionToTree (const cs::Progression& p)
{
    juce::ValueTree t ("Progression");
    t.setProperty ("tonic", p.tonic, nullptr);
    t.setProperty ("scale", p.scale, nullptr);
    t.setProperty ("bars", p.bars, nullptr);
    t.setProperty ("snap", p.snap, nullptr);
    t.setProperty ("adventure", p.adventure, nullptr);
    t.setProperty ("syncopation", p.syncopation, nullptr);
    t.setProperty ("bassMotion", p.bassMotion, nullptr);
    t.setProperty ("mood", p.mood, nullptr);
    t.setProperty ("lockRhythm", p.locks.rhythm, nullptr);
    t.setProperty ("lockRoots", p.locks.roots, nullptr);
    t.setProperty ("lockColors", p.locks.colors, nullptr);
    t.setProperty ("lockVoicing", p.locks.voicing, nullptr);

    for (int i = 0; i < p.numSlots; ++i)
    {
        const cs::Slot& s = p.slots[(size_t) i];
        juce::ValueTree st ("Slot");
        st.setProperty ("start", s.start, nullptr);
        st.setProperty ("root", s.chord.root, nullptr);
        st.setProperty ("type", juce::String (cs::chordTypes()[(size_t) s.chord.type].suffix), nullptr);
        st.setProperty ("bassInt", s.chord.bassInt, nullptr);
        st.setProperty ("locked", s.locked, nullptr);
        st.setProperty ("targetB", s.targetB, nullptr);
        st.setProperty ("targetT", s.targetT, nullptr);
        st.setProperty ("targetE", s.targetE, nullptr);
        st.setProperty ("velocity", s.voicing.velocity, nullptr);

        juce::StringArray notes;
        for (int j = 0; j < s.voicing.n; ++j)
            notes.add (juce::String (s.voicing.notes[(size_t) j]));
        st.setProperty ("notes", notes.joinIntoString (" "), nullptr);
        t.appendChild (st, nullptr);
    }
    return t;
}

int chordTypeFromSuffix (const juce::String& suffix)
{
    for (int i = 0; i < cs::numChordTypes(); ++i)
        if (suffix == cs::chordTypes()[(size_t) i].suffix)
            return i;
    return 0;
}

/** Rebuilds a progression from saved state, clamping everything: a session
    file is outside input and must never be able to put the model in a bad
    state. */
bool progressionFromTree (const juce::ValueTree& t, cs::Progression& p)
{
    if (! t.hasType ("Progression") || t.getNumChildren() == 0)
        return false;

    p = cs::Progression();
    p.tonic = cs::mod12 ((int) t.getProperty ("tonic", 9));
    p.scale = juce::jlimit (0, cs::numScales() - 1, (int) t.getProperty ("scale", 1));
    p.bars = juce::jlimit (1, cs::kMaxBars, (int) t.getProperty ("bars", 4));
    const int snap = (int) t.getProperty ("snap", 2);
    p.snap = (snap == 1 || snap == 2 || snap == 4) ? snap : 2;
    p.adventure = juce::jlimit (0.0f, 1.0f, (float) t.getProperty ("adventure", 0.3f));
    p.syncopation = juce::jlimit (0.0f, 1.0f, (float) t.getProperty ("syncopation", 0.3f));
    p.bassMotion = juce::jlimit (0.0f, 1.0f, (float) t.getProperty ("bassMotion", 0.0f));
    p.mood = juce::jlimit (-1, (int) cs::moods().size() - 1, (int) t.getProperty ("mood", -1));
    p.locks.rhythm = (bool) t.getProperty ("lockRhythm", false);
    p.locks.roots = (bool) t.getProperty ("lockRoots", false);
    p.locks.colors = (bool) t.getProperty ("lockColors", false);
    p.locks.voicing = (bool) t.getProperty ("lockVoicing", false);

    p.numSlots = juce::jlimit (1, std::min (cs::kMaxSlots, p.lengthSteps()), t.getNumChildren());
    cs::Rng tmp (1);
    for (int i = 0; i < p.numSlots; ++i)
    {
        const auto st = t.getChild (i);
        cs::Slot& s = p.slots[(size_t) i];
        s.start = juce::jlimit (0, p.lengthSteps() - 1, (int) st.getProperty ("start", 0));
        s.chord.root = cs::mod12 ((int) st.getProperty ("root", 0));
        s.chord.type = chordTypeFromSuffix (st.getProperty ("type", "").toString());
        s.chord.bassInt = juce::jlimit (0, 11, (int) st.getProperty ("bassInt", 0));
        cs::sanitizeBass (s.chord);
        s.locked = (bool) st.getProperty ("locked", false);
        s.targetB = juce::jlimit (0.0f, 1.0f, (float) st.getProperty ("targetB", 0.5f));
        s.targetT = juce::jlimit (0.0f, 1.0f, (float) st.getProperty ("targetT", 0.4f));
        s.targetE = juce::jlimit (0.0f, 1.0f, (float) st.getProperty ("targetE", 0.6f));

        juce::StringArray notes;
        notes.addTokens (st.getProperty ("notes", "").toString(), " ", "");
        notes.removeEmptyStrings();
        s.voicing.n = 0;
        for (const auto& n : notes)
            if (s.voicing.n < cs::kMaxNotes)
                s.voicing.notes[(size_t) s.voicing.n++] = juce::jlimit (0, 127, n.getIntValue());
        std::sort (s.voicing.notes.begin(), s.voicing.notes.begin() + s.voicing.n);
        s.voicing.velocity = juce::jlimit (1, 127, (int) st.getProperty ("velocity", 96));

        if (s.voicing.n == 0)
            s.voicing = cs::makeVoicing (s.chord, nullptr, cs::centreForBrightness (s.targetB), s.targetE, tmp, 1.0f, 0.0f);
    }
    cs::sanitizeRhythm (p);
    cs::updateActuals (p);
    return true;
}

} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ChordSlotsProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "gate", 1 }, "Gate",
                                                             juce::NormalisableRange<float> (0.1f, 1.0f, 0.01f), 1.0f));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "synthOn", 1 }, "Preview Synth", true));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "synthVol", 1 }, "Preview Volume",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.7f));
    return layout;
}

ChordSlotsProcessor::ChordSlotsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Params", createLayout()),
      rng ((unsigned) juce::Random::getSystemRandom().nextInt())
{
    gateParam = apvts.getRawParameterValue ("gate");
    synthOnParam = apvts.getRawParameterValue ("synthOn");
    synthVolParam = apvts.getRawParameterValue ("synthVol");

    model = cs::makeDefaultProgression (rng);
    publish();
    play = pendingPlay;
}

//==============================================================================
cs::Progression ChordSlotsProcessor::getProgression() const
{
    const juce::ScopedLock sl (modelLock);
    return model;
}

void ChordSlotsProcessor::pushHistory()
{
    history.push_back (model);
    if (history.size() > 64)
        history.erase (history.begin());
}

void ChordSlotsProcessor::beginGesture()
{
    const juce::ScopedLock sl (modelLock);
    pushHistory();
}

bool ChordSlotsProcessor::canUndo() const
{
    const juce::ScopedLock sl (modelLock);
    return ! history.empty();
}

bool ChordSlotsProcessor::undo()
{
    {
        const juce::ScopedLock sl (modelLock);
        if (history.empty())
            return false;
        model = history.back();
        history.pop_back();
        publish();
    }
    sendChangeMessage();
    return true;
}

void ChordSlotsProcessor::publish()
{
    const cs::PlayData d = cs::toPlayData (model);
    const juce::SpinLock::ScopedLockType sl (playLock);
    pendingPlay = d;
    playDirty.store (true);
}

//==============================================================================
void ChordSlotsProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    synth.prepare (sampleRate);
    heldCount = 0;
    heldSlot = currentSlot = -1;
    auditionCount = 0;
    wasPlaying = false;
}

void ChordSlotsProcessor::releaseResources()
{
    synth.allNotesOff();
}

bool ChordSlotsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void ChordSlotsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    midi.clear(); // incoming MIDI is not used (yet); we only emit our own

    if (playDirty.load())
    {
        const juce::SpinLock::ScopedTryLockType tl (playLock);
        if (tl.isLocked())
        {
            play = pendingPlay;
            playDirty.store (false);
        }
    }

    double bpm = 120.0;
    bool hostPlaying = false;
    bool havePpq = false;
    double ppq = 0.0;
    if (auto* ph = getPlayHead())
        if (const auto pos = ph->getPosition())
        {
            if (const auto b = pos->getBpm())
                bpm = *b;
            hostPlaying = pos->getIsPlaying();
            if (const auto q = pos->getPpqPosition())
            {
                ppq = *q;
                havePpq = true;
            }
        }
    if (! (bpm >= 20.0 && bpm <= 999.0))
        bpm = 120.0;
    lastBpm.store (bpm);

    const double len = (double) juce::jmax (1, play.lengthSteps);
    const double stepsPerSample = bpm / 60.0 * 4.0 / sampleRate;
    const float gate = juce::jlimit (0.1f, 1.0f, gateParam->load());
    const bool playing = (hostPlaying || previewOn.load()) && play.numSlots > 0 && numSamples > 0;

    auto releaseHeld = [&] (int sample)
    {
        for (int i = 0; i < heldCount; ++i)
            midi.addEvent (juce::MidiMessage::noteOff (kMidiChannel, held[i]), sample);
        heldCount = 0;
        heldSlot = -1;
    };

    auto releaseAudition = [&] (int sample)
    {
        for (int i = 0; i < auditionCount; ++i)
            midi.addEvent (juce::MidiMessage::noteOff (kMidiChannel, auditionNotes[i]), sample);
        auditionCount = 0;
    };

    auto startSlot = [&] (int slot, int sample)
    {
        releaseHeld (sample);
        const cs::PlaySlot& s = play.slots[slot];
        for (int i = 0; i < s.n && i < cs::kMaxNotes; ++i)
        {
            held[heldCount++] = s.notes[i];
            midi.addEvent (juce::MidiMessage::noteOn (kMidiChannel, s.notes[i], (juce::uint8) juce::jlimit (1, 127, s.velocity)), sample);
        }
        heldSlot = slot;
        currentSlot = slot;
    };

    auto offStep = [&] (int slot)
    {
        const cs::PlaySlot& s = play.slots[slot];
        return (double) s.start + (double) (s.end - s.start) * (double) gate;
    };

    if (playing)
    {
        releaseAudition (0);
        auditionRequest.store (-1);

        double startStep = expectedStep;
        bool jumped = ! wasPlaying;

        if (hostPlaying && havePpq)
        {
            double hostStep = std::fmod (ppq * 4.0, len);
            if (hostStep < 0.0)
                hostStep += len;

            double diff = hostStep - std::fmod (expectedStep, len);
            diff -= len * std::round (diff / len);
            if (jumped || std::abs (diff) > 0.25)
            {
                jumped = true; // transport was relocated: restart from the host position
                startStep = hostStep;
            }
            else if (std::abs (diff) > 0.02)
            {
                startStep = hostStep; // tempo drift: resync quietly
            }
        }
        else if (jumped)
        {
            startStep = 0.0;
        }

        startStep = std::fmod (startStep, len);
        if (jumped)
        {
            releaseHeld (0);
            currentSlot = -1;
        }

        // Make sure the chord under the playhead is the one sounding. This
        // covers starting mid-slot, relocations and edits made while playing.
        bool startedAtZero = false;
        const int slotNow = play.slotAt (startStep);
        if (slotNow >= 0 && slotNow != currentSlot)
        {
            if (startStep < offStep (slotNow))
            {
                startSlot (slotNow, 0);
                startedAtZero = true;
            }
            else
            {
                releaseHeld (0);
                currentSlot = slotNow;
            }
        }

        struct Event
        {
            int sample;
            bool on;
            int slot;
        };
        Event events[cs::kMaxSlots * 4];
        int numEvents = 0;
        const double endStep = startStep + numSamples * stepsPerSample;

        for (int k = 0; k < 2; ++k)
            for (int i = 0; i < play.numSlots; ++i)
            {
                const double tOn = (double) play.slots[i].start + k * len;
                if (tOn >= startStep && tOn < endStep)
                {
                    const int sample = juce::jlimit (0, numSamples - 1, (int) ((tOn - startStep) / stepsPerSample));
                    if (! (sample == 0 && startedAtZero && i == currentSlot))
                        events[numEvents++] = { sample, true, i };
                }

                if (gate < 0.999f)
                {
                    const double tOff = offStep (i) + k * len;
                    if (tOff > startStep && tOff < endStep)
                        events[numEvents++] = { juce::jlimit (0, numSamples - 1, (int) ((tOff - startStep) / stepsPerSample)), false, i };
                }
            }

        std::sort (events, events + numEvents, [] (const Event& a, const Event& b)
        {
            return a.sample != b.sample ? a.sample < b.sample : (! a.on && b.on);
        });

        for (int e = 0; e < numEvents; ++e)
        {
            if (events[e].on)
                startSlot (events[e].slot, events[e].sample);
            else if (heldSlot == events[e].slot)
                releaseHeld (events[e].sample);
        }

        expectedStep = std::fmod (endStep, len);
        uiStep.store (expectedStep);
    }
    else
    {
        if (wasPlaying)
        {
            releaseHeld (0);
            currentSlot = -1;
        }
        uiStep.store (-1.0);

        const int request = auditionRequest.exchange (-1);
        if (request >= 0 && request < play.numSlots && numSamples > 0)
        {
            releaseAudition (0);
            const cs::PlaySlot& s = play.slots[request];
            for (int i = 0; i < s.n && i < cs::kMaxNotes; ++i)
            {
                auditionNotes[auditionCount++] = s.notes[i];
                midi.addEvent (juce::MidiMessage::noteOn (kMidiChannel, s.notes[i], (juce::uint8) juce::jlimit (1, 127, s.velocity)), 0);
            }
            auditionSamplesLeft = (int) (0.9 * sampleRate);
        }
        else if (auditionCount > 0)
        {
            if (auditionSamplesLeft <= numSamples)
                releaseAudition (juce::jlimit (0, juce::jmax (0, numSamples - 1), auditionSamplesLeft));
            else
                auditionSamplesLeft -= numSamples;
        }
    }
    wasPlaying = playing;

    const float gain = synthOnParam->load() > 0.5f ? synthVolParam->load() : 0.0f;
    synth.render (buffer, midi, gain);
}

//==============================================================================
juce::String ChordSlotsProcessor::describeProgression() const
{
    const cs::Progression p = getProgression();
    juce::StringArray names;
    for (int i = 0; i < p.numSlots; ++i)
        names.add (juce::String::fromUTF8 (cs::chordName (p.slots[(size_t) i].chord, p.tonic, p.scale).c_str()));
    return names.joinIntoString (" ");
}

juce::MidiFile ChordSlotsProcessor::createMidiFile() const
{
    const cs::Progression p = getProgression();
    const float gate = juce::jlimit (0.1f, 1.0f, gateParam->load());

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / lastBpm.load())), 0.0);

    for (int i = 0; i < p.numSlots; ++i)
    {
        const cs::Slot& s = p.slots[(size_t) i];
        const double on = (double) s.start * kTicksPerStep;
        const double off = on + juce::jmax (1.0, (double) (p.slotEnd (i) - s.start) * kTicksPerStep * (double) gate - 1.0);
        for (int j = 0; j < s.voicing.n; ++j)
        {
            const int note = s.voicing.notes[(size_t) j];
            seq.addEvent (juce::MidiMessage::noteOn (kMidiChannel, note, (juce::uint8) s.voicing.velocity), on);
            seq.addEvent (juce::MidiMessage::noteOff (kMidiChannel, note), off);
        }
    }
    seq.addEvent (juce::MidiMessage::endOfTrack(), (double) p.lengthSteps() * kTicksPerStep);
    seq.updateMatchedPairs();

    juce::MidiFile file;
    file.setTicksPerQuarterNote (kTicksPerQuarter);
    file.addTrack (seq);
    return file;
}

juce::File ChordSlotsProcessor::writeTempMidiFile() const
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ChordSlots");
    if (! dir.createDirectory())
        return {};

    const auto name = juce::File::createLegalFileName ("ChordSlots " + describeProgression()).substring (0, 80);
    const auto file = dir.getChildFile (name + ".mid");
    file.deleteFile();

    juce::FileOutputStream out (file);
    if (! out.openedOk() || ! createMidiFile().writeTo (out))
        return {};
    out.flush();
    return file;
}

//==============================================================================
void ChordSlotsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("ChordSlotsState");
    root.setProperty ("version", 1, nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (progressionToTree (getProgression()), nullptr);

    if (const auto xml = root.createXml())
        copyXmlToBinary (*xml, dest);
}

void ChordSlotsProcessor::setStateInformation (const void* data, int size)
{
    const auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr)
        return;

    const auto root = juce::ValueTree::fromXml (*xml);
    if (! root.hasType ("ChordSlotsState"))
        return;

    const auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid())
        apvts.replaceState (params.createCopy());

    cs::Progression loaded;
    if (progressionFromTree (root.getChildWithName ("Progression"), loaded))
    {
        {
            const juce::ScopedLock sl (modelLock);
            model = loaded;
            history.clear();
            publish();
        }
        sendChangeMessage();
    }
}

juce::AudioProcessorEditor* ChordSlotsProcessor::createEditor()
{
    return new ChordSlotsEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChordSlotsProcessor();
}
