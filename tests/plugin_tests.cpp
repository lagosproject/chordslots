// Headless tests of the audio processor: drives processBlock with a fake host
// transport and checks the MIDI and audio that come out.
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "../plugin/PluginProcessor.h"
#include "../plugin/PluginEditor.h"
#include "../plugin/ui/SongPanel.h"
#include "../plugin/ui/ChordPickerOverlay.h"
#include "../plugin/ui/TimelineView.h"

#include <chrono>
#include <cstdio>
#include <map>
#include <memory>
#include <thread>
#include <tuple>

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
    explicit Host (double sr = 48000.0, int bs = 512)
        : sampleRate (sr), blockSize (bs), processorStorage (std::make_unique<ChordSlotsProcessor>()),
          processor (*processorStorage)
    {
        processor.setPlayHead (&playHead);
        processor.setRateAndBufferSizeDetails (sr, bs);
        processor.prepareToPlay (sr, bs);
    }

    void setBpm (double bpm)
    {
        playHead.bpm = bpm;
        run (1);
        processor.publish();
        position = 0;
        events.clear();
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
        std::map<std::pair<int, int>, int> held;
        for (const auto& e : events)
        {
            auto& count = held[{ e.channel, e.note }];
            if (e.on)
            {
                CHECK (count == 0); // never re-trigger a note that is still held
                ++count;
            }
            else
            {
                CHECK (count == 1); // never release a note that is not held
                --count;
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
    std::unique_ptr<ChordSlotsProcessor> processorStorage;
    ChordSlotsProcessor& processor;
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

void testStandaloneTempo()
{
    using Processor = juce::AudioProcessor;
    Processor::setTypeOfNextNewPlugin (Processor::wrapperType_Standalone);
    Host h;
    Processor::setTypeOfNextNewPlugin (Processor::wrapperType_Undefined);
    h.processor.edit ([] (cs::Progression& p, cs::Rng&)
    {
        p.melodySourceSteps = 96.0;
        p.melody = { { 0.25, 0.5, 72 } };
    });

    std::unique_ptr<juce::AudioProcessorEditor> editor (h.processor.createEditor());
    auto* tempo = dynamic_cast<juce::Slider*> (editor->findChildWithID ("standalone-bpm"));
    CHECK (tempo != nullptr && tempo->isVisible());
    if (tempo == nullptr)
        return;
    tempo->setValue (90.0, juce::sendNotificationSync);
    CHECK (h.processor.getLastBpm() == 90.0);
    CHECK (! h.processor.isAutoMidiTempoEnabled());
    const auto midi = h.processor.createMidiFile();
    const auto* track = midi.getTrack (0);
    CHECK (std::any_of (track->begin(), track->end(), [] (const auto* event)
    {
        const auto& message = event->message;
        return message.isTempoMetaEvent() && std::abs (message.getTempoSecondsPerQuarterNote() - 60.0 / 90.0) < 1.0e-6;
    }));

    h.processor.auditionMelody();
    h.run (1 + (int) (32000 / h.blockSize));
    CHECK (std::any_of (h.events.begin(), h.events.end(), [] (const auto& e)
    {
        return e.on && e.channel == 3 && e.note == 72 && e.sample == 32000;
    }));

    juce::MemoryBlock state;
    h.processor.getStateInformation (state);
    tempo->setValue (140.0, juce::sendNotificationSync);
    h.processor.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (h.processor.getStandaloneBpm() == 90.0);
    CHECK (! h.processor.isAutoMidiTempoEnabled());
}

void testAcousticPianoPreview()
{
    // The legacy project value is the choice index, regardless of the host's normalized range.
    Host original;
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (original.processor.apvts.getParameter ("synthModel"));
    CHECK (choice != nullptr);
    if (choice == nullptr)
        return;

    CHECK (choice->choices == juce::StringArray ({ "Clean Saw", "Rhodes EP", "Felt Piano", "Acoustic Piano" }));
    Host restored;
    for (const int index : { 2, 3 })
    {
        *choice = index;
        juce::MemoryBlock state;
        original.processor.getStateInformation (state);
        restored.processor.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (juce::roundToInt (restored.processor.apvts.getRawParameterValue ("synthModel")->load()) == index);
    }

    PreviewSynth synth;
    synth.prepare (48000.0);
    synth.setSoundModel (PreviewSoundModel::AcousticPiano);
    juce::AudioBuffer<float> audio (2, 2048);
    juce::MidiBuffer note;
    note.addEvent (juce::MidiMessage::noteOn (1, 28, 0.8f), 0);
    const auto renderNote = [&]
    {
        synth.allNotesOff();
        audio.clear();
        synth.render (audio, note, 1.0f);
        return audio.getMagnitude (0, audio.getNumSamples());
    };
    const auto channelsMatch = [&]
    {
        return std::equal (audio.getReadPointer (0), audio.getReadPointer (0) + audio.getNumSamples(),
                           audio.getReadPointer (1));
    };

    // Sample playback is mono in both channels. The loading fallback is stereo.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (5);
    bool loaded = false;
    do
    {
        loaded = renderNote() > 0.001f && channelsMatch();
        if (! loaded)
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
    }
    while (! loaded && std::chrono::steady_clock::now() < deadline);
    CHECK (loaded);

    synth.setSoundModel (PreviewSoundModel::FeltPiano);
    juce::MidiBuffer empty;
    audio.clear();
    synth.render (audio, empty, 1.0f);
    CHECK (audio.getMagnitude (0, audio.getNumSamples()) > 0.001f && channelsMatch());
    CHECK (renderNote() > 0.001f && ! channelsMatch());

    restored.processor.setPreviewPlaying (true);
    restored.run (4);
    CHECK (restored.peak > 0.0f && ! restored.nonFinite);
    auto* restoredChoice = dynamic_cast<juce::AudioParameterChoice*> (restored.processor.apvts.getParameter ("synthModel"));
    *restoredChoice = 2;
    restored.run (8);
    *restoredChoice = 3;
    restored.run (8);
    restored.processor.setPreviewPlaying (false);
    restored.run (4);
    CHECK (restored.heldAfterReplay() == 0);
}

int renderPianoComparison (const juce::File& directory)
{
    PreviewSynth synth; // Retain the shared bank warmed by the test.
    testAcousticPianoPreview();
    if (failures != 0 || directory.createDirectory().failed())
        return 1;

    constexpr int rate = 48000, blockSize = 256, blocks = 1500;
    juce::AudioBuffer<float> block (2, blockSize);
    juce::AudioBuffer<float> take (2, blocks * blockSize);
    synth.prepare (rate);

    std::vector<juce::MidiBuffer> events (blocks);
    const auto chord = [&] (double time, double duration, std::initializer_list<int> notes)
    {
        const int start = (int) (time * rate);
        const int end = (int) ((time + duration) * rate);
        for (const int key : notes)
        {
            events[(size_t) (start / blockSize)].addEvent (juce::MidiMessage::noteOn (1, key, 0.8f), start % blockSize);
            events[(size_t) (end / blockSize)].addEvent (juce::MidiMessage::noteOff (1, key), end % blockSize);
        }
    };
    chord (0.1, 0.75, { 28, 52, 55, 59, 64 });              // Em
    chord (1.0, 3.50, { 33, 52, 55, 60, 64 });              // Am7
    for (const double time : { 4.8, 5.05, 5.30, 5.55 })
        chord (time, 0.15, { 35, 51, 57, 59, 66 });         // B7
    chord (6.0, 1.25, { 28, 52, 55, 59, 64 });              // Em

    juce::WavAudioFormat wav;
    for (const auto model : { PreviewSoundModel::FeltPiano, PreviewSoundModel::AcousticPiano })
    {
        synth.setSoundModel (model);
        for (int b = 0; b < blocks; ++b)
        {
            block.clear();
            synth.render (block, events[(size_t) b], 1.0f);
            for (int channel = 0; channel < 2; ++channel)
                take.copyFrom (channel, b * blockSize, block, channel, 0, blockSize);
        }

        const auto name = model == PreviewSoundModel::FeltPiano ? "felt" : "acoustic";
        const auto path = directory.getChildFile (juce::String (name) + ".wav");
        if (path.existsAsFile() && ! path.deleteFile())
            return 1;
        std::unique_ptr<juce::OutputStream> stream = path.createOutputStream();
        auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions()
            .withSampleRate (rate).withNumChannels (2).withBitsPerSample (24));
        if (writer == nullptr || ! writer->writeFromAudioSampleBuffer (take, 0, take.getNumSamples()))
            return 1;

        juce::MidiBuffer polyphonic;
        for (int key : { 28, 40, 47, 52, 55, 59, 64, 67 })
            polyphonic.addEvent (juce::MidiMessage::noteOn (1, key, 0.8f), 0);
        juce::MidiBuffer empty;
        constexpr int iterations = 16000, phraseBlocks = 96;
        juce::ScopedNoDenormals noDenormals;
        std::array<double, 5> timings {};
        for (auto& timing : timings)
        {
            const auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < iterations; ++i)
            {
                const bool restart = i % phraseBlocks == 0;
                if (restart)
                    synth.allNotesOff();
                block.clear();
                synth.render (block, restart ? polyphonic : empty, 1.0f);
            }
            timing = std::chrono::duration<double, std::micro> (std::chrono::steady_clock::now() - start).count()
                / iterations;
        }
        synth.allNotesOff();
        std::sort (timings.begin(), timings.end());
        std::printf ("%s: %.3f us/block (median of five runs), peak %.2f dBFS\n", name, timings[2],
                     (double) juce::Decibels::gainToDecibels (take.getMagnitude (0, take.getNumSamples())));
    }
    return 0;
}

void testCompStyleAudition()
{
    Host h;
    h.setBpm (120.0);
    const auto selected = h.processor.getPerformanceParams().compStyle;
    h.processor.auditionCompStyle (cs::CompStyle::Clave32);
    h.run ((int) std::ceil (5.0 * h.sampleRate / h.blockSize));

    // The second bar of the clave needs to sound even if the first slot is shorter.
    CHECK (std::any_of (h.events.begin(), h.events.end(), [&] (const auto& e)
    {
        return e.on && e.sample == std::llround (144 * samplesPerStep (h));
    }));
    CHECK (h.heldAfterReplay() == 0);
    CHECK (h.processor.getPerformanceParams().compStyle == selected);
}

void testShldRhythmStateAndPhase()
{
    Host h;
    auto* comp = h.processor.apvts.getParameter ("compStyle");
    comp->setValueNotifyingHost (comp->convertTo0to1 ((float) cs::CompStyle::PulseEighth));
    juce::MemoryBlock legacy;
    h.processor.getStateInformation (legacy);
    auto root = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (legacy.getData(), (int) legacy.getSize()));
    auto params = root.getChildWithName ("Params");
    params.removeChild (params.getChildWithProperty ("id", "chordRhythm"), nullptr);
    juce::AudioProcessor::copyXmlToBinary (*root.createXml(), legacy);

    auto* rhythm = h.processor.apvts.getParameter ("chordRhythm");
    rhythm->setValueNotifyingHost (rhythm->convertTo0to1 (4.0f));
    CHECK (h.processor.getPerformanceParams().compStyle == cs::CompStyle::ShldHipHop2);

    cs::CompHit hits[8];
    const auto style = h.processor.getPerformanceParams().compStyle;
    CHECK (cs::getCompHits (style, 10, hits, 8, 4, 4, 28) == 0);
    CHECK (cs::getCompHits (style, 22, hits, 8, 4, 4, 30) == 1);
    CHECK (hits[0].stepOffset == 12 && hits[0].durationSteps == 10);

    juce::MemoryBlock saved;
    h.processor.getStateInformation (saved);
    h.processor.setStateInformation (legacy.getData(), (int) legacy.getSize());
    CHECK (h.processor.getPerformanceParams().compStyle == cs::CompStyle::PulseEighth);
    h.processor.setStateInformation (saved.getData(), (int) saved.getSize());
    CHECK (h.processor.getPerformanceParams().compStyle == cs::CompStyle::ShldHipHop2);
}

void testBassAndArpAuditions()
{
    Host bass;
    auto preview = bass.processor.getProgression();
    preview.bars = 1;
    preview.numSlots = 1;
    preview.bassStyle = cs::BassStyle::OctavePump;
    bass.processor.auditionBassPattern (preview);
    bass.run ((int) std::ceil (3.0 * bass.sampleRate / bass.blockSize));
    std::vector<int> pitches;
    for (const auto& event : bass.events)
    {
        CHECK (event.channel == 2);
        if (event.on)
            pitches.push_back (event.note);
    }
    CHECK (pitches.size() == 8);
    CHECK (pitches.size() >= 2 && pitches[1] == pitches[0] + 12);
    CHECK (bass.peak > 0 && bass.heldAfterReplay() == 0);
    CHECK (bass.processor.getProgression().bassStyle == cs::BassStyle::Sustain);

    for (auto style : { cs::CompStyle::ArpUp, cs::CompStyle::ArpDown })
    {
        Host arp;
        auto* split = arp.processor.apvts.getParameter ("splitMidi");
        split->setValueNotifyingHost (1.0f);
        arp.processor.auditionCompStyle (style);
        arp.run ((int) std::ceil (3.0 * arp.sampleRate / arp.blockSize));
        pitches.clear();
        for (const auto& event : arp.events)
            if (event.on && event.channel == 1)
                pitches.push_back (event.note);
        CHECK (pitches.size() == 8);
        CHECK (pitches.size() >= 2 && (style == cs::CompStyle::ArpUp ? pitches[0] < pitches[1] : pitches[0] > pitches[1]));
        CHECK (arp.heldAfterReplay() == 0);
    }
}

void testConfigurableArpComping()
{
    using Mode = ChordSlotsProcessor::MidiExportMode;
    Host h;
    const auto setParam = [&] (const char* id, float value)
    {
        auto* param = h.processor.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    };
    CHECK (h.processor.createMidiFile (Mode::ArpOnly).getNumTracks() == 0);
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (h.processor.createEditor());
        for (auto* child : editor->getChildren())
            if (auto* button = dynamic_cast<juce::TextButton*> (child);
                button != nullptr && button->getButtonText() == "Feel")
                button->onClick();

        auto* rate = editor->findChildWithID ("arp-rate");
        auto* direction = dynamic_cast<juce::ComboBox*> (editor->findChildWithID ("arp-direction"));
        auto* length = editor->findChildWithID ("arp-length");
        auto* octaves = editor->findChildWithID ("arp-octaves");
        auto* pulseRate = dynamic_cast<juce::ComboBox*> (editor->findChildWithID ("pulse-rate"));
        auto* patternAccent = editor->findChildWithID ("comp-pattern-accent");
        auto* hear = editor->findChildWithID ("hear-arp");
        CHECK (rate != nullptr && length != nullptr && octaves != nullptr && pulseRate != nullptr && patternAccent != nullptr && hear != nullptr);
        CHECK (editor->findChildWithID ("comp-hit-length") == nullptr);
        CHECK (direction != nullptr && ! direction->isVisible());
        if (rate != nullptr && length != nullptr && octaves != nullptr && pulseRate != nullptr && patternAccent != nullptr && hear != nullptr)
        {
            CHECK (! rate->isVisible() && ! length->isVisible() && ! octaves->isVisible() && ! hear->isVisible());
            CHECK (! pulseRate->isVisible() && ! patternAccent->isVisible());
            setParam ("chordRhythm", (float) ((int) cs::CompStyle::ArpUpDown - cs::kNumCompStyles + 1));
            editor->resized();
            CHECK (rate->isVisible() && length->isVisible() && octaves->isVisible() && hear->isVisible());
            if (direction != nullptr)
            {
                CHECK (direction->isVisible());
                direction->setSelectedId (2, juce::sendNotificationSync);
                CHECK (h.processor.getPerformanceParams().compStyle == cs::CompStyle::ArpDown);
            }
            CHECK (! pulseRate->isVisible() && ! patternAccent->isVisible());
            setParam ("chordRhythm", 0);
            CHECK (h.processor.getPerformanceParams().arpDirection == cs::CompStyle::ArpDown);
            setParam ("compStyle", (float) cs::CompStyle::PulseEighth);
            editor->resized();
            CHECK (direction != nullptr && ! direction->isVisible());
            CHECK (! rate->isVisible() && ! length->isVisible() && ! octaves->isVisible() && ! hear->isVisible());
            CHECK (pulseRate->isVisible() && patternAccent->isVisible());
            pulseRate->setSelectedId (5, juce::sendNotificationSync);
            CHECK (h.processor.getPerformanceParams().pulseRateSteps == 6);
            setParam ("pulseRate", 0);
            setParam ("arpDirection", 0);
            setParam ("compStyle", (float) cs::CompStyle::Sustain);
        }
    }
    h.processor.edit ([] (cs::Progression& p, cs::Rng& rng)
    {
        p.bars = 1;
        p.numSlots = 2;
        cs::layoutEven (p);
        cs::setChord (p, 0, { 0, 0 }, rng);
        cs::setChord (p, 1, { 5, 0 }, rng);
    });
    const auto readNotes = [] (const juce::MidiFile& file)
    {
        std::vector<std::pair<double, int>> notes;
        if (file.getNumTracks() > 0)
            for (const auto* e : *file.getTrack (0))
                if (e->message.isNoteOn())
                    notes.emplace_back (e->message.getTimeStamp(), e->message.getNoteNumber());
        return notes;
    };
    const auto chords = readNotes (h.processor.createMidiFile());
    setParam ("chordRhythm", (float) ((int) cs::CompStyle::ArpUp - cs::kNumCompStyles + 1));
    CHECK (readNotes (h.processor.createMidiFile()) != chords);
    const auto up = readNotes (h.processor.createMidiFile (Mode::ArpOnly));
    CHECK (up.size() == 8 && up[0].second < up[1].second);
    const auto original = h.processor.getProgression();
    const auto& voiced = original.slots[0].voicing;
    CHECK (! up.empty() && up.front().second == voiced.notes[(size_t) voiced.bassCount]);

    h.processor.edit ([] (cs::Progression& p, cs::Rng& rng) { cs::setChord (p, 1, { 7, 0 }, rng); });
    const auto updated = readNotes (h.processor.createMidiFile (Mode::ArpOnly));
    CHECK (updated.size() == 8 && updated[4].first == 960.0);
    if (up.size() == 8 && updated.size() == 8)
    {
        CHECK (std::equal (up.begin(), up.begin() + 4, updated.begin()));
        CHECK (! std::equal (up.begin() + 4, up.end(), updated.begin() + 4));
    }

    setParam ("chordRhythm", (float) ((int) cs::CompStyle::ArpDown - cs::kNumCompStyles + 1));
    setParam ("arpRate", 4);
    setParam ("arpLength", 50);
    setParam ("gate", 0.5f);
    const auto arpMidi = h.processor.createMidiFile (Mode::ArpOnly);
    const auto down = readNotes (arpMidi);
    CHECK (down.size() == 16 && down[0].second > down[1].second && down[1].first == 120.0);
    CHECK (down == readNotes (h.processor.createMidiFile (Mode::ChordsOnly)));
    for (const auto* event : *arpMidi.getTrack (0))
        if (event->message.isNoteOn())
            CHECK (event->noteOffObject != nullptr
                   && event->noteOffObject->message.getTimeStamp() - event->message.getTimeStamp() == 59.0);
    h.processor.auditionArp();
    h.run (1);
    CHECK (h.processor.isArpAuditioning());
    h.run ((int) std::ceil (2.0 * h.sampleRate / h.blockSize));
    CHECK (! h.processor.isArpAuditioning());
    std::vector<std::pair<double, int>> heard;
    for (const auto& e : h.events)
        if (e.on)
            heard.emplace_back (e.sample / 50.0, e.note);
    CHECK (heard == down && h.heldAfterReplay() == 0);

    juce::MemoryBlock state;
    h.processor.getStateInformation (state);
    Host restored;
    restored.processor.setStateInformation (state.getData(), (int) state.getSize());
    const auto restoredPerf = restored.processor.getPerformanceParams();
    CHECK (restoredPerf.compStyle == cs::CompStyle::ArpDown);
    CHECK (restoredPerf.arpRateSteps == 6 && restoredPerf.arpLength == 0.5f);
    auto legacy = juce::ValueTree::fromXml (*juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize()));
    auto parameters = legacy.getChildWithName ("Params");
    parameters.removeChild (parameters.getChildWithProperty ("id", "arpDirection"), nullptr);
    juce::AudioProcessor::copyXmlToBinary (*legacy.createXml(), state);
    restored.processor.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (restored.processor.getPerformanceParams().compStyle == cs::CompStyle::ArpDown);
    auto* rhythm = restored.processor.apvts.getParameter ("chordRhythm");
    rhythm->setValueNotifyingHost (0.0f);
    CHECK (restored.processor.getPerformanceParams().arpDirection == cs::CompStyle::ArpDown);
    rhythm->setValueNotifyingHost (rhythm->convertTo0to1 ((float) ((int) cs::CompStyle::ArpUp - cs::kNumCompStyles + 1)));
    CHECK (restored.processor.getPerformanceParams().compStyle == cs::CompStyle::ArpDown);
    restored.processor.edit ([] (cs::Progression& p, cs::Rng&)
    {
        p.bars = 8;
        p.numSlots = 1;
    });
    CHECK (readNotes (restored.processor.createMidiFile (Mode::ArpOnly)).size() == 128);

    setParam ("arpOctaves", 1);
    setParam ("chordRhythm", (float) ((int) cs::CompStyle::ArpUpDown - cs::kNumCompStyles + 1));
    h.processor.edit ([] (cs::Progression& p, cs::Rng&) { p.numSlots = 1; p.bars = 4; });
    std::vector<int> ascending;
    for (int octave : { 0, 12 })
        for (int i = voiced.bassCount; i < voiced.n; ++i)
            if (voiced.notes[(size_t) i] + octave <= 127)
                ascending.push_back (voiced.notes[(size_t) i] + octave);
    std::sort (ascending.begin(), ascending.end());
    ascending.erase (std::unique (ascending.begin(), ascending.end()), ascending.end());
    const auto upDown = readNotes (h.processor.createMidiFile (Mode::ArpOnly));
    CHECK (ascending.size() > 2 && upDown.size() == 64);
    if (ascending.size() > 2 && upDown.size() >= 2 * ascending.size())
    {
        for (size_t i = 0; i < ascending.size(); ++i)
            CHECK (upDown[i].second == ascending[i]);
        CHECK (upDown[ascending.size()].second == ascending[ascending.size() - 2]);
        CHECK (upDown[2 * ascending.size() - 2].second == ascending.front());
        CHECK (upDown[2 * ascending.size() - 1].second == ascending[1]);
    }

    setParam ("chordRhythm", 0);
    setParam ("compStyle", (float) cs::CompStyle::PulseEighth);
    setParam ("compHitLength", 50);
    setParam ("compPatternAccent", 0);
    const auto pulse = h.processor.getPlayData().slots[0];
    CHECK (pulse.hitAt (1).stepOffset == 12);
    CHECK (pulse.hitAt (1).durationSteps == 5 && pulse.hitAt (1).velScale == 1.0f);
    setParam ("pulseRate", 5);
    h.processor.edit ([] (cs::Progression& p, cs::Rng&) { p.bars = 8; });
    const auto fastPulse = h.processor.getPlayData().slots[0];
    CHECK (fastPulse.numHits == 128);
    CHECK (fastPulse.hitAt (127).stepOffset == 762);
}

void testSeparatorLocksAndAlternativeAudition()
{
    Host host;
    host.processor.edit ([] (cs::Progression& p, cs::Rng& rng)
    {
        const int original = p.slots[1].start;
        cs::moveCut (p, 1, original - p.snap);
        p.slots[1].cutLocked = true;
        p.slots[1].locked = false;
        const int pinned = p.slots[1].start;
        CHECK (pinned != original);
        cs::moveCut (p, 1, original);
        CHECK (! cs::removeSlot (p, 1));
        cs::roll (p, rng);
        CHECK (p.slots[1].start == pinned);
        CHECK (! p.slots[1].locked);
        p.melody = { { 0.0, 0.125, 96 } };
    });

    juce::MemoryBlock saved;
    host.processor.getStateInformation (saved);
    Host restored;
    restored.processor.setStateInformation (saved.getData(), (int) saved.getSize());
    const auto before = restored.processor.getProgression();
    CHECK (before.slots[1].cutLocked);
    CHECK (before.slots[1].start == host.processor.getProgression().slots[1].start);

    const auto alternatives = cs::alternatives (before, 1, 1);
    CHECK (! alternatives.empty());
    if (alternatives.empty())
        return;

    restored.processor.auditionAlternative (1, alternatives.front(), false);
    restored.run (1);
    const auto isolated = restored.events;
    CHECK (! isolated.empty());
    CHECK (std::all_of (isolated.begin(), isolated.end(), [] (const auto& e)
    {
        return e.on && e.sample == 0 && e.channel != 3;
    }));

    restored.processor.auditionAlternative (1, alternatives.front(), true);
    const int loopBlocks = (int) std::ceil (before.lengthSteps() * samplesPerStep (restored) / restored.blockSize);
    restored.run (loopBlocks + 4);
    CHECK (restored.events.size() > isolated.size() * 2);
    CHECK (std::any_of (restored.events.begin(), restored.events.end(), [] (const auto& e)
    {
        return e.on && e.channel == 3 && e.note == 96;
    }));
    CHECK (restored.heldAfterReplay() == 0);
    restored.processor.auditionAlternative (1, alternatives.front(), true);
    restored.run (1);
    restored.processor.stopAlternativeAudition();
    restored.run (1);
    CHECK (restored.heldAfterReplay() == 0);
    const auto stopped = restored.events.size();
    restored.run (loopBlocks);
    CHECK (restored.events.size() == stopped);
    const auto after = restored.processor.getProgression();
    for (int i = 0; i < before.numSlots; ++i)
        CHECK (before.slots[(size_t) i].chord == after.slots[(size_t) i].chord);
}

void testInspectAlternatives()
{
    constexpr int tonic = 9, dorian = 2, slot = 0;
    CHECK (cs::scaleMembership ({ 2, 0 }, tonic, dorian).outsideChord == 0); // D
    CHECK (cs::scaleMembership ({ 2, 1 }, tonic, dorian).outsideChord == (1u << 5)); // Dm: F
    CHECK (cs::scaleMembership ({ 4, 9 }, tonic, dorian).outsideChord == (1u << 8)); // E7: G#
    const auto extension = cs::scaleMembership ({ 2, 7 }, tonic, dorian); // Dmaj7: C#
    CHECK (extension.outsideChord == (1u << 1) && ! extension.outsideBass);
    const auto bassOnly = cs::scaleMembership ({ 2, 0, 1 }, tonic, dorian); // D/Eb
    CHECK (bassOnly.outsideChord == 0 && bassOnly.outsideBass);

    Host host;
    host.processor.edit ([=] (cs::Progression& p, cs::Rng& rng)
    {
        p.tonic = tonic;
        p.scale = dorian;
        p.adventure = 1.0f;
        cs::setChord (p, slot, { tonic, 1 }, rng);
    });
    const auto before = host.processor.getProgression();
    const auto rng = host.processor.copyVoicingRng();
    const auto expected = cs::inspectAlternatives (before, slot, 6, 3, rng);
    CHECK (expected.size() == 9);
    for (size_t i = 0; i < expected.size(); ++i)
        CHECK ((cs::scaleMembership (expected[i].chord, tonic, dorian).outsideChord != 0) == (i >= 6));
    const auto predict = [&] (size_t index)
    {
        auto p = before;
        p.slots[slot].chord = expected[index].chord;
        p.slots[slot].voicing = expected[index].voicing;
        cs::updateActuals (p, slot);
        return p;
    };

    ChordPickerOverlay inspect (host.processor);
    inspect.setSize (420, 272);
    inspect.open (slot, true);
    juce::Viewport* viewport = nullptr;
    juce::ToggleButton* toggle = nullptr;
    for (auto* child : inspect.getChildren())
    {
        if (auto* v = dynamic_cast<juce::Viewport*> (child)) viewport = v;
        if (auto* t = dynamic_cast<juce::ToggleButton*> (child)) toggle = t;
    }
    CHECK (viewport != nullptr && toggle != nullptr);
    if (viewport == nullptr || toggle == nullptr)
        return;
    const auto rows = [&]
    {
        std::vector<juce::TextButton*> result;
        for (auto* component : viewport->getViewedComponent()->getChildren())
            if (auto* row = dynamic_cast<juce::TextButton*> (component))
                result.push_back (row);
        return result;
    };
    CHECK (! toggle->getToggleState() && rows().size() == 6);
    toggle->setToggleState (true, juce::dontSendNotification);
    toggle->onClick();
    const auto allRows = rows();
    CHECK (allRows.size() == expected.size());
    CHECK (viewport->getViewedComponent()->getHeight() > viewport->getHeight());
    if (allRows.size() != expected.size() || expected.empty())
        return;

    size_t oppositeRow = expected.size();
    for (size_t i = 0; i < expected.size(); ++i)
    {
        const auto test = predict (i);
        const auto& predicted = test.slots[slot];
        CHECK (allRows[i]->getButtonText().startsWith (juce::String::fromUTF8 (
            cs::chordName (predicted.chord, tonic, dorian).c_str()) + "   "));
        CHECK (allRows[i]->getWidth() == viewport->getViewedComponent()->getWidth());
        if ((predicted.tension - before.slots[slot].tension) * (predicted.brightness - before.slots[slot].brightness) < 0
            && std::abs (predicted.tension - before.slots[slot].tension) >= 0.005f
            && std::abs (predicted.brightness - before.slots[slot].brightness) >= 0.005f)
            oppositeRow = i;
    }
    CHECK (oppositeRow < expected.size());
    const auto selected = oppositeRow < expected.size() ? oppositeRow : 0;
    auto* row = allRows[selected];
    row->onClick();
    host.run (1);
    std::vector<int> heard;
    for (const auto& event : host.events)
        if (event.on)
            heard.push_back (event.note);

    const auto predicted = predict (selected);
    const auto firstContextEvent = host.events.size();
    host.processor.auditionAlternative (predicted, slot, true);
    host.run (1);
    std::vector<int> heardInContext;
    for (size_t i = firstContextEvent; i < host.events.size(); ++i)
        if (host.events[i].on)
            heardInContext.push_back (host.events[i].note);
    host.processor.stopAlternativeAudition();
    host.run (1);
    CHECK (host.heldAfterReplay() == 0);

    auto* replace = dynamic_cast<juce::TextButton*> (row->getChildComponent (0));
    CHECK (replace != nullptr && replace->getButtonText() == "Replace");
    if (replace == nullptr)
        return;
    replace->onClick();
    const auto after = host.processor.getProgression();
    const auto& v = after.slots[slot].voicing;
    CHECK (after.slots[slot].chord == predicted.slots[slot].chord);
    CHECK (v.n == expected[selected].voicing.n && v.notes == expected[selected].voicing.notes);
    CHECK (std::abs (after.slots[slot].tension - predicted.slots[slot].tension) < 1.0e-6f);
    CHECK (std::abs (after.slots[slot].brightness - predicted.slots[slot].brightness) < 1.0e-6f);
    CHECK (std::abs (cs::fitScore (after) - cs::fitScore (predicted)) < 1.0e-6f);
    std::sort (heard.begin(), heard.end());
    std::sort (heardInContext.begin(), heardInContext.end());
    CHECK (heard == std::vector<int> (v.notes.begin(), v.notes.begin() + v.n));
    CHECK (heardInContext == heard);
}

// Exercise the actual dock callbacks, including preview isolation and variation refresh.
void testPhase3Views (const juce::File& output = {})
{
    Host host;
    host.processor.edit ([] (cs::Progression& p, cs::Rng& rng)
    {
        cs::moveCut (p, 1, p.snap);
        cs::setChord (p, 0, { 1, 1 }, rng);
        cs::setChord (p, 1, { 5, 0 }, rng);
        cs::setChord (p, 2, { 9, 1 }, rng);
        cs::setChord (p, 3, { 7, 0 }, rng);
    });
    std::unique_ptr<juce::AudioProcessorEditor> editor (host.processor.createEditor());
    auto button = [&] (const juce::String& text) -> juce::TextButton*
    {
        for (auto* child : editor->getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (child); b && b->getButtonText() == text) return b;
        return nullptr;
    };
    TimelineView* timeline = nullptr;
    for (auto* child : editor->getChildren())
        if (auto* t = dynamic_cast<TimelineView*> (child)) timeline = t;
    CHECK (timeline && button ("Notes") && button ("Feel") && button ("Play"));
    if (! timeline || ! button ("Notes")) return;
    button ("Notes")->setToggleState (true, juce::dontSendNotification);
    button ("Notes")->onClick();
    timeline->setSelectedSlot (0);
    timeline->onSlotSelected (0);
    timeline->setPlayhead (37.0);
    auto snapshot = [&] (juce::String name)
    {
        if (output == juce::File()) return;
        output.createDirectory();
        auto file = output.getChildFile (name + ".png");
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat png;
        CHECK (stream.openedOk() && png.writeImageToStream (editor->createComponentSnapshot (editor->getLocalBounds()), stream));
    };
    for (auto size : { juce::Point<int> (1040, 650), juce::Point<int> (900, 570) })
    {
        editor->setSize (size.x, size.y);
        const auto playBounds = button ("Play")->getBounds();
        const auto saveBounds = button ("Save MIDI")->getBounds();
        snapshot ("harmony-" + juce::String (size.x));
        button ("Feel")->onClick();
        CHECK (timeline->getSelectedSlot() == 0);
        CHECK (timeline->getPlayhead() == 37.0);
        CHECK (button ("Play")->getBounds() == playBounds);
        CHECK (button ("Save MIDI")->getBounds() == saveBounds);
        const auto before = host.processor.getProgression();
        // Same undoable mutation used by the bass-pattern browser's selection callback.
        host.processor.edit ([] (cs::Progression& p, cs::Rng&) { p.bassStyle = cs::BassStyle::OctavePump; });
        host.processor.sendSynchronousChangeMessage();
        const auto after = host.processor.getProgression();
        CHECK (after.bassStyle == cs::BassStyle::OctavePump);
        for (int i = 0; i < before.numSlots; ++i)
            CHECK (after.slots[(size_t) i].chord == before.slots[(size_t) i].chord
                   && after.slots[(size_t) i].start == before.slots[(size_t) i].start);
        snapshot ("groove-" + juce::String (size.x));
        button ("Play")->onClick();
        CHECK (host.processor.isPreviewPlaying());
        host.run (100);
        CHECK (! host.events.empty());
        host.processor.setPreviewPlaying (false);
        const auto file = host.processor.writeTempMidiFile();
        juce::FileInputStream stream (file);
        juce::MidiFile midi;
        CHECK (stream.openedOk() && midi.readFrom (stream));
        CHECK (midi.getNumTracks() > 0);
        button ("Harmony")->onClick();
        CHECK (timeline->getSelectedSlot() == 0);
        CHECK (button ("Notes")->getToggleState()); // compact MIDI mode survives the round trip
        snapshot ("harmony-" + juce::String (size.x)); // matched final pattern in both views
    }
}

void testSelectedSlotInspector()
{
    Host host;
    host.processor.edit ([] (cs::Progression& p, cs::Rng& rng)
    {
        p.tonic = 9;
        p.scale = 2;
        cs::moveCut (p, 1, p.snap); // short first slot, uneven progression
        cs::setChord (p, 0, { 9, 1 }, rng);
    });
    std::unique_ptr<juce::AudioProcessorEditor> editor (host.processor.createEditor());
    editor->setVisible (true);
    TimelineView* timeline = nullptr;
    LanesView* lanes = nullptr;
    ChordPickerOverlay* inspector = nullptr;
    for (auto* child : editor->getChildren())
    {
        if (auto* t = dynamic_cast<TimelineView*> (child)) timeline = t;
        if (auto* l = dynamic_cast<LanesView*> (child)) lanes = l;
        if (auto* i = dynamic_cast<ChordPickerOverlay*> (child)) inspector = i;
    }
    CHECK (timeline != nullptr && lanes != nullptr && inspector != nullptr);
    if (! timeline || ! lanes || ! inspector) return;
    timeline->setSelectedSlot (0);
    timeline->onSlotSelected (0);
    timeline->onOpenAlternatives (0);
    auto button = [] (juce::Component& parent, const juce::String& text) -> juce::TextButton*
    {
        for (auto* child : parent.getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (child); b && b->getButtonText() == text) return b;
        return nullptr;
    };
    for (auto size : { juce::Point<int> (1040, 650), juce::Point<int> (900, 570) })
    {
        editor->setSize (size.x, size.y);
        CHECK (inspector->isVisible());
        CHECK (! lanes->isVisible());
        auto* view = [&]() -> juce::Viewport*
        {
            for (auto* child : inspector->getChildren())
                if (auto* viewport = dynamic_cast<juce::Viewport*> (child)) return viewport;
            return nullptr;
        }();
        CHECK (view && view->isVisible());
        if (view)
        {
            const auto bounds = editor->getLocalArea (view, view->getLocalBounds());
            CHECK (bounds.getY() >= timeline->getBottom());
            CHECK (bounds.getRight() <= timeline->getRight() && bounds.getHeight() >= 30);
        }
        auto* close = button (*inspector, "Close");
        CHECK (close && editor->getLocalArea (close, close->getLocalBounds()).getY() >= timeline->getBottom());
        CHECK (editor->getComponentAt (timeline->getBounds().getCentre()) == timeline);
        for (auto* child : inspector->getChildren())
            if (child->isVisible()) CHECK (inspector->getLocalBounds().contains (child->getBounds()));
    }
    // Set up both undo and redo history. Preview must preserve both stacks.
    host.processor.edit ([] (cs::Progression& p, cs::Rng&) { p.variety = 0.123f; });
    CHECK (host.processor.undo());
    host.processor.sendSynchronousChangeMessage();
    juce::MemoryBlock beforeState, afterState;
    host.processor.getStateInformation (beforeState);
    const auto before = host.processor.getProgression();
    CHECK (host.processor.canUndo() && host.processor.canRedo());
    juce::Viewport* viewport = nullptr;
    for (auto* child : inspector->getChildren())
        if (auto* v = dynamic_cast<juce::Viewport*> (child)) viewport = v;
    CHECK (viewport != nullptr);
    if (! viewport) return;
    std::vector<juce::TextButton*> rows;
    for (auto* child : viewport->getViewedComponent()->getChildren())
        if (auto* b = dynamic_cast<juce::TextButton*> (child)) rows.push_back (b);
    CHECK (rows.size() >= 2);
    if (rows.size() < 2) return;
    for (int i = 0; i < 2; ++i)
    {
        auto* audition = button (*rows[(size_t) i], "Audition");
        CHECK (audition != nullptr);
        if (audition) audition->onClick();
        host.run (100);
    }
    CHECK (! host.events.empty());
    host.processor.getStateInformation (afterState);
    CHECK (beforeState == afterState);
    CHECK (host.processor.canUndo() && host.processor.canRedo());
    // The pending redo is the marker, proving audition added no history entry.
    CHECK (host.processor.redo());
    CHECK (host.processor.getProgression().variety == 0.123f);
    CHECK (host.processor.undo());
    const auto checkNeighbours = [&] (const cs::Progression& p)
    {
        CHECK (p.numSlots == before.numSlots);
        for (int i = 1; i < before.numSlots; ++i)
        {
            const auto& a = before.slots[(size_t) i];
            const auto& b = p.slots[(size_t) i];
            CHECK (a.chord == b.chord && a.voicing.notes == b.voicing.notes && a.voicing.n == b.voicing.n);
            CHECK (a.start == b.start && a.targetB == b.targetB && a.targetT == b.targetT && a.targetE == b.targetE);
        }
    };
    auto* replace = button (*rows.front(), "Replace");
    CHECK (replace != nullptr);
    if (replace) replace->onClick();
    const auto replaced = host.processor.getProgression();
    CHECK (replaced.slots[0].chord != before.slots[0].chord);
    checkNeighbours (replaced);
    CHECK (host.processor.undo());
    CHECK (host.processor.getProgression().slots[0].chord == before.slots[0].chord);
    CHECK (host.processor.getProgression().slots[0].voicing.notes == before.slots[0].voicing.notes);
    checkNeighbours (host.processor.getProgression());
    CHECK (host.processor.redo());
    CHECK (host.processor.getProgression().slots[0].chord == replaced.slots[0].chord);
    host.processor.sendSynchronousChangeMessage();
    // Manual controls use the same one-slot undoable edit paths as before docking.
    auto* inspectTab = button (*inspector, "Inspector");
    auto* substitutionsTab = button (*inspector, "Substitutions");
    CHECK (inspectTab && substitutionsTab);
    if (inspectTab)
    {
        inspectTab->onClick();
        CHECK (inspectTab->getToggleState() && ! viewport->isVisible());
    }
    auto* root = button (*inspector, replaced.slots[0].chord.root == 0 ? "D" : "C");
    CHECK (root != nullptr && root->isVisible());
    const auto safeNotes = cs::safeMelodyNotes (replaced.slots[0].chord, replaced.tonic, replaced.scale);
    const auto* cRoot = button (*inspector, "C");
    CHECK (cRoot && (bool) cRoot->getProperties().getWithDefault ("safeNote", false)
                         == std::binary_search (safeNotes.begin(), safeNotes.end(), 0));
    if (root) root->onClick();
    CHECK (host.processor.getProgression().slots[0].chord.root == (replaced.slots[0].chord.root == 0 ? 2 : 0));
    checkNeighbours (host.processor.getProgression());
    CHECK (host.processor.undo());
    host.processor.sendSynchronousChangeMessage();
    juce::ComboBox* quality = nullptr;
    for (auto* child : inspector->getChildren())
        if (auto* q = dynamic_cast<juce::ComboBox*> (child)) quality = q;
    CHECK (quality != nullptr);
    if (quality)
    {
        const int type = (replaced.slots[0].chord.type + 1) % (int) cs::chordTypes().size();
        quality->setSelectedId (type + 1, juce::dontSendNotification);
        quality->onChange();
        CHECK (host.processor.getProgression().slots[0].chord.type == type);
        checkNeighbours (host.processor.getProgression());
        CHECK (host.processor.undo());
        host.processor.sendSynchronousChangeMessage();
    }
    // Same slot ordinal follows the edit variation, with its own chord and history.
    host.processor.setEditVariation (1);
    host.processor.sendSynchronousChangeMessage();
    CHECK (inspector->isVisible() && inspector->getSlot() == 0);
    CHECK (! host.processor.canUndo());
    auto* lock = button (*inspector, "Chord lock");
    CHECK (lock != nullptr);
    const bool wasLocked = host.processor.getProgression().slots[0].locked;
    if (lock) lock->onClick();
    CHECK (host.processor.getProgression().slots[0].locked != wasLocked);
    CHECK (host.processor.undo());
    host.processor.setEditVariation (0);
    host.processor.sendSynchronousChangeMessage();
    CHECK (inspector->isVisible() && inspector->getSlot() == 0);
    CHECK (host.processor.getProgression().slots[0].chord == replaced.slots[0].chord);
    // A surviving ordinal refreshes; a removed ordinal closes and clears selection.
    host.processor.edit ([] (cs::Progression& p, cs::Rng&) { cs::setNumSlots (p, 2); });
    host.processor.sendSynchronousChangeMessage();
    CHECK (inspector->isVisible() && inspector->getSlot() == 0);
    timeline->setSelectedSlot (1);
    timeline->onSlotSelected (1);
    host.processor.edit ([] (cs::Progression& p, cs::Rng&) { cs::setNumSlots (p, 1); });
    host.processor.sendSynchronousChangeMessage();
    CHECK (! inspector->isVisible() && timeline->getSelectedSlot() == -1);
    CHECK (lanes->isVisible());
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

void testHarmonizeMelody()
{
    juce::MidiMessageSequence accompaniment, melody, drums;
    const auto addNote = [] (juce::MidiMessageSequence& track, int channel, int pitch, double begin, double end)
    {
        auto on = juce::MidiMessage::noteOn (channel, pitch, (juce::uint8) 90);
        auto off = juce::MidiMessage::noteOff (channel, pitch);
        on.setTimeStamp (begin);
        off.setTimeStamp (end);
        track.addEvent (on);
        track.addEvent (off);
    };
    for (int n = 0; n < 8; ++n)
    {
        addNote (accompaniment, 1, 45, n * 480.0, n * 480.0 + 400);
        addNote (drums, 10, 78, n * 240.0, n * 240.0 + 100);
    }
    addNote (melody, 2, 64, 120, 600);
    addNote (melody, 2, 67, 480, 960);
    accompaniment.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0);
    accompaniment.addEvent (juce::MidiMessage::keySignatureMetaEvent (-1, true), 0);
    accompaniment.addEvent (juce::MidiMessage::tempoMetaEvent (400000), 960);
    melody.addEvent (juce::MidiMessage::tempoMetaEvent (600000), 240);
    melody.addEvent (juce::MidiMessage::endOfTrack(), 1920);

    juce::MidiFile midi;
    midi.setTicksPerQuarterNote (480);
    midi.addTrack (accompaniment);
    midi.addTrack (drums);
    midi.addTrack (melody);
    juce::TemporaryFile temp (".mid");
    const auto file = temp.getFile();
    {
        juce::FileOutputStream out (file);
        CHECK (out.openedOk() && midi.writeTo (out, 1));
    }

    Host host;
    const auto before = host.processor.getProgression();
    CHECK (host.processor.harmonizeMidi (file));
    host.processor.edit ([] (cs::Progression& p, cs::Rng&) { p.melodyFit = 0.4f; }, false);
    juce::MemoryBlock state;
    host.processor.getStateInformation (state);

    Host restored;
    restored.processor.setStateInformation (state.getData(), (int) state.getSize());
    const auto p = restored.processor.getProgression();
    CHECK (p.numSlots == before.numSlots);
    for (int i = 0; i < p.numSlots; ++i)
        CHECK (p.slots[(size_t) i].start == before.slots[(size_t) i].start);
    CHECK (std::abs (p.melodyFit - 0.4f) < 1.0e-5f);
    CHECK (! p.fitMelodyToLoop && p.melodyLengthSteps() == 96.0);
    CHECK (p.melodySourceBpm == 120.0);
    CHECK (p.melodyFilename == file.getFileName().toStdString());
    CHECK (p.melodySourceTonic == 2 && p.melodySourceScale == 1);
    CHECK (! p.autoMelodyKey && p.tonic == before.tonic && p.scale == before.scale);
    CHECK (p.melody.size() == 2);
    if (p.melody.size() == 2)
    {
        CHECK (p.melody[0].pitch == 64 && p.melody[1].pitch == 67);
        CHECK (p.melody[0].start == 0.0625 && p.melody[0].end == 0.3125);
        CHECK (p.melody[1].start == 0.25 && p.melody[1].end == 0.5);
    }

    const auto checkAudition = [] (Host& h, double expectedStart, double expectedEnd)
    {
        h.events.clear();
        const auto start = h.position;
        const auto sps = samplesPerStep (h);
        h.processor.apvts.getParameter ("splitMidi")->setValueNotifyingHost (1.0f);
        h.processor.auditionMelody();
        h.run (1);
        CHECK (h.processor.isMelodyAuditioning());
        h.run ((int) std::ceil (h.processor.getProgression().lengthSteps() * sps / h.blockSize) + 1);
        CHECK (! h.processor.isMelodyAuditioning());
        for (int channel : { 1, 2 })
            CHECK (std::any_of (h.events.begin(), h.events.end(), [channel] (const auto& e) { return e.on && e.channel == channel; }));
        for (bool on : { true, false })
        {
            const auto expectedSample = start + std::llround ((on ? expectedStart : expectedEnd) * sps);
            CHECK (std::any_of (h.events.begin(), h.events.end(), [=] (const auto& e)
            {
                return e.channel == 3 && e.note == 64 && e.on == on && e.sample == expectedSample
                    && (! on || e.velocity == 90);
            }));
        }
        CHECK (h.heldAfterReplay() == 0);
    };
    checkAudition (restored, 6.0, 30.0);
    restored.processor.edit ([] (cs::Progression& progression, cs::Rng&) { progression.fitMelodyToLoop = true; });
    restored.processor.getStateInformation (state);
    Host fitted;
    fitted.processor.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (fitted.processor.getProgression().fitMelodyToLoop);
    checkAudition (fitted, p.lengthSteps() / 16.0, p.lengthSteps() * 5.0 / 16.0);
    CHECK (restored.processor.undo());
    CHECK (! restored.processor.getProgression().fitMelodyToLoop);
    restored.processor.setAutoMelodyKey (true);
    const auto autoKey = restored.processor.getProgression();
    CHECK (autoKey.autoMelodyKey && autoKey.tonic == 2 && autoKey.scale == 1);
    for (int i = 0; i < autoKey.numSlots; ++i)
        CHECK (autoKey.slots[(size_t) i].start == before.slots[(size_t) i].start);
    CHECK (restored.processor.undo());
    CHECK (! restored.processor.getProgression().autoMelodyKey && restored.processor.getProgression().tonic == before.tonic);

    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (restored.processor.createEditor());
        editor->setSize (900, 570);
        auto* filename = dynamic_cast<juce::Label*> (editor->findChildWithID ("midi-filename"));
        auto* timing = dynamic_cast<juce::TextButton*> (editor->findChildWithID ("midi-timing"));
        auto* details = dynamic_cast<juce::TextButton*> (editor->findChildWithID ("midi-details"));
        auto* key = dynamic_cast<juce::Label*> (editor->findChildWithID ("midi-key-details"));
        auto* tempo = dynamic_cast<juce::Label*> (editor->findChildWithID ("midi-tempo-details"));
        CHECK (filename != nullptr && filename->isVisible() && filename->getText() == file.getFileName());
        CHECK (editor->findChildWithID ("midi-hear-melody")->isVisible());
        CHECK (timing != nullptr && details != nullptr && key != nullptr && tempo != nullptr);
        if (timing != nullptr && details != nullptr && key != nullptr && tempo != nullptr)
        {
            CHECK (! key->isVisible() && ! tempo->isVisible());
            timing->onClick();
            CHECK (restored.processor.getProgression().fitMelodyToLoop && timing->getButtonText() == "Fit to Loop");
            details->setToggleState (true, juce::dontSendNotification);
            details->onClick();
            CHECK (key->isVisible() && key->getText().contains ("D Natural Minor"));
            CHECK (tempo->isVisible() && tempo->getText().contains ("120.0 BPM"));
        }
    }

    juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Standalone);
    Host dropped;
    juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Undefined);
    dropped.processor.setStandaloneBpm (95.0);
    std::unique_ptr<juce::AudioProcessorEditor> editor (dropped.processor.createEditor());
    auto& dropTarget = dynamic_cast<juce::FileDragAndDropTarget&> (*editor);
    const juce::StringArray files { file.getFullPathName() };
    CHECK (dropTarget.isInterestedInFileDrag (files));
    dropTarget.filesDropped (files, 10, 10);
    CHECK (dropped.processor.getProgression().melody.empty());
    CHECK (dropped.processor.getStandaloneBpm() == 95.0);
    CHECK (juce::PopupMenu::dismissAllActiveMenus());
    CHECK (dropped.processor.getProgression().melody.empty());
    CHECK (dropped.processor.harmonizeMidi (file));
    CHECK (dropped.processor.getProgression().melody.size() == 2);
    CHECK (dropped.processor.getStandaloneBpm() == 120.0);
    auto* tempo = dynamic_cast<juce::Slider*> (editor->findChildWithID ("standalone-bpm"));
    CHECK (tempo != nullptr);
    if (tempo != nullptr)
    {
        tempo->setValue (96.0, juce::sendNotificationSync);
        CHECK (dropped.processor.harmonizeMidi (file));
        CHECK (dropped.processor.getStandaloneBpm() == 96.0);
        dropped.processor.setAutoMidiTempoEnabled (true);
        CHECK (dropped.processor.getStandaloneBpm() == 120.0);
    }

    juce::MidiMessageSequence untagged;
    addNote (untagged, 1, 60, 0, 480);
    addNote (untagged, 1, 64, 480, 960);
    addNote (untagged, 1, 67, 960, 1440);
    addNote (untagged, 1, 60, 1440, 1920);
    juce::MidiFile untaggedMidi;
    untaggedMidi.setTicksPerQuarterNote (480);
    untaggedMidi.addTrack (untagged);
    juce::TemporaryFile untaggedFile (".mid");
    {
        juce::FileOutputStream out (untaggedFile.getFile());
        CHECK (out.openedOk() && untaggedMidi.writeTo (out, 0));
    }
    dropped.processor.setAutoMelodyKey (true);
    CHECK (dropped.processor.harmonizeMidi (untaggedFile.getFile()));
    const auto estimated = dropped.processor.getProgression();
    CHECK (estimated.melodySourceTonic == 0 && estimated.melodySourceScale == 0);
    CHECK (estimated.tonic == 0 && estimated.scale == 0);
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
        p.voicingStyle = (int) cs::VoicingStyle::Spread;
        p.variety = 0.83f;
        p.ending = cs::Ending::LeadBackToStart;
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
    CHECK (pa.voicingStyle == pb.voicingStyle && pb.voicingStyle == (int) cs::VoicingStyle::Spread);
    CHECK (std::abs (pa.adventure - pb.adventure) < 1.0e-4f);
    CHECK (std::abs (pb.variety - 0.83f) < 1.0e-4f);
    CHECK (pb.ending == cs::Ending::LeadBackToStart);
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
        CHECK (hV3.processor.getProgression().variety == 0.5f);
        CHECK (hV3.processor.getProgression().ending == cs::Ending::Free);
        CHECK (hV3.processor.getProgression().voicingStyle == (int) cs::VoicingStyle::Close);
    }

    // Default voicing style for new progression is Piano
    {
        Host fresh;
        CHECK (fresh.processor.getProgression().voicingStyle == (int) cs::VoicingStyle::Piano);
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

void testSongMode()
{
    Host h (48000.0, 512);
    cs::Song chain;
    chain.numEntries = 2;
    chain.entries[0] = { 0, 0 };
    chain.entries[1] = { 0, 7 };
    h.processor.setSong (chain);
    h.processor.setSongEnabled (true);

    const auto progression = h.processor.getProgression (0);
    const auto midi = h.processor.createSongMidiFile();
    CHECK (midi.getNumTracks() == 2);
    CHECK (std::abs (midi.getTrack (0)->getEndTime() - 2.0 * progression.lengthSteps() * 20.0) < 1.0);
    bool foundSecondMarker = false;
    for (int i = 0; i < midi.getTrack (0)->getNumEvents(); ++i)
    {
        const auto& message = midi.getTrack (0)->getEventPointer (i)->message;
        foundSecondMarker = foundSecondMarker || (message.isMetaEvent() && message.getMetaEventType() == 6
            && midi.getTrack (0)->getEventPointer (i)->message.getTimeStamp() >= progression.lengthSteps() * 20.0
            && juce::String (message.getTextFromTextMetaEvent()).startsWith ("Section"));
    }
    CHECK (foundSecondMarker);

    juce::MemoryBlock state;
    h.processor.getStateInformation (state);
    auto restored = std::make_unique<ChordSlotsProcessor>();
    restored->setStateInformation (state.getData(), (int) state.getSize());
    const auto roundTrip = restored->getSong();
    CHECK (roundTrip.numEntries == 2 && roundTrip.entries[1].variation == 0 && roundTrip.entries[1].transpose == 7);
    CHECK (restored->isSongEnabled());

    const auto play = h.processor.getPlayData (0);
    std::map<int, bool> expected;
    for (int i = play.slots[0].bassCount; i < play.slots[0].n; ++i)
        expected[(play.slots[0].notes[i] + 7) % 128] = true;
    h.playHead.playing = true;
    h.playHead.ppq = (progression.lengthSteps() - 0.2) / cs::kStepsPerQuarter;
    h.run (1);
    bool switchedWithinBlock = false;
    for (const auto& event : h.events)
        if (event.on && event.sample >= 180 && event.sample <= 230 && expected.count (event.note) > 0)
            switchedWithinBlock = true;
    CHECK (switchedWithinBlock);

    const int seekSlot = juce::jlimit (0, play.numSlots - 1, 1);
    expected.clear();
    for (int i = play.slots[seekSlot].bassCount; i < play.slots[seekSlot].n; ++i)
        expected[(play.slots[seekSlot].notes[i] + 7) % 128] = true;
    h.events.clear();
    h.playHead.ppq = (progression.lengthSteps() + play.slots[seekSlot].start + 0.2) / cs::kStepsPerQuarter;
    const auto seekSample = h.position;
    h.run (1);
    bool chasedTransposedNotes = false;
    for (const auto& event : h.events)
        if (event.on && event.sample == seekSample && expected.count (event.note) > 0)
            chasedTransposedNotes = true;
    CHECK (chasedTransposedNotes);
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

void testModalSuggestionsAndRoll()
{
    const auto compare = [] (cs::Chord tonic, int mode, int reference,
                             cs::Chord characteristic, cs::Chord referenceChord, cs::CorpusStyle autoStyle)
    {
        cs::Progression start;
        cs::Rng init (123);
        cs::roll (start, init);
        for (int i = 0; i < start.numSlots; ++i)
            cs::setChord (start, i, i == 1 ? cs::Chord { 7, 0 } : tonic, init);
        start.locks.rhythm = true;
        auto modal = start;
        auto other = start;
        cs::setTonalContext (modal, tonic.root, mode);
        cs::setTonalContext (other, tonic.root, reference);
        CHECK (cs::styleForProgression (modal) == autoStyle && cs::styleForProgression (other) == autoStyle);
        CHECK (cs::tonesOutsideScale (characteristic, tonic.root, mode) == 0);
        CHECK (cs::tonesOutsideScale (characteristic, tonic.root, reference) == 1);

        const auto matches = [] (cs::Chord a, cs::Chord b) { return a.root == b.root && a.type == b.type; };
        const auto rank = [&] (const std::vector<cs::Chord>& suggestions, cs::Chord chord)
        {
            const auto found = std::find_if (suggestions.begin(), suggestions.end(),
                                             [&] (cs::Chord candidate) { return matches (candidate, chord); });
            CHECK (found != suggestions.end());
            return found - suggestions.begin();
        };
        for (int style : { -1, (int) cs::CorpusStyle::Free })
        {
            modal.style = other.style = style;
            const auto modalSuggestions = cs::alternatives (modal, 1, 999);
            const auto otherSuggestions = cs::alternatives (other, 1, 999);
            CHECK (rank (modalSuggestions, characteristic) < rank (otherSuggestions, characteristic));
            CHECK (rank (otherSuggestions, referenceChord) < rank (modalSuggestions, referenceChord));
        }

        const auto rollCount = [&] (cs::Progression p)
        {
            p.style = -1;
            int count = 0;
            for (int seed = 1; seed <= 64; ++seed)
            {
                auto rolled = p;
                cs::Rng rng (seed);
                cs::roll (rolled, rng);
                count += std::any_of (rolled.slots.begin(), rolled.slots.begin() + rolled.numSlots,
                                      [&] (const cs::Slot& s) { return matches (s.chord, characteristic); });
            }
            return count;
        };
        CHECK (rollCount (modal) > rollCount (other));
    };

    compare ({ 9, 1 }, 2, 1, { 2, 0 }, { 2, 1 }, cs::CorpusStyle::MinorLoops);
    compare ({ 0, 0 }, 5, 0, { 10, 0 }, { 4, 1 }, cs::CorpusStyle::PopRock);
}

void testTonalContextAndTranspose()
{
    Host host;
    auto& processor = host.processor;

    // Project state in the previous format, with explicit inversions and no transposition metadata.
    const auto legacy = juce::parseXML (R"(
        <ChordSlotsState version="4">
          <ProgressionA><Progression tonic="0" scale="0" bars="2" snap="12">
            <Slot start="0" root="0" type="" notes="36 64 67 72"/>
            <Slot start="36" root="7" type="" bassInt="4" locked="1" notes="47 62 67 71"/>
            <Slot start="96" root="9" type="m" notes="45 60 64 69"/>
            <Slot start="132" root="5" type="" notes="41 60 65 69"/>
          </Progression></ProgressionA>
        </ChordSlotsState>)");
    juce::MemoryBlock state;
    juce::AudioProcessor::copyXmlToBinary (*legacy, state);
    processor.setStateInformation (state.getData(), (int) state.getSize());
    processor.apvts.getParameter ("splitMidi")->setValueNotifyingHost (1.0f);

    using MidiNote = std::tuple<double, int, int, bool>;
    const auto exportedNotes = [] (const ChordSlotsProcessor& source)
    {
        std::vector<MidiNote> notes;
        const auto file = source.createMidiFile();
        for (int track = 0; track < file.getNumTracks(); ++track)
            for (const auto* event : *file.getTrack (track))
            {
                const auto& m = event->message;
                if (m.isNoteOnOrOff())
                    notes.emplace_back (m.getTimeStamp(), m.getChannel(), m.getNoteNumber(), m.isNoteOn());
            }
        std::sort (notes.begin(), notes.end());
        return notes;
    };
    const auto audition = [&host]
    {
        host.events.clear();
        host.processor.auditionSlot (1);
        host.run (1);
        std::vector<std::pair<int, int>> notes;
        for (const auto& event : host.events)
            if (event.on)
                notes.emplace_back (event.channel, event.note);
        return notes;
    };

    const auto original = processor.getProgression();
    const auto originalMidi = exportedNotes (processor);
    const auto originalPreview = audition();
    CHECK (originalMidi.size() == 32);
    CHECK (originalPreview.size() == 4);
    CHECK (std::find (originalPreview.begin(), originalPreview.end(), std::make_pair (2, 47)) != originalPreview.end());

    const auto sameVoicings = [&original] (const cs::Progression& p, int shift)
    {
        CHECK (p.numSlots == original.numSlots && p.bars == original.bars);
        for (int i = 0; i < original.numSlots; ++i)
        {
            const auto& before = original.slots[(size_t) i];
            const auto& after = p.slots[(size_t) i];
            CHECK (after.start == before.start && after.locked == before.locked);
            CHECK (after.chord.root == cs::mod12 (before.chord.root + shift));
            CHECK (after.chord.type == before.chord.type && after.chord.bassInt == before.chord.bassInt);
            CHECK (after.voicing.n == before.voicing.n && after.voicing.bassCount == before.voicing.bassCount);
            for (int j = 0; j < before.voicing.n; ++j)
                CHECK (after.voicing.notes[(size_t) j] == before.voicing.notes[(size_t) j] + shift);
        }
    };

    CHECK (cs::romanNumeral (original.slots[2].chord, original.tonic) == "vi");
    processor.edit ([] (cs::Progression& p, cs::Rng&) { cs::goToRelative (p); });
    const auto relative = processor.getProgression();
    CHECK (relative.tonic == 9 && relative.scale == 1);
    CHECK (cs::romanNumeral (relative.slots[2].chord, relative.tonic) == "i");
    sameVoicings (relative, 0);
    CHECK (exportedNotes (processor) == originalMidi);
    CHECK (audition() == originalPreview);
    CHECK (cs::alternatives (relative, 2, 10) != cs::alternatives (original, 2, 10));
    processor.edit ([] (cs::Progression& p, cs::Rng&) { cs::goToRelative (p); });
    CHECK (processor.getProgression().tonic == 0 && processor.getProgression().scale == 0);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    const auto select = [&editor] (const juce::String& name, int selectedId)
    {
        for (auto* child : editor->getChildren())
            if (auto* box = dynamic_cast<juce::ComboBox*> (child); box != nullptr && box->getName() == name)
            {
                box->setSelectedId (selectedId, juce::dontSendNotification);
                box->onChange();
                return;
            }
        CHECK (false);
    };
    TimelineView* timeline = nullptr;
    for (auto* child : editor->getChildren())
    {
        if (auto* box = dynamic_cast<juce::ComboBox*> (child); box != nullptr && box->getName() == "Scale")
        {
            CHECK (box->getNumItems() == cs::numScales());
            CHECK (box->getItemText (0) == "Major (Ionian)" && box->getItemId (0) == 1);
            CHECK (box->getItemText (1) == "Natural Minor (Aeolian)" && box->getItemId (1) == 2);
            for (int i = 2; i < cs::numScales(); ++i)
                CHECK (box->getItemText (i) == cs::scales()[(size_t) i].name && box->getItemId (i) == i + 1);
        }
        if (auto* view = dynamic_cast<TimelineView*> (child))
            timeline = view;
    }
    select ("Tonic", 10);
    CHECK (processor.getProgression().tonic == 9 && processor.getProgression().scale == 0);
    sameVoicings (processor.getProgression(), 0);
    CHECK (timeline != nullptr);
    if (timeline != nullptr && ! originalMidi.empty())
    {
        int low = 127, high = 0;
        for (const auto& event : originalMidi)
        {
            low = std::min (low, std::get<2> (event));
            high = std::max (high, std::get<2> (event));
        }
        timeline->setMidiVisible (true);
        const auto rowColour = [&] (int pc)
        {
            const int pitch = low + cs::mod12 (pc - low);
            CHECK (pitch <= high);
            constexpr int noteTop = 73;
            const float rowHeight = (float) (timeline->getHeight() - noteTop) / (high - low + 1);
            const int y = (int) (noteTop + (high - pitch + 0.5f) * rowHeight);
            const auto image = timeline->createComponentSnapshot (timeline->getLocalBounds());
            return image.getPixelAt (4, y); // Pitch column, before the note labels.
        };
        const auto checkModeRows = [&] (int tonicId, int firstScale, int secondScale, int pc)
        {
            select ("Tonic", tonicId);
            select ("Scale", firstScale);
            processor.dispatchPendingMessages();
            const auto highlighted = rowColour (pc);
            select ("Scale", secondScale);
            processor.dispatchPendingMessages();
            CHECK (highlighted != rowColour (pc));
            sameVoicings (processor.getProgression(), 0);
            CHECK (exportedNotes (processor) == originalMidi);
            CHECK (audition() == originalPreview);
        };
        checkModeRows (10, 3, 2, 6); // F-sharp in A Dorian.
        checkModeRows (10, 2, 3, 5); // F in A Aeolian.
        checkModeRows (1, 6, 1, 10); // B-flat in C Mixolydian.
        timeline->setMidiVisible (false);
    }
    select ("Tonic", 1);
    select ("Scale", 1);
    select ("Scale", 2);
    const auto minor = processor.getProgression();
    CHECK (minor.tonic == 0 && minor.scale == 1);
    CHECK ((cs::scaleMask (minor.scale) & (1u << 4)) == 0);
    CHECK ((cs::scaleMask (minor.scale) & (1u << 3)) != 0);
    sameVoicings (minor, 0);
    CHECK (exportedNotes (processor) == originalMidi);
    CHECK (audition() == originalPreview);
    processor.getStateInformation (state);
    Host restored;
    restored.processor.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (restored.processor.getProgression().tonic == 0 && restored.processor.getProgression().scale == 1);
    CHECK (exportedNotes (restored.processor) == originalMidi);
    CHECK (processor.undo());

    CHECK (processor.transposeProgression (2));
    const auto transposed = processor.getProgression();
    CHECK (transposed.tonic == 2 && transposed.scale == 0);
    sameVoicings (transposed, 2);
    auto shiftedMidi = originalMidi;
    for (auto& event : shiftedMidi)
        std::get<2> (event) += 2;
    CHECK (exportedNotes (processor) == shiftedMidi);
    auto shiftedPreview = originalPreview;
    for (auto& note : shiftedPreview)
        note.second += 2;
    CHECK (audition() == shiftedPreview);
    processor.getStateInformation (state);
    restored.processor.setStateInformation (state.getData(), (int) state.getSize());
    CHECK (restored.processor.getProgression().tonic == 2 && restored.processor.getProgression().scale == 0);
    sameVoicings (restored.processor.getProgression(), 2);
    CHECK (exportedNotes (restored.processor) == shiftedMidi);

    processor.edit ([] (cs::Progression& p, cs::Rng&)
    {
        p.slots[0].voicing.notes[0] = 0;
        p.slots[3].voicing.notes[3] = 127;
    });
    const auto atLimits = exportedNotes (processor);
    CHECK (! processor.transposeProgression (1));
    CHECK (! processor.transposeProgression (-1));
    CHECK (processor.getProgression().tonic == 2);
    CHECK (exportedNotes (processor) == atLimits);
    CHECK (processor.undo());
    CHECK (exportedNotes (processor) == shiftedMidi);
}

void testChordInversion()
{
    Host h;
    h.processor.apvts.getParameter ("splitMidi")->setValueNotifyingHost (1.0f);
    h.processor.edit ([] (cs::Progression& p, cs::Rng&)
    {
        p.numSlots = 1;
        p.bassStyle = cs::BassStyle::Sustain;
        auto& slot = p.slots[0];
        slot.start = 0;
        slot.chord = { 10, 0, 0 };
        slot.voicing = { 4, { 34, 58, 62, 65 }, 96, 1 };
    });

    const auto verify = [] (Host& host, const std::vector<int>& expectedChord, int expectedBass)
    {
        host.events.clear();
        host.processor.auditionSlot (0);
        host.run (1);
        std::vector<int> preview[2], exported[2];
        for (const auto& event : host.events)
            if (event.on && (event.channel == 1 || event.channel == 2))
                preview[event.channel - 1].push_back (event.note);

        const auto file = host.processor.createMidiFile (ChordSlotsProcessor::MidiExportMode::Master);
        CHECK (file.getNumTracks() == 2);
        for (int track = 0; track < file.getNumTracks(); ++track)
            for (const auto* event : *file.getTrack (track))
            {
                const auto& message = event->message;
                if (message.isNoteOn() && (message.getChannel() == 1 || message.getChannel() == 2))
                    exported[message.getChannel() - 1].push_back (message.getNoteNumber());
            }

        CHECK (preview[0] == expectedChord);
        CHECK (preview[1] == std::vector<int> { expectedBass });
        CHECK (exported[0] == preview[0]);
        CHECK (exported[1] == preview[1]);
    };
    const auto invert = [&] (cs::InversionDirection direction)
    {
        h.processor.edit ([direction] (cs::Progression& p, cs::Rng&) { cs::invertChord (p, 0, direction); });
    };

    verify (h, { 58, 62, 65 }, 34);
    invert (cs::InversionDirection::Up);
    verify (h, { 62, 65, 70 }, 34);
    invert (cs::InversionDirection::Up);
    verify (h, { 65, 70, 74 }, 34);

    h.processor.edit ([] (cs::Progression& p, cs::Rng&) { cs::setBass (p, 0, 4); });
    verify (h, { 65, 70, 74 }, 38);
    juce::MemoryBlock state;
    h.processor.getStateInformation (state);
    Host restored;
    restored.processor.setStateInformation (state.getData(), (int) state.getSize());
    verify (restored, { 65, 70, 74 }, 38);

    invert (cs::InversionDirection::Down);
    verify (h, { 62, 65, 70 }, 38);
    invert (cs::InversionDirection::Down);
    verify (h, { 58, 62, 65 }, 38);
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

void testChordMarkersP74()
{
    struct Marker { juce::String text; double tick; };
    Host h;

    auto prepareVariation = [&] (int index, int tonic, const char* firstSuffix, const char* secondSuffix)
    {
        h.processor.setEditVariation (index);
        h.processor.edit ([=] (cs::Progression& p, cs::Rng& rng)
        {
            p.tonic = tonic;
            p.scale = 0;
            cs::setNumSlots (p, 2);
            cs::layoutEven (p);
            const auto typeFor = [] (const char* suffix)
            {
                for (int i = 0; i < cs::numChordTypes(); ++i)
                    if (std::strcmp (cs::chordTypes()[(size_t) i].suffix, suffix) == 0) return i;
                return 0;
            };
            p.slots[0].chord = { tonic, typeFor (firstSuffix), 0 };
            p.slots[1].chord = { cs::mod12 (tonic + 7), typeFor (secondSuffix), 0 };
            cs::voiceAll (p, rng);
        });
    };
    prepareVariation (0, 0, "", "7");
    prepareVariation (1, 2, "m7", "maj7");
    h.processor.setEditVariation (0);

    constexpr int ticksPerStep = 20; // 480 PPQ / 24 steps per quarter note
    auto expectedFor = [&] (const cs::Progression& p, double offset, int transpose)
    {
        std::vector<Marker> out;
        for (int i = 0; i < p.numSlots; ++i)
        {
            auto chord = p.slots[(size_t) i].chord;
            chord.root = cs::mod12 (chord.root + transpose);
            const auto name = cs::chordName (chord, cs::mod12 (p.tonic + transpose), p.scale);
            out.push_back ({ juce::String::fromUTF8 (name.c_str()),
                             offset + p.slots[(size_t) i].start * ticksPerStep });
        }
        return out;
    };

    auto verifySerialized = [&] (ChordSlotsProcessor::MidiExportMode mode, int expectedFormat,
                                const std::vector<Marker>& expectedChords,
                                const std::vector<Marker>& expectedSections)
    {
        const juce::File file = h.processor.writeTempMidiFile (mode);
        CHECK (file.existsAsFile());
        if (! file.existsAsFile()) return;

        juce::MemoryBlock bytes;
        CHECK (file.loadFileAsData (bytes));
        if (bytes.getSize() >= 14)
        {
            const auto* raw = static_cast<const juce::uint8*> (bytes.getData());
            CHECK (std::memcmp (raw, "MThd", 4) == 0);
            const int format = (int (raw[8]) << 8) | int (raw[9]);
            CHECK (format == expectedFormat);
        }
        else CHECK (false);

        juce::FileInputStream input (file);
        juce::MidiFile parsed;
        CHECK (input.openedOk() && parsed.readFrom (input));
        CHECK (parsed.getNumTracks() == (expectedFormat == 0 ? 1 : 2));
        std::vector<Marker> chordMarkers, sectionMarkers;
        for (int track = 0; track < parsed.getNumTracks(); ++track)
        {
            const auto* sequence = parsed.getTrack (track);
            if (sequence == nullptr) continue;
            for (int i = 0; i < sequence->getNumEvents(); ++i)
            {
                const auto& message = sequence->getEventPointer (i)->message;
                if (! message.isMetaEvent() || message.getMetaEventType() != 6) continue;
                const Marker marker { message.getTextFromTextMetaEvent(), message.getTimeStamp() };
                if (marker.text.startsWith ("Section ")) sectionMarkers.push_back (marker);
                else chordMarkers.push_back (marker);
                CHECK (track == 0);
            }
        }
        CHECK (chordMarkers.size() == expectedChords.size());
        for (size_t i = 0; i < std::min (chordMarkers.size(), expectedChords.size()); ++i)
        {
            CHECK (chordMarkers[i].text == expectedChords[i].text);
            CHECK (std::abs (chordMarkers[i].tick - expectedChords[i].tick) < 0.01);
        }
        CHECK (sectionMarkers.size() == expectedSections.size());
        for (size_t i = 0; i < std::min (sectionMarkers.size(), expectedSections.size()); ++i)
        {
            CHECK (sectionMarkers[i].text == expectedSections[i].text);
            CHECK (std::abs (sectionMarkers[i].tick - expectedSections[i].tick) < 0.01);
        }
        file.deleteFile();
    };

    const auto a = h.processor.getProgression (0);
    const auto b = h.processor.getProgression (1);
    const auto expectedA = expectedFor (a, 0.0, 0);
    if (auto* split = h.processor.apvts.getParameter ("splitMidi"))
        split->setValueNotifyingHost (0.0f);
    for (const auto mode : { ChordSlotsProcessor::MidiExportMode::Master,
                             ChordSlotsProcessor::MidiExportMode::ChordsOnly,
                             ChordSlotsProcessor::MidiExportMode::BassOnly })
        verifySerialized (mode, 0, expectedA, {});

    if (auto* split = h.processor.apvts.getParameter ("splitMidi"))
        split->setValueNotifyingHost (1.0f);
    verifySerialized (ChordSlotsProcessor::MidiExportMode::Master, 1, expectedA, {});

    cs::Song song;
    song.numEntries = 2;
    song.entries[0] = { 0, 2 };
    song.entries[1] = { 1, -5 };
    h.processor.setSong (song);
    std::vector<Marker> expectedSong = expectedFor (a, 0.0, 2);
    const double bOffset = a.lengthSteps() * ticksPerStep;
    const auto expectedB = expectedFor (b, bOffset, -5);
    expectedSong.insert (expectedSong.end(), expectedB.begin(), expectedB.end());
    const std::vector<Marker> sections = { { "Section A", 0.0 }, { "Section B", bOffset } };
    verifySerialized (ChordSlotsProcessor::MidiExportMode::Song, 1, expectedSong, sections);
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
        p.variety = 0.21f;
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
        p.variety = 0.79f;
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
    CHECK (std::abs (h2.processor.getProgression (0).variety - 0.21f) < 1.0e-4f);
    CHECK (std::abs (h2.processor.getProgression (1).variety - 0.79f) < 1.0e-4f);

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

void testMidiVariationSwitchIsDeferredToMessageThread()
{
    Host h (48000.0, 256);
    h.processor.stopTimer(); // Drive timer service explicitly for a deterministic handoff test.
    auto* variation = h.processor.apvts.getRawParameterValue ("variation");
    CHECK (variation != nullptr);
    if (variation == nullptr)
        return;

    h.injectNoteOn (26); // MIDI keyswitch note 26 selects Variation C (index 2).
    h.run (1);

    // The audio callback exposes the new target immediately, but must not
    // change APVTS or call setValueNotifyingHost on the real-time thread.
    CHECK (h.processor.getPlayingVariation() == 2);
    CHECK (juce::roundToInt (variation->load()) == 0);

    // A later keyswitch supersedes the pending one before the message-thread
    // timer runs; only the latest selection is reflected to the host/UI.
    h.injectNoteOn (27);
    h.run (1);
    CHECK (h.processor.getPlayingVariation() == 3);
    CHECK (juce::roundToInt (variation->load()) == 0);

    // If host automation changes the APVTS value before deferred notification,
    // that newer host choice wins and the old keyswitch request is discarded.
    auto* variationParameter = h.processor.apvts.getParameter ("variation");
    variationParameter->setValueNotifyingHost (variationParameter->convertTo0to1 (1.0f));
    CHECK (h.processor.getPlayingVariation() == 1);
    h.processor.timerCallback();
    CHECK (juce::roundToInt (variation->load()) == 1);
    CHECK (h.processor.getPlayingVariation() == 1);

    h.processor.setPlayingVariation (3);
    h.processor.setFollowPlayback (true);
    h.injectNoteOn (25);
    h.run (1);
    CHECK (h.processor.getEditVariation() == 3);
    h.processor.timerCallback();
    CHECK (h.processor.getEditVariation() == 1);
    CHECK (h.processor.getPlayingVariation() == 1);
}

void testExtendedChordTypesStateRoundTrip()
{
    Host a;
    a.processor.edit ([] (cs::Progression& p, cs::Rng& rng)
    {
        cs::setNumSlots (p, 3);
        int idx_m11 = -1, idx_7sharp9 = -1, idx_69 = -1;
        for (int i = 0; i < cs::numChordTypes(); ++i)
        {
            if (std::string (cs::chordTypes()[(size_t) i].suffix) == "m11") idx_m11 = i;
            if (std::string (cs::chordTypes()[(size_t) i].suffix) == "7#9") idx_7sharp9 = i;
            if (std::string (cs::chordTypes()[(size_t) i].suffix) == "6/9") idx_69 = i;
        }
        CHECK (idx_m11 >= 0 && idx_7sharp9 >= 0 && idx_69 >= 0);

        p.slots[0].chord = { 0, idx_m11, 0 };      // Cm11
        p.slots[1].chord = { 4, idx_7sharp9, 0 };  // E7#9
        p.slots[2].chord = { 7, idx_69, 0 };       // G6/9
        cs::voiceAll (p, rng);
    });

    juce::MemoryBlock state;
    a.processor.getStateInformation (state);

    Host b;
    b.processor.setStateInformation (state.getData(), (int) state.getSize());

    const cs::Progression pb = b.processor.getProgression();
    CHECK (pb.numSlots == 3);
    CHECK (std::string (cs::chordTypes()[(size_t) pb.slots[0].chord.type].suffix) == "m11");
    CHECK (std::string (cs::chordTypes()[(size_t) pb.slots[1].chord.type].suffix) == "7#9");
    CHECK (std::string (cs::chordTypes()[(size_t) pb.slots[2].chord.type].suffix) == "6/9");
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
        h.setBpm (97.3);
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

// P1.7: Export parity test across comp styles, split on/off, strum 0/max
void testExportPlaybackParity()
{
    for (double bpm : { 120.0, 200.0, 60.0 })
    {
        for (int index = 0; index < cs::kNumCompStyles + cs::kNumExtraCompStyles; ++index)
        {
            for (bool split : { false, true })
            {
                for (float strum : { 0.0f, 1.0f })
                {
                    Host h (48000.0, 256);
                    h.setBpm (bpm);

                const bool shld = index >= cs::kNumCompStyles;
                auto* style = h.processor.apvts.getParameter (shld ? "chordRhythm" : "compStyle");
                const int choice = shld ? index - cs::kNumCompStyles + 1 : index;
                style->setValueNotifyingHost (style->convertTo0to1 ((float) choice));
                if (shld)
                    h.processor.edit ([] (cs::Progression& p, cs::Rng&) { p.slots[1].start = 30; }, false);
                if (auto* p = h.processor.apvts.getParameter ("splitMidi"))
                    p->setValueNotifyingHost (split ? 1.0f : 0.0f);
                if (auto* p = h.processor.apvts.getParameter ("strum"))
                    p->setValueNotifyingHost (p->convertTo0to1 (strum));

                const cs::Progression prog = h.processor.getProgression();
                const double sps = samplesPerStep (h);
                constexpr int kTicksPerQuarter = 480;
                constexpr int kTicksPerStep = 20;
                const double ticksPerSample = (h.playHead.bpm * (double) kTicksPerQuarter) / (60.0 * h.sampleRate);
                const int totalBlocks = (int) std::ceil ((double) prog.lengthSteps() * sps / (double) h.blockSize);

                // Export reference (Format 1 master file if split, Format 0 if non-split)
                const juce::MidiFile file = h.processor.createMidiFile (ChordSlotsProcessor::MidiExportMode::Master);
                struct EventKey
                {
                    int channel;
                    int note;
                    bool on;
                    int velocity;
                    double tick;
                };
                std::vector<EventKey> exportEvents;
                for (int t = 0; t < file.getNumTracks(); ++t)
                {
                    const auto* track = file.getTrack (t);
                    for (int e = 0; e < track->getNumEvents(); ++e)
                    {
                        const auto& m = track->getEventPointer (e)->message;
                        if (m.isNoteOn() || m.isNoteOff())
                        {
                            exportEvents.push_back ({
                                m.getChannel(),
                                m.getNoteNumber(),
                                m.isNoteOn(),
                                m.isNoteOn() ? (int) m.getVelocity() : 0,
                                m.getTimeStamp()
                            });
                        }
                    }
                }

                // Render 1 loop through processBlock
                h.playHead.ppq = 0.0;
                h.playHead.playing = true;
                h.run (totalBlocks);

                // Stop transport
                h.playHead.playing = false;
                h.run (10);
                CHECK (h.heldAfterReplay() == 0); // Zero stuck notes at transport stop

                // Collect rendered events during the 1 loop
                const double loopEndTick = (double) prog.lengthSteps() * (double) kTicksPerStep;
                std::vector<EventKey> renderedEvents;
                for (const auto& ev : h.events)
                {
                    const double tick = (double) ev.sample * ticksPerSample;
                    const bool isLoop1 = ev.on ? (tick < loopEndTick - 0.5) : (tick < loopEndTick + 0.5);
                    if (isLoop1)
                    {
                        renderedEvents.push_back ({
                            ev.channel,
                            ev.note,
                            ev.on,
                            ev.on ? ev.velocity : 0,
                            tick
                        });
                    }
                }

                // Assert that every exported note has off-tick > on-tick
                std::map<std::pair<int, int>, std::vector<double>> expOnTicks;
                for (const auto& exp : exportEvents)
                {
                    const auto key = std::make_pair (exp.channel, exp.note);
                    if (exp.on)
                    {
                        expOnTicks[key].push_back (exp.tick);
                    }
                    else
                    {
                        CHECK (! expOnTicks[key].empty());
                        if (! expOnTicks[key].empty())
                        {
                            const double onTick = expOnTicks[key].front();
                            expOnTicks[key].erase (expOnTicks[key].begin());
                            CHECK (exp.tick > onTick);
                        }
                    }
                }
                for (const auto& kv : expOnTicks)
                    CHECK (kv.second.empty());

                // Assert that every live note-on has a later note-off
                std::map<std::pair<int, int>, std::vector<double>> liveOnTicks;
                for (const auto& ren : renderedEvents)
                {
                    const auto key = std::make_pair (ren.channel, ren.note);
                    if (ren.on)
                    {
                        liveOnTicks[key].push_back (ren.tick);
                    }
                    else
                    {
                        CHECK (! liveOnTicks[key].empty());
                        if (! liveOnTicks[key].empty())
                        {
                            const double onTick = liveOnTicks[key].front();
                            liveOnTicks[key].erase (liveOnTicks[key].begin());
                            CHECK (ren.tick > onTick);
                        }
                    }
                }
                for (const auto& kv : liveOnTicks)
                    CHECK (kv.second.empty());

                CHECK (renderedEvents.size() == exportEvents.size());

                // Match each export event with a rendered event within +-1 tick
                std::vector<bool> matched (renderedEvents.size(), false);
                int matchCount = 0;
                for (const auto& exp : exportEvents)
                {
                    bool found = false;
                    for (size_t r = 0; r < renderedEvents.size(); ++r)
                    {
                        if (matched[r])
                            continue;
                        const auto& ren = renderedEvents[r];
                        if (ren.channel == exp.channel &&
                            ren.note == exp.note &&
                            ren.on == exp.on &&
                            (! exp.on || ren.velocity == exp.velocity) &&
                            std::abs (ren.tick - exp.tick) <= 1.0)
                        {
                            matched[r] = true;
                            found = true;
                            ++matchCount;
                            break;
                        }
                    }
                    if (! found)
                    {
                        std::printf ("Mismatch in style %d split %d strum %.1f: exp ch%d note%d on%d vel%d tick%.2f (matched %d/%zu)\n",
                                     index, (int) split, (double) strum,
                                     exp.channel, exp.note, (int) exp.on, exp.velocity, exp.tick,
                                     matchCount, exportEvents.size());
                        break;
                    }
                }
                CHECK (matchCount == (int) exportEvents.size());
            }
        }
    }
    }
}

void testBassPatternExportPlaybackParity()
{
    for (double bpm : { 60.0, 120.0, 200.0 })
        for (int style = (int) cs::BassStyle::Sustain; style <= (int) cs::BassStyle::Walking; ++style)
            for (bool split : { false, true })
            {
                Host h (48000.0, 256);
                h.setBpm (bpm);
                h.processor.edit ([style] (cs::Progression& p, cs::Rng&)
                {
                    p.bars = 1;
                    p.numSlots = 4;
                    cs::layoutEven (p);
                    p.bassStyle = (cs::BassStyle) style;
                });
                if (auto* param = h.processor.apvts.getParameter ("splitMidi"))
                    param->setValueNotifyingHost (split ? 1.0f : 0.0f);

                const auto progression = h.processor.getProgression();
                const int blocks = (int) std::ceil (progression.lengthSteps() * samplesPerStep (h) / h.blockSize);
                const auto exported = h.processor.createMidiFile();
                struct MidiKey { int channel, note, velocity; bool on; double tick; };
                std::vector<MidiKey> expected;
                for (int t = 0; t < exported.getNumTracks(); ++t)
                    for (int e = 0; e < exported.getTrack (t)->getNumEvents(); ++e)
                    {
                        const auto& msg = exported.getTrack (t)->getEventPointer (e)->message;
                        if (msg.isNoteOn() || msg.isNoteOff())
                            expected.push_back ({ msg.getChannel(), msg.getNoteNumber(), msg.isNoteOn() ? (int) msg.getVelocity() : 0,
                                                  msg.isNoteOn(), msg.getTimeStamp() });
                    }

                h.playHead.playing = true;
                h.playHead.ppq = 0.0;
                h.run (blocks);
                h.playHead.playing = false;
                h.run (1);
                CHECK (h.heldAfterReplay() == 0);

                std::vector<MidiKey> rendered;
                const double ticksPerSample = bpm * 480.0 / (60.0 * h.sampleRate);
                const double loopEnd = progression.lengthSteps() * 20.0;
                for (const auto& event : h.events)
                {
                    const double tick = event.sample * ticksPerSample;
                    if (tick <= loopEnd + 1.0)
                        rendered.push_back ({ event.channel, event.note, event.on ? event.velocity : 0, event.on, tick });
                }
                if (rendered.size() != expected.size())
                    std::printf ("Bass parity count mismatch: style=%d bpm=%.0f split=%d rendered=%zu expected=%zu\n",
                                 style, bpm, (int) split, rendered.size(), expected.size());
                CHECK (rendered.size() == expected.size());
                std::vector<bool> matched (rendered.size(), false);
                int matches = 0;
                for (const auto& exp : expected)
                    for (size_t i = 0; i < rendered.size(); ++i)
                    {
                        const auto& live = rendered[i];
                        if (! matched[i] && live.channel == exp.channel && live.note == exp.note && live.on == exp.on
                            && live.velocity == exp.velocity && std::abs (live.tick - exp.tick) <= 2.0)
                        {
                            matched[i] = true;
                            ++matches;
                            break;
                        }
                    }
                if (matches != (int) expected.size())
                    std::printf ("Bass parity match mismatch: style=%d bpm=%.0f split=%d matched=%d expected=%zu\n",
                                 style, bpm, (int) split, matches, expected.size());
                CHECK (matches == (int) expected.size());
            }
}

// P1.9: Safe event buffering under extreme block size and tempo
void testLargeBlockDenseCompingSafety()
{
    Host h (48000.0, 65536);
    h.setBpm (999.0);

    if (auto* p = h.processor.apvts.getParameter ("compStyle"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) (int) cs::CompStyle::PulseEighth));
    if (auto* p = h.processor.apvts.getParameter ("splitMidi"))
        p->setValueNotifyingHost (1.0f);
    if (auto* p = h.processor.apvts.getParameter ("strum"))
        p->setValueNotifyingHost (p->convertTo0to1 (1.0f));

    h.processor.edit ([] (cs::Progression& prog, cs::Rng& rng)
    {
        prog.bars = 8;
        prog.numSlots = 16;
        cs::layoutEven (prog);
        cs::roll (prog, rng);
    });

    const cs::Progression p = h.processor.getProgression();
    const double sps = samplesPerStep (h);
    const int totalBlocks = (int) std::ceil ((double) p.lengthSteps() * sps / (double) h.blockSize);

    // Render 1 loop
    h.playHead.ppq = 0.0;
    h.playHead.playing = true;
    h.run (totalBlocks + 1);

    // Stop transport
    h.playHead.playing = false;
    h.run (2);

    CHECK (! h.events.empty());
    CHECK (h.heldAfterReplay() == 0); // Assert zero stuck notes after transport stop

    // Assert every note-on has a matching note-off
    std::map<std::pair<int, int>, int> sounding;
    for (const auto& ev : h.events)
    {
        const auto key = std::make_pair (ev.channel, ev.note);
        if (ev.on)
            ++sounding[key];
        else
        {
            CHECK (sounding[key] > 0);
            --sounding[key];
        }
    }
    for (const auto& kv : sounding)
        CHECK (kv.second == 0);
}

void testBassPatternBlockSpanningManyLoops()
{
    Host h (48000.0, 65536);
    h.setBpm (999.0);
    h.processor.edit ([] (cs::Progression& p, cs::Rng&)
    {
        p.bars = 1;
        p.numSlots = 2;
        cs::layoutEven (p);
        p.bassStyle = cs::BassStyle::OctavePump;
    });
    if (auto* param = h.processor.apvts.getParameter ("splitMidi"))
        param->setValueNotifyingHost (1.0f);

    const auto progression = h.processor.getProgression();
    CHECK (samplesPerStep (h) * h.blockSize / progression.lengthSteps() > 5.0);
    h.playHead.ppq = 0.0;
    h.playHead.playing = true;
    h.run (1);
    const double stepPerSample = h.playHead.bpm * 24.0 / (60.0 * h.sampleRate);
    bool bassBeyondThirdLoop = false;
    int bassStarts = 0;
    for (const auto& event : h.events)
    {
        if (event.channel == 2 && event.on)
        {
            ++bassStarts;
            bassBeyondThirdLoop = bassBeyondThirdLoop || event.sample * stepPerSample >= 2.0 * progression.lengthSteps();
        }
    }
    CHECK (bassStarts >= 20);
    CHECK (bassBeyondThirdLoop);
    h.playHead.playing = false;
    h.run (1);
    CHECK (h.heldAfterReplay() == 0);
}

// P1.10: Thread safety of tempo reads — host tempo change detected on audio thread and re-published
void testHostTempoChangeRepublish()
{
    Host h (48000.0, 256);
    h.setBpm (120.0);

    // Strum creates tempo-dependent voice offsets (cap = slotLenMs * 0.5)
    if (auto* p = h.processor.apvts.getParameter ("strum"))
        p->setValueNotifyingHost (p->convertTo0to1 (1.0f));

    // Progression with a short 2-step slot (slot 0: 2 steps)
    // At 120 BPM: 2 steps = 250 ms -> cap = 125 ms -> spread = 80 ms (strumMaxMs)
    // At 200 BPM: 2 steps = 150 ms -> cap = 75 ms -> spread = 75 ms (< 80 ms)
    h.processor.edit ([] (cs::Progression& prog, cs::Rng&)
    {
        prog.bars = 1;
        prog.numSlots = 2;
        prog.slots[0].start = 0;
        prog.slots[1].start = 2;
        prog.slots[0].voicing.notes[0] = 60;
        prog.slots[0].voicing.notes[1] = 64;
        prog.slots[0].voicing.notes[2] = 67;
        prog.slots[0].voicing.n = 3;
    });

    // Run blocks at 120 BPM
    h.playHead.playing = true;
    h.run (10);

    CHECK (std::abs (h.processor.getPublishedBpm() - 120.0) < 0.01);
    const cs::PlayData play120 = h.processor.getPlayData();
    const float offset120 = play120.slots[0].offsetMs[play120.slots[0].n - 1];
    CHECK (std::abs (offset120 - 14.5833f) < 0.05f);

    // Change fake host tempo to 200 BPM and run a block
    h.playHead.bpm = 200.0;
    h.run (1);

    // Tempo was read on the audio thread, but message thread re-publish has not run yet
    CHECK (std::abs (h.processor.getLastBpm() - 200.0) < 0.01);
    CHECK (std::abs (h.processor.getPublishedBpm() - 120.0) < 0.01);

    // Trigger the re-publish path (via timer callback)
    h.processor.timerCallback();

    // Check newly baked PlayData uses 200 BPM
    CHECK (std::abs (h.processor.getPublishedBpm() - 200.0) < 0.01);
    const cs::PlayData play200 = h.processor.getPlayData();
    const float offset200 = play200.slots[0].offsetMs[play200.slots[0].n - 1];
    CHECK (std::abs (offset200 - 8.75f) < 0.05f);
    CHECK (offset200 < offset120);

    // Exported MIDI file also reflects 200 BPM
    const juce::MidiFile midi200 = h.processor.createMidiFile();
    CHECK (midi200.getNumTracks() > 0);

    // Clean playback continue & stop
    h.run (20);
    h.playHead.playing = false;
    h.run (5);
    CHECK (h.heldAfterReplay() == 0);
}

} // namespace

void testEndingSelector()
{
    Host host;
    const auto before = host.processor.getProgression();
    std::unique_ptr<juce::AudioProcessorEditor> editor (host.processor.createEditor());
    juce::TextButton* settings = nullptr;
    for (auto* child : editor->getChildren())
        if (auto* button = dynamic_cast<juce::TextButton*> (child); button && button->getButtonText() == "Roll settings")
            settings = button;
    auto* panel = editor->findChildWithID ("roll-settings-panel");
    auto* ending = panel != nullptr ? dynamic_cast<juce::ComboBox*> (panel->findChildWithID ("ending")) : nullptr;
    CHECK (settings && ending && ! ending->isVisible());
    if (! settings || ! ending) return;
    settings->onClick();
    CHECK (settings->getToggleState());
    CHECK (panel && panel->isVisible() && panel->getY() > settings->getBottom()
           && editor->getLocalBounds().contains (panel->getBounds()));
    juce::TextButton* more = nullptr;
    for (auto* child : panel->getChildren())
        if (auto* button = dynamic_cast<juce::TextButton*> (child); button && button->getButtonText() == "More options")
            more = button;
    CHECK (more && ! ending->isVisible());
    if (! more) return;
    more->setToggleState (true, juce::dontSendNotification);
    more->onClick();
    CHECK (ending->isVisible() && ! ending->getBounds().isEmpty());
    CHECK (ending->getSelectedId() == 1 && ending->getNumItems() == 3);
    CHECK (ending->getItemText (0) == "Free");
    CHECK (ending->getItemText (1) == "Resolve to Tonic");
    CHECK (ending->getItemText (2) == "Lead Back to Start");
    for (int id : { 2, 3, 1 })
    {
        ending->setSelectedId (id, juce::dontSendNotification);
        ending->onChange();
        const auto after = host.processor.getProgression();
        CHECK (after.ending == (cs::Ending) (id - 1));
        CHECK (after.numSlots == before.numSlots);
        for (int i = 0; i < before.numSlots; ++i)
        {
            const auto& a = after.slots[(size_t) i];
            const auto& b = before.slots[(size_t) i];
            CHECK (a.start == b.start && a.chord == b.chord);
            CHECK (a.voicing.notes == b.voicing.notes && a.voicing.velocity == b.voicing.velocity);
        }
    }
    CHECK (editor->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)));
    CHECK (! settings->getToggleState() && ! panel->isVisible());
    settings->onClick();
    CHECK (settings->getToggleState() && panel->isVisible() && ending->isVisible());
    settings->onClick();
    CHECK (! settings->getToggleState() && ! panel->isVisible());
}


static juce::TextButton* songTestButton (juce::Component& root, const juce::String& name)
{
    if (auto* b = dynamic_cast<juce::TextButton*> (&root); b && b->getButtonText() == name) return b;
    for (auto* c : root.getChildren()) if (auto* b = songTestButton (*c, name)) return b;
    return nullptr;
}

static void phase4Snapshot (juce::Component& editor, const juce::File& dir, const juce::String& name, float scale = 1.0f)
{
    dir.createDirectory();
    auto file = dir.getChildFile (name + ".png");
    file.deleteFile();
    juce::FileOutputStream stream (file);
    juce::PNGImageFormat png;
    CHECK (stream.openedOk() && png.writeImageToStream (editor.createComponentSnapshot (editor.getLocalBounds(), true, scale), stream));
}

static void testFollowupFixes (const juce::File& output = {})
{
    Host h;
    std::unique_ptr<juce::AudioProcessorEditor> editor (h.processor.createEditor());
    auto* panel = dynamic_cast<SongPanel*> (editor->findChildWithID ("song-panel"));
    CHECK (panel != nullptr);
    if (! panel) return;
    // Start ordinary preview before opening Song, then use its Stop action.
    songTestButton (*editor, "Play")->onClick();
    CHECK (h.processor.isPreviewPlaying() && ! h.processor.isSongEnabled());
    songTestButton (*editor, "Song")->onClick();
    panel->refresh();
    static_cast<juce::TextButton*> (editor->findChildWithID ("preview-play"))->onClick();
    CHECK (! h.processor.isPreviewPlaying() && ! h.processor.isSongEnabled());
    songTestButton (*panel, "+ Add section")->onClick();
    static_cast<juce::TextButton*> (editor->findChildWithID ("preview-play"))->onClick();
    CHECK (h.processor.isPreviewPlaying() && h.processor.isSongEnabled());
    static_cast<juce::TextButton*> (editor->findChildWithID ("preview-play"))->onClick();
    CHECK (! h.processor.isPreviewPlaying() && h.processor.isSongEnabled());
    songTestButton (*editor, "Harmony")->onClick();

    juce::TemporaryFile target (".mid");
    const auto file = target.getFile();
    CHECK (SongPanel::saveMidiTo (file, h.processor.createSongMidiFile()).wasOk());
    {
        juce::FileInputStream in (file); juce::MidiFile midi;
        CHECK (midi.readFrom (in) && midi.getNumTracks() == 2);
    }
    CHECK (file.replaceWithText ("existing destination"));
   #if JUCE_WINDOWS
    // POSIX permits replacing a read-only file in a writable directory.
    CHECK (file.setReadOnly (true));
    CHECK (SongPanel::saveMidiTo (file, h.processor.createSongMidiFile()).failed());
    CHECK (file.loadFileAsString() == "existing destination");
    CHECK (file.setReadOnly (false));
   #endif
    CHECK (SongPanel::saveMidiTo (file, h.processor.createSongMidiFile()).wasOk());

    TimelineView* timeline = nullptr;
    ChordPickerOverlay* inspector = nullptr;
    for (auto* c : editor->getChildren()) {
        if (auto* t = dynamic_cast<TimelineView*> (c)) timeline = t;
        if (auto* i = dynamic_cast<ChordPickerOverlay*> (c)) inspector = i;
    }
    CHECK (timeline && inspector);
    if (! timeline || ! inspector) return;
    auto* play = songTestButton (*editor, "Play");
    auto* roll = editor->findChildWithID ("start-with-chords");
    auto* melody = editor->findChildWithID ("start-with-melody");
    CHECK (play && roll && melody);
    if (! play || ! roll || ! melody) return;
    for (auto size : { juce::Point<int> (1042, 679), juce::Point<int> (900, 570) }) {
        editor->setSize (size.x, size.y);
        for (bool open : { false, true, false }) {
            if (open) timeline->onOpenAlternatives (0); else inspector->onClose();
            CHECK (play == songTestButton (*editor, "Play"));
            CHECK (play->isVisible());
            CHECK (roll->isVisible() && melody->isVisible());
            CHECK (roll->getY() == melody->getY() && roll->getWidth() == melody->getWidth());
            auto* settings = songTestButton (*editor, "Roll settings");
            CHECK (settings && roll->getRight() <= settings->getX() && settings->getRight() < melody->getX());
            if (output != juce::File()) phase4Snapshot (*editor, output,
                "harmony-" + juce::String (size.x) + (open ? "-open" : "-closed"));
            const auto harmonyBounds = play->getBounds();
            songTestButton (*editor, "Feel")->onClick();
            CHECK (play->getBounds() == harmonyBounds);
            if (output != juce::File()) phase4Snapshot (*editor, output, "groove-" + juce::String (size.x));
            songTestButton (*editor, "Harmony")->onClick();
            CHECK (play->getBounds() == harmonyBounds);
        }
    }

    h.processor.edit ([] (cs::Progression& p, cs::Rng&) { cs::setBars (p, 8); cs::setNumSlots (p, 16); });
    h.processor.sendSynchronousChangeMessage();
    timeline->setSelectedSlot (15);
    timeline->onSlotSelected (15);
    timeline->onOpenAlternatives (15);
    CHECK (inspector->getRight() == timeline->getRight() && inspector->getY() >= timeline->getBottom());
    CHECK (editor->getLocalBounds().contains (inspector->getBounds()));
    if (output != juce::File())
    {
        phase4Snapshot (*editor, output, "harmony-900-16-slots");
        songTestButton (*inspector, "Inspector")->onClick();
        phase4Snapshot (*editor, output, "harmony-900-inspector");
        inspector->onClose();
        songTestButton (*editor, "Roll settings")->onClick();
        phase4Snapshot (*editor, output, "harmony-900-roll-settings");
        songTestButton (*editor, "Roll settings")->onClick();
    }
}

static void testPhase4 (const juce::File& output)
{
    Host h;
    std::unique_ptr<juce::AudioProcessorEditor> editor (h.processor.createEditor());
    auto* panel = dynamic_cast<SongPanel*> (editor->findChildWithID ("song-panel"));
    CHECK (panel != nullptr);
    if (! panel) return;
    editor->setSize (900, 570);
    songTestButton (*editor, "Song")->onClick();
    CHECK (panel->isVisible() && editor->getLocalBounds().contains (panel->getBounds()));
    CHECK (! editor->findChildWithID ("preview-play")->isEnabled());
    phase4Snapshot (*editor, output, "song-empty-900");
    phase4Snapshot (*editor, output, "song-empty-900-150pct", 1.5f);
    auto* add = songTestButton (*panel, "+ Add section");
    add->onClick(); add->onClick(); add->onClick();
    CHECK (h.processor.getSong().numEntries == 3);
    auto* viewport = [&]() -> juce::Viewport* { for (auto* c : panel->getChildren()) if (auto* v = dynamic_cast<juce::Viewport*> (c)) return v; return nullptr; }();
    CHECK (viewport != nullptr);
    if (! viewport) return;
    auto* rows = viewport->getViewedComponent();
    const int vars[] = { 1, 0, 2 }, shifts[] = { 7, -5, 12 };
    for (int i = 0; i < 3; ++i) {
        auto* section = dynamic_cast<juce::TextButton*> (rows->findChildWithID ("song-section-" + juce::String (i)));
        CHECK (section != nullptr);
        if (! section) return;
        section->onClick();
        auto* v = dynamic_cast<juce::ComboBox*> (panel->findChildWithID ("song-variation"));
        auto* t = dynamic_cast<juce::Slider*> (panel->findChildWithID ("song-transpose"));
        CHECK (v && t);
        v->setSelectedId (vars[i] + 1, juce::sendNotificationSync);
        t->setValue (shifts[i], juce::sendNotificationSync);
        CHECK (h.processor.getSong().entries[i].variation == vars[i]);
        CHECK (h.processor.getSong().entries[i].transpose == shifts[i]);
    }
    panel->refresh();
    phase4Snapshot (*editor, output, "song-populated-900");
    phase4Snapshot (*editor, output, "song-populated-900-150pct", 1.5f);
    editor->setSize (1040, 650); phase4Snapshot (*editor, output, "song-populated-1040");
    static_cast<juce::TextButton*> (editor->findChildWithID ("preview-play"))->onClick();
    CHECK (h.processor.isSongEnabled() && h.processor.isPreviewPlaying());
    h.run (400);
    CHECK (! h.events.empty());
    static_cast<juce::TextButton*> (editor->findChildWithID ("preview-play"))->onClick();
    CHECK (! h.processor.isPreviewPlaying());
    h.processor.setSongEnabled (false);
    // Compare every exported note to each variation's independent export, including track and timestamp.
    using Note = std::tuple<int, int, bool, int>;
    std::vector<Note> expected[2], actual[2];
    double offset = 0;
    for (int section = 0; section < 3; ++section) {
        h.processor.setEditVariation (vars[section]);
        h.processor.apvts.getParameter ("splitMidi")->setValueNotifyingHost (1.0f);
        const auto midi = h.processor.createMidiFile();
        CHECK (midi.getNumTracks() == 2);
        for (int tr = 0; tr < 2; ++tr) for (int e = 0; e < midi.getTrack (tr)->getNumEvents(); ++e) {
            const auto& m = midi.getTrack (tr)->getEventPointer (e)->message;
            if (m.isNoteOnOrOff()) expected[tr].emplace_back ((int) std::lround (m.getTimeStamp() + offset), juce::jlimit (0, 127, m.getNoteNumber() + shifts[section]), m.isNoteOn(), m.isNoteOn() ? m.getVelocity() : 0);
        }
        offset += h.processor.getProgression().lengthSteps() * 20;
    }
    const auto file = h.processor.writeTempMidiFile (ChordSlotsProcessor::MidiExportMode::Song);
    file.copyFileTo (output.getChildFile ("song-export.mid"));
    juce::FileInputStream input (file); juce::MidiFile midi;
    CHECK (midi.readFrom (input) && midi.getNumTracks() == 2);
    for (int tr = 0; tr < 2; ++tr) {
        for (int e = 0; e < midi.getTrack (tr)->getNumEvents(); ++e) {
            const auto& m = midi.getTrack (tr)->getEventPointer (e)->message;
            if (m.isNoteOnOrOff()) actual[tr].emplace_back ((int) std::lround (m.getTimeStamp()), m.getNoteNumber(), m.isNoteOn(), m.isNoteOn() ? m.getVelocity() : 0);
        }
        std::sort (actual[tr].begin(), actual[tr].end()); std::sort (expected[tr].begin(), expected[tr].end());
        CHECK (! actual[tr].empty() && actual[tr] == expected[tr]);
    }
    static_cast<juce::TextButton*> (rows->findChildWithID ("song-section-0"))->onClick();
    songTestButton (*panel, "Remove section")->onClick();
    CHECK (h.processor.getSong().numEntries == 2 && h.processor.getSong().entries[0].transpose == -5);
    for (int i = 0; i < 20; ++i) add->onClick();
    CHECK (h.processor.getSong().numEntries == 16 && ! add->isEnabled());
    editor->setSize (900, 570); viewport->setViewPosition (rows->getWidth(), 0);
    CHECK (viewport->getViewPositionX() > 0);
    phase4Snapshot (*editor, output, "song-full-900");
    songTestButton (*panel, "Clear chain")->onClick();
    CHECK (h.processor.getSong().numEntries == 0);
    songTestButton (*editor, "Harmony")->onClick();
    CHECK (! panel->isVisible());
    testSongMode();
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc == 3 && juce::String (argv[1]) == "--followup") {
        testFollowupFixes (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        testPhase4 (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        std::printf ("Follow-up: %d checks, %d failures\n", checks, failures);
        return failures ? 1 : 0;
    }

    if (argc == 4 && juce::String (argv[1]) == "--song-window")
    {
        Host h;
        juce::DocumentWindow window ("Song bounds verification", juce::Colours::black, juce::DocumentWindow::allButtons);
        auto* editor = h.processor.createEditor();
        const float scale = juce::String (argv[2]).getFloatValue();
        editor->setSize (900, 570);
        editor->setScaleFactor (scale);
        struct ScaledContent : juce::Component { ~ScaledContent() override { deleteAllChildren(); } };
        auto* content = new ScaledContent();
        content->addAndMakeVisible (editor);
        content->setSize (juce::roundToInt (900 * scale), juce::roundToInt (570 * scale));
        window.setContentOwned (content, true);
        window.centreWithSize (window.getWidth(), window.getHeight());
        window.setVisible (true);
        songTestButton (*editor, "Song")->onClick();
        juce::Timer::callAfterDelay (500, [&] {
        auto dir = juce::File::getCurrentWorkingDirectory().getChildFile (argv[3]);
        phase4Snapshot (window, dir, "window-" + juce::String (scale));
        for (int i = 0; i < juce::Desktop::getInstance().getNumComponents(); ++i) {
            auto* c = juce::Desktop::getInstance().getComponent (i);
            const auto bounds = c->getScreenBounds();
            std::printf ("desktop %d: %s %s\n", i, c->getName().toRawUTF8(), bounds.toString().toRawUTF8());
            if (c != &window && c->isVisible()) {
                phase4Snapshot (*c, dir, "popup-" + juce::String (scale) + "-" + juce::String (i));
                auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (bounds);
                std::printf ("popup inside screen=%d inside window=%d\n", display && display->userBounds.contains (bounds.toFloat()), window.getScreenBounds().contains (bounds));
            }
        }
        juce::PopupMenu::dismissAllActiveMenus();
        juce::MessageManager::getInstance()->stopDispatchLoop();
        });
        juce::MessageManager::getInstance()->runDispatchLoop();
        return failures ? 1 : 0;
    }
    if (argc == 3 && juce::String (argv[1]) == "--phase4")
    {
        testPhase4 (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        std::printf ("Phase 4: %d checks, %d failures\n", checks, failures);
        return failures ? 1 : 0;
    }
    if (argc == 3 && juce::String (argv[1]) == "--preview-piano-ab")
        return renderPianoComparison (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));

    if (argc == 4 && juce::String (argv[1]) == "--ending-midi")
    {
        const auto directory = juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]);
        if (directory.createDirectory().failed())
            return 1;
        const auto seed = (unsigned) juce::String (argv[3]).getIntValue();
        ChordSlotsProcessor processor;
        for (int scale : { 0, 2 })
            for (int mode = 0; mode < 3; ++mode)
            {
                processor.edit ([=] (cs::Progression& p, cs::Rng&)
                {
                    p = cs::Progression {};
                    p.tonic = scale == 0 ? 0 : 9;
                    p.scale = scale;
                    p.adventure = 0.10f;
                    p.ending = (cs::Ending) mode;
                    p.locks.rhythm = true;
                    for (int i = 0; i < p.numSlots; ++i)
                        p.slots[(size_t) i].start = 96 * i;
                    cs::Rng seeded (seed);
                    cs::roll (p, seeded);
                }, false);
                const char* names[] { "Free", "Resolve-to-Tonic", "Lead-Back-to-Start" };
                const auto name = juce::String (scale == 0 ? "C-major-" : "A-dorian-") + names[mode];
                const auto path = directory.getChildFile (name + ".mid");
                if (! path.deleteFile())
                    return 1;
                juce::FileOutputStream out (path);
                if (! out.openedOk() || ! processor.createMidiFile().writeTo (out, 1))
                    return 1;
                std::printf ("%s: %s\n", name.toRawUTF8(), processor.describeProgression().toRawUTF8());
            }
        return 0;
    }

    if (argc == 3 && juce::String (argv[1]) == "--phase3")
    {
        testPhase3Views (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        std::printf ("%d checks, %d failures\n", checks, failures);
        return failures == 0 ? 0 : 1;
    }

    // `--snapshot out.png [mood] [--groove]` renders the editor off-screen instead of running the tests.
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        auto processor = std::make_unique<ChordSlotsProcessor>();
        if (argc > 3)
        {
            const int mood = juce::jlimit (0, (int) cs::moods().size() - 1, juce::String (argv[3]).getIntValue());
            processor->edit ([mood] (cs::Progression& p, cs::Rng& rng)
            {
                cs::applyMood (p, mood);
                cs::roll (p, rng);
            });
        }
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());
        if (argc > 4 && juce::String (argv[4]) == "--groove")
        {
            bool opened = false;
            for (auto* child : editor->getChildren())
            {
                auto* button = dynamic_cast<juce::TextButton*> (child);
                if (button != nullptr && button->getButtonText() == "Feel" && button->onClick)
                {
                    button->onClick();
                    opened = true;
                    break;
                }
            }
            if (! opened)
            {
                std::fprintf (stderr, "Could not open the Groove & Feel tab\n");
                return 1;
            }
        }
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
    testStandaloneTempo();
    testAcousticPianoPreview();
    testCompStyleAudition();
    testShldRhythmStateAndPhase();
    testBassAndArpAuditions();
    testConfigurableArpComping();
    testSeparatorLocksAndAlternativeAudition();
    testInspectAlternatives();
    testPhase3Views();
    testSelectedSlotInspector();
    testEditWhilePlaying();
    testStateRoundTrip();
    testEndingSelector();
    testTonalContextAndTranspose();
    testModalSuggestionsAndRoll();
    testHarmonizeMelody();
    testMidiFile();
    testSongMode();
    testPerformanceAndStrum();
    testFollowupFixes();
    testIncomingMidiTriggerIntegration();
    testChordInversion();
    testSplitMidiOutput();
    testChordMarkersP74();
    testExportPlaybackParity();
    testBassPatternExportPlaybackParity();
    testBassPatternBlockSpanningManyLoops();
    testLargeBlockDenseCompingSafety();
    testHostTempoChangeRepublish();
    testVariationBankAndHistory();
    testMidiVariationSwitchIsDeferredToMessageThread();
    testExtendedChordTypesStateRoundTrip();
    testMeterAndTripletGrid();
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
