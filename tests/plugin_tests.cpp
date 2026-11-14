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
        info.setTimeSignature (TimeSignature { 4, 4 });
        return info;
    }

    double bpm = 120.0;
    double ppq = 0.0;
    bool playing = false;
};

struct NoteEvent
{
    juce::int64 sample;
    int note;
    bool on;
    int velocity;
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
                    events.push_back ({ position + meta.samplePosition, m.getNoteNumber(), true, m.getVelocity() });
                else if (m.isNoteOff())
                    events.push_back ({ position + meta.samplePosition, m.getNoteNumber(), false, 0 });
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

double samplesPerStep (const Host& h) { return h.sampleRate * 60.0 / h.playHead.bpm / 4.0; }

void testHostSyncedPlayback()
{
    Host h;
    h.processor.edit ([] (cs::Progression& p, cs::Rng& rng)
    {
        p.bars = 2;
        p.numSlots = 4;
        cs::layoutEven (p);
        p.slots[1].start = 6;  // irregular cuts
        p.slots[2].start = 16;
        p.slots[3].start = 27;
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
    h.playHead.ppq = 3.5; // step 14 of 16; slot 1 spans 8..16, gate closes at 12
    h.playHead.playing = true;
    h.run ((int) std::ceil (1.0 * sps / h.blockSize));
    CHECK (h.events.empty());
    h.run ((int) std::ceil (2.0 * sps / h.blockSize));
    CHECK (! h.events.empty() && h.events.front().on);

    // With gate 0.5 each chord lasts half its slot.
    h.run ((int) std::ceil (16.0 * sps / h.blockSize));
    juce::int64 on = -1;
    int measured = 0;
    for (const auto& ev : h.events)
    {
        if (ev.on && on < 0)
            on = ev.sample;
        else if (! ev.on && on >= 0)
        {
            CHECK (std::abs ((double) (ev.sample - on) - 4.0 * sps) <= 2.0);
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
            case 3: h.processor.edit ([&] (cs::Progression& p, cs::Rng&) { cs::moveCut (p, 1, random.nextInt (64)); }); break;
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
        CHECK (m.getTimeStamp() <= (double) p.lengthSteps() * 120.0);
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
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
