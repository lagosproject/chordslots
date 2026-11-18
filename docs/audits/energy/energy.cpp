// Reuse the repository's fake host and note-event capture.
#define main chordslots_plugin_tests_main
#include "../../../tests/plugin_tests.cpp"
#undef main

#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace
{
constexpr int edited = 1;
constexpr int ppq = 480;

void writeSlots (std::ostream& out, const std::string& id, const cs::Progression& p)
{
    const auto play = cs::toPlayData (p);
    for (int i = 0; i < p.numSlots; ++i)
    {
        const auto& s = p.slots[i];
        out << id << ',' << i + 1 << ',' << s.start << ',' << p.slotEnd (i) << ','
            << s.chord.root << ',' << s.chord.type << ',' << s.chord.bassInt << ','
            << s.targetB << ',' << s.targetT << ',' << s.targetE << ',' << s.brightness << ','
            << s.tension << ',' << s.energy << ',' << s.voicing.velocity << ',' << s.voicing.n
            << ',' << play.slots[i].primaryBass << ',' << s.voicing.bassCount << ',';
        for (int j = 0; j < s.voicing.n; ++j)
            out << (j ? " " : "") << s.voicing.notes[j];
        out << '\n';
    }
}

void capture (std::ostream& out, const std::string& id, const cs::Progression& p, bool split, bool preview)
{
    Host h (48000, 256);
    const auto set = [&] (const char* name, float value)
    {
        auto* param = h.processor.apvts.getParameter (name);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    };
    set ("gate", 0.8f);
    set ("accent", 0);
    set ("humanize", 0);
    set ("strum", 0);
    set ("compStyle", 0);
    set ("chordRhythm", 0);
    set ("synthOn", 1);
    set ("splitMidi", split ? 1.0f : 0.0f);
    h.processor.edit ([&] (cs::Progression& destination, cs::Rng&) { destination = p; });

    const auto write = [&] (const char* route, double tick, int channel, int note, bool on, int velocity)
    {
        out << id << ',' << split << ',' << route << ',' << tick << ',' << channel << ','
            << note << ',' << on << ',' << velocity << '\n';
    };
    if (! preview)
    {
        const auto file = h.processor.createMidiFile();
        juce::MemoryOutputStream bytes;
        if (! file.writeTo (bytes, split ? 1 : 0))
            throw std::runtime_error ("MIDI write failed");
        if (id == "Piano-101-edit-95")
        {
            const juce::File sample = juce::File::getCurrentWorkingDirectory().getChildFile (
                split ? "piano-101-split.mid" : "piano-101-combined.mid");
            if (! sample.replaceWithData (bytes.getData(), bytes.getDataSize()))
                throw std::runtime_error ("MIDI sample write failed");
        }
        juce::MemoryInputStream stream (bytes.getData(), bytes.getDataSize(), false);
        juce::MidiFile decoded;
        if (! decoded.readFrom (stream, false) || decoded.getTimeFormat() != ppq)
            throw std::runtime_error ("MIDI readback failed");
        for (int t = 0; t < decoded.getNumTracks(); ++t)
            for (const auto* event : *decoded.getTrack (t))
            {
                const auto& m = event->message;
                if (m.isNoteOn() || m.isNoteOff())
                    write ("smf", m.getTimeStamp(), m.getChannel(), m.getNoteNumber(), m.isNoteOn(),
                           m.isNoteOn() ? (int) m.getVelocity() : 0);
            }
    }

    const int blocks = (int) std::ceil (p.lengthSteps() * samplesPerStep (h) / h.blockSize);
    h.playHead.playing = ! preview;
    h.processor.setPreviewPlaying (preview);
    h.run (blocks);
    h.playHead.playing = false;
    h.processor.setPreviewPlaying (false);
    h.run (1);
    if (h.heldAfterReplay() != 0 || h.peak <= 0 || h.nonFinite || failures != 0)
        throw std::runtime_error ("Invalid processor output");
    for (const auto& e : h.events)
        write (preview ? "preview" : "live", e.sample * h.playHead.bpm * ppq / (60 * h.sampleRate),
               e.channel, e.note, e.on, e.velocity);
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::ofstream slots ("slots.csv"), events ("midi-events.csv"), slash ("slash-bass.csv");
    for (auto* out : { &slots, &slash })
        *out << std::setprecision (9)
             << "case,slot,start,end,root,type,bassInt,targetB,targetT,targetE,brightness,tension,energy,velocity,count,primaryBass,bassCount,notes\n";
    events << std::setprecision (12) << "case,split,route,tick,channel,note,on,velocity\n";

    for (auto style : { cs::VoicingStyle::Piano, cs::VoicingStyle::Close, cs::VoicingStyle::Rootless })
        for (int seed : { 101, 102, 103, 104, 105, 106, 107, 108 })
        {
            cs::Progression base;
            base.bars = 2;
            base.tonic = 0;
            base.scale = seed % 2;
            base.voicingStyle = (int) style;
            base.style = (int) cs::CorpusStyle::Free;
            base.locks.rhythm = true;
            cs::layoutEven (base);
            for (int i = 0; i < base.numSlots; ++i)
            {
                base.slots[i].targetB = 0.35f + 0.1f * i;
                base.slots[i].targetT = 0.30f + 0.1f * i;
                base.slots[i].targetE = 0.50f;
            }
            cs::Rng checkpoint (seed);
            cs::roll (base, checkpoint);
            const std::string prefix = std::string (cs::voicingStyleName (style)) + "-" + std::to_string (seed);
            writeSlots (slots, prefix + "-base-50", base);
            if (seed == 101)
            {
                auto inverted = base;
                auto inversionRng = checkpoint;
                auto chord = inverted.slots[edited].chord;
                chord.bassInt = 4; // Ab major with C in the bass.
                cs::setChord (inverted, edited, chord, inversionRng);
                writeSlots (slash, prefix + "-base-50", inverted);
                for (int level : { 10, 50, 95 })
                {
                    auto p = inverted;
                    auto rng = inversionRng;
                    p.slots[edited].targetE = level / 100.0f;
                    cs::applyEnergy (p, rng);
                    if (p.slots[edited].chord != chord)
                        throw std::runtime_error ("Slash chord invariant failed");
                    writeSlots (slash, prefix + "-edit-" + std::to_string (level), p);
                }
            }
            for (const auto& mode : { std::string ("edit"), std::string ("roll") })
                for (int level : { 10, 25, 50, 59, 60, 61, 75, 84, 85, 86, 95, 100 })
                {
                    auto p = base;
                    auto rng = checkpoint;
                    p.slots[edited].targetE = level / 100.0f;
                    if (mode == "edit")
                        cs::applyEnergy (p, rng);
                    else
                    {
                        p.locks.roots = p.locks.colors = true;
                        cs::roll (p, rng);
                    }
                    for (int i = 0; i < p.numSlots; ++i)
                        if (p.slots[i].chord != base.slots[i].chord || p.slots[i].start != base.slots[i].start
                            || p.slots[i].targetB != base.slots[i].targetB || p.slots[i].targetT != base.slots[i].targetT)
                            throw std::runtime_error ("Fixed chord or target invariant failed");
                    const auto id = prefix + "-" + mode + "-" + std::to_string (level);
                    writeSlots (slots, id, p);
                    if (level == 10 || level == 50 || level == 95)
                        for (bool split : { false, true })
                            for (bool preview : { false, true })
                                capture (events, id, p, split, preview);
                }
        }
    return slots && events && slash ? 0 : 1;
}
