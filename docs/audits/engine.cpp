#include "chordslots/Generator.h"
#include <iostream>
#include <bitset>
#include <stdexcept>
#include <iomanip>
#include <string>

using namespace cs;

void brightness(bool edit)
{
    std::cout << "scale,seed,target,slot,root,type,bass,chord,harmonic,rawH,centre,upperMidi,allMidi,actual,poolMin,poolMax,notes\n";
    for (int scale : {0, 1, 2, 3, 4, 7})
    {
        const auto pool = buildCandidates(0, scale, 0.3f);
        float lo = 1, hi = 0;
        for (auto& c : pool) { lo = std::min(lo, c.brightness); hi = std::max(hi, c.brightness); }
        for (int seed = 101; seed <= 120; ++seed)
            for (float b : {0.1f, 0.5f, 0.9f})
            {
                Progression p;
                p.tonic = 0;
                p.scale = scale;
                p.style = (int) CorpusStyle::Free;
                p.locks.rhythm = true;
                layoutEven(p);
                // Start lane edits from the same medium-target progression.
                Rng rng(seed);
                if (edit)
                {
                    Rng baseline(seed);
                    roll(p, baseline);
                }
                for (int i = 0; i < p.numSlots; ++i) p.slots[i].targetB = b;
                if (edit) rerollSlots(p, (1u << p.numSlots) - 1u, rng);
                else roll(p, rng);
                for (int i = 0; i < p.numSlots; ++i)
                {
                    const auto& s = p.slots[i];
                    int upper = 0, upperCount = 0, total = 0;
                    for (int j = 0; j < s.voicing.n; ++j)
                    {
                        const int note = s.voicing.notes[j];
                        total += note;
                        if (note >= kUpperFloor) { upper += note; ++upperCount; }
                    }
                    const auto c = s.chord;
                    const float rawH = 0.55f * qualityBrightness(c.type) + 0.25f * modalBrightness(c, p.tonic, scale);
                    std::cout << scales()[scale].name << ',' << seed << ',' << b << ',' << i+1 << ','
                              << c.root << ',' << c.type << ',' << c.bassInt << ',' << chordName(c, 0, scale) << ','
                              << chordMetrics(c, 0, scale).brightness << ',' << rawH << ','
                              << centreForBrightness(b) << ',' << double(upper)/upperCount << ','
                              << double(total)/s.voicing.n << ',' << s.brightness << ',' << lo << ',' << hi << ',';
                    for (int j = 0; j < s.voicing.n; ++j) std::cout << (j ? " " : "") << s.voicing.notes[j];
                    std::cout << '\n';
                }
            }
    }
}

void tension()
{
    std::cout << "progression,slot,chord,mapping,staticSymbol,staticVoiced,contextLinear,contextLoop\n";
    int id, slots, tonic, scale;
    while (std::cin >> id >> slots >> tonic >> scale)
    {
        Progression p;
        p.numSlots = slots;
        p.tonic = tonic;
        p.scale = scale;
        std::vector<int> errors;
        for (int i = 0; i < slots; ++i)
        {
            auto& s = p.slots[i];
            int rootHint;
            std::cin >> rootHint >> s.voicing.n;
            if (!std::cin || s.voicing.n < 1 || s.voicing.n > kMaxNotes || slots > kMaxSlots)
                throw std::runtime_error("Invalid stimulus");
            unsigned mask = 0;
            for (int j = 0; j < s.voicing.n; ++j)
            {
                std::cin >> s.voicing.notes[j];
                mask |= 1u << mod12(s.voicing.notes[j]);
            }
            int best = 100;
            for (int root = 0; root < 12; ++root)
                for (int type = 0; type < (int) chordTypes().size(); ++type)
                {
                    if (rootHint >= 0 && root != rootHint) continue;
                    Chord c {root, type};
                    const unsigned cm = chordMask(c);
                    const int error = 2 * static_cast<int>(std::bitset<12>(cm ^ mask).count())
                                      + (rootHint < 0 && root != mod12(s.voicing.notes[0]));
                    if (error >= best) continue;
                    best = error;
                    s.chord = c;
                    const int inversion = mod12(s.voicing.notes[0] - root);
                    if (std::find(chordTypes()[type].iv.begin(), chordTypes()[type].iv.end(), inversion)
                        != chordTypes()[type].iv.end()) s.chord.bassInt = inversion;
                }
            errors.push_back(best / 2);
        }
        for (int i = 0; i < slots; ++i)
        {
            const auto& s = p.slots[i];
            const auto* notes = s.voicing.notes.data();
            const float voiced = voicedTension(s.chord, tonic, scale, notes, s.voicing.n);
            const float loop = measuredContextualTension(p, i);
            const float linear = i ? loop : blendContextualTension(voiced,
                tensileStrain(voicingSpiralCenter(s.chord, tonic, scale, notes, s.voicing.n), progressionSpiralKey(p)), 0);
            std::cout << id << ',' << i+1 << ',' << chordName(s.chord, tonic, scale) << ',' << errors[i] << ','
                      << tensionOf(s.chord, tonic, scale) << ',' << voiced << ',' << linear << ',' << loop << '\n';
        }
    }
}

int main(int argc, char** argv)
{
    std::cout << std::setprecision(8);
    if (argc == 2 && std::string(argv[1]) == "tension") tension();
    else brightness(argc == 2 && std::string(argv[1]) == "edit");
}
