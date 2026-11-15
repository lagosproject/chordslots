// Headless tests of the audio processor: drives processBlock with a fake host
// transport and checks the MIDI and audio that come out.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "../plugin/PluginProcessor.h"

#include <cstdio>
#include <map>

static int failures = 0;
static int checks = 0;

#define CHECK(cond) \
    do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (false)

namespace
{

struct FakePlayHead final : juce::AudioPlayHead
{
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm);
        info.setIsPlaying (playing);
        info.setPpqPosition (ppq);
        info.setTimeSignature (TimeSignature { timeSigNum, timeSigDen });
        return info;
    }

    double bpm = 120.0;
    double ppq = 0.0;
    bool playing = false;
    int timeSigNum = 4;
    int timeSigDen = 4;
};

struct NoteEvent
{
    juce::int64 sample;
    int note;
    bool on;
    int velocity;
    int channel = 1;
};

struct Host
{
    explicit Host (double sr = 48000.0, int bs = 512) : sampleRate (sr), blockSize (bs)
    {
        processor.setPlayHead (&playHead);
        processor.setRateAndBufferSizeDetails (sr, bs);
        processor.prepareToPlay (sr, bs);
    }

    void run (int numBlocks)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        for (int b = 0; b < numBlocks; ++b)
        {
            juce::MidiBuffer midi;
            midi.swapWith (injectedMidi);
            processor.processBlock (buffer, midi);

            for (const auto meta : midi)
            {
                const auto m = meta.getMessage();
                CHECK (meta.samplePosition >= 0 && meta.samplePosition < blockSize);
                if (m.isNoteOn())
                    events.push_back ({ position + meta.samplePosition, m.getNoteNumber(), true, m.getVelocity(), m.getChannel() });
                else if (m.isNoteOff())
                    events.push_back ({ position + meta.samplePosition, m.getNoteNumber(), false, 0, m.getChannel() });
            }

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < blockSize; ++i)
                {
                    const float v = buffer.getSample (ch, i);
                    if (! std::isfinite (v))
                        nonFinite = true;
                    peak = juce::jmax (peak, std::abs (v));
                }

            position += blockSize;
            if (playHead.playing)
                playHead.ppq += blockSize / sampleRate * playHead.bpm / 60.0;
        }
    }

    void injectNoteOn (int note, int vel = 100, int sample = 0)
    {
        injectedMidi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) juce::jlimit (1, 127, vel)), sample);
    }

    void injectNoteOff (int note, int sample = 0)
    {
        injectedMidi.addEvent (juce::MidiMessage::noteOff (1, note), sample);
    }

    void injectCC (int cc, int val, int sample = 0)
    {
        injectedMidi.addEvent (juce::MidiMessage::controllerEvent (1, cc, val), sample);
    }

    /** Number of notes still sounding according to the emitted MIDI; also
        checks that on/off events are properly paired. */
    int heldAfterReplay() const
    {
        std::map<int, int> held;
        for (const auto& e : events)
        {
            if (e.on)
            {
                CHECK (held[e.note] == 0); // never re-trigger a note that is still held
                ++held[e.note];
            }
            else
            {
                CHECK (held[e.note] == 1); // never release a note that is not held
                --held[e.note];
            }
        }
        int n = 0;
        for (const auto& kv : held)
            n += kv.second;
        return n;
    }

    std::vector<juce::int64> chordStarts() const
    {
        std::vector<juce::int64> starts;
        for (const auto& e : events)
            if (e.on && (starts.empty() || starts.back() != e.sample))
                starts.push_back (e.sample);
        return starts;
    }

    double sampleRate;
    int blockSize;
    FakePlayHead playHead;
    ChordSlotsProcessor processor;
    std::vector<NoteEvent> events;
    juce::MidiBuffer injectedMidi;
    juce::int64 position = 0;
    float peak = 0.0f;
    bool nonFinite = false;
};

double samplesPerStep (const Host& h) { return h.sampleRate * 60.0 / h.playHead.bpm / (double) cs::kStepsPerQuarter; }

void testHostSyncedPlayback()
{
    Host h;
    h.processor.edit ([] (cs::Progression& p, cs::Rng& rng)
    {
        p.bars = 2;
        p.numSlots = 4;
        cs::layoutEven (p);
        p.slots[1].start = 36;  // irregular cuts
        p.slots[2].start = 96;
        p.slots[3].start = 162;
        p.locks.rhythm = true;
        for (int i = 0; i < p.numSlots; ++i)
            p.slots[(size_t) i].targetE = 1.0f; // densest, loudest voicings
        cs::roll (p, rng);
    });
    const cs::Progression p = h.processor.getProgression();

    h.run (4); // stopped: silence, no MIDI
    CHECK (h.events.empty());
    CHECK (h.processor.getPlayPosition() < 0.0);

    h.playHead.playing = true;
    const juce::int64 playStart = h.position;
    const double sps = samplesPerStep (h);
    const int loops = 3;
    const int blocks = (int) std::ceil (loops * p.lengthSteps() * sps / h.blockSize);
    h.run (blocks);
    CHECK (h.processor.getPlayPosition() >= 0.0);

    // Every slot of every loop starts once, on time (within one sample).
    const auto starts = h.chordStarts();
    CHECK ((int) starts.size() >= loops * p.numSlots);
    for (int i = 0; i < loops * p.numSlots && i < (int) starts.size(); ++i)
    {
        const int loop = i / p.numSlots, slot = i % p.numSlots;
        const double expected = (double) playStart + (loop * p.lengthSteps() + p.slots[(size_t) slot].start) * sps;
        CHECK (std::abs ((double) starts[(size_t) i] - expected) <= 1.5);
    }

    // The notes are the slot's voicing, at its velocity.
    size_t e = 0;
    for (int slot = 0; slot < p.numSlots; ++slot)
    {
        while (e < h.events.size() && ! h.events[e].on)
            ++e;
        for (int j = 0; j < p.slots[(size_t) slot].voicing.n; ++j, ++e)
        {
            CHECK (e < h.events.size() && h.events[e].on);
            if (e < h.events.size())
            {
                CHECK (h.events[e].note == p.slots[(size_t) slot].voicing.notes[(size_t) j]);
                CHECK (h.events[e].velocity == p.slots[(size_t) slot].voicing.velocity);
            }
        }
    }

    h.playHead.playing = false;
    h.run (40);
    CHECK (h.heldAfterReplay() == 0); // stopping releases everything
    CHECK (! h.nonFinite);
    CHECK (h.peak > 0.02f);  // the preview synth is audible
    CHECK (h.peak < 0.99f);  // and does not clip on the densest voicing
    std::printf ("  preview synth peak at full energy: %.2f\n", (double) h.peak);
}

void testGateAndRelocation()
{
    Host h;
    h.processor.apvts.getParameter ("gate")->setValueNotifyingHost (
        h.processor.apvts.getParameter ("gate")->convertTo0to1 (0.5f));
    h.processor.edit ([] (cs::Progression& p, cs::Rng&)
    {
        p.bars = 1;
        p.numSlots = 2;
        cs::layoutEven (p);
    });
    const double sps = samplesPerStep (h);

    // Start in the middle of slot 1 but after its gate has closed: silence
    // until the loop wraps to slot 0.
    h.playHead.ppq = 3.5; // step 84 of 96; slot 1 spans 48..96, gate closes at 72
    h.playHead.playing = true;
    h.run ((int) std::ceil (6.0 * sps / h.blockSize));
    CHECK (h.events.empty());
    h.run ((int) std::ceil (10.0 * sps / h.blockSize));
    CHECK (! h.events.empty() && h.events.front().on);

    // With gate 0.5 each chord lasts half its slot.
    h.run ((int) std::ceil (96.0 * sps / h.blockSize));
    juce::int64 on = -1;
    int measured = 0;
    for (const auto& ev : h.events)
    {
        if (ev.on && on < 0)
            on = ev.sample;
        else if (! ev.on && on >= 0)
        {
            CHECK (std::abs ((double) (ev.sample - on) - 24.0 * sps) <= 2.0);
            on = -1;
            ++measured;
        }
    }
    CHECK (measured >= 2);

    // Relocating into the first half of a slot starts that slot's chord at once.
    const size_t mark = h.events.size();
    h.playHead.ppq = 100.0 * 4.0 + 2.25; // bar 101, step 9 -> slot 1, gate still open
    h.run (1);
    bool sawOnAtZero = false;
    for (size_t i = mark; i < h.events.size(); ++i)
        sawOnAtZero = sawOnAtZero || (h.events[i].on && h.events[i].sample == h.position - h.blockSize);
    CHECK (sawOnAtZero);

    h.playHead.playing = false;
    h.run (4);
    CHECK (h.heldAfterReplay() == 0);
}

void testPreviewAndAudition()
{
    Host h;
    const cs::Progression p = h.processor.getProgression();

    h.processor.auditionSlot (1);
    h.run (2);
    CHECK ((int) h.events.size() == p.slots[1].voicing.n);
    h.run ((int) (1.2 * h.sampleRate / h.blockSize));
    CHECK (h.heldAfterReplay() == 0); // the audition releases itself

    h.events.clear();
    h.processor.setPreviewPlaying (true); // internal clock, host stopped
    h.run ((int) std::ceil (p.lengthSteps() * samplesPerStep (h) / h.blockSize) + 2);
    CHECK ((int) h.chordStarts().size() >= p.numSlots);
    h.processor.setPreviewPlaying (false);
    h.run (4);
    CHECK (h.heldAfterReplay() == 0);
}

void testEditWhilePlaying()
{
    Host h;
    h.playHead.playing = true;
    juce::Random random (3);
    for (int i = 0; i < 300; ++i)
    {
        h.run (3);
        switch (random.nextInt (5))
        {
            case 0: h.processor.edit ([] (cs::Progression& p, cs::Rng& rng) { cs::roll (p, rng); }); break;
            case 1: h.processor.edit ([&] (cs::Progression& p, cs::Rng& rng) { cs::setNumSlots (p, 1 + random.nextInt (16)); cs::roll (p, rng); }); break;
            case 2: h.processor.edit ([&] (cs::Progression& p, cs::Rng&) { cs::setBars (p, 1 << random.nextInt (4)); }); break;
            case 3: h.processor.edit ([&] (cs::Progression& p, cs::Rng&) { cs::moveCut (p, 1, random.nextInt (p.lengthSteps())); }); break;
            default: h.processor.undo(); break;
        }
    }
    h.playHead.playing = false;
    h.run (8);
    CHECK (h.heldAfterReplay() == 0); // no stuck notes, whatever was edited mid-playback
    CHECK (! h.nonFinite);
}

void testStateRoundTrip()
{
    Host a;
    a.processor.edit ([] (cs::Progression& p, cs::Rng& rng)
    {
        p.tonic = 3;
        p.bars = 2;
        cs::setNumSlots (p, 5);
        cs::applyMood (p, 5);
        p.style = (int) cs::CorpusStyle::SoulRnb;
        p.locks.colors = true;
        cs::roll (p, rng);
        p.slots[2].locked = true;
        cs::moveCut (p, 2, 11);
    });
    juce::MemoryBlock state;
    a.processor.getStateInformation (state);

    Host b;
    b.processor.setStateInformation (state.getData(), (int) state.getSize());
    const cs::Progression pa = a.processor.getProgression(), pb = b.processor.getProgression();

    CHECK (pa.tonic == pb.tonic && pa.scale == pb.scale && pa.bars == pb.bars && pa.numSlots == pb.numSlots);
    CHECK (pa.mood == pb.mood && pa.locks.colors == pb.locks.colors && pa.snap == pb.snap);
    CHECK (pa.style == pb.style && pb.style == (int) cs::CorpusStyle::SoulRnb);
    CHECK (std::abs (pa.adventure - pb.adventure) < 1.0e-4f);
    for (int i = 0; i < pa.numSlots; ++i)
    {
        const cs::Slot& x = pa.slots[(size_t) i];
        const cs::Slot& y = pb.slots[(size_t) i];
        CHECK (x.start == y.start && x.chord == y.chord && x.locked == y.locked);
        CHECK (x.voicing.n == y.voicing.n && x.voicing.notes == y.voicing.notes && x.voicing.velocity == y.voicing.velocity);
        CHECK (std::abs (x.targetB - y.targetB) < 1.0e-4f && std::abs (x.brightness - y.brightness) < 1.0e-4f);
    }
    CHECK (a.processor.describeProgression() == b.processor.describeProgression());

    // v3 state without the "style" attribute loads as Auto (-1)
    {
        Host hV3;
        juce::ValueTree v3Root ("ChordSlotsState");
        v3Root.setProperty ("version", 3, nullptr);
        juce::ValueTree v3Container ("ProgressionA");
        juce::ValueTree v3Prog ("Progression");
        v3Prog.setProperty ("bars", 2, nullptr);
        // Note: NO "style" property set here
        juce::ValueTree slot0 ("Slot"); slot0.setProperty ("start", 0, nullptr); slot0.setProperty ("root", 0, nullptr); slot0.setProperty ("type", "", nullptr); v3Prog.appendChild (slot0, nullptr);
        v3Container.appendChild (v3Prog, nullptr);
        v3Root.appendChild (v3Container, nullptr);

        juce::MemoryBlock v3Block;
        juce::AudioProcessor::copyXmlToBinary (*v3Root.createXml(), v3Block);
        hV3.processor.setStateInformation (v3Block.getData(), (int) v3Block.getSize());
        CHECK (hV3.processor.getProgression().style == -1);
    }

    // Junk and hostile state must be ignored or clamped, never crash.
    const char junk[] = "this is not a ChordSlots state";
    b.processor.setStateInformation (junk, (int) sizeof (junk));
    b.processor.setStateInformation (nullptr, 0);

    juce::ValueTree root ("ChordSlotsState");
    juce::ValueTree prog ("Progression");
    prog.setProperty ("tonic", -77, nullptr);
    prog.setProperty ("scale", 9999, nullptr);
    prog.setProperty ("bars", 100000, nullptr);
    prog.setProperty ("snap", 0, nullptr);
    for (int i = 0; i < 40; ++i)
    {
        juce::ValueTree slot ("Slot");
        slot.setProperty ("start", i % 2 == 0 ? 100000 : -5, nullptr);
        slot.setProperty ("root", 1234, nullptr);
        slot.setProperty ("type", "nonsense", nullptr);
        slot.setProperty ("notes", "999 -4 abc 60 60 60 60 60 60 60 60 60 60 60", nullptr);
        slot.setProperty ("velocity", 100000, nullptr);
        prog.appendChild (slot, nullptr);
    }
    root.appendChild (prog, nullptr);
    juce::MemoryBlock hostile;
    juce::AudioProcessor::copyXmlToBinary (*root.createXml(), hostile);
    b.processor.setStateInformation (hostile.getData(), (int) hostile.getSize());

    const cs::Progression ph = b.processor.getProgression();
    CHECK (ph.tonic >= 0 && ph.tonic < 12 && ph.scale < cs::numScales() && ph.bars <= cs::kMaxBars);
    CHECK (ph.numSlots >= 1 && ph.numSlots <= cs::kMaxSlots && ph.slots[0].start == 0);
    for (int i = 1; i < ph.numSlots; ++i)
        CHECK (ph.slots[(size_t) i].start > ph.slots[(size_t) (i - 1)].start && ph.slots[(size_t) i].start < ph.lengthSteps());
    b.playHead.playing = true;
    b.run (200);
    b.playHead.playing = false;
    b.run (4);
    CHECK (! b.nonFinite);
}

void testMidiFile()
{
    Host h;
    const cs::Progression p = h.processor.getProgression();
    const juce::MidiFile file = h.processor.createMidiFile();
    CHECK (file.getNumTracks() == 1 && file.getTimeFormat() == 480);

    int ons = 0, offs = 0, expected = 0;
    for (int i = 0; i < p.numSlots; ++i)
        expected += p.slots[(size_t) i].voicing.n;
    const auto* track = file.getTrack (0);
    for (int i = 0; i < track->getNumEvents(); ++i)
    {
        const auto& m = track->getEventPointer (i)->message;
        ons += m.isNoteOn() ? 1 : 0;
        offs += m.isNoteOff() ? 1 : 0;
        CHECK (m.getTimeStamp() <= (double) p.lengthSteps() * 20.0);
    }
    CHECK (ons == expected && offs == expected);

    const juce::File tmp = h.processor.writeTempMidiFile();
    CHECK (tmp.existsAsFile() && tmp.getSize() > 30);
    juce::FileInputStream in (tmp);
    juce::MidiFile readBack;
    CHECK (in.openedOk() && readBack.readFrom (in) && readBack.getNumTracks() == 1);
    tmp.deleteFile();
}

void testPerformanceAndStrum()
{
    Host h (48000.0, 128); // Small buffer to ensure offsets cross block boundaries

    if (auto* p = h.processor.apvts.getParameter ("strum"))
        p->setValueNotifyingHost (p->convertTo0to1 (0.8f));
    if (auto* p = h.processor.apvts.getParameter ("accent"))
        p->setValueNotifyingHost (p->convertTo0to1 (0.6f));
    if (auto* p = h.processor.apvts.getParameter ("humanize"))
        p->setValueNotifyingHost (p->convertTo0to1 (0.4f));

    h.playHead.playing = true;
    h.run (100);
    h.playHead.playing = false;
    h.run (10);
    // Verify notes are properly paired even with cross-block strumming
    CHECK (h.heldAfterReplay() == 0);

    // Mid-strum jump/relocation test
    h.playHead.playing = true;
    h.run (20);
    h.playHead.ppq = 1.05; // jump mid-slot
    h.run (50);
    h.playHead.playing = false;
    h.run (10);
    CHECK (h.heldAfterReplay() == 0);

    // Verify MIDI export has staggered onsets and valid note lengths
    const juce::MidiFile file = h.processor.createMidiFile();
    const auto* track = file.getTrack (0);
    CHECK (track != nullptr);
    int noteOns = 0;
    std::map<int, double> activeOns;
    for (int i = 0; i < track->getNumEvents(); ++i)
    {
        const auto& m = track->getEventPointer (i)->message;
        if (m.isNoteOn())
        {
            ++noteOns;
            activeOns[m.getNoteNumber()] = m.getTimeStamp();
        }
        else if (m.isNoteOff())
        {
            CHECK (activeOns.count (m.getNoteNumber()) > 0);
            CHECK (m.getTimeStamp() >= activeOns[m.getNoteNumber()]);
            activeOns.erase (m.getNoteNumber());
        }
    }
    CHECK (noteOns > 0);
    CHECK (activeOns.empty());
}

void testIncomingMidiTriggerIntegration()
{
    Host h (48000.0, 128);
    const cs::Progression p = h.processor.getProgression();

    // 1. Live trigger when stopped
    h.injectNoteOn (60, 100);
    h.run (1);
    CHECK (h.processor.getTriggeredSlot() == 0);
    CHECK (h.heldAfterReplay() == p.slots[0].voicing.n);

    // Release note 60
    h.injectNoteOff (60);
    h.run (1);
    CHECK (h.heldAfterReplay() == 0);

    // 2. Held-key stack & Last-Note Priority (60 -> 62 -> release 62 -> 60 returns)
    h.injectNoteOn (60, 90);
    h.run (1);
    CHECK (h.processor.getTriggeredSlot() == 0);
    CHECK (h.heldAfterReplay() == p.slots[0].voicing.n);

    h.injectNoteOn (62, 110);
    h.run (1);
    CHECK (h.processor.getTriggeredSlot() == 1);
    CHECK (h.heldAfterReplay() == p.slots[1].voicing.n);

    // Release 62 -> should cleanly fall back to slot 0!
    h.injectNoteOff (62);
    h.run (1);
    CHECK (h.processor.getTriggeredSlot() == 0);
    CHECK (h.heldAfterReplay() == p.slots[0].voicing.n);

    // Release 60 -> all released
    h.injectNoteOff (60);
    h.run (1);
    CHECK (h.heldAfterReplay() == 0);

    // 3. Sustain pedal (CC 64)
    h.injectNoteOn (60, 100);
    h.run (1);
    h.injectCC (64, 127);
    h.run (1);
    h.injectNoteOff (60);
    h.run (1);
    // Still held by sustain pedal!
    CHECK (h.heldAfterReplay() == p.slots[0].voicing.n);

    // Release sustain pedal
    h.injectCC (64, 0);
    h.run (1);
    CHECK (h.heldAfterReplay() == 0);

    // 4. Strum fast tap cancellation
    if (auto* param = h.processor.apvts.getParameter ("strum"))
        param->setValueNotifyingHost (param->convertTo0to1 (0.8f));

    h.injectNoteOn (60, 100, 0);
    h.injectNoteOff (60, 10);
    h.run (10);
    // Tap finished: no late ringing notes!
    CHECK (h.heldAfterReplay() == 0);

    // 5. Progression edit while chord is held (resilience test)
    h.injectNoteOn (60, 100);
    h.run (1);
    h.processor.edit ([] (cs::Progression& prog, cs::Rng& rng) { cs::roll (prog, rng); });
    h.injectNoteOff (60);
    h.run (1);
    CHECK (h.heldAfterReplay() == 0);
}

void testSplitMidiOutput()
{
    // 1. Legacy migration: ensure bassCount is recomputed if 0
    {
        cs::Voicing v;
        v.n = 4;
        v.notes = { 36, 48, 64, 67 };
        v.bassCount = 0; // Legacy state with 0 bassCount
        CHECK (v.computeBassCount() == 1); // 36 is < 48
    }

    // 2. Playback routing with splitMidi = true
    {
        Host h (48000.0, 256);
        if (auto* p = h.processor.apvts.getParameter ("splitMidi"))
            p->setValueNotifyingHost (1.0f);
        CHECK (h.processor.isSplitMidi());

        h.playHead.playing = true;
        h.run (100);
        h.playHead.playing = false;
        h.run (10);

        CHECK (! h.events.empty());
        int ch1Ons = 0, ch2Ons = 0;
        int ch1Offs = 0, ch2Offs = 0;
        int activeCh2Notes = 0;

        for (const auto& e : h.events)
        {
            if (e.channel == 1)
            {
                if (e.on)
                {
                    ch1Ons++;
                    CHECK (e.note >= cs::kUpperFloor); // Chords must be in upper register
                }
                else
                {
                    ch1Offs++;
                }
            }
            else if (e.channel == 2)
            {
                if (e.on)
                {
                    ch2Ons++;
                    activeCh2Notes++;
                    // Monophonic bass safety: at most 1 simultaneous note on channel 2
                    CHECK (activeCh2Notes <= 1);
                    CHECK (e.note < cs::kUpperFloor); // Bass must be below upper floor
                }
                else
                {
                    ch2Offs++;
                    activeCh2Notes--;
                    CHECK (activeCh2Notes >= 0);
                }
            }
            else
            {
                CHECK (false); // No other channels used
            }
        }
        CHECK (ch1Ons > 0 && ch2Ons > 0);
        CHECK (ch1Ons == ch1Offs);
        CHECK (ch2Ons == ch2Offs);
    }

    // 3. Continuous bass even when slot has no bass note in voicing (bassCount == 0)
    {
        Host h (48000.0, 256);
        if (auto* p = h.processor.apvts.getParameter ("splitMidi"))
            p->setValueNotifyingHost (1.0f);

        // Edit progression to have only high notes in voicing (bassCount = 0)
        h.processor.edit ([] (cs::Progression& pr, cs::Rng&)
        {
            for (int i = 0; i < pr.numSlots; ++i)
            {
                pr.slots[(size_t) i].voicing.n = 3;
                pr.slots[(size_t) i].voicing.notes = { 60, 64, 67 };
                pr.slots[(size_t) i].voicing.bassCount = 0;
            }
        });

        h.playHead.playing = true;
        h.run (60);
        h.playHead.playing = false;
        h.run (10);

        int ch2Ons = 0;
        for (const auto& e : h.events)
            if (e.channel == 2 && e.on)
                ch2Ons++;

        // Channel 2 must still receive synthesized continuous bass notes!
        CHECK (ch2Ons > 0);
    }

    // 4. Strum separation: bass note on grid (sample offset 0), upper notes delayed
    {
        Host h (48000.0, 128);
        if (auto* p = h.processor.apvts.getParameter ("splitMidi"))
            p->setValueNotifyingHost (1.0f);
        if (auto* p = h.processor.apvts.getParameter ("strum"))
            p->setValueNotifyingHost (p->convertTo0to1 (0.8f)); // Upward strum

        h.playHead.playing = true;
        h.run (40);
        h.playHead.playing = false;
        h.run (10);

        // Find the first chord's events (at sample ~0..4000)
        juce::int64 firstBassSample = -1;
        juce::int64 maxUpperSample = -1;
        for (const auto& e : h.events)
        {
            if (e.sample < 4000)
            {
                if (e.channel == 2 && e.on && firstBassSample < 0)
                    firstBassSample = e.sample;
                if (e.channel == 1 && e.on)
                    maxUpperSample = std::max (maxUpperSample, e.sample);
            }
        }
        CHECK (firstBassSample == 0); // Bass is strictly on the grid
        CHECK (maxUpperSample > firstBassSample); // Upper notes roll with strum
    }

    // 5. Live trigger in split mode
    {
        Host h (48000.0, 256);
        if (auto* p = h.processor.apvts.getParameter ("splitMidi"))
            p->setValueNotifyingHost (1.0f);

        // Trigger note 60 (Slot 0)
        h.injectNoteOn (60, 100, 0);
        h.run (1);

        int liveCh1Ons = 0, liveCh2Ons = 0;
        for (const auto& e : h.events)
        {
            if (e.on && e.channel == 1) liveCh1Ons++;
            if (e.on && e.channel == 2) liveCh2Ons++;
        }
        CHECK (liveCh1Ons > 0);
        CHECK (liveCh2Ons == 1); // Strictly monophonic bass

        // Release note 60
        h.injectNoteOff (60, 0);
        h.run (1);

        int liveCh1Offs = 0, liveCh2Offs = 0;
        for (const auto& e : h.events)
        {
            if (! e.on && e.channel == 1) liveCh1Offs++;
            if (! e.on && e.channel == 2) liveCh2Offs++;
        }
        CHECK (liveCh1Ons == liveCh1Offs);
        CHECK (liveCh2Ons == liveCh2Offs);
    }

    // 6. MIDI file export checks
    {
        Host h;
        if (auto* p = h.processor.apvts.getParameter ("splitMidi"))
            p->setValueNotifyingHost (1.0f);

        // Master export in split mode -> Format 1 with 2 tracks
        const juce::MidiFile masterFile = h.processor.createMidiFile (ChordSlotsProcessor::MidiExportMode::Master);
        CHECK (masterFile.getNumTracks() == 2);
        CHECK (masterFile.getTimeFormat() == 480);

        const auto* chordsTrack = masterFile.getTrack (0);
        const auto* bassTrack = masterFile.getTrack (1);
        CHECK (chordsTrack != nullptr && bassTrack != nullptr);

        int ch1Count = 0, ch2Count = 0;
        for (int i = 0; i < chordsTrack->getNumEvents(); ++i)
        {
            const auto& m = chordsTrack->getEventPointer (i)->message;
            if (m.isNoteOnOrOff())
            {
                CHECK (m.getChannel() == 1);
                ch1Count++;
            }
        }
        for (int i = 0; i < bassTrack->getNumEvents(); ++i)
        {
            const auto& m = bassTrack->getEventPointer (i)->message;
            if (m.isNoteOnOrOff())
            {
                CHECK (m.getChannel() == 2);
                ch2Count++;
            }
        }
        CHECK (ch1Count > 0 && ch2Count > 0);

        // Chords-only file -> 1 track, only channel 1
        const juce::MidiFile chordsFile = h.processor.createMidiFile (ChordSlotsProcessor::MidiExportMode::ChordsOnly);
        CHECK (chordsFile.getNumTracks() == 1);
        for (int i = 0; i < chordsFile.getTrack (0)->getNumEvents(); ++i)
        {
            const auto& m = chordsFile.getTrack (0)->getEventPointer (i)->message;
            if (m.isNoteOnOrOff())
                CHECK (m.getChannel() == 1);
        }

        // Bass-only file -> 1 track, only channel 2
        const juce::MidiFile bassFile = h.processor.createMidiFile (ChordSlotsProcessor::MidiExportMode::BassOnly);
        CHECK (bassFile.getNumTracks() == 1);
        for (int i = 0; i < bassFile.getTrack (0)->getNumEvents(); ++i)
        {
            const auto& m = bassFile.getTrack (0)->getEventPointer (i)->message;
            if (m.isNoteOnOrOff())
                CHECK (m.getChannel() == 2);
        }

        // Temp file generation for drag-and-drop
        const juce::File fChords = h.processor.writeTempMidiFile (ChordSlotsProcessor::MidiExportMode::ChordsOnly);
        CHECK (fChords.existsAsFile() && fChords.getFileName().contains ("Chords"));
        fChords.deleteFile();

        const juce::File fBass = h.processor.writeTempMidiFile (ChordSlotsProcessor::MidiExportMode::BassOnly);
        CHECK (fBass.existsAsFile() && fBass.getFileName().contains ("Bass"));
        fBass.deleteFile();
    }
}

void testVariationBankAndHistory()
{
    Host h (48000.0, 256);
    // 1. Initially all 4 variations exist
    CHECK (h.processor.getEditVariation() == 0);
    CHECK (h.processor.getPlayingVariation() == 0);
    CHECK (! h.processor.isFollowingPlayback());

    // Edit variation 0 (A)
    h.processor.edit ([] (cs::Progression& p, cs::Rng& rng)
    {
        p.tonic = 2; // D
        p.bars = 4;
        cs::setNumSlots (p, 6);
        cs::applyMood (p, 3);
        cs::roll (p, rng);
    });

    // Edit variation 1 (B)
    h.processor.setEditVariation (1);
    CHECK (h.processor.getEditVariation() == 1);
    h.processor.edit ([] (cs::Progression& p, cs::Rng& rng)
    {
        p.tonic = 7; // G
        p.bars = 2;
        cs::setNumSlots (p, 4);
        cs::applyMood (p, 7);
        cs::roll (p, rng);
    });

    // Check that variation 0 and variation 1 are distinct and independent
    const cs::Progression pA = h.processor.getProgression (0);
    const cs::Progression pB = h.processor.getProgression (1);
    CHECK (pA.tonic == 2 && pA.bars == 4 && pA.numSlots == 6);
    CHECK (pB.tonic == 7 && pB.bars == 2 && pB.numSlots == 4);
    CHECK (h.processor.getProgression().tonic == 7); // currently editing B

    // 2. Decoupled edit target vs playing variation
    CHECK (h.processor.getPlayingVariation() == 0);
    h.processor.setPlayingVariation (2);
    CHECK (h.processor.getPlayingVariation() == 2);
    CHECK (h.processor.getEditVariation() == 1); // still editing B

    // 3. Follow playback mode
    h.processor.setFollowPlayback (true);
    CHECK (h.processor.isFollowingPlayback());
    h.processor.setPlayingVariation (3);
    CHECK (h.processor.getPlayingVariation() == 3);
    // When followPlayback is true, setPlayingVariation updates edit target:
    CHECK (h.processor.getEditVariation() == 3);

    h.processor.setFollowPlayback (false);

    // 4. Undo isolation per variation stack
    h.processor.setEditVariation (2);
    const int tonicBefore = h.processor.getProgression().tonic;
    CHECK (! h.processor.canUndo());
    h.processor.edit ([] (cs::Progression& p, cs::Rng&) { p.tonic = (p.tonic + 1) % 12; });
    CHECK (h.processor.canUndo());
    CHECK (h.processor.getProgression().tonic != tonicBefore);

    // Switch to variation 3 (no edits made yet)
    h.processor.setEditVariation (3);
    CHECK (! h.processor.canUndo());

    // Switch back to 2, undo restores state
    h.processor.setEditVariation (2);
    CHECK (h.processor.canUndo());
    h.processor.undo();
    CHECK (h.processor.getProgression().tonic == tonicBefore);
    CHECK (! h.processor.canUndo());

    // 5. Copy variation with reversible undo
    h.processor.setEditVariation (1);
    const cs::Progression bBeforeCopy = h.processor.getProgression (1);
    h.processor.copyVariation (0, 1);
    const cs::Progression bAfterCopy = h.processor.getProgression (1);
    CHECK (bAfterCopy.tonic == pA.tonic && bAfterCopy.bars == pA.bars && bAfterCopy.numSlots == pA.numSlots);
    // Since copyVariation pushed history on dest, undo on Variation 1 restores previous progression!
    CHECK (h.processor.canUndo());
    h.processor.undo();
    const cs::Progression bAfterUndo = h.processor.getProgression (1);
    CHECK (bAfterUndo.tonic == bBeforeCopy.tonic && bAfterUndo.bars == bBeforeCopy.bars);

    // 6. Audio thread quantized switching test
    h.playHead.playing = true;
    h.playHead.bpm = 120.0;
    h.run (100);

    // Switch variation mid-playback
    if (auto* param = h.processor.apvts.getParameter ("quantizeSwitch"))
        param->setValueNotifyingHost (param->convertTo0to1 (0.0f)); // Immediate
    h.processor.setPlayingVariation (1);
    h.run (10);
    // Check no stuck notes
    h.playHead.playing = false;
    h.run (10);
    CHECK (h.heldAfterReplay() == 0);

    // 7. State persistence round-trip (v2)
    juce::MemoryBlock state;
    h.processor.getStateInformation (state);

    Host h2 (48000.0, 256);
    h2.processor.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (h2.processor.getProgression (0).tonic == h.processor.getProgression (0).tonic);
    CHECK (h2.processor.getProgression (1).tonic == h.processor.getProgression (1).tonic);

    // 8. State persistence legacy migration (v1 format: single <ChordSlotsState><Progression .../></ChordSlotsState>)
    juce::ValueTree v1Root ("ChordSlotsState");
    juce::ValueTree v1Prog ("Progression");
    v1Prog.setProperty ("tonic", 5, nullptr);
    v1Prog.setProperty ("scale", 1, nullptr);
    v1Prog.setProperty ("bars", 2, nullptr);
    v1Prog.setProperty ("snap", 1, nullptr);
    juce::ValueTree slot0 ("Slot");
    slot0.setProperty ("start", 0, nullptr);
    slot0.setProperty ("root", 5, nullptr);
    slot0.setProperty ("type", "Maj", nullptr);
    slot0.setProperty ("notes", "53 60 65 69", nullptr);
    slot0.setProperty ("velocity", 90, nullptr);
    v1Prog.appendChild (slot0, nullptr);
    v1Root.appendChild (v1Prog, nullptr);

    juce::MemoryBlock v1Block;
    juce::AudioProcessor::copyXmlToBinary (*v1Root.createXml(), v1Block);
    h2.processor.setStateInformation (v1Block.getData(), (int) v1Block.getSize());

    CHECK (h2.processor.getProgression (0).tonic == 5);
    CHECK (h2.processor.getProgression (1).tonic == 5);
    CHECK (h2.processor.getProgression (0).slots[0].chord.root == 5);
}

void testMeterAndTripletGrid()
{
    // 1. Playback in 3/4 meter
    {
        Host h (48000.0, 256);
        h.playHead.timeSigNum = 3;
        h.playHead.timeSigDen = 4;
        h.processor.setMeter (3, 4);

        const cs::Progression p = h.processor.getProgression();
        CHECK (p.timeSigNum == 3 && p.timeSigDen == 4);
        CHECK (p.stepsPerBar() == 72);
        CHECK (p.lengthSteps() == 4 * 72);

        h.playHead.playing = true;
        const double sps = samplesPerStep (h);
        const int blocks = (int) std::ceil (2 * p.lengthSteps() * sps / h.blockSize);
        h.run (blocks);
        CHECK (! h.events.empty());
        h.playHead.playing = false;
        h.run (8);
        CHECK (h.heldAfterReplay() == 0);
    }

    // 2. Playback in 6/8 meter
    {
        Host h (48000.0, 256);
        h.playHead.timeSigNum = 6;
        h.playHead.timeSigDen = 8;
        h.processor.setMeter (6, 8);

        const cs::Progression p = h.processor.getProgression();
        CHECK (p.timeSigNum == 6 && p.timeSigDen == 8);
        CHECK (p.stepsPerBar() == 72);

        h.playHead.playing = true;
        h.run (50);
        h.playHead.playing = false;
        h.run (8);
        CHECK (h.heldAfterReplay() == 0);
        CHECK (! h.nonFinite);
    }

    // 3. Triplet cut timing (1/8T = 8 steps, 1/16T = 4 steps)
    {
        Host h (44100.0, 128); // Awkward sample rate & small buffer
        h.playHead.bpm = 97.3;
        h.processor.edit ([] (cs::Progression& p, cs::Rng&)
        {
            p.bars = 1;
            p.numSlots = 3;
            p.snap = 4; // 1/16T
            p.slots[0].start = 0;
            p.slots[1].start = 8; // 1/8T
            p.slots[2].start = 24; // 1 beat
            p.locks.rhythm = true;
        });

        h.playHead.playing = true;
        const cs::Progression p = h.processor.getProgression();
        const double sps = samplesPerStep (h);
        const int blocks = (int) std::ceil (2 * p.lengthSteps() * sps / h.blockSize);
        h.run (blocks);
        CHECK (! h.events.empty());
        h.playHead.playing = false;
        h.run (8);
        CHECK (h.heldAfterReplay() == 0);
    }

    // 4. MIDI file export in 3/4 and 6/8
    {
        Host h;
        h.processor.setMeter (3, 4);
        const juce::MidiFile file34 = h.processor.createMidiFile();
        CHECK (file34.getNumTracks() >= 1);
        const auto* track34 = file34.getTrack (0);
        bool foundSig34 = false;
        for (int i = 0; i < track34->getNumEvents(); ++i)
        {
            const auto& m = track34->getEventPointer (i)->message;
            if (m.isTimeSignatureMetaEvent())
            {
                int num = 0, den = 0;
                m.getTimeSignatureInfo (num, den);
                if (num == 3 && den == 4)
                    foundSig34 = true;
            }
        }
        CHECK (foundSig34);

        h.processor.setMeter (6, 8);
        const juce::MidiFile file68 = h.processor.createMidiFile();
        const auto* track68 = file68.getTrack (0);
        bool foundSig68 = false;
        for (int i = 0; i < track68->getNumEvents(); ++i)
        {
            const auto& m = track68->getEventPointer (i)->message;
            if (m.isTimeSignatureMetaEvent())
            {
                int num = 0, den = 0;
                m.getTimeSignatureInfo (num, den);
                if (num == 6 && den == 8)
                    foundSig68 = true;
            }
        }
        CHECK (foundSig68);
    }

    // 5. Legacy v2 state migration (scaling slot starts 6x)
    {
        Host h;
        juce::ValueTree v2Root ("ChordSlotsState");
        v2Root.setProperty ("version", 2, nullptr);
        juce::ValueTree v2Container ("ProgressionA");
        juce::ValueTree v2Prog ("Progression");
        v2Prog.setProperty ("bars", 2, nullptr);
        v2Prog.setProperty ("snap", 2, nullptr); // legacy 1/8 note snap
        juce::ValueTree slot0 ("Slot"); slot0.setProperty ("start", 0, nullptr); slot0.setProperty ("root", 0, nullptr); slot0.setProperty ("notes", "60 64 67", nullptr); v2Prog.appendChild (slot0, nullptr);
        juce::ValueTree slot1 ("Slot"); slot1.setProperty ("start", 6, nullptr); slot1.setProperty ("root", 5, nullptr); slot1.setProperty ("notes", "65 69 72", nullptr); v2Prog.appendChild (slot1, nullptr);
        juce::ValueTree slot2 ("Slot"); slot2.setProperty ("start", 16, nullptr); slot2.setProperty ("root", 7, nullptr); slot2.setProperty ("notes", "67 71 74", nullptr); v2Prog.appendChild (slot2, nullptr);
        v2Container.appendChild (v2Prog, nullptr);
        v2Root.appendChild (v2Container, nullptr);

        juce::MemoryBlock block;
        juce::AudioProcessor::copyXmlToBinary (*v2Root.createXml(), block);
        h.processor.setStateInformation (block.getData(), (int) block.getSize());

        const cs::Progression p = h.processor.getProgression (0);
        CHECK (p.slots[0].start == 0);
        CHECK (p.slots[1].start == 36);  // 6 * 6 = 36
        CHECK (p.slots[2].start == 96);  // 16 * 6 = 96
        CHECK (p.snap == 12);            // legacy 2 mapped to 12
    }
}

} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // `--snapshot out.png [mood]` renders the editor off-screen instead of running the tests.
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        ChordSlotsProcessor processor;
        if (argc > 3)
        {
            const int mood = juce::jlimit (0, (int) cs::moods().size() - 1, juce::String (argv[3]).getIntValue());
            processor.edit ([mood] (cs::Progression& p, cs::Rng& rng)
            {
                cs::applyMood (p, mood);
                cs::roll (p, rng);
            });
        }
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        const juce::Image image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[2]));
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat png;
        const bool ok = out.openedOk() && png.writeImageToStream (image, out);
        std::printf ("%s %s\n", ok ? "wrote" : "could not write", file.getFullPathName().toRawUTF8());
        return ok ? 0 : 1;
    }

    testHostSyncedPlayback();
    testGateAndRelocation();
    testPreviewAndAudition();
    testEditWhilePlaying();
    testStateRoundTrip();
    testMidiFile();
    testPerformanceAndStrum();
    testIncomingMidiTriggerIntegration();
    testSplitMidiOutput();
    testVariationBankAndHistory();
    testMeterAndTripletGrid();
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
