#include "chordslots/Generator.h"
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace cs;

void writeSlot(const Slot& s, const Progression& p)
{
    const auto c = s.chord;
    const float rawH = 0.55f * qualityBrightness(c.type) + 0.25f * modalBrightness(c, p.tonic, p.scale);
    int sum = 0, count = 0;
    for (int i = 0; i < s.voicing.n; ++i)
        if (s.voicing.notes[i] >= kUpperFloor)
        {
            sum += s.voicing.notes[i];
            ++count;
        }
    if (count == 0) throw std::runtime_error("No upper voices");
    std::cout << c.root << ',' << c.type << ',' << c.bassInt << ',' << chordName(c, p.tonic, p.scale)
              << ',' << rawH << ',' << double(sum) / count << ',' << s.tension << ',' << s.voicing.velocity << ',';
    for (int i = 0; i < s.voicing.n; ++i)
        std::cout << (i ? " " : "") << s.voicing.notes[i];
}

int main(int argc, char** argv)
{
    if (argc != 2 || (std::string(argv[1]) != "2" && std::string(argv[1]) != "4")) return 2;
    const int edited = std::stoi(argv[1]) - 1;
    std::cout << std::setprecision(9)
              << "scale,seed,edited,start,target,slot,centre,rootBefore,typeBefore,bassBefore,chordBefore,rawBefore,upperBefore,tensionBefore,velocityBefore,notesBefore,rootAfter,typeAfter,bassAfter,chordAfter,rawAfter,upperAfter,tensionAfter,velocityAfter,notesAfter\n";

    for (int scale : {0, 1, 2})
        for (int seed = 101; seed <= 120; ++seed)
        {
            Progression base;
            base.tonic = 0;
            base.scale = scale;
            base.style = (int) CorpusStyle::Free;
            base.locks.rhythm = true;
            layoutEven(base);
            Rng baseRng(seed);
            roll(base, baseRng);
            const auto savedRng = baseRng;

            // The two starts have the same sounding base from the 0.5 Roll.
            // Their only pre-edit difference is the target metadata of the selected slot.
            for (float start : {0.4f, 0.5f})
            {
                Progression before = base;
                before.slots[edited].targetB = start;
                for (float target : {start, start == 0.4f ? 0.6f : 0.7f})
                {
                    Progression after = before;
                    Rng editRng = savedRng;
                    after.slots[edited].targetB = target;
                    rerollSlots(after, 1u << edited, editRng);

                    for (int i = 0; i < base.numSlots; ++i)
                    {
                        std::cout << scales()[scale].name << ',' << seed << ',' << edited+1 << ','
                                  << start << ',' << target << ',' << i+1 << ',' << centreForBrightness(target) << ',';
                        writeSlot(before.slots[i], before);
                        std::cout << ',';
                        writeSlot(after.slots[i], after);
                        std::cout << '\n';
                    }
                }
            }
            // Emit a full serialization for cross-build and cross-position equality checks.
            std::cerr << scales()[scale].name << ',' << seed << ',' << savedRng << '\n';
        }
}
