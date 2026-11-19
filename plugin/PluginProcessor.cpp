// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{

constexpr int kMidiChannelChords = 1;
constexpr int kMidiChannelBass   = 2;
constexpr int kTicksPerQuarter = 480;
constexpr int kTicksPerStep = kTicksPerQuarter / cs::kStepsPerQuarter;

bool readMidi (const juce::File& file, juce::MidiFile& midi)
{
    juce::FileInputStream stream (file);
    return stream.openedOk() && midi.readFrom (stream);
}

bool isMelodyNote (const juce::MidiMessageSequence::MidiEventHolder& event)
{
    return event.message.isNoteOn() && event.message.getChannel() != 10
        && event.noteOffObject != nullptr
        && event.message.getTimeStamp() >= 0.0
        && event.noteOffObject->message.getTimeStamp() > event.message.getTimeStamp();
}

double gatedDurationTicks (int steps, float gate)
{
    return std::max (1.0, steps * (double) kTicksPerStep * gate - (gate >= 0.999f ? 1.0 : 0.0));
}

double bassDurationTicks (const cs::BassHit& hit, float gate)
{
    return hit.glide ? hit.durationSteps * (double) kTicksPerStep : gatedDurationTicks (hit.durationSteps, gate);
}

int bassVelocity (const cs::PlaySlot& slot, float scale = 1.0f)
{
    const int index = slot.bassCount - 1;
    const int base = index >= 0 && index < slot.n && slot.velocities[index] > 0 ? slot.velocities[index] : slot.velocity;
    return juce::jlimit (1, 127, (int) std::lround (base * scale));
}

juce::ValueTree progressionToTree (const cs::Progression& p)
{
    juce::ValueTree t ("Progression");
    t.setProperty ("tonic", p.tonic, nullptr);
    t.setProperty ("scale", p.scale, nullptr);
    t.setProperty ("bars", p.bars, nullptr);
    t.setProperty ("timeSigNum", p.timeSigNum, nullptr);
    t.setProperty ("timeSigDen", p.timeSigDen, nullptr);
    t.setProperty ("snap", p.snap, nullptr);
    t.setProperty ("adventure", p.adventure, nullptr);
    t.setProperty ("variety", p.variety, nullptr);
    t.setProperty ("melodyFit", p.melodyFit, nullptr);
    t.setProperty ("syncopation", p.syncopation, nullptr);
    t.setProperty ("bassMotion", p.bassMotion, nullptr);
    t.setProperty ("bassPattern", (int) p.bassStyle, nullptr);
    t.setProperty ("mood", p.mood, nullptr);
    t.setProperty ("style", p.style, nullptr);
    t.setProperty ("voicingStyle", p.voicingStyle, nullptr);
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
        st.setProperty ("cutLocked", s.cutLocked, nullptr);
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
    if (! p.melody.empty())
    {
        juce::ValueTree melody ("Melody");
        for (const auto& note : p.melody)
        {
            juce::ValueTree item ("Note");
            item.setProperty ("start", note.start, nullptr);
            item.setProperty ("end", note.end, nullptr);
            item.setProperty ("pitch", note.pitch, nullptr);
            melody.appendChild (item, nullptr);
        }
        t.appendChild (melody, nullptr);
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
bool progressionFromTree (const juce::ValueTree& t, cs::Progression& p, int stateVersion = 3)
{
    if (! t.hasType ("Progression"))
        return false;

    std::array<juce::ValueTree, cs::kMaxSlots> slots;
    int numSlots = 0;
    for (const auto child : t)
    {
        if (child.hasType ("Slot"))
            slots[(size_t) numSlots++] = child;
        if (numSlots == cs::kMaxSlots)
            break;
    }
    if (numSlots == 0)
        return false;

    p = cs::Progression();
    p.tonic = cs::mod12 ((int) t.getProperty ("tonic", 9));
    p.scale = juce::jlimit (0, cs::numScales() - 1, (int) t.getProperty ("scale", 1));
    p.bars = juce::jlimit (1, cs::kMaxBars, (int) t.getProperty ("bars", 4));
    p.timeSigNum = juce::jlimit (1, 16, (int) t.getProperty ("timeSigNum", 4));
    p.timeSigDen = (int) t.getProperty ("timeSigDen", 4) == 8 ? 8 : 4;
    int snap = (int) t.getProperty ("snap", 12);
    if (stateVersion < 3)
    {
        snap = (snap == 4) ? 24 : ((snap == 2) ? 12 : 6);
    }
    const int validSnaps[] = { 24, 16, 12, 8, 6, 4, 3 };
    if (std::none_of (std::begin (validSnaps), std::end (validSnaps), [snap] (int s) { return s == snap; }))
        snap = 12;
    p.snap = snap;
    p.adventure = juce::jlimit (0.0f, 1.0f, (float) t.getProperty ("adventure", 0.3f));
    const float variety = (float) t.getProperty ("variety", 0.5f);
    p.variety = std::isfinite (variety) ? cs::clamp01 (variety) : 0.5f;
    const float melodyFit = (float) t.getProperty ("melodyFit", p.melodyFit);
    if (std::isfinite (melodyFit))
        p.melodyFit = cs::clamp01 (melodyFit);
    for (const auto item : t.getChildWithName ("Melody"))
    {
        if (p.melody.size() == cs::kMaxMelodyNotes)
            break;
        const float start = (float) item.getProperty ("start", -1.0f);
        const float end = (float) item.getProperty ("end", -1.0f);
        const int pitch = (int) item.getProperty ("pitch", -1);
        if (item.hasType ("Note") && start >= 0.0f && end <= 1.0f && end > start && pitch >= 0 && pitch <= 127)
            p.melody.push_back ({ start, end, pitch });
    }
    p.syncopation = juce::jlimit (0.0f, 1.0f, (float) t.getProperty ("syncopation", 0.3f));
    p.bassMotion = juce::jlimit (0.0f, 1.0f, (float) t.getProperty ("bassMotion", 0.0f));
    p.bassStyle = (cs::BassStyle) juce::jlimit (0, 4, (int) t.getProperty ("bassPattern", 0));
    p.mood = juce::jlimit (-1, (int) cs::moods().size() - 1, (int) t.getProperty ("mood", -1));
    p.style = juce::jlimit (-1, (int) cs::CorpusStyle::Free, (int) t.getProperty ("style", -1));
    p.voicingStyle = juce::jlimit (0, cs::kNumVoicingStyles - 1, (int) t.getProperty ("voicingStyle", (int) cs::VoicingStyle::Close));
    p.locks.rhythm = (bool) t.getProperty ("lockRhythm", false);
    p.locks.roots = (bool) t.getProperty ("lockRoots", false);
    p.locks.colors = (bool) t.getProperty ("lockColors", false);
    p.locks.voicing = (bool) t.getProperty ("lockVoicing", false);

    p.numSlots = std::min (numSlots, p.lengthSteps());
    cs::Rng tmp (1);
    for (int i = 0; i < p.numSlots; ++i)
    {
        const auto& st = slots[(size_t) i];
        cs::Slot& s = p.slots[(size_t) i];
        int rawStart = (int) st.getProperty ("start", 0);
        if (stateVersion < 3)
            rawStart *= 6;
        s.start = juce::jlimit (0, p.lengthSteps() - 1, rawStart);
        s.chord.root = cs::mod12 ((int) st.getProperty ("root", 0));
        s.chord.type = chordTypeFromSuffix (st.getProperty ("type", "").toString());
        s.chord.bassInt = juce::jlimit (0, 11, (int) st.getProperty ("bassInt", 0));
        cs::sanitizeBass (s.chord);
        s.locked = (bool) st.getProperty ("locked", false);
        s.cutLocked = i > 0 && (bool) st.getProperty ("cutLocked", false);
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
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "variation", 1 }, "Variation",
                                                             juce::StringArray { "A", "B", "C", "D" }, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "quantizeSwitch", 1 }, "Switch Quantize",
                                                             juce::StringArray { "Next Bar", "Next Slot", "Immediate" }, 0));
    juce::StringArray compStyles;
    for (auto* name : cs::kCompStyleNames)
        compStyles.add (name);
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "compStyle", 1 }, "Comping Style",
                                                             compStyles, 0));
    juce::StringArray chordRhythms { "Off" };
    for (auto* name : cs::kExtraCompStyleNames)
        chordRhythms.add (name);
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "chordRhythm", 1 }, "Chord Rhythm",
        chordRhythms, 0, juce::AudioParameterChoiceAttributes().withAutomatable (false)));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "synthModel", 1 }, "Synth Model",
                                                             juce::StringArray { "Clean Saw", "Rhodes EP", "Felt Piano" }, 0));
    return layout;
}

ChordSlotsProcessor::ChordSlotsProcessor()
    : AudioProcessor (
#if defined(CHORDSLOTS_MIDI_FX)
          BusesProperties()),
#else
          BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#endif
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
    variationParam = apvts.getRawParameterValue ("variation");
    quantizeSwitchParam = apvts.getRawParameterValue ("quantizeSwitch");
    compStyleParam = apvts.getRawParameterValue ("compStyle");
    chordRhythmParam = apvts.getRawParameterValue ("chordRhythm");
    synthModelParam = apvts.getRawParameterValue ("synthModel");

    apvts.addParameterListener ("gate", this);
    apvts.addParameterListener ("strum", this);
    apvts.addParameterListener ("accent", this);
    apvts.addParameterListener ("humanize", this);
    apvts.addParameterListener ("triggerMode", this);
    apvts.addParameterListener ("splitMidi", this);
    apvts.addParameterListener ("compStyle", this);
    apvts.addParameterListener ("chordRhythm", this);
    apvts.addParameterListener ("synthModel", this);

    variations[0] = cs::makeDefaultProgression (rng);
    for (int i = 1; i < cs::kNumVariations; ++i)
        variations[(size_t) i] = variations[0];

    publish();
    playBank = pendingPlayBank;
    startTimerHz (10);
}

ChordSlotsProcessor::~ChordSlotsProcessor()
{
    stopTimer();
    apvts.removeParameterListener ("gate", this);
    apvts.removeParameterListener ("strum", this);
    apvts.removeParameterListener ("accent", this);
    apvts.removeParameterListener ("humanize", this);
    apvts.removeParameterListener ("triggerMode", this);
    apvts.removeParameterListener ("splitMidi", this);
    apvts.removeParameterListener ("compStyle", this);
    apvts.removeParameterListener ("chordRhythm", this);
    apvts.removeParameterListener ("synthModel", this);
}

void ChordSlotsProcessor::parameterChanged (const juce::String& paramID, float val)
{
    if (paramID == "gate")
    {
        sendChangeMessage();
        return;
    }
    if (paramID == "triggerMode" || paramID == "splitMidi")
        resetTrigger();
    else if (paramID == "synthModel")
        setSynthModel (static_cast<PreviewSoundModel> (juce::roundToInt (val)));
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
    cfg.numSlots = playBank.variations[activePlayingVariation].numSlots;
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
    if (compStyleParam != nullptr)
        p.compStyle = static_cast<cs::CompStyle> (juce::jlimit (0, cs::kNumCompStyles - 1, juce::roundToInt (compStyleParam->load())));
    const int rhythm = chordRhythmParam != nullptr ? juce::roundToInt (chordRhythmParam->load()) : 0;
    if (rhythm > 0 && rhythm <= cs::kNumExtraCompStyles)
        p.compStyle = static_cast<cs::CompStyle> (cs::kNumCompStyles + rhythm - 1);
    p.seed = perfSeed.load();
    return p;
}

void ChordSlotsProcessor::setSynthModel (PreviewSoundModel model)
{
    synth.setSoundModel (model);
}

bool ChordSlotsProcessor::loadMidiToCurves (const juce::File& file)
{
    juce::MidiFile midi;
    if (! readMidi (file, midi))
        return false;

    const int numTracks = midi.getNumTracks();
    if (numTracks == 0)
        return false;

    double maxTick = 0.0;
    for (int t = 0; t < numTracks; ++t)
    {
        const auto* track = midi.getTrack (t);
        if (track != nullptr && track->getNumEvents() > 0)
            maxTick = std::max (maxTick, track->getEndTime());
    }

    if (maxTick <= 0.0)
        return false;

    edit ([&] (cs::Progression& p, cs::Rng&)
    {
        const int nSlots = p.numSlots;
        const double ticksPerSlot = maxTick / (double) nSlots;

        for (int s = 0; s < nSlots; ++s)
        {
            const double slotStartTick = (double) s * ticksPerSlot;
            const double slotEndTick = (double) (s + 1) * ticksPerSlot;

            std::array<bool, 12> pcSeen{};
            int noteCount = 0;
            float velSum = 0.0f;
            float pitchSum = 0.0f;

            for (int t = 0; t < numTracks; ++t)
            {
                const auto* track = midi.getTrack (t);
                if (track == nullptr) continue;
                for (int e = 0; e < track->getNumEvents(); ++e)
                {
                    const auto& m = track->getEventPointer (e)->message;
                    if (m.isNoteOn())
                    {
                        const double ts = track->getEventTime (e);
                        if (ts >= slotStartTick && ts < slotEndTick)
                        {
                            const int n = m.getNoteNumber();
                            pcSeen[(size_t) cs::mod12 (n)] = true;
                            noteCount++;
                            velSum += (float) m.getVelocity();
                            pitchSum += (float) n;
                        }
                    }
                }
            }

            if (noteCount > 0)
            {
                int pcs[12];
                int pcCount = 0;
                for (int pc = 0; pc < 12; ++pc)
                    if (pcSeen[(size_t) pc])
                        pcs[pcCount++] = pc;

                float lofSum = 0.0f;
                for (int i = 0; i < pcCount; ++i)
                {
                    int d = cs::mod12 (pcs[i] - p.tonic);
                    int fifths = (d * 7) % 12;
                    if (fifths > 6) fifths -= 12;
                    lofSum += (float) (fifths + 6) / 12.0f;
                }
                const float bright = pcCount > 0 ? juce::jlimit (0.05f, 0.95f, lofSum / (float) pcCount) : 0.5f;

                float tension = 0.2f;
                for (int i = 0; i < pcCount; ++i)
                {
                    for (int j = i + 1; j < pcCount; ++j)
                    {
                        int iv = std::abs (pcs[i] - pcs[j]) % 12;
                        if (iv > 6) iv = 12 - iv;
                        if (iv == 1) tension += 0.30f;
                        else if (iv == 6) tension += 0.20f;
                    }
                }
                tension = juce::jlimit (0.05f, 0.95f, tension);

                const float avgPitch = pitchSum / (float) noteCount;
                const float avgVel = velSum / (float) noteCount;
                const float energy = juce::jlimit (0.05f, 0.95f, 0.4f * (avgVel / 127.0f)
                                                               + 0.3f * ((avgPitch - 36.0f) / 60.0f)
                                                               + 0.3f * std::min (1.0f, (float) noteCount / 6.0f));

                p.slots[(size_t) s].targetB = bright;
                p.slots[(size_t) s].targetT = tension;
                p.slots[(size_t) s].targetE = energy;
            }
        }
    });

    return true;
}

bool ChordSlotsProcessor::harmonizeMidi (const juce::File& file)
{
    juce::MidiFile midi;
    if (! readMidi (file, midi))
        return false;

    const juce::MidiMessageSequence* selected = nullptr;
    std::pair<int, int> best {};
    for (int i = 0; i < midi.getNumTracks(); ++i)
    {
        const auto* track = midi.getTrack (i);
        std::pair<int, int> score {};
        for (int n = 0; n < track->getNumEvents(); ++n)
        {
            const auto& event = *track->getEventPointer (n);
            if (isMelodyNote (event))
            {
                score.first += event.message.getNoteNumber() > 60;
                ++score.second;
            }
        }
        if (score > best)
        {
            best = score;
            selected = track;
        }
    }
    if (selected == nullptr || (size_t) best.second > cs::kMaxMelodyNotes)
        return false;

    double endTick = 0.0;
    for (int n = 0; n < selected->getNumEvents(); ++n)
    {
        const auto& event = *selected->getEventPointer (n);
        if (isMelodyNote (event))
            endTick = std::max (endTick, event.noteOffObject->message.getTimeStamp());
    }
    if (! std::isfinite (endTick) || endTick <= 0.0)
        return false;

    std::vector<cs::MelodyNote> notes;
    notes.reserve ((size_t) best.second);
    for (int n = 0; n < selected->getNumEvents(); ++n)
    {
        const auto& event = *selected->getEventPointer (n);
        if (isMelodyNote (event))
        {
            const float start = (float) (event.message.getTimeStamp() / endTick);
            const float end = (float) (event.noteOffObject->message.getTimeStamp() / endTick);
            if (end > start)
                notes.push_back ({ start, end, event.message.getNoteNumber() });
        }
    }
    if (notes.empty())
        return false;

    edit ([&notes] (cs::Progression& p, cs::Rng& rng)
    {
        p.melody = std::move (notes);
        cs::roll (p, rng);
    });
    return true;
}

void ChordSlotsProcessor::refeel() noexcept
{
    perfSeed.fetch_add (1);
    publish();
    sendChangeMessage();
}

//==============================================================================
int ChordSlotsProcessor::getPlayingVariation() const noexcept
{
    const int parameterValue = variationParam != nullptr
        ? juce::jlimit (0, cs::kNumVariations - 1, juce::roundToInt (variationParam->load()))
        : 0;
    const int request = pendingMidiVariation.load();
    if (request >= 0 && (request >> 2) == parameterValue)
        return request & 0x3;
    return parameterValue;
}

void ChordSlotsProcessor::setPlayingVariation (int index)
{
    index = juce::jlimit (0, cs::kNumVariations - 1, index);
    pendingMidiVariation.store (-1);
    if (followPlayback.load())
        setEditVariation (index);
    if (auto* param = apvts.getParameter ("variation"))
        param->setValueNotifyingHost (param->convertTo0to1 ((float) index));
}

void ChordSlotsProcessor::requestMidiPlayingVariation (int index) noexcept
{
    const int target = juce::jlimit (0, cs::kNumVariations - 1, index);
    const int base = variationParam != nullptr
        ? juce::jlimit (0, cs::kNumVariations - 1, juce::roundToInt (variationParam->load()))
        : 0;
    pendingMidiVariation.store ((base << 2) | target);
}

void ChordSlotsProcessor::setFollowPlayback (bool follow) noexcept
{
    followPlayback.store (follow);
    if (follow)
        setEditVariation (getPlayingVariation());
}

void ChordSlotsProcessor::setEditVariation (int index)
{
    index = juce::jlimit (0, cs::kNumVariations - 1, index);
    if (editVariation.exchange (index) != index)
    {
        sendChangeMessage();
    }
}

cs::Progression ChordSlotsProcessor::getProgression() const
{
    return getProgression (editVariation.load());
}

cs::Progression ChordSlotsProcessor::getProgression (int variationIndex) const
{
    const juce::ScopedLock sl (modelLock);
    const int idx = juce::jlimit (0, cs::kNumVariations - 1, variationIndex >= 0 ? variationIndex : editVariation.load());
    return variations[(size_t) idx];
}

void ChordSlotsProcessor::pushHistory (int variationIndex)
{
    const int idx = juce::jlimit (0, cs::kNumVariations - 1, variationIndex);
    histories[(size_t) idx].push_back (variations[(size_t) idx]);
    redoStacks[(size_t) idx].clear(); // a new edit ends the redo branch
    if (histories[(size_t) idx].size() > 64)
        histories[(size_t) idx].erase (histories[(size_t) idx].begin());
}

void ChordSlotsProcessor::beginGesture()
{
    const juce::ScopedLock sl (modelLock);
    pushHistory (editVariation.load());
}

bool ChordSlotsProcessor::canUndo() const
{
    const juce::ScopedLock sl (modelLock);
    const int idx = editVariation.load();
    return ! histories[(size_t) idx].empty();
}

bool ChordSlotsProcessor::undo()
{
    {
        const juce::ScopedLock sl (modelLock);
        const int idx = editVariation.load();
        if (histories[(size_t) idx].empty())
            return false;
        redoStacks[(size_t) idx].push_back (variations[(size_t) idx]);
        variations[(size_t) idx] = histories[(size_t) idx].back();
        histories[(size_t) idx].pop_back();
        publish();
    }
    sendChangeMessage();
    return true;
}

bool ChordSlotsProcessor::canRedo() const
{
    const juce::ScopedLock sl (modelLock);
    return ! redoStacks[(size_t) editVariation.load()].empty();
}

bool ChordSlotsProcessor::redo()
{
    {
        const juce::ScopedLock sl (modelLock);
        const int idx = editVariation.load();
        if (redoStacks[(size_t) idx].empty())
            return false;
        histories[(size_t) idx].push_back (variations[(size_t) idx]);
        variations[(size_t) idx] = redoStacks[(size_t) idx].back();
        redoStacks[(size_t) idx].pop_back();
        publish();
    }
    sendChangeMessage();
    return true;
}

void ChordSlotsProcessor::setMeter (int num, int den)
{
    edit ([num, den] (cs::Progression& p, cs::Rng&)
    {
        cs::setMeter (p, num, den);
    });
}

void ChordSlotsProcessor::copyVariation (int src, int dst)
{
    if (src < 0 || src >= cs::kNumVariations || dst < 0 || dst >= cs::kNumVariations || src == dst)
        return;

    {
        const juce::ScopedLock sl (modelLock);
        pushHistory (dst);
        variations[(size_t) dst] = variations[(size_t) src];
        publish();
    }
    sendChangeMessage();
}

void ChordSlotsProcessor::timerCallback()
{
    // MIDI keyswitches arrive on the audio thread. Defer host notification and
    // follow-playback UI updates here so the CLAP/VST/AU host APIs are never
    // called from processBlock. Compare-exchange preserves any newer request
    // which arrived while this update was being applied.
    const int request = pendingMidiVariation.load();
    if (request >= 0)
    {
        const int base = request >> 2;
        const int requestedVariation = request & 0x3;
        const int currentVariation = variationParam != nullptr
            ? juce::jlimit (0, cs::kNumVariations - 1, juce::roundToInt (variationParam->load()))
            : 0;
        if (base == currentVariation && pendingMidiVariation.load() == request)
        {
            if (followPlayback.load())
                setEditVariation (requestedVariation);
            if (auto* param = apvts.getParameter ("variation"))
                param->setValueNotifyingHost (param->convertTo0to1 ((float) requestedVariation));
        }

        int expected = request;
        pendingMidiVariation.compare_exchange_strong (expected, -1);
    }

    if (tempoDirty.exchange (false))
    {
        publish();
        sendChangeMessage();
    }
}

void ChordSlotsProcessor::publish()
{
    const juce::ScopedLock sl (modelLock);
    const cs::PerformanceParams perf = getPerformanceParams();
    const double bpm = lastBpm.load();
    publishedBpm.store (bpm);
    cs::PlayBank pb;
    for (int i = 0; i < cs::kNumVariations; ++i)
        pb.variations[i] = cs::toPlayData (variations[(size_t) i], perf, bpm);
    pb.song = cs::sanitizeSong (song);

    const juce::SpinLock::ScopedLockType psl (playLock);
    pendingPlayBank = pb;
    playDirty.store (true);
}

cs::PlayData ChordSlotsProcessor::getPlayData (int variationIndex) const
{
    const juce::SpinLock::ScopedLockType sl (playLock);
    return pendingPlayBank.variations[(size_t) juce::jlimit (0, cs::kNumVariations - 1, variationIndex)];
}

cs::Song ChordSlotsProcessor::getSong() const
{
    const juce::ScopedLock sl (modelLock);
    return cs::sanitizeSong (song);
}

void ChordSlotsProcessor::setSong (const cs::Song& value)
{
    { const juce::ScopedLock sl (modelLock); song = cs::sanitizeSong (value); }
    publish();
    sendChangeMessage();
}

//==============================================================================
void ChordSlotsProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
#if ! defined(CHORDSLOTS_MIDI_FX)
    synth.prepare (sampleRate);
#endif
    heldCount = 0;
    heldSlot = currentSlot = -1;
    auditionCount = 0;
    alternativeEvent = -1;
    alternativeHeld = {};
    pendingCount = 0;
    wasPlaying = false;
    resetTrigger();
    totalSamplesProcessed = 0;
}

void ChordSlotsProcessor::releaseResources()
{
#if ! defined(CHORDSLOTS_MIDI_FX)
    synth.allNotesOff();
#endif
    pendingCount = 0;
    heldCount = 0;
    resetTrigger();
}

bool ChordSlotsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if defined(CHORDSLOTS_MIDI_FX)
    return layouts.inputBuses.isEmpty() && layouts.outputBuses.isEmpty();
#else
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
#endif
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
            playBank = pendingPlayBank;
            playDirty.store (false);
        }
    }

    const int targetVar = getPlayingVariation();
    const auto quantize = quantizeSwitchParam != nullptr
        ? (cs::SwitchQuantize) juce::jlimit (0, 2, juce::roundToInt (quantizeSwitchParam->load()))
        : cs::SwitchQuantize::NextBar;

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
            if (const auto sig = pos->getTimeSignature())
            {
                if (sig->numerator > 0 && sig->denominator > 0)
                {
                    hostTimeSigNum.store (sig->numerator);
                    hostTimeSigDen.store (sig->denominator);
                }
            }
        }
    if (! (bpm >= 20.0 && bpm <= 999.0))
        bpm = 120.0;
    lastBpm.store (bpm);
    if (std::abs (bpm - publishedBpm.load()) > 0.01)
        tempoDirty.store (true);

    const bool playing = (hostPlaying || previewOn.load()) && numSamples > 0;
    const bool songPlayback = playing && songEnabled.load() && playBank.song.numEntries > 0;
    bool songStartDiscontinuity = songPlayback && ! songWasPlaying;
    const int songTotalSteps = songPlayback
        ? cs::songLengthSteps (playBank.song, playBank.variations, cs::kNumVariations) : 0;
    double songStartStep = expectedSongStep;
    cs::SongPosition songPosition;
    if (songPlayback && songTotalSteps > 0)
    {
        songStartStep = hostPlaying && havePpq
            ? std::fmod (ppq * (double) cs::kStepsPerQuarter, (double) songTotalSteps)
            : std::fmod (songStartStep, (double) songTotalSteps);
        if (songStartStep < 0.0)
            songStartStep += songTotalSteps;
        if (hostPlaying && havePpq)
        {
            double delta = songStartStep - expectedSongStep;
            delta -= songTotalSteps * std::round (delta / songTotalSteps);
            songStartDiscontinuity = songStartDiscontinuity || std::abs (delta) > 0.25;
        }
        songPosition = cs::songPositionAt (playBank.song, playBank.variations, cs::kNumVariations,
                                            (int) std::floor (songStartStep));
        if (songPosition.entry >= 0)
            activePlayingVariation = songPosition.variation;
    }
    if (! playing && activePlayingVariation != targetVar)
        activePlayingVariation = targetVar;

    const cs::PlayData& play = playBank.variations[activePlayingVariation];
    const double len = (double) juce::jmax (1, play.lengthSteps);
    const double stepsPerSample = (bpm / 60.0) * (double) cs::kStepsPerQuarter / sampleRate;
    const float gate = juce::jlimit (0.1f, 1.0f, gateParam->load());
    const cs::TriggerConfig trigCfg = getTriggerConfig();
    const bool split = isSplitMidi();

    auto clearPending = [&]
    {
        pendingCount = 0;
    };

    auto releaseAudition = [&] (int sample)
    {
        for (int i = 0; i < auditionCount; ++i)
            midi.addEvent (juce::MidiMessage::noteOff (auditionNotes[i].channel, auditionNotes[i].note), sample);
        auditionCount = 0;
    };

    auto releaseAlternative = [&]
    {
        if (alternativeEvent < 0)
            return;
        for (int ch = 0; ch < 2; ++ch)
            for (int note = 0; note < 128; ++note)
                if (alternativeHeld[(size_t) ch][(size_t) note])
                    midi.addEvent (juce::MidiMessage::noteOff (ch == 0 ? kMidiChannelChords : kMidiChannelBass, note), 0);
        alternativeHeld = {};
        alternativeEvent = -1;
    };
    if (numSamples > 0)
    {
        const bool available = ! playing && ! triggerEngine.isTriggerActive() && incomingMidi.isEmpty();
        if (! available)
            releaseAlternative();
        const juce::SpinLock::ScopedTryLockType lock (alternativeLock);
        if (lock.isLocked() && alternativeReady)
        {
            alternativeReady = false;
            if (available)
            {
                releaseAlternative();
                releaseAudition (0);
                auditionRequest.store (-1);
                activeAlternative.swap (pendingAlternative);
                alternativeEvent = 0;
                alternativeSample = 0;
            }
        }
    }

    auto triggerNote = [&] (int note, int vel, int sample, int channel = kMidiChannelChords, bool isBass = false)
    {
        if (heldCount < kMaxHeldNotes)
        {
            held[heldCount++] = { note, channel, isBass };
            midi.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) juce::jlimit (1, 127, vel)), sample);
        }
    };

    auto releaseHeld = [&] (int sample)
    {
        for (int i = 0; i < heldCount; ++i)
            midi.addEvent (juce::MidiMessage::noteOff (held[i].channel, held[i].note), sample);
        heldCount = 0;
        heldSlot = -1;
    };

    if (songPlayback != songWasPlaying)
    {
        releaseHeld (0);
        pendingCount = 0;
        currentSlot = -1;
        songWasPlaying = songPlayback;
    }
    else if (songPlayback && songStartDiscontinuity)
    {
        releaseHeld (0);
        pendingCount = 0;
        currentSlot = -1;
    }

    auto releaseBass = [&] (int sample)
    {
        int remaining = 0;
        for (int i = 0; i < heldCount; ++i)
        {
            if (held[i].isBass)
                midi.addEvent (juce::MidiMessage::noteOff (held[i].channel, held[i].note), sample);
            else
                held[remaining++] = held[i];
        }
        heldCount = remaining;
    };

    auto releaseUpper = [&] (int sample)
    {
        int remaining = 0;
        for (int i = 0; i < heldCount; ++i)
        {
            if (! held[i].isBass)
                midi.addEvent (juce::MidiMessage::noteOff (held[i].channel, held[i].note), sample);
            else
                held[remaining++] = held[i];
        }
        heldCount = remaining;

        int remainingP = 0;
        for (int pIdx = 0; pIdx < pendingCount; ++pIdx)
        {
            if (pendingNotes[pIdx].channel == kMidiChannelBass)
                pendingNotes[remainingP++] = pendingNotes[pIdx];
        }
        pendingCount = remainingP;
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
        // Notes triggered live from a MIDI keyboard stay sustained until released (not comped).
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
                    triggerNote (note, vel, onSample, kMidiChannelChords, false);
                else if (pendingCount < kMaxPendingNotes)
                    pendingNotes[pendingCount++] = { note, vel, kMidiChannelChords, onSample - numSamples, slot };
            }

            // Monophonic bass note to Ch 2 on the grid (offset 0)
            const int bassNote = s.primaryBass >= 0 ? s.primaryBass : (s.bassCount > 0 ? s.notes[(size_t) (s.bassCount - 1)] : 36);
            const int baseVel = (s.bassCount > 0 && s.velocities[(size_t) (s.bassCount - 1)] > 0) ? s.velocities[(size_t) (s.bassCount - 1)] : s.velocity;
            const int bassVel = juce::jlimit (1, 127, (int) std::lround (baseVel * (incomingVel / 100.0f)));

            if (voiceCount < cs::kMaxNotes)
                voices[voiceCount++] = { bassNote, kMidiChannelBass };

            triggerNote (bassNote, bassVel, sample, kMidiChannelBass, true);

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
                    triggerNote (note, vel, onSample, kMidiChannelChords, false);
                else if (pendingCount < kMaxPendingNotes)
                    pendingNotes[pendingCount++] = { note, vel, kMidiChannelChords, onSample - numSamples, slot };
            }
        }

        heldSlot = slot;
        currentSlot = slot;
        uiTriggeredSlot.store (slot);
        triggerGlowExpirySamples = totalSamplesProcessed + sample + (juce::int64) (0.12 * sampleRate);
    };

    const double msToTicks = (double) kTicksPerQuarter * bpm / 60000.0;
    const double ticksPerSample = stepsPerSample * (double) kTicksPerStep;

    auto slotDurationSteps = [&] (int slot) -> double
    {
        const auto& s = play.slots[slot];
        const double durTicks = juce::jmax (1.0, (double) (s.end - s.start) * (double) kTicksPerStep * (double) gate - (gate >= 0.999f ? 1.0 : 0.0));
        return durTicks / (double) kTicksPerStep;
    };

    auto hitDurationSteps = [&] (int slot, int hitIdx) -> double
    {
        const auto& s = play.slots[slot];
        if (hitIdx < 0 || hitIdx >= s.numHits)
            return 0.0;
        const double durTicks = juce::jmax (1.0, (double) s.hits[hitIdx].durationSteps * (double) kTicksPerStep * (double) gate - (gate >= 0.999f ? 1.0 : 0.0));
        return durTicks / (double) kTicksPerStep;
    };

    const int bassChannel = split ? kMidiChannelBass : kMidiChannelChords;
    const auto releasePatternBass = [&] (int note, int sample)
    {
        for (int i = 0; i < heldCount; ++i)
            if (held[i].isBass && held[i].channel == bassChannel && held[i].note == note)
            {
                midi.addEvent (juce::MidiMessage::noteOff (bassChannel, note), sample);
                held[i] = held[--heldCount];
                return;
            }
    };

    const auto startPatternBass = [&] (const cs::PlaySlot& slot, const cs::BassHit& hit, int sample)
    {
        releasePatternBass (hit.pitch, sample);
        triggerNote (hit.pitch, bassVelocity (slot, hit.velocityScale), sample, bassChannel, true);
    };

    const bool patternedBass = play.bassStyle != cs::BassStyle::Sustain;
    auto startSlotBass = [&] (int slot, int sample)
    {
        if (patternedBass)
            return;
        releaseBass (sample);
        const cs::PlaySlot& s = play.slots[slot];
        if (split)
        {
            const int bassNote = s.primaryBass >= 0 ? s.primaryBass : (s.bassCount > 0 ? s.notes[(size_t) (s.bassCount - 1)] : 36);
            const int bassVel = (s.bassCount > 0 && s.velocities[(size_t) (s.bassCount - 1)] > 0) ? s.velocities[(size_t) (s.bassCount - 1)] : s.velocity;
            triggerNote (bassNote, bassVel, sample, kMidiChannelBass, true);
        }
        else if (s.sustainBassUnderComping)
        {
            for (int i = 0; i < s.bassCount && i < s.n && i < cs::kMaxNotes; ++i)
            {
                const int note = s.notes[i];
                const int vel = s.velocities[i] > 0 ? s.velocities[i] : s.velocity;
                triggerNote (note, vel, sample, kMidiChannelChords, true);
            }
        }
    };

    auto startSlotUpperHit = [&] (int slot, int hitIdx, int sample, double hitStartStep = -1.0, double currentPlayStep = -1.0)
    {
        releaseUpper (sample);
        const cs::PlaySlot& s = play.slots[slot];
        if (hitIdx < 0 || hitIdx >= s.numHits)
            return;

        const auto& hit = s.hits[hitIdx];
        double elapsedTicks = 0.0;
        if (hitStartStep >= 0.0 && currentPlayStep > hitStartStep)
            elapsedTicks = (currentPlayStep - hitStartStep) * (double) kTicksPerStep;

        const int firstUpper = (split || patternedBass || s.sustainBassUnderComping) ? s.bassCount : 0;
        const double hitDurTicks = juce::jmax (1.0, (double) hit.durationSteps * (double) kTicksPerStep * (double) gate - 1.0);
        const double hitDurationMs = (double) hit.durationSteps * (60000.0 / (bpm * (double) cs::kStepsPerQuarter));
        double maxOffsetMs = 0.0;
        for (int i = firstUpper; i < s.n && i < cs::kMaxNotes; ++i)
            maxOffsetMs = std::max (maxOffsetMs, (double) s.offsetMs[i]);
        const double hitScale = cs::compHitOffsetScale (hitDurationMs, maxOffsetMs);

        for (int i = firstUpper; i < s.n && i < cs::kMaxNotes; ++i)
        {
            const int voice = hit.selectedVoice (firstUpper, s.n);
            if (voice >= 0 && i != voice)
                continue;
            const int note = s.notes[i];
            const int baseVel = s.velocities[i] > 0 ? s.velocities[i] : s.velocity;
            const int vel = juce::jlimit (1, 127, (int) std::lround ((float) baseVel * hit.velScale));
            const double noteOffsetTicks = (double) s.offsetMs[i] * hitScale * msToTicks;
            const double delayTicks = std::min (hitDurTicks - 1.0, noteOffsetTicks);

            if (elapsedTicks >= delayTicks)
            {
                triggerNote (note, vel, sample, kMidiChannelChords, false);
            }
            else
            {
                const double remainingTicks = delayTicks - elapsedTicks;
                const int delaySamples = (int) std::lround (remainingTicks / ticksPerSample);
                const int onSample = sample + delaySamples;

                if (onSample < numSamples)
                    triggerNote (note, vel, onSample, kMidiChannelChords, false);
                else if (pendingCount < kMaxPendingNotes)
                    pendingNotes[pendingCount++] = { note, vel, kMidiChannelChords, onSample - numSamples, slot };
            }
        }
    };

    // --- Ingest incoming MIDI events ---
    for (const auto meta : incomingMidi)
    {
        const auto m = meta.getMessage();
        const int sPos = juce::jlimit (0, numSamples - 1, meta.samplePosition);

        if (m.isNoteOn())
        {
            const int noteNum = m.getNoteNumber();
            if (noteNum >= 24 && noteNum <= 27)
            {
                requestMidiPlayingVariation (noteNum - 24);
                continue;
            }

            auto act = triggerEngine.onNoteOn (noteNum, m.getVelocity(), trigCfg);
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
            const int noteNum = m.getNoteNumber();
            if (noteNum >= 24 && noteNum <= 27)
                continue;

            auto act = triggerEngine.onNoteOff (noteNum, trigCfg);
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

    if (songPlayback && ! triggerSuppressesLoop && songTotalSteps > 0)
    {
        releaseAudition (0);
        auditionRequest.store (-1);
        const double songStart = songStartStep;
        const double songEnd = songStart + numSamples * stepsPerSample;
        int eventCount = 0;
        auto addSongEvent = [&] (double absStep, int note, int velocity, int channel, bool isBass, SongEventKind kind)
        {
            if (absStep < songStart - 1.0e-8 || absStep >= songEnd || eventCount >= kMaxSongEvents)
                return;
            const int sample = juce::jlimit (0, juce::jmax (0, numSamples - 1),
                (int) std::lround ((absStep - songStart) / stepsPerSample));
            songEvents[eventCount++] = { sample, juce::jlimit (0, 127, note), juce::jlimit (1, 127, velocity), channel, isBass, kind };
        };
        auto addSongNote = [&] (double on, double off, int pitch, int velocity, int channel, bool isBass, int transpose)
        {
            if (off < songStart - 1.0e-8 || on >= songEnd || off <= on)
                return;
            if (on < songStart - 1.0e-8 && ! songStartDiscontinuity)
            {
                addSongEvent (off, juce::jlimit (0, 127, pitch + transpose), velocity, channel, isBass, SongEventKind::Off);
                return;
            }
            const double clippedOn = std::max (on, songStart);
            const int note = juce::jlimit (0, 127, pitch + transpose);
            addSongEvent (clippedOn, note, velocity, channel, isBass, SongEventKind::On);
            addSongEvent (off, note, velocity, channel, isBass, SongEventKind::Off);
        };
        const double stepsPerMs = bpm * (double) cs::kStepsPerQuarter / 60000.0;
        const int firstCycle = (int) std::floor (songStart / songTotalSteps) - 1;
        const int lastCycle = (int) std::floor (songEnd / songTotalSteps);
        for (int cycle = firstCycle; cycle <= lastCycle; ++cycle)
        {
            const double cycleBase = (double) cycle * songTotalSteps;
            double sectionStart = cycleBase;
            for (int entry = 0; entry < playBank.song.numEntries; ++entry)
            {
                const auto songEntry = cs::sanitizeSongEntry (playBank.song.entries[entry]);
                const auto& section = playBank.variations[songEntry.variation];
                const double sectionEnd = sectionStart + section.lengthSteps;
                if (sectionEnd >= songStart - 1.0e-8 && sectionStart < songEnd)
                {
                    const int bassChannelForSong = split ? kMidiChannelBass : kMidiChannelChords;
                    for (int si = 0; si < section.numSlots; ++si)
                    {
                        const auto& slot = section.slots[si];
                        const double slotStart = sectionStart + slot.start;
                        const bool patterned = section.bassStyle != cs::BassStyle::Sustain;
                        const bool separateBass = split || patterned || slot.sustainBassUnderComping;
                        if (patterned)
                        {
                            for (int h = 0; h < slot.numBassHits; ++h)
                            {
                                const auto& hit = slot.bassHits[h];
                                const double on = sectionStart + slot.start + hit.stepOffset;
                                const double duration = bassDurationTicks (hit, gate) / (double) kTicksPerStep;
                                addSongNote (on, std::min (on + duration, sectionEnd), hit.pitch, bassVelocity (slot, hit.velocityScale),
                                             bassChannelForSong, true, songEntry.transpose);
                            }
                        }
                        else
                        {
                            const double slotOff = slotStart + gatedDurationTicks (slot.end - slot.start, gate) / kTicksPerStep;
                            if (split)
                            {
                                const int bassNote = slot.primaryBass >= 0 ? slot.primaryBass
                                    : (slot.bassCount > 0 ? slot.notes[slot.bassCount - 1] : 36);
                                const int bassVel = slot.bassCount > 0 && slot.velocities[slot.bassCount - 1] > 0
                                    ? slot.velocities[slot.bassCount - 1] : slot.velocity;
                                addSongNote (slotStart, slotOff, bassNote, bassVel, bassChannelForSong, true, songEntry.transpose);
                            }
                            else if (slot.sustainBassUnderComping)
                                for (int n = 0; n < slot.bassCount && n < slot.n; ++n)
                                {
                                    const int vel = slot.velocities[n] > 0 ? slot.velocities[n] : slot.velocity;
                                    addSongNote (slotStart, slotOff, slot.notes[n], vel, bassChannelForSong, true, songEntry.transpose);
                                }
                        }

                        const int firstUpper = separateBass ? slot.bassCount : 0;
                        for (int h = 0; h < slot.numHits; ++h)
                        {
                            const auto& hit = slot.hits[h];
                            const double hitOn = sectionStart + slot.start + hit.stepOffset;
                            const double hitOff = hitOn + gatedDurationTicks (hit.durationSteps, gate) / kTicksPerStep;
                            const double hitMs = hit.durationSteps * (60000.0 / (bpm * cs::kStepsPerQuarter));
                            double maxOffsetMs = 0.0;
                            for (int n = firstUpper; n < slot.n; ++n)
                                maxOffsetMs = std::max (maxOffsetMs, (double) slot.offsetMs[n]);
                            const double hitScale = cs::compHitOffsetScale (hitMs, maxOffsetMs);
                            for (int n = firstUpper; n < slot.n; ++n)
                            {
                                const int voice = hit.selectedVoice (firstUpper, slot.n);
                                if (voice >= 0 && n != voice)
                                    continue;
                                const double strum = std::min (hitOff - hitOn - 1.0 / kTicksPerStep,
                                    (double) slot.offsetMs[n] * hitScale * stepsPerMs);
                                const int vel = juce::jlimit (1, 127,
                                    (int) std::lround ((slot.velocities[n] > 0 ? slot.velocities[n] : slot.velocity) * hit.velScale));
                                addSongNote (hitOn + strum, hitOff, slot.notes[n], vel, kMidiChannelChords, false,
                                             songEntry.transpose);
                            }
                        }
                    }
                }
                sectionStart = sectionEnd;
            }
        }
        std::sort (songEvents, songEvents + eventCount, [] (const SongEvent& a, const SongEvent& b)
        {
            if (a.sample != b.sample) return a.sample < b.sample;
            return a.kind == SongEventKind::Off && b.kind == SongEventKind::On;
        });
        for (int i = 0; i < eventCount; ++i)
        {
            const auto& event = songEvents[i];
            if (event.kind == SongEventKind::On)
                triggerNote (event.note, event.velocity, event.sample, event.channel, event.bass);
            else
                for (int heldIndex = 0; heldIndex < heldCount; ++heldIndex)
                    if (held[heldIndex].note == event.note && held[heldIndex].channel == event.channel)
                    {
                        midi.addEvent (juce::MidiMessage::noteOff (event.channel, event.note), event.sample);
                        held[heldIndex] = held[--heldCount];
                        break;
                    }
        }
        expectedSongStep = std::fmod (songEnd, (double) songTotalSteps);
        if (expectedSongStep < 0.0)
            expectedSongStep += songTotalSteps;
        uiStep.store (songPosition.localStep + (songEnd - songStart));
    }
    else if (playing && ! triggerSuppressesLoop)
    {
        releaseAudition (0);
        auditionRequest.store (-1);

        double startStep = expectedStep;
        bool jumped = ! wasPlaying;

        if (hostPlaying && havePpq)
        {
            double hostStep = std::fmod (ppq * (double) cs::kStepsPerQuarter, len);
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

        // Handle quantized variation switching during playback
        if (activePlayingVariation != targetVar)
        {
            bool shouldSwitch = false;
            int switchSample = 0;
            if (quantize == cs::SwitchQuantize::Immediate)
            {
                shouldSwitch = true;
                switchSample = 0;
            }
            else if (quantize == cs::SwitchQuantize::NextBar)
            {
                const int curBar = (int) (startStep / (double) cs::kStepsPerBar);
                const int nextBar = (int) ((startStep + numSamples * stepsPerSample) / (double) cs::kStepsPerBar);
                if (curBar != nextBar || startStep + numSamples * stepsPerSample >= len)
                {
                    shouldSwitch = true;
                    const double barStep = (double) (curBar + 1) * (double) cs::kStepsPerBar;
                    switchSample = juce::jlimit (0, numSamples - 1, (int) std::lround ((barStep - startStep) / stepsPerSample));
                }
            }
            else if (quantize == cs::SwitchQuantize::NextSlot)
            {
                const int sNow = play.slotAt (startStep);
                const int sEnd = sNow >= 0 ? play.slots[sNow].end : (int) len;
                if (startStep + numSamples * stepsPerSample >= (double) sEnd)
                {
                    shouldSwitch = true;
                    switchSample = juce::jlimit (0, numSamples - 1, (int) std::lround (((double) sEnd - startStep) / stepsPerSample));
                }
            }

            if (shouldSwitch)
            {
                releaseHeld (switchSample);
                clearPending();
                activePlayingVariation = targetVar;
                currentSlot = -1;
                jumped = true;
            }
        }

        if (jumped)
        {
            releaseHeld (0);
            clearPending();
            currentSlot = -1;
        }

        // Make sure the chord under the playhead is the one sounding. This
        // covers starting mid-slot, relocations and edits made while playing.
        bool startedAtZeroBass = false;
        bool startedAtZeroHit = false;
        int startedHitIdx = -1;

        const int slotNow = play.slotAt (startStep);
        if (slotNow >= 0 && slotNow != currentSlot)
        {
            currentSlot = slotNow;
            heldSlot = slotNow;
            const auto& ps = play.slots[slotNow];
            const double bassOff = (double) ps.start + slotDurationSteps (slotNow);
            if (startStep < bassOff)
            {
                startSlotBass (slotNow, 0);
                startedAtZeroBass = true;
            }
            else if (! patternedBass)
            {
                releaseBass (0);
            }

            // Restore a sustain crossing the playhead only when starting inside
            // a loop or relocating an already-running transport. At a fresh
            // start on step zero there is no previous-loop note to recall.
            if (patternedBass && jumped && (wasPlaying || startStep > 0.0))
            {
                // Recall a hit already sounding at a seek position, including
                // a glide from the final slot across the loop wrap.
                for (int distance = std::min (1, play.numSlots - 1); distance >= 0; --distance)
                {
                    const int slot = (slotNow + play.numSlots - distance) % play.numSlots;
                    const auto& bassSlot = play.slots[slot];
                    const double offset = bassSlot.start > startStep ? -len : 0.0;
                    for (int h = 0; h < bassSlot.numBassHits; ++h)
                    {
                        const auto& hit = bassSlot.bassHits[h];
                        const double on = bassSlot.start + hit.stepOffset + offset;
                        if (startStep > on && startStep < on + bassDurationTicks (hit, gate) / kTicksPerStep)
                            startPatternBass (bassSlot, hit, 0);
                    }
                }
            }

            for (int h = 0; h < ps.numHits; ++h)
            {
                const double tHitOn = (double) (ps.start + ps.hits[h].stepOffset);
                const double tHitOff = tHitOn + hitDurationSteps (slotNow, h);
                if (startStep >= tHitOn && startStep < tHitOff)
                {
                    startSlotUpperHit (slotNow, h, 0, tHitOn, startStep);
                    startedAtZeroHit = true;
                    startedHitIdx = h;
                    break;
                }
            }
            if (! startedAtZeroHit)
                releaseUpper (0);
        }

        // Fixed upper bound keeps the audio callback allocation-free. Large host
        // blocks may span several short loops, so enumerate their actual loop range.
        int numEvents = 0;
        auto pushEvent = [&] (int sample, BlockEventType type, int slot, int hitIdx)
        {
            if (numEvents < kMaxBlockEvents)
                blockEvents[numEvents++] = { sample, type, slot, hitIdx };
        };
        double endStep = startStep + numSamples * stepsPerSample;
        if (std::abs (endStep - std::round (endStep)) < 1e-7)
            endStep = std::round (endStep);

        const int firstLoop = (int) std::floor (startStep / len) - 1;
        const int lastLoop = (int) std::floor (endStep / len);
        for (int k = firstLoop; k <= lastLoop; ++k)
        {
            const double loopOffset = (double) k * len;
            for (int i = 0; i < play.numSlots; ++i)
            {
                const auto& ps = play.slots[i];
                if (patternedBass)
                    for (int h = 0; h < ps.numBassHits; ++h)
                    {
                        const auto& hit = ps.bassHits[h];
                        const double on = ps.start + hit.stepOffset + loopOffset;
                        const double off = on + bassDurationTicks (hit, gate) / kTicksPerStep;
                        if (on >= startStep && on < endStep)
                            pushEvent (juce::jlimit (0, numSamples - 1, (int) std::lround ((on - startStep) / stepsPerSample)),
                                       BlockEventType::BassStart, i, h);
                        if (off >= startStep && off < endStep)
                            pushEvent (juce::jlimit (0, numSamples - 1, (int) std::lround ((off - startStep) / stepsPerSample)),
                                       BlockEventType::BassOff, i, h);
                    }

                // Bass note timeline
                const double tBassOn = (double) ps.start + loopOffset;
                const double tBassOff = (double) ps.start + slotDurationSteps (i) + loopOffset;

                if (tBassOn >= startStep && tBassOn < endStep)
                {
                    const int sample = juce::jlimit (0, numSamples - 1, (int) std::lround ((tBassOn - startStep) / stepsPerSample));
                    if (! (sample == 0 && startedAtZeroBass && i == currentSlot))
                        pushEvent (sample, BlockEventType::SlotStart, i, 0);
                }

                if (! patternedBass && tBassOff > startStep && tBassOff < endStep)
                {
                    const int sample = juce::jlimit (0, numSamples - 1, (int) std::lround ((tBassOff - startStep) / stepsPerSample));
                    pushEvent (sample, BlockEventType::SlotOff, i, 0);
                }

                // Upper hits timeline
                for (int h = 0; h < ps.numHits; ++h)
                {
                    const double tHitOn = (double) (ps.start + ps.hits[h].stepOffset) + loopOffset;
                    const double tHitOff = (double) (ps.start + ps.hits[h].stepOffset) + hitDurationSteps (i, h) + loopOffset;

                    if (tHitOn >= startStep && tHitOn < endStep)
                    {
                        const int sample = juce::jlimit (0, numSamples - 1, (int) std::lround ((tHitOn - startStep) / stepsPerSample));
                        if (! (sample == 0 && startedAtZeroHit && i == currentSlot && h == startedHitIdx))
                            pushEvent (sample, BlockEventType::HitStart, i, h);
                    }

                    if (tHitOff > startStep && tHitOff < endStep)
                    {
                        const int sample = juce::jlimit (0, numSamples - 1, (int) std::lround ((tHitOff - startStep) / stepsPerSample));
                        pushEvent (sample, BlockEventType::HitOff, i, h);
                    }
                }
            }
        }

        std::sort (blockEvents, blockEvents + numEvents, [] (const BlockEvent& a, const BlockEvent& b)
        {
            if (a.sample != b.sample)
                return a.sample < b.sample;
            const bool aIsOff = (a.type == BlockEventType::SlotOff || a.type == BlockEventType::HitOff || a.type == BlockEventType::BassOff);
            const bool bIsOff = (b.type == BlockEventType::SlotOff || b.type == BlockEventType::HitOff || b.type == BlockEventType::BassOff);
            if (aIsOff != bIsOff)
                return aIsOff;
            return false;
        });

        for (int e = 0; e < numEvents; ++e)
        {
            switch (blockEvents[e].type)
            {
                case BlockEventType::SlotStart:
                    startSlotBass (blockEvents[e].slot, blockEvents[e].sample);
                    heldSlot = blockEvents[e].slot;
                    currentSlot = blockEvents[e].slot;
                    break;
                case BlockEventType::SlotOff:
                    releaseBass (blockEvents[e].sample);
                    break;
                case BlockEventType::HitStart:
                    startSlotUpperHit (blockEvents[e].slot, blockEvents[e].hitIdx, blockEvents[e].sample);
                    break;
                case BlockEventType::HitOff:
                    releaseUpper (blockEvents[e].sample);
                    break;
                case BlockEventType::BassStart:
                {
                    const auto& bassSlot = play.slots[blockEvents[e].slot];
                    startPatternBass (bassSlot, bassSlot.bassHits[blockEvents[e].hitIdx], blockEvents[e].sample);
                    break;
                }
                case BlockEventType::BassOff:
                    releasePatternBass (play.slots[blockEvents[e].slot].bassHits[blockEvents[e].hitIdx].pitch, blockEvents[e].sample);
                    break;
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
            releaseAlternative();
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

    if (alternativeEvent >= 0 && numSamples > 0)
    {
        const auto& preview = *activeAlternative;
        const double samplesPerTick = preview.secondsPerTick * sampleRate;
        while (alternativeEvent < preview.notes.getNumEvents())
        {
            const auto& msg = preview.notes.getEventPointer (alternativeEvent)->message;
            const auto offset = (int64_t) std::llround (msg.getTimeStamp() * samplesPerTick) - alternativeSample;
            if (offset >= numSamples)
                break;
            if (msg.isNoteOnOrOff())
            {
                midi.addEvent (msg, (int) std::max (int64_t { 0 }, offset));
                alternativeHeld[msg.getChannel() == kMidiChannelBass ? 1 : 0][(size_t) msg.getNoteNumber()] = msg.isNoteOn();
            }
            ++alternativeEvent;
        }
        if (alternativeEvent >= preview.notes.getNumEvents())
            alternativeEvent = -1;
        else
        {
            alternativeSample += numSamples;
            if (preview.inContext)
                uiStep.store (alternativeSample / (samplesPerTick * kTicksPerStep));
        }
    }

    if (! triggerEngine.isTriggerActive() && totalSamplesProcessed >= triggerGlowExpirySamples)
        uiTriggeredSlot.store (-1);

    totalSamplesProcessed += numSamples;
    wasPlaying = playing && ! triggerEngine.isTriggerActive();

    const float gain = synthOnParam->load() > 0.5f ? synthVolParam->load() : 0.0f;
#if ! defined(CHORDSLOTS_MIDI_FX)
    synth.render (buffer, midi, gain);
#else
    juce::ignoreUnused (gain);
#endif
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

namespace
{
void appendMidiSection (const cs::Progression& p, const cs::PerformanceParams& perf, double bpm, float gate,
                        double startTick, double clipEnd, int transpose,
                        juce::MidiMessageSequence* chords, juce::MidiMessageSequence* bass)
{
    const auto play = cs::toPlayData (p, perf, bpm);
    const bool combined = chords != nullptr && chords == bass;
    const bool patterned = play.bassStyle != cs::BassStyle::Sustain;
    const int bassChannel = combined ? kMidiChannelChords : kMidiChannelBass;
    const double msToTicks = kTicksPerQuarter * bpm / 60000.0;
    const auto notePair = [transpose] (juce::MidiMessageSequence& seq, int channel, int pitch, int velocity,
                                       double start, double end)
    {
        const int note = juce::jlimit (0, 127, pitch + transpose);
        seq.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) juce::jlimit (1, 127, velocity)), start);
        seq.addEvent (juce::MidiMessage::noteOff (channel, note), end);
    };

    auto& markerTrack = chords != nullptr ? *chords : *bass;
    markerTrack.addEvent (juce::MidiMessage::timeSignatureMetaEvent (p.timeSigNum, p.timeSigDen), startTick);
    for (int i = 0; i < play.numSlots; ++i)
    {
        const auto& slot = play.slots[i];
        const double base = startTick + slot.start * (double) kTicksPerStep;
        const double off = base + gatedDurationTicks (slot.end - slot.start, gate);
        auto chord = p.slots[(size_t) i].chord;
        chord.root = cs::mod12 (chord.root + transpose);
        const auto name = cs::chordName (chord, cs::mod12 (p.tonic + transpose), p.scale);
        const auto label = juce::String::fromUTF8 (name.c_str());
        markerTrack.addEvent (juce::MidiMessage::textMetaEvent (1, label), base);
        markerTrack.addEvent (juce::MidiMessage::textMetaEvent (6, label), base);

        const bool separateBass = ! combined || patterned || slot.sustainBassUnderComping;
        if (bass != nullptr)
        {
            if (patterned)
            {
                for (int h = 0; h < slot.numBassHits; ++h)
                {
                    const auto& hit = slot.bassHits[h];
                    const double on = base + hit.stepOffset * (double) kTicksPerStep;
                    const double end = std::min (clipEnd, on + bassDurationTicks (hit, gate));
                    notePair (*bass, bassChannel, hit.pitch, bassVelocity (slot, hit.velocityScale), on, end);
                }
            }
            else if (! combined)
            {
                notePair (*bass, bassChannel, slot.primaryBass, bassVelocity (slot), base, off);
            }
            else if (separateBass)
            {
                for (int j = 0; j < std::min (slot.bassCount, slot.n); ++j)
                    notePair (*bass, bassChannel, slot.notes[j], slot.velocities[j], base, off);
            }
        }

        if (chords == nullptr)
            continue;

        const int firstUpper = separateBass ? slot.bassCount : 0;
        double maxOffset = 0.0;
        for (int j = firstUpper; j < slot.n; ++j)
            maxOffset = std::max (maxOffset, (double) slot.offsetMs[j]);

        for (int h = 0; h < slot.numHits; ++h)
        {
            const auto& hit = slot.hits[h];
            const double on = base + hit.stepOffset * (double) kTicksPerStep;
            const double end = on + gatedDurationTicks (hit.durationSteps, gate);
            const double hitMs = hit.durationSteps * 60000.0 / (bpm * cs::kStepsPerQuarter);
            const double scale = cs::compHitOffsetScale (hitMs, maxOffset);
            for (int j = firstUpper; j < slot.n; ++j)
            {
                const int voice = hit.selectedVoice (firstUpper, slot.n);
                if (voice >= 0 && j != voice)
                    continue;
                const double noteOn = std::min (end - 1.0, on + slot.offsetMs[j] * scale * msToTicks);
                const int velocity = (int) std::lround (slot.velocities[j] * hit.velScale);
                notePair (*chords, kMidiChannelChords, slot.notes[j], velocity, noteOn, end);
            }
        }
    }
}

void closeMidiTrack (juce::MidiFile& file, juce::MidiMessageSequence& sequence, double endTick)
{
    sequence.addEvent (juce::MidiMessage::endOfTrack(), endTick);
    sequence.updateMatchedPairs();
    file.addTrack (sequence);
}
} // namespace

void ChordSlotsProcessor::auditionCompStyle (cs::CompStyle style)
{
    auto p = getProgression();
    if (p.numSlots <= 0)
        return;

    p.numSlots = 1;
    p.slots[0].start = 0;
    p.bars = cs::compPreviewBars (style);
    p.timeSigNum = p.timeSigDen = 4;
    p.bassStyle = cs::BassStyle::Sustain;

    auto perf = getPerformanceParams();
    perf.compStyle = style;
    const auto bpm = lastBpm.load();
    const auto end = p.lengthSteps() * (double) kTicksPerStep;
    auto preview = std::make_unique<AlternativePreview>();
    preview->secondsPerTick = 60.0 / (bpm * kTicksPerQuarter);
    auto& notes = preview->notes;
    juce::MidiMessageSequence bass;
    appendMidiSection (p, perf, bpm, gateParam->load(), 0.0, end, 0, &notes, isSplitMidi() ? &bass : &notes);
    notes.addSequence (bass, 0.0);
    notes.addEvent (juce::MidiMessage::endOfTrack(), end);
    queueAudition (std::move (preview));
}

void ChordSlotsProcessor::auditionBassPattern (const cs::Progression& p)
{
    const auto bpm = lastBpm.load();
    const auto end = p.lengthSteps() * (double) kTicksPerStep;
    auto preview = std::make_unique<AlternativePreview>();
    preview->secondsPerTick = 60.0 / (bpm * kTicksPerQuarter);
    appendMidiSection (p, getPerformanceParams(), bpm, gateParam->load(), 0.0, end, 0, nullptr, &preview->notes);
    preview->notes.addEvent (juce::MidiMessage::endOfTrack(), end);
    queueAudition (std::move (preview));
}

void ChordSlotsProcessor::auditionAlternative (int slot, cs::Chord chord, bool inContext)
{
    auto p = getProgression();
    if (slot < 0 || slot >= p.numSlots)
        return;

    // Audition the same voicing a subsequent swap would produce.
    auto previewRng = [&]
    {
        const juce::ScopedLock lock (modelLock);
        return rng;
    }();
    cs::setChord (p, slot, chord, previewRng);
    auto preview = std::make_unique<AlternativePreview>();
    const auto bpm = lastBpm.load();
    const bool split = isSplitMidi();
    preview->secondsPerTick = 60.0 / (bpm * kTicksPerQuarter);
    preview->inContext = inContext;

    auto& notes = preview->notes;
    if (inContext)
    {
        juce::MidiMessageSequence bass;
        const auto end = p.lengthSteps() * (double) kTicksPerStep;
        const auto gate = juce::jlimit (0.1f, 1.0f, gateParam->load());
        appendMidiSection (p, getPerformanceParams(), bpm, gate, 0.0, end, 0, &notes, split ? &bass : &notes);
        notes.addSequence (bass, 0.0);
        notes.addEvent (juce::MidiMessage::endOfTrack(), end);
    }
    else
    {
        const auto& v = p.slots[(size_t) slot].voicing;
        for (int i = 0; i < v.n; ++i)
        {
            const int channel = split && i < v.bassCount ? kMidiChannelBass : kMidiChannelChords;
            const int note = v.notes[(size_t) i];
            notes.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) v.velocity), 0.0);
            notes.addEvent (juce::MidiMessage::noteOff (channel, note), 0.9 / preview->secondsPerTick);
        }
    }
    queueAudition (std::move (preview));
}

void ChordSlotsProcessor::queueAudition (std::unique_ptr<AlternativePreview> preview)
{
    preview->notes.updateMatchedPairs();
    const juce::SpinLock::ScopedLockType lock (alternativeLock);
    pendingAlternative.swap (preview);
    alternativeReady = true;
}

juce::MidiFile ChordSlotsProcessor::createMidiFile (MidiExportMode mode) const
{
    if (mode == MidiExportMode::Song)
        return createSongMidiFile();
    const auto p = getProgression();
    const auto perf = getPerformanceParams();
    const double bpm = lastBpm.load();
    const float gate = juce::jlimit (0.1f, 1.0f, gateParam->load());
    const double endTick = p.lengthSteps() * (double) kTicksPerStep;
    juce::MidiFile file;
    file.setTicksPerQuarterNote (kTicksPerQuarter);
    juce::MidiMessageSequence first, second;
    const bool split = mode == MidiExportMode::Master && isSplitMidi();
    first.addEvent (juce::MidiMessage::textMetaEvent (3,
        mode == MidiExportMode::BassOnly ? "Bass" : (mode == MidiExportMode::ChordsOnly || split ? "Chords" : "ChordSlots")), 0.0);
    first.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / bpm)), 0.0);
    if (split)
        second.addEvent (juce::MidiMessage::textMetaEvent (3, "Bass"), 0.0);

    auto* chords = mode == MidiExportMode::BassOnly ? nullptr : &first;
    auto* bass = mode == MidiExportMode::ChordsOnly ? nullptr : (split ? &second : &first);
    appendMidiSection (p, perf, bpm, gate, 0.0, endTick, 0, chords, bass);
    closeMidiTrack (file, first, endTick);
    if (split)
        closeMidiTrack (file, second, endTick);
    return file;
}

juce::MidiFile ChordSlotsProcessor::createSongMidiFile() const
{
    const auto arrangement = getSong();
    const auto perf = getPerformanceParams();
    const double bpm = lastBpm.load();
    const float gate = juce::jlimit (0.1f, 1.0f, gateParam->load());
    juce::MidiFile file;
    file.setTicksPerQuarterNote (kTicksPerQuarter);
    juce::MidiMessageSequence chords, bass;
    chords.addEvent (juce::MidiMessage::textMetaEvent (3, "ChordSlots Song"), 0.0);
    bass.addEvent (juce::MidiMessage::textMetaEvent (3, "Bass"), 0.0);
    chords.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / bpm)), 0.0);
    double offset = 0.0;
    for (int i = 0; i < arrangement.numEntries; ++i)
    {
        const auto e = arrangement.entries[i];
        const auto p = getProgression (e.variation);
        const double length = p.lengthSteps() * (double) kTicksPerStep;
        const juce::String marker = "Section " + juce::String::charToString ((juce::juce_wchar) ('A' + e.variation));
        chords.addEvent (juce::MidiMessage::textMetaEvent (6, marker), offset);
        appendMidiSection (p, perf, bpm, gate, offset, offset + length, e.transpose, &chords, &bass);
        offset += length;
    }
    closeMidiTrack (file, chords, offset);
    closeMidiTrack (file, bass, offset);
    return file;
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
    else if (mode == MidiExportMode::Song)
        suffix = " Song";

    const auto name = juce::File::createLegalFileName ("ChordSlots" + suffix + " " + describeProgression()).substring (0, 80);
    const auto file = dir.getChildFile (name + ".mid");
    file.deleteFile();

    juce::FileOutputStream out (file);
    const auto midi = createMidiFile (mode);
    if (! out.openedOk() || ! midi.writeTo (out, midi.getNumTracks() == 1 ? 0 : 1))
        return {};
    out.flush();
    return file;
}

//==============================================================================
void ChordSlotsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("ChordSlotsState");
    root.setProperty ("version", 4, nullptr);
    root.setProperty ("editVariation", editVariation.load(), nullptr);
    root.setProperty ("followPlayback", followPlayback.load(), nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    juce::ValueTree songTree ("Song");
    songTree.setProperty ("enabled", songEnabled.load(), nullptr);
    const auto arrangement = getSong();
    songTree.setProperty ("count", arrangement.numEntries, nullptr);
    for (int i = 0; i < arrangement.numEntries; ++i)
    {
        juce::ValueTree entry ("Entry");
        entry.setProperty ("variation", arrangement.entries[i].variation, nullptr);
        entry.setProperty ("transpose", arrangement.entries[i].transpose, nullptr);
        songTree.appendChild (entry, nullptr);
    }
    root.appendChild (songTree, nullptr);

    const char* varNames[cs::kNumVariations] = { "ProgressionA", "ProgressionB", "ProgressionC", "ProgressionD" };
    for (int i = 0; i < cs::kNumVariations; ++i)
    {
        auto vt = progressionToTree (getProgression (i));
        juce::ValueTree container (varNames[i]);
        container.appendChild (vt, nullptr);
        root.appendChild (container, nullptr);
    }

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

    pendingMidiVariation.store (-1);

    const int ver = (int) root.getProperty ("version", 1);
    const char* varNames[cs::kNumVariations] = { "ProgressionA", "ProgressionB", "ProgressionC", "ProgressionD" };

    {
        const juce::ScopedLock sl (modelLock);
        for (int i = 0; i < cs::kNumVariations; ++i)
        {
            histories[(size_t) i].clear();
            redoStacks[(size_t) i].clear();
        }

        song = {};
        const auto songTree = root.getChildWithName ("Song");
        songEnabled.store ((bool) songTree.getProperty ("enabled", false));
        if (songTree.isValid())
        {
            cs::Song restored;
            for (int i = 0; i < songTree.getNumChildren() && restored.numEntries < cs::kMaxSongEntries; ++i)
            {
                const auto item = songTree.getChild (i);
                if (item.hasType ("Entry"))
                    restored.entries[restored.numEntries++] = { (int) item.getProperty ("variation", 0),
                                                                 (int) item.getProperty ("transpose", 0) };
            }
            song = cs::sanitizeSong (restored);
        }

        if (ver >= 2)
        {
            for (int i = 0; i < cs::kNumVariations; ++i)
            {
                const auto container = root.getChildWithName (varNames[i]);
                if (container.isValid())
                {
                    const auto pt = container.getChildWithName ("Progression");
                    if (pt.isValid())
                        progressionFromTree (pt, variations[(size_t) i], ver);
                }
            }
            editVariation.store (juce::jlimit (0, cs::kNumVariations - 1, (int) root.getProperty ("editVariation", 0)));
            followPlayback.store ((bool) root.getProperty ("followPlayback", false));
        }
        else
        {
            // Legacy v1 preset migration: single <Progression> child
            cs::Progression legacy;
            if (progressionFromTree (root.getChildWithName ("Progression"), legacy, ver))
            {
                for (int i = 0; i < cs::kNumVariations; ++i)
                    variations[(size_t) i] = legacy;
            }
            editVariation.store (0);
            followPlayback.store (false);
        }
        publish();
    }

    // Restore parameter tree AFTER models are restored!
    auto params = root.getChildWithName (apvts.state.getType()).createCopy();
    if (params.isValid())
    {
        // APVTS retains the current value for missing entries, so migrate
        // presets saved before the chord-rhythm parameter explicitly.
        if (! params.getChildWithProperty ("id", "chordRhythm").isValid())
        {
            juce::ValueTree rhythm ("PARAM");
            rhythm.setProperty ("id", "chordRhythm", nullptr);
            rhythm.setProperty ("value", 0, nullptr);
            params.appendChild (rhythm, nullptr);
        }
        apvts.replaceState (params);
    }
    else
    {
        apvts.getParameter ("chordRhythm")->setValueNotifyingHost (0.0f);
    }
    pendingMidiVariation.store (-1);

    if (synthModelParam != nullptr)
        setSynthModel (static_cast<PreviewSoundModel> (juce::roundToInt (synthModelParam->load())));

    sendChangeMessage();
}

juce::AudioProcessorEditor* ChordSlotsProcessor::createEditor()
{
    return new ChordSlotsEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChordSlotsProcessor();
}
