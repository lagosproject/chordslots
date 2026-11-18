// Task 3 probe: how much does each preset component contribute to making moods differ?
// Components of a mood: scale, adventure, syncopation, style (default corpus style), B/T/E target curves.
#include "chordslots/Generator.h"
#include <cstdio>
#include <cmath>
#include <array>
#include <set>
using namespace cs;
constexpr int NTOK = 12 * 5;
using Hist = std::array<double, NTOK>;
struct Cfg { int scale, adv, sync, style, b, t, e; };
static Cfg same(int m) { return { m, m, m, m, m, m, m }; }
static int token(Chord c, int tonic) { return mod12(c.root - tonic) * 5 + (int) qualityClassFromIntervals(chordTypes()[(size_t) c.type].iv); }
struct Out { Hist h {}; double n = 0, bAct = 0, tAct = 0, eAct = 0, tonic = 0, runs = 0, minorQ = 0, outside = 0; };
static void gen(const Cfg& c, int seed, Out& o)
{
    const auto& M = moods();
    Progression p; p.tonic = 0; layoutEven(p);
    p.scale = M[(size_t) c.scale].scale; p.adventure = M[(size_t) c.adv].adventure; p.syncopation = M[(size_t) c.sync].syncopation;
    p.mood = c.style; p.style = -1;
    for (int i = 0; i < p.numSlots; ++i) {
        const float x = ((float) i + 0.5f) / (float) p.numSlots; auto& s = p.slots[(size_t) i];
        s.targetB = sampleCurve(M[(size_t) c.b].b, x); s.targetT = sampleCurve(M[(size_t) c.t].t, x); s.targetE = sampleCurve(M[(size_t) c.e].e, x);
    }
    Rng rng(seed); roll(p, rng);
    bool tn = false;
    for (int i = 0; i < p.numSlots; ++i) {
        const auto& s = p.slots[(size_t) i]; o.h[(size_t) token(s.chord, p.tonic)] += 1; o.n += 1;
        o.bAct += s.brightness; o.tAct += s.tension; o.eAct += s.energy;
        if (mod12(s.chord.root - p.tonic) == 0) tn = true;
        if (qualityClassFromIntervals(chordTypes()[(size_t) s.chord.type].iv) == QualityClass::Minor) o.minorQ += 1;
        o.outside += tonesOutsideScale(s.chord, p.tonic, p.scale, false);
    }
    o.tonic += tn; o.runs += 1;
}
static Out run(const Cfg& c, int seeds, int parity = -1) { Out o; for (int s = 1; s <= seeds; ++s) if (parity < 0 || (s & 1) == parity) gen(c, s, o); return o; }
static double jsd(const Hist& a, const Hist& b)
{
    double sa = 0, sb = 0; for (int i = 0; i < NTOK; ++i) { sa += a[(size_t) i]; sb += b[(size_t) i]; }
    double r = 0; for (int i = 0; i < NTOK; ++i) { double p = a[(size_t) i] / sa, q = b[(size_t) i] / sb, m = 0.5 * (p + q);
        if (p > 0) r += 0.5 * p * std::log2(p / m); if (q > 0) r += 0.5 * q * std::log2(q / m); } return r;
}
int main(int argc, char** argv)
{
    const int seeds = atoi(argv[1]); const int nm = (int) moods().size(); const int mode = argc > 2 ? atoi(argv[2]) : -1; // -1 = baseline tables, k = ablation pair k
    std::vector<Out> base(nm); std::vector<bool> have(nm, false);
    auto B = [&](int m) -> Out& { if (! have[(size_t) m]) { base[(size_t) m] = run(same(m), seeds); have[(size_t) m] = true; } return base[(size_t) m]; };
    if (mode < 0) { for (int m = 0; m < nm; ++m) B(m);
    for (int m = 0; m < nm; ++m)
        printf("STAT,%s,%s,B=%.3f,T=%.3f,E=%.3f,tonic=%.2f,minorFrac=%.2f,outside=%.3f,noise=%.4f\n", moods()[(size_t) m].name, scales()[(size_t) moods()[(size_t) m].scale].name,
            base[(size_t) m].bAct / base[(size_t) m].n, base[(size_t) m].tAct / base[(size_t) m].n, base[(size_t) m].eAct / base[(size_t) m].n,
            base[(size_t) m].tonic / base[(size_t) m].runs, base[(size_t) m].minorQ / base[(size_t) m].n, base[(size_t) m].outside / base[(size_t) m].n,
            jsd(run(same(m), seeds, 0).h, run(same(m), seeds, 1).h));
    for (int a = 0; a < nm; ++a) for (int b = a + 1; b < nm; ++b)
        printf("PAIR,%s,%s,jsd=%.4f\n", moods()[(size_t) a].name, moods()[(size_t) b].name, jsd(base[(size_t) a].h, base[(size_t) b].h)); return 0; }
    // component ablation for chosen pairs: start at X, swap one component to Y
    const std::vector<std::pair<const char*, const char*>> pairs = { {"Dark","Tense"},{"Tense","Dark"},{"Epic","Euphoric"},{"Euphoric","Epic"},{"Epic","Uplifting"},{"Uplifting","Epic"},{"Euphoric","Uplifting"},{"Uplifting","Euphoric"},{"Melancholic","Dark"},{"Hard","Epic"} };
    auto idx = [&](const char* n) { for (int i = 0; i < nm; ++i) if (std::string(moods()[(size_t) i].name) == n) return i; return -1; };
    const char* names[] = { "scale", "adventure", "syncopation", "style", "B", "T", "E", "curvesBTE", "scale+adv+sync" };
    for (int pi = 0; pi < (int) pairs.size(); ++pi) { if (pi != mode) continue; auto& pr = pairs[(size_t) pi];
        int X = idx(pr.first), Y = idx(pr.second); const double gap = jsd(B(X).h, B(Y).h);
        for (int k = 0; k < 9; ++k) {
            Cfg c = same(X);
            switch (k) { case 0: c.scale = Y; break; case 1: c.adv = Y; break; case 2: c.sync = Y; break; case 3: c.style = Y; break; case 4: c.b = Y; break; case 5: c.t = Y; break; case 6: c.e = Y; break;
                case 7: c.b = c.t = c.e = Y; break; case 8: c.scale = c.adv = c.sync = Y; break; }
            Out o = run(c, seeds);
            printf("ABL,%s->%s,%s,gap=%.4f,jsdToY=%.4f,closed=%.2f,B=%.3f,T=%.3f,E=%.3f\n", pr.first, pr.second, names[k], gap, jsd(o.h, B(Y).h),
                1.0 - jsd(o.h, B(Y).h) / gap, o.bAct / o.n, o.tAct / o.n, o.eAct / o.n);
        }
    }
}
