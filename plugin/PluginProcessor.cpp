// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{

constexpr int kMidiChannelChords = 1;
constexpr int kMidiChannelBass   = 2;
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
        else
            s.voicing.bassCount = s.voicing.computeBassCount();
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
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "strum", 1 }, "Strum",
                                                             juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "accent", 1 }, "Melody Accent",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "humanize", 1 }, "Humanize",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "triggerMode", 1 }, "Trigger Mode",
                                                             juce::StringArray { "Auto", "Trigger Only", "Loop Only" }, 0));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { "triggerRoot", 1 }, "Trigger Root",
                                                           0, 127, 60));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "triggerLayout", 1 }, "Trigger Layout",
                                                             juce::StringArray { "White Keys", "Chromatic" }, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "splitMidi", 1 }, "Split MIDI", false));
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
    strumParam = apvts.getRawParameterValue ("strum");
    accentParam = apvts.getRawParameterValue ("accent");
    humanizeParam = apvts.getRawParameterValue ("humanize");
    triggerModeParam = apvts.getRawParameterValue ("triggerMode");
    triggerRootParam = apvts.getRawParameterValue ("triggerRoot");
    triggerLayoutParam = apvts.getRawParameterValue ("triggerLayout");
    splitMidiParam = apvts.getRawParameterValue ("splitMidi");

    apvts.addParameterListener ("strum", this);
    apvts.addParameterListener ("accent", this);
    apvts.addParameterListener ("humanize", this);
    apvts.addParameterListener ("triggerMode", this);
    apvts.addParameterListener ("splitMidi", this);

    model = cs::makeDefaultProgression (rng);
    publish();
    play = pendingPlay;
}

ChordSlotsProcessor::~ChordSlotsProcessor()
{
    apvts.removeParameterListener ("strum", this);
    apvts.removeParameterListener ("accent", this);
    apvts.removeParameterListener ("humanize", this);
    apvts.removeParameterListener ("triggerMode", this);
    apvts.removeParameterListener ("splitMidi", this);
}

void ChordSlotsProcessor::parameterChanged (const juce::String& paramID, float)
{
    if (paramID == "triggerMode" || paramID == "splitMidi")
        resetTrigger();
    publish();
    sendChangeMessage();
}

bool ChordSlotsProcessor::isSplitMidi() const noexcept
{
    return splitMidiParam != nullptr && splitMidiParam->load() > 0.5f;
}

cs::TriggerConfig ChordSlotsProcessor::getTriggerConfig() const noexcept
{
    cs::TriggerConfig cfg;
    if (triggerModeParam != nullptr)
        cfg.mode = (cs::TriggerMode) juce::jlimit (0, 2, juce::roundToInt (triggerModeParam->load()));
    if (triggerRootParam != nullptr)
        cfg.rootNote = juce::jlimit (0, 127, juce::roundToInt (triggerRootParam->load()));
    if (triggerLayoutParam != nullptr)
        cfg.layout = (cs::TriggerLayout) juce::jlimit (0, 1, juce::roundToInt (triggerLayoutParam->load()));
    cfg.numSlots = play.numSlots;
    return cfg;
}

void ChordSlotsProcessor::resetTrigger() noexcept
{
    triggerEngine.reset();
    uiTriggeredSlot.store (-1);
}

cs::PerformanceParams ChordSlotsProcessor::getPerformanceParams() const noexcept
{
    cs::PerformanceParams p;
    if (strumParam != nullptr) p.strum = strumParam->load();
    if (accentParam != nullptr) p.accent = accentParam->load();
    if (humanizeParam != nullptr) p.humanize = humanizeParam->load();
    p.seed = perfSeed.load();
    return p;
}

void ChordSlotsProcessor::refeel() noexcept
{
    perfSeed.fetch_add (1);
    publish();
    sendChangeMessage();
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
    const cs::PerformanceParams perf = getPerformanceParams();
    const cs::PlayData d = cs::toPlayData (model, perf, lastBpm.load());
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
    pendingCount = 0;
    wasPlaying = false;
    resetTrigger();
    totalSamplesProcessed = 0;
}

void ChordSlotsProcessor::releaseResources()
{
    synth.allNotesOff();
    pendingCount = 0;
    heldCount = 0;
    resetTrigger();
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

    juce::MidiBuffer incomingMidi;
    incomingMidi.swapWith (midi);

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
    const cs::TriggerConfig trigCfg = getTriggerConfig();
    const bool split = isSplitMidi();

    auto clearPending = [&]
    {
        pendingCount = 0;
    };

    auto releaseHeld = [&] (int sample)
    {
        for (int i = 0; i < heldCount; ++i)
            midi.addEvent (juce::MidiMessage::noteOff (held[i].channel, held[i].note), sample);
        heldCount = 0;
        heldSlot = -1;
    };

    auto releaseAudition = [&] (int sample)
    {
        for (int i = 0; i < auditionCount; ++i)
            midi.addEvent (juce::MidiMessage::noteOff (auditionNotes[i].channel, auditionNotes[i].note), sample);
        auditionCount = 0;
    };

    auto triggerNote = [&] (int note, int vel, int sample, int channel = kMidiChannelChords)
    {
        if (heldCount < cs::kMaxNotes)
        {
            held[heldCount++] = { note, channel };
            midi.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) juce::jlimit (1, 127, vel)), sample);
        }
    };

    auto emitReleases = [&] (const cs::TriggerEngine::TriggerAction& act, int sample)
    {
        for (int i = 0; i < act.releaseCount; ++i)
        {
            const int note = act.releaseNotes[i];
            const int channel = act.releaseChannels[i] > 0 ? act.releaseChannels[i] : kMidiChannelChords;
            for (int h = 0; h < heldCount; ++h)
            {
                if (held[h].note == note && (held[h].channel == channel || act.releaseChannels[i] == 0))
                {
                    const int ch = held[h].channel;
                    held[h] = held[heldCount - 1];
                    --heldCount;
                    midi.addEvent (juce::MidiMessage::noteOff (ch, note), sample);
                    break;
                }
            }
        }
        clearPending();
    };

    auto triggerLiveSlot = [&] (int slot, int incomingVel, int sample)
    {
        if (slot < 0 || slot >= play.numSlots)
            return;

        const cs::PlaySlot& s = play.slots[slot];
        cs::PerfInput in;
        in.n = std::min (s.n, cs::kPerfMaxNotes);
        in.bassCount = s.bassCount;
        for (int j = 0; j < in.n; ++j)
        {
            in.pitch[(size_t) j] = s.notes[j];
            const int baseVel = s.velocities[j] > 0 ? s.velocities[j] : s.velocity;
            const int scaledVel = juce::jlimit (1, 127, (int) std::lround (baseVel * (incomingVel / 100.0f)));
            in.velocity[(size_t) j] = scaledVel;
        }

        const cs::PerformanceParams perf = getPerformanceParams();
        const int pass = (int) (liveTriggerPass++ & 0x7fffffff);
        // Nominal duration of 500ms so live strum is never truncated by a short 1/16 slot
        const cs::PerfOutput out = cs::applyPerformance (in, perf, 500.0f, slot, pass);

        clearPending();

        if (split)
        {
            cs::TriggerEngine::SoundingVoice voices[cs::kMaxNotes];
            int voiceCount = 0;

            // Upper voices to Ch 1 with performance offsets
            for (int j = s.bassCount; j < out.n; ++j)
            {
                const int note = out.notes[(size_t) j].pitch;
                const int vel = out.notes[(size_t) j].velocity;
                const float offsetMs = out.notes[(size_t) j].offsetMs;
                const int delaySamples = (int) std::lround (offsetMs * 0.001 * sampleRate);
                const int onSample = sample + delaySamples;

                if (voiceCount < cs::kMaxNotes)
                    voices[voiceCount++] = { note, kMidiChannelChords };

                if (onSample < numSamples)
                    triggerNote (note, vel, onSample, kMidiChannelChords);
                else if (pendingCount < kMaxPendingNotes)
                    pendingNotes[pendingCount++] = { note, vel, kMidiChannelChords, onSample - numSamples, slot };
            }

            // Monophonic bass note to Ch 2 on the grid (offset 0)
            const int bassNote = s.primaryBass >= 0 ? s.primaryBass : (s.bassCount > 0 ? s.notes[(size_t) (s.bassCount - 1)] : 36);
            const int baseVel = (s.bassCount > 0 && s.velocities[(size_t) (s.bassCount - 1)] > 0) ? s.velocities[(size_t) (s.bassCount - 1)] : s.velocity;
            const int bassVel = juce::jlimit (1, 127, (int) std::lround (baseVel * (incomingVel / 100.0f)));

            if (voiceCount < cs::kMaxNotes)
                voices[voiceCount++] = { bassNote, kMidiChannelBass };

            triggerNote (bassNote, bassVel, sample, kMidiChannelBass);

            triggerEngine.setSoundingVoices (voices, voiceCount);
        }
        else
        {
            cs::TriggerEngine::SoundingVoice voices[cs::kMaxNotes];
            for (int j = 0; j < out.n; ++j)
                voices[j] = { out.notes[(size_t) j].pitch, kMidiChannelChords };
            triggerEngine.setSoundingVoices (voices, out.n);

            for (int j = 0; j < out.n; ++j)
            {
                const int note = out.notes[(size_t) j].pitch;
                const int vel = out.notes[(size_t) j].velocity;
                const float offsetMs = out.notes[(size_t) j].offsetMs;
                const int delaySamples = (int) std::lround (offsetMs * 0.001 * sampleRate);
                const int onSample = sample + delaySamples;

                if (onSample < numSamples)
                    triggerNote (note, vel, onSample, kMidiChannelChords);
                else if (pendingCount < kMaxPendingNotes)
                    pendingNotes[pendingCount++] = { note, vel, kMidiChannelChords, onSample - numSamples, slot };
            }
        }

        heldSlot = slot;
        currentSlot = slot;
        uiTriggeredSlot.store (slot);
        triggerGlowExpirySamples = totalSamplesProcessed + sample + (juce::int64) (0.12 * sampleRate);
    };

    auto startSlot = [&] (int slot, int slotSampleOffset, double slotStartStep = -1.0, double currentPlayStep = -1.0)
    {
        releaseHeld (slotSampleOffset);
        clearPending();

        const cs::PlaySlot& s = play.slots[slot];
        double elapsedMs = 0.0;
        if (slotStartStep >= 0.0 && currentPlayStep > slotStartStep)
        {
            const double elapsedSteps = currentPlayStep - slotStartStep;
            elapsedMs = (elapsedSteps / stepsPerSample) * (1000.0 / sampleRate);
        }

        if (split)
        {
            // Upper voices to Ch 1
            for (int i = s.bassCount; i < s.n && i < cs::kMaxNotes; ++i)
            {
                const int note = s.notes[i];
                const int vel = s.velocities[i] > 0 ? s.velocities[i] : s.velocity;
                const float offsetMs = s.offsetMs[i];

                if (elapsedMs >= (double) offsetMs)
                {
                    triggerNote (note, vel, slotSampleOffset, kMidiChannelChords);
                }
                else
                {
                    const double remainingMs = (double) offsetMs - elapsedMs;
                    const int delaySamples = (int) std::lround (remainingMs * 0.001 * sampleRate);
                    const int onSample = slotSampleOffset + delaySamples;

                    if (onSample < numSamples)
                        triggerNote (note, vel, onSample, kMidiChannelChords);
                    else if (pendingCount < kMaxPendingNotes)
                        pendingNotes[pendingCount++] = { note, vel, kMidiChannelChords, onSample - numSamples, slot };
                }
            }

            // Monophonic bass note to Ch 2 on the grid
            const int bassNote = s.primaryBass >= 0 ? s.primaryBass : (s.bassCount > 0 ? s.notes[(size_t) (s.bassCount - 1)] : 36);
            const int bassVel = (s.bassCount > 0 && s.velocities[(size_t) (s.bassCount - 1)] > 0) ? s.velocities[(size_t) (s.bassCount - 1)] : s.velocity;
            triggerNote (bassNote, bassVel, slotSampleOffset, kMidiChannelBass);
        }
        else
        {
            for (int i = 0; i < s.n && i < cs::kMaxNotes; ++i)
            {
                const int note = s.notes[i];
                const int vel = s.velocities[i] > 0 ? s.velocities[i] : s.velocity;
                const float offsetMs = s.offsetMs[i];

                if (elapsedMs >= (double) offsetMs)
                {
                    triggerNote (note, vel, slotSampleOffset, kMidiChannelChords);
                }
                else
                {
                    const double remainingMs = (double) offsetMs - elapsedMs;
                    const int delaySamples = (int) std::lround (remainingMs * 0.001 * sampleRate);
                    const int onSample = slotSampleOffset + delaySamples;

                    if (onSample < numSamples)
                        triggerNote (note, vel, onSample, kMidiChannelChords);
                    else if (pendingCount < kMaxPendingNotes)
                        pendingNotes[pendingCount++] = { note, vel, kMidiChannelChords, onSample - numSamples, slot };
                }
            }
        }
        heldSlot = slot;
        currentSlot = slot;
    };

    auto offStep = [&] (int slot)
    {
        const cs::PlaySlot& s = play.slots[slot];
        return (double) s.start + (double) (s.end - s.start) * (double) gate;
    };

    // --- Ingest incoming MIDI events ---
    for (const auto meta : incomingMidi)
    {
        const auto m = meta.getMessage();
        const int sPos = juce::jlimit (0, numSamples - 1, meta.samplePosition);

        if (m.isNoteOn())
        {
            auto act = triggerEngine.onNoteOn (m.getNoteNumber(), m.getVelocity(), trigCfg);
            if (act.releaseCount > 0 || act.startSlot >= 0)
            {
                if (heldCount > 0 && act.releaseCount == 0)
                    releaseHeld (sPos);

                emitReleases (act, sPos);
                if (act.startSlot >= 0)
                    triggerLiveSlot (act.startSlot, act.startVelocity, sPos);
            }
        }
        else if (m.isNoteOff())
        {
            auto act = triggerEngine.onNoteOff (m.getNoteNumber(), trigCfg);
            if (act.releaseCount > 0 || act.startSlot >= 0)
            {
                emitReleases (act, sPos);
                if (act.startSlot >= 0)
                    triggerLiveSlot (act.startSlot, act.startVelocity, sPos);
            }
        }
        else if (m.isController() && m.getControllerNumber() == 64)
        {
            auto act = triggerEngine.onControlChange (64, m.getControllerValue());
            if (act.releaseCount > 0)
                emitReleases (act, sPos);
        }
        else if (m.isAllNotesOff() || (m.isController() && (m.getControllerNumber() == 120 || m.getControllerNumber() == 123)))
        {
            auto act = triggerEngine.reset();
            emitReleases (act, sPos);
            releaseHeld (sPos);
            for (int ch : { kMidiChannelChords, kMidiChannelBass })
                midi.addEvent (juce::MidiMessage::allNotesOff (ch), sPos);
            uiTriggeredSlot.store (-1);
        }
    }

    // Process pending note-ons scheduled across block boundaries (for both loop and live triggers)
    int remainingPending = 0;
    for (int pIdx = 0; pIdx < pendingCount; ++pIdx)
    {
        auto& pn = pendingNotes[pIdx];
        if (pn.slot == currentSlot)
        {
            if (pn.samplesUntilOn < numSamples)
            {
                const int sPos = juce::jlimit (0, numSamples - 1, pn.samplesUntilOn >= 0 ? pn.samplesUntilOn : 0);
                triggerNote (pn.note, pn.velocity, sPos, pn.channel);
            }
            else
            {
                pn.samplesUntilOn -= numSamples;
                pendingNotes[remainingPending++] = pn;
            }
        }
    }
    pendingCount = remainingPending;

    const bool triggerSuppressesLoop = triggerEngine.isTriggerActive() || trigCfg.mode == cs::TriggerMode::TriggerOnly;

    if (playing && ! triggerSuppressesLoop)
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
            clearPending();
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
                startSlot (slotNow, 0, (double) play.slots[slotNow].start, startStep);
                startedAtZero = true;
            }
            else
            {
                releaseHeld (0);
                clearPending();
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
                startSlot (events[e].slot, events[e].sample, -1.0, -1.0);
            else if (heldSlot == events[e].slot)
            {
                releaseHeld (events[e].sample);
                clearPending();
            }
        }

        expectedStep = std::fmod (endStep, len);
        uiStep.store (expectedStep);
    }
    else
    {
        if (wasPlaying && ! triggerEngine.isTriggerActive())
        {
            releaseHeld (0);
            clearPending();
            for (int ch : { kMidiChannelChords, kMidiChannelBass })
                midi.addEvent (juce::MidiMessage::allNotesOff (ch), 0);
            currentSlot = -1;
        }
        uiStep.store (-1.0);

        const int request = auditionRequest.exchange (-1);
        if (request >= 0 && request < play.numSlots && numSamples > 0 && ! triggerEngine.isTriggerActive())
        {
            releaseAudition (0);
            const cs::PlaySlot& s = play.slots[request];
            if (split)
            {
                for (int i = s.bassCount; i < s.n && i < cs::kMaxNotes; ++i)
                {
                    auditionNotes[auditionCount++] = { s.notes[i], kMidiChannelChords };
                    midi.addEvent (juce::MidiMessage::noteOn (kMidiChannelChords, s.notes[i], (juce::uint8) juce::jlimit (1, 127, s.velocity)), 0);
                }
                const int bassNote = s.primaryBass >= 0 ? s.primaryBass : (s.bassCount > 0 ? s.notes[(size_t) (s.bassCount - 1)] : 36);
                auditionNotes[auditionCount++] = { bassNote, kMidiChannelBass };
                midi.addEvent (juce::MidiMessage::noteOn (kMidiChannelBass, bassNote, (juce::uint8) juce::jlimit (1, 127, s.velocity)), 0);
            }
            else
            {
                for (int i = 0; i < s.n && i < cs::kMaxNotes; ++i)
                {
                    auditionNotes[auditionCount++] = { s.notes[i], kMidiChannelChords };
                    midi.addEvent (juce::MidiMessage::noteOn (kMidiChannelChords, s.notes[i], (juce::uint8) juce::jlimit (1, 127, s.velocity)), 0);
                }
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

    if (! triggerEngine.isTriggerActive() && totalSamplesProcessed >= triggerGlowExpirySamples)
        uiTriggeredSlot.store (-1);

    totalSamplesProcessed += numSamples;
    wasPlaying = playing && ! triggerEngine.isTriggerActive();

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

juce::MidiFile ChordSlotsProcessor::createMidiFile (MidiExportMode mode) const
{
    const cs::Progression p = getProgression();
    const cs::PerformanceParams perf = getPerformanceParams();
    const double bpm = lastBpm.load();
    const cs::PlayData playData = cs::toPlayData (p, perf, bpm);
    const float gate = juce::jlimit (0.1f, 1.0f, gateParam->load());
    const bool split = isSplitMidi();

    const double msToTicks = (double) kTicksPerQuarter * bpm / 60000.0;
    const double endTick = (double) p.lengthSteps() * kTicksPerStep;

    juce::MidiFile file;
    file.setTicksPerQuarterNote (kTicksPerQuarter);

    if (mode == MidiExportMode::Master)
    {
        if (split)
        {
            // Format 1: Track 0 = Chords (Ch 1), Track 1 = Bass (Ch 2)
            juce::MidiMessageSequence chordsSeq;
            chordsSeq.addEvent (juce::MidiMessage::textMetaEvent (3, "Chords"), 0.0);
            chordsSeq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
            chordsSeq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / bpm)), 0.0);

            juce::MidiMessageSequence bassSeq;
            bassSeq.addEvent (juce::MidiMessage::textMetaEvent (3, "Bass"), 0.0);

            for (int i = 0; i < playData.numSlots; ++i)
            {
                const cs::PlaySlot& ps = playData.slots[(size_t) i];
                const double baseOn = (double) ps.start * kTicksPerStep;
                const double off = baseOn + juce::jmax (1.0, (double) (p.slotEnd (i) - ps.start) * kTicksPerStep * (double) gate - 1.0);

                // Upper structure notes to Ch 1
                for (int j = ps.bassCount; j < ps.n; ++j)
                {
                    const int note = ps.notes[(size_t) j];
                    const int vel = ps.velocities[(size_t) j];
                    const double noteOffsetTicks = (double) ps.offsetMs[(size_t) j] * msToTicks;
                    const double noteOn = std::min (off - 1.0, baseOn + noteOffsetTicks);
                    chordsSeq.addEvent (juce::MidiMessage::noteOn (kMidiChannelChords, note, (juce::uint8) juce::jlimit (1, 127, vel)), noteOn);
                    chordsSeq.addEvent (juce::MidiMessage::noteOff (kMidiChannelChords, note), off);
                }

                // Monophonic bass note to Ch 2 on the grid
                const int bassNote = ps.primaryBass >= 0 ? ps.primaryBass : (ps.bassCount > 0 ? ps.notes[(size_t) (ps.bassCount - 1)] : 36);
                const int bassVel = (ps.bassCount > 0 && ps.velocities[(size_t) (ps.bassCount - 1)] > 0) ? ps.velocities[(size_t) (ps.bassCount - 1)] : ps.velocity;
                bassSeq.addEvent (juce::MidiMessage::noteOn (kMidiChannelBass, bassNote, (juce::uint8) juce::jlimit (1, 127, bassVel)), baseOn);
                bassSeq.addEvent (juce::MidiMessage::noteOff (kMidiChannelBass, bassNote), off);
            }

            chordsSeq.addEvent (juce::MidiMessage::endOfTrack(), endTick);
            bassSeq.addEvent (juce::MidiMessage::endOfTrack(), endTick);
            chordsSeq.updateMatchedPairs();
            bassSeq.updateMatchedPairs();

            file.addTrack (chordsSeq);
            file.addTrack (bassSeq);
            return file;
        }
        else
        {
            // Non-split Format 0 single track
            juce::MidiMessageSequence seq;
            seq.addEvent (juce::MidiMessage::textMetaEvent (3, "ChordSlots"), 0.0);
            seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
            seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / bpm)), 0.0);

            for (int i = 0; i < playData.numSlots; ++i)
            {
                const cs::PlaySlot& ps = playData.slots[(size_t) i];
                const double baseOn = (double) ps.start * kTicksPerStep;
                const double off = baseOn + juce::jmax (1.0, (double) (p.slotEnd (i) - ps.start) * kTicksPerStep * (double) gate - 1.0);
                for (int j = 0; j < ps.n; ++j)
                {
                    const int note = ps.notes[(size_t) j];
                    const int vel = ps.velocities[(size_t) j];
                    const double noteOffsetTicks = (double) ps.offsetMs[(size_t) j] * msToTicks;
                    const double noteOn = std::min (off - 1.0, baseOn + noteOffsetTicks);
                    seq.addEvent (juce::MidiMessage::noteOn (kMidiChannelChords, note, (juce::uint8) juce::jlimit (1, 127, vel)), noteOn);
                    seq.addEvent (juce::MidiMessage::noteOff (kMidiChannelChords, note), off);
                }
            }
            seq.addEvent (juce::MidiMessage::endOfTrack(), endTick);
            seq.updateMatchedPairs();
            file.addTrack (seq);
            return file;
        }
    }
    else if (mode == MidiExportMode::ChordsOnly)
    {
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, "Chords"), 0.0);
        seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
        seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / bpm)), 0.0);

        for (int i = 0; i < playData.numSlots; ++i)
        {
            const cs::PlaySlot& ps = playData.slots[(size_t) i];
            const double baseOn = (double) ps.start * kTicksPerStep;
            const double off = baseOn + juce::jmax (1.0, (double) (p.slotEnd (i) - ps.start) * kTicksPerStep * (double) gate - 1.0);
            for (int j = ps.bassCount; j < ps.n; ++j)
            {
                const int note = ps.notes[(size_t) j];
                const int vel = ps.velocities[(size_t) j];
                const double noteOffsetTicks = (double) ps.offsetMs[(size_t) j] * msToTicks;
                const double noteOn = std::min (off - 1.0, baseOn + noteOffsetTicks);
                seq.addEvent (juce::MidiMessage::noteOn (kMidiChannelChords, note, (juce::uint8) juce::jlimit (1, 127, vel)), noteOn);
                seq.addEvent (juce::MidiMessage::noteOff (kMidiChannelChords, note), off);
            }
        }
        seq.addEvent (juce::MidiMessage::endOfTrack(), endTick);
        seq.updateMatchedPairs();
        file.addTrack (seq);
        return file;
    }
    else // BassOnly
    {
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, "Bass"), 0.0);
        seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
        seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / bpm)), 0.0);

        for (int i = 0; i < playData.numSlots; ++i)
        {
            const cs::PlaySlot& ps = playData.slots[(size_t) i];
            const double baseOn = (double) ps.start * kTicksPerStep;
            const double off = baseOn + juce::jmax (1.0, (double) (p.slotEnd (i) - ps.start) * kTicksPerStep * (double) gate - 1.0);

            const int bassNote = ps.primaryBass >= 0 ? ps.primaryBass : (ps.bassCount > 0 ? ps.notes[(size_t) (ps.bassCount - 1)] : 36);
            const int bassVel = (ps.bassCount > 0 && ps.velocities[(size_t) (ps.bassCount - 1)] > 0) ? ps.velocities[(size_t) (ps.bassCount - 1)] : ps.velocity;
            seq.addEvent (juce::MidiMessage::noteOn (kMidiChannelBass, bassNote, (juce::uint8) juce::jlimit (1, 127, bassVel)), baseOn);
            seq.addEvent (juce::MidiMessage::noteOff (kMidiChannelBass, bassNote), off);
        }
        seq.addEvent (juce::MidiMessage::endOfTrack(), endTick);
        seq.updateMatchedPairs();
        file.addTrack (seq);
        return file;
    }
}

juce::File ChordSlotsProcessor::writeTempMidiFile (MidiExportMode mode) const
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ChordSlots");
    if (! dir.createDirectory())
        return {};

    juce::String suffix;
    if (mode == MidiExportMode::ChordsOnly)
        suffix = " Chords";
    else if (mode == MidiExportMode::BassOnly)
        suffix = " Bass";

    const auto name = juce::File::createLegalFileName ("ChordSlots" + suffix + " " + describeProgression()).substring (0, 80);
    const auto file = dir.getChildFile (name + ".mid");
    file.deleteFile();

    juce::FileOutputStream out (file);
    if (! out.openedOk() || ! createMidiFile (mode).writeTo (out))
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
