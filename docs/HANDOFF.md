# ChordSlots — Handoff (2026-11-16)

This document lets another agent (Codex, Claude, a human) continue `IMPLEMENTATION_PLAN.md`
without the previous session. Read this first, then `IMPLEMENTATION_PLAN.md`, then the
prompt for the phase you are about to run in `docs/phase-prompts/`.

## 1. Where things stand

| Phase | Status | Notes |
|---|---|---|
| P0 Stabilize + eval harness | ✅ merged | `chordslots-cli eval`, baseline in `tests/eval_baseline.json`, ctest `eval` guard |
| P1 Correctness fixes | ✅ merged | prior scoring (backoff instead of PMI), style cluster by tonality + working Style menu, `bVII` weight, bass always present, targeted rolls via `slotCost`, AA′ as tied variables, live comping with export parity, bounded event list, tempo re-publish on the message thread |
| P2 Chord model | ✅ merged | properties derived from interval sets (golden fixture `tests/p2_golden_fixture.h`), 11 new chord types, no dropped chord tones |
| P3 Voicing engine | ✅ merged | `core/include/chordslots/Voicing.h`: 7 styles (Close, Piano default, Spread, Drop2, Drop3, Rootless, Stacked), omission rules, low-interval rule, gap rule, semitone avoidance, assignment voice-leading distance, per-variation VOICING menu |
| P4 Real corpus prior | ⏳ ready | prompt `docs/phase-prompts/P4.md`; data must be downloaded first (section 5) |
| P5 Exact inference | ⏳ ready | `docs/phase-prompts/P5.md` |
| P6 Contextual tension | ⏳ ready | `docs/phase-prompts/P6.md`; **needs the user's blind listening test** |
| P7 Producer features | ⏳ ready | `docs/phase-prompts/P7.md` |
| P8 Formats & CI | ⏳ ready | `docs/phase-prompts/P8.md`; **needs a Mac + Logic for the AU MIDI FX check**; GitHub CI is outward-facing, so confirm with the user before enabling it |

(If the table says P3 is merged but `git log` disagrees, trust `git log`.)

## 2. How the work was done (keep doing it this way)

- **Executor / reviewer split.** One agent implements a phase from a prompt; another
  reviews by re-running every check itself. Never accept a claim that was not run.
- **One branch per phase** (`phase/pN` from `main`), one commit per task, fast-forward
  merge into `main` only after review. Back up before each phase.
- **Every behaviour change needs a test that fails without it.**
- **The evaluation harness is the referee.** `./build/chordslots-cli eval 1000 --compare tests/eval_baseline.json`
  (~5–6 min) before changing the baseline. Guards are one-directional (see `tools/cli.cpp`
  `kGuardThresholds`). A failed guard is allowed only with a measured explanation (bisect
  across the phase's commits with a small probe if needed: that is how P1's Fit drop was
  traced to the prior, not to the `bVII` change the executor blamed). Then regenerate:
  `eval 1000 --out tests/eval_baseline.json --bigrams tests/eval_bigrams_baseline.json`.
- **Refactors must be byte-identical:** build the CLI at `main` and at the refactor commit
  (`git archive <rev> core tools | tar -x -C /tmp/x`), run `eval 200 --out`, and compare sha256.
- **Listen with a probe.** Metrics do not catch bad voicings. Print voicings for
  `Dm7 G7 Cmaj7 Am7`, `Am F C G`, `Gmadd9 G13 C7#9` in every style
  (`chordslots-cli voicings`) and read them like a musician.

## 3. Review checklist (what went wrong before, check every phase)

1. **Audio thread:** no allocation, no locks beyond the existing try-lock, every
   fixed-size array write bounds-checked (P1.7 shipped an unchecked `events[512]`).
2. **Host playhead only inside `processBlock`.** P1.9 called `getPlayHead()` from
   `publish()` / `createMidiFile()` (message thread = undefined behaviour). Tempo now flows
   via `lastBpm` → `tempoDirty` → 10 Hz timer → `publish()`.
3. **No circular metrics.** Velocity is set from the Energy target, so measured energy
   must stay `0.6·velocity + 0.4·density`. An executor tried 0.8/0.2 to pass a test.
4. **No silently loosened tests.** Any loosened tolerance is a deviation and needs a reason.
5. **Infinite loops / runtime.** P3.4 shipped `while (candTop <= x) top += 12;` (a hang).
   The core suite must stay under 180 s (`ctest -R core`, Release; it was 158 s after P3.7, so there is little headroom: sample loops rather than adding exhaustive ones). If a test binary runs
   longer than 15 minutes, it is a bug: attach `gdb` (start the binary under gdb, then
   interrupt it and run `bt`).
6. **Roll time** must stay in the tens of milliseconds (it runs on the message thread):
   time 40 rolls × 4/8/16 slots before and after any generator change.
7. **"Unavoidable" claims:** verify them musically (P3's "unavoidable" rootless m9 semitone
   was avoidable).
8. **Reports contain factual slips** (wrong constants, "beam search" for Gibbs). Check the code.

## 4. Build & environment notes

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4`, then
  `ctest --test-dir build` (5 suites: core, performance, trigger, eval, plugin).
- Quick core loop without CMake: `g++ -std=c++17 -O1 -Icore/include tests/core_tests.cpp -o /tmp/ct && /tmp/ct`.
- UI snapshot without a host: `./build/chordslots_plugin_tests --snapshot ui.png`.
- Linux without root: `./scripts/fetch-linux-headers.sh`.
- The original machine is slow (full JUCE build 10+ min). Don't change global compile
  flags (that forces a full rebuild).
- pluginval was not run locally (blocked there). P8 puts it in CI.

## 5. Corpus data for P4 (not in git)

Download into a folder **outside** the repo and expose it through the git-ignored path
`third_party/chord-data` (e.g. a symlink). Verify the checksums first.

| File | URL | Checksum |
|---|---|---|
| `raw/choco-v1.0.0.zip` (467 MB) | https://zenodo.org/api/records/7706751/files/smashub/choco-v1.0.0.zip/content | md5 `c26f23805e0f6beef7a0624df6be1ed0`, sha256 `cfd3531f4bc23afcede3e092a6fe50e31ee4280d7c53db33fd17ec7444a99c01` |
| `raw/rs200_harmony_clt.zip` | http://rockcorpus.midside.com/harmonic_analyses/rs200_harmony_clt.zip | sha256 `796582dcb5e2a945cb1dcf5307dc193ccb5a2e574866a4c7d6958888fa2dd30a` |
| `raw/rs200_harmony.zip` | http://rockcorpus.midside.com/harmonic_analyses/rs200_harmony.zip | sha256 `ad715fbd132380a3f6ecc686a87008d5e6d6ae8d05cf23ea09094229eabe3bab` |

Layout expected by the P4 prompt:
```
third_party/chord-data/
  raw/{choco-v1.0.0.zip, rs200_harmony_clt.zip, rs200_harmony.zip}
  choco/   ← unzip -q raw/choco-v1.0.0.zip 'smashub-choco-f7dd3ee/partitions/billboard/*' 'smashub-choco-f7dd3ee/partitions/rock-corpus/*'
  rs200/   ← unzip raw/rs200_harmony_clt.zip and raw/rs200_harmony.zip here
  SOURCES.txt
```
**Licences (decided):** use only Billboard (listed CC0 on ChoCo's metadata page and
CC BY 4.0 in ChoCo's LICENSE.md, so treat it as CC BY 4.0 with attribution) and RS200
(CC BY 4.0, stated on rockcorpus.midside.com). **Exclude** the non-commercial ChoCo
partitions (JAAH, Chordify, Mozart: CC BY-NC-SA), Band-in-a-box ("not for commercial
use"), and everything with unspecified, ODbL or SA terms. The McGill Billboard pages
returned 404 on 2026-11-16, so the licence is known only through ChoCo. If that changes,
re-check it.

## 6. Decisions already made (don't relitigate without new evidence)

- Rewriting from scratch: no. The core/plugin split and the infrastructure are solid.
- New progressions default to the Piano voicing; old projects load with their saved notes.
- Dark-mood Fit dropped about 0.04 in P3 because the mud ban rejects low clusters.
  Accepted: clean voicings beat a lower brightness fit.
- Hard/Uplifting variety dropped in P1 because the minor prior is now correct. Accepted;
  the Variety knob in P5 addresses it.
- Energy error is higher for Close/Rootless/Stacked (~0.10) than for Piano (0.048).
  Accepted for now; per-style density normalisation is in P7.5.
- Not doing: more moods, AI/LLM suggestions, a bigger built-in synth.

## 7. Known small issues (backlog, folded into P7.5 unless noted)

- `voiceLeadingDistance` DP table is sized for ≤ 8 voices; add a guard/clamp.
- GCC 15 false-positive `-Warray-bounds` around `std::sort` in `violatesLowIntervalLimit`.
- Truncated labels in the footer at the default size (`ADVE…`, `Rhy…`); knob readouts lack units.
- Trap-stab comment in `PluginProcessor.h` says hits are ≥ 12 steps apart (they can be 6).
- `rollSlotBass` cycles inversions without context (intended for now).
- After P3.7 (semitone avoidance), **Stacked** often loses its octave-doubled top note
  and looks like the other styles (e.g. Dm7 `D2 C4 D4 F4 A4`). Restore the "wide EDM
  stack" character (top doubled an octave up when Energy > 0.5) without reintroducing
  upper semitones. P7.5.
- Rootless triads are thin (Am → `A2 C4 E4`). That is expected (no root, no 7th), but
  consider adding the 9th for triads in Rootless. P7.5.
