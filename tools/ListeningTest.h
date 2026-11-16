// Blind listening fixtures for ChordSlots.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "chordslots/Generator.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>

namespace listening
{
constexpr int slots = 4, repeats = 2, ppq = 480, velocity = 84, sampleRate = 44100;
constexpr int samplesPerSlot = sampleRate * 3 / 5; // one beat at 100 BPM

inline void writeInteger (std::ostream& out, uint32_t value, int bytes, bool bigEndian = false)
{
    for (int i = 0; i < bytes; ++i)
        out.put ((char) (value >> (8 * (bigEndian ? bytes - i - 1 : i))));
}

inline void writeDelta (std::ostream& out, uint32_t value)
{
    uint8_t chunks[4];
    int count = 0;
    chunks[count++] = value & 127;
    while ((value >>= 7) != 0)
        chunks[count++] = (value & 127) | 128;
    while (count != 0)
        out.put ((char) chunks[--count]);
}

inline void writeMidi (const cs::Progression& p, const std::filesystem::path& path)
{
    std::ostringstream track;
    const uint8_t tempo[] { 0, 0xff, 0x51, 3, 0x09, 0x27, 0xc0 }; // 600,000 us per beat
    track.write (reinterpret_cast<const char*> (tempo), sizeof (tempo));
    for (int beat = 0; beat < slots * repeats; ++beat)
    {
        const auto& voice = p.slots[(size_t) (beat % slots)].voicing;
        for (int note = 0; note < voice.n; ++note)
        {
            writeDelta (track, note == 0 && beat != 0 ? ppq / 10 : 0);
            track.put ((char) 0x90);
            track.put ((char) voice.notes[(size_t) note]);
            track.put (velocity);
        }
        for (int note = 0; note < voice.n; ++note)
        {
            writeDelta (track, note == 0 ? ppq * 9 / 10 : 0);
            track.put ((char) 0x80);
            track.put ((char) voice.notes[(size_t) note]);
            track.put (0);
        }
    }
    writeDelta (track, ppq / 10);
    track.put ((char) 0xff);
    track.put (0x2f);
    track.put (0);

    const auto data = track.str();
    std::ofstream out (path, std::ios::binary);
    out.write ("MThd", 4);
    writeInteger (out, 6, 4, true);
    writeInteger (out, 0, 2, true);
    writeInteger (out, 1, 2, true);
    writeInteger (out, ppq, 2, true);
    out.write ("MTrk", 4);
    writeInteger (out, (uint32_t) data.size(), 4, true);
    out.write (data.data(), (std::streamsize) data.size());
    out.close();
    if (! out)
        throw std::runtime_error ("Could not write " + path.string());
}

inline void writeAudio (const cs::Progression& p, const std::filesystem::path& path)
{
    constexpr int gate = samplesPerSlot * 9 / 10;
    constexpr int dataBytes = slots * repeats * samplesPerSlot * 2;
    std::ofstream out (path, std::ios::binary);
    out.write ("RIFF", 4);
    writeInteger (out, 36 + dataBytes, 4);
    out.write ("WAVEfmt ", 8);
    writeInteger (out, 16, 4);
    writeInteger (out, 1, 2); // PCM
    writeInteger (out, 1, 2); // mono
    writeInteger (out, sampleRate, 4);
    writeInteger (out, sampleRate * 2, 4);
    writeInteger (out, 2, 2);
    writeInteger (out, 16, 2);
    out.write ("data", 4);
    writeInteger (out, dataBytes, 4);

    for (int beat = 0; beat < slots * repeats; ++beat)
    {
        const auto& voice = p.slots[(size_t) (beat % slots)].voicing;
        std::array<double, cs::kMaxNotes> frequencies {};
        for (int i = 0; i < voice.n; ++i)
            frequencies[(size_t) i] = 6.283185307179586 * cs::midiToHz (voice.notes[(size_t) i]) / sampleRate;
        const double gain = 0.22 / std::sqrt (std::max (voice.n, 1));
        for (int sample = 0; sample < gate; ++sample)
        {
            const double envelope = std::min ({ 1.0, sample / 360.0, (gate - sample - 1) / 530.0 });
            double wave = 0.0;
            for (int i = 0; i < voice.n; ++i)
            {
                const double phase = frequencies[(size_t) i] * sample;
                wave += std::sin (phase) + 0.25 * std::sin (2 * phase) + 0.07 * std::sin (3 * phase);
            }
            const auto pcm = (int16_t) std::lround (32767 * std::clamp (gain * envelope * wave, -0.98, 0.98));
            writeInteger (out, (uint16_t) pcm, 2);
        }
        for (int sample = gate; sample < samplesPerSlot; ++sample)
            writeInteger (out, 0, 2);
    }
    out.close();
    if (! out)
        throw std::runtime_error ("Could not write " + path.string());
}

struct Clip
{
    cs::Progression progression;
    float legacy = 0.0f;
    float contextual = 0.0f;
    int mood = 0;
};

inline auto pitchesOf (const Clip& clip)
{
    std::array<int, slots * cs::kMaxNotes> notes {};
    for (int slot = 0; slot < slots; ++slot)
    {
        const auto& voiced = clip.progression.slots[(size_t) slot].voicing;
        for (int note = 0; note < voiced.n; ++note)
            notes[(size_t) (slot * cs::kMaxNotes + note)] = voiced.notes[(size_t) note] + 1;
    }
    return notes;
}

inline std::vector<double> ranks (const std::vector<Clip>& pool, float Clip::* member)
{
    std::vector<size_t> order (pool.size());
    std::iota (order.begin(), order.end(), 0);
    std::stable_sort (order.begin(), order.end(), [&] (size_t a, size_t b)
    {
        return pool[a].*member < pool[b].*member;
    });
    std::vector<double> result (order.size());
    for (size_t first = 0, end = 0; first < order.size(); first = end)
    {
        for (end = first + 1; end < order.size() && pool[order[first]].*member == pool[order[end]].*member; ++end)
        {
        }
        for (size_t i = first; i < end; ++i)
            result[order[i]] = 0.5 * (double) (first + end - 1);
    }
    return result;
}

inline std::vector<Clip> chooseClips()
{
    cs::Rng rng (20261116);
    std::vector<Clip> pool;
    constexpr int perMood = 160;
    for (int mood = 0; mood < (int) cs::moods().size(); ++mood)
        for (int sample = 0; sample < perMood; ++sample)
        {
            auto& clip = pool.emplace_back();
            auto& p = clip.progression;
            clip.mood = mood;
            p.bars = 1;
            p.numSlots = slots;
            p.locks.rhythm = true;
            cs::layoutEven (p);
            cs::applyMood (p, mood);
            cs::roll (p, rng);
            for (int slot = 0; slot < slots; ++slot)
            {
                clip.legacy += cs::tensionOf (p.slots[(size_t) slot].chord, p.tonic, p.scale) / slots;
                clip.contextual += cs::measuredContextualTension (p, slot) / slots;
            }
        }

    const auto legacy = ranks (pool, &Clip::legacy), contextual = ranks (pool, &Clip::contextual);
    std::vector<size_t> order (pool.size());
    std::iota (order.begin(), order.end(), 0);
    std::stable_sort (order.begin(), order.end(), [&] (size_t a, size_t b)
    {
        return std::abs (legacy[a] - contextual[a]) > std::abs (legacy[b] - contextual[b]);
    });

    std::set<decltype (pitchesOf (pool.front()))> heard;
    std::vector<Clip> chosen;
    const auto add = [&] (size_t i)
    {
        if (! heard.insert (pitchesOf (pool[i])).second)
            return false;
        chosen.push_back (pool[i]);
        return true;
    };
    for (size_t i : order)
    {
        add (i);
        if (chosen.size() == 10)
            break;
    }

    const auto meanRank = [&] (size_t i) { return 0.5 * (legacy[i] + contextual[i]); };
    std::stable_sort (order.begin(), order.end(), [&] (size_t a, size_t b) { return meanRank (a) < meanRank (b); });
    for (int mood = 0; mood < (int) cs::moods().size(); ++mood)
    {
        std::vector<size_t> withinMood;
        for (size_t i : order)
            if (pool[i].mood == mood)
                withinMood.push_back (i);
        const double middle = meanRank (withinMood[withinMood.size() / 2]);
        std::stable_sort (withinMood.begin(), withinMood.end(), [&] (size_t a, size_t b)
        {
            return std::abs (meanRank (a) - middle) < std::abs (meanRank (b) - middle);
        });
        for (size_t i : withinMood)
            if (add (i))
                break;
    }

    for (double quantile : { 0.25, 0.75 })
    {
        const double target = (double) (pool.size() - 1) * quantile;
        std::stable_sort (order.begin(), order.end(), [&] (size_t a, size_t b)
        {
            return std::abs (meanRank (a) - target) < std::abs (meanRank (b) - target);
        });
        for (size_t i : order)
            if (add (i))
                break;
    }
    if (chosen.size() != 20)
        throw std::runtime_error ("Could not find 20 distinct clips");
    std::shuffle (chosen.begin(), chosen.end(), rng);
    return chosen;
}

inline int run (int argc, char** argv)
{
    namespace fs = std::filesystem;
    fs::path outDir, keyPath;
    bool valid = true;
    for (int i = 2; i < argc; i += 2)
    {
        const std::string option = argv[i];
        if (i + 1 == argc)
        {
            valid = false;
            break;
        }
        if (option == "--out" && outDir.empty())
            outDir = argv[i + 1];
        else if (option == "--key" && keyPath.empty())
            keyPath = argv[i + 1];
        else
        {
            valid = false;
            break;
        }
    }
    if (! valid || outDir.empty())
    {
        std::fprintf (stderr, "Usage: chordslots-cli listening-test --out clips-dir [--key separate-key.csv]\n");
        return 2;
    }

    try
    {
        outDir = fs::weakly_canonical (fs::absolute (outDir));
        if (! outDir.has_filename())
            outDir = outDir.parent_path();
        if (keyPath.empty())
            keyPath = outDir.parent_path() / (outDir.filename().string() + "-answer-key.csv");
        keyPath = fs::weakly_canonical (fs::absolute (keyPath));
        const auto relative = keyPath.lexically_relative (outDir);
        if (! relative.empty() && *relative.begin() != "..")
            throw std::runtime_error ("The answer key must be outside the clips directory");
        if (fs::exists (keyPath) || (fs::exists (outDir) && ! fs::is_empty (outDir)))
            throw std::runtime_error ("Choose a new answer key and an empty output directory");

        const auto clips = chooseClips();
        fs::create_directories (outDir);
        fs::create_directories (keyPath.parent_path());
        std::ofstream ratings (outDir / "ratings.csv");
        std::ofstream key (keyPath);
        ratings << "id,rating\n";
        key << "id,static,contextual\n" << std::setprecision (9);
        for (size_t i = 0; i < clips.size(); ++i)
        {
            std::ostringstream name;
            name << "clip-" << std::setw (2) << std::setfill ('0') << i + 1;
            const auto id = name.str();
            writeMidi (clips[i].progression, outDir / (id + ".mid"));
            writeAudio (clips[i].progression, outDir / (id + ".wav"));
            ratings << id << ",\n";
            key << id << ',' << clips[i].legacy << ',' << clips[i].contextual << '\n';
        }
        ratings.close();
        key.close();
        if (! ratings || ! key)
            throw std::runtime_error ("Could not write the listening CSV files");
        std::printf ("Wrote 20 MIDI and WAV pairs and a ratings template. Answer key saved separately.\n");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf (stderr, "Listening test: %s\n", error.what());
        return 1;
    }
}
} // namespace listening
