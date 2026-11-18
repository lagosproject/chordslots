# ChordSlots audits

| Audit | Report |
|---|---|
| Tension against listeners and Brightness sweep | [Report and tables](REPORT.md) |
| Brightness when editing a single slot | [Report and tables](local-edit/REPORT.md) |
| Energy with fixed chords and MIDI parity | [Report and tables](energy/REPORT.md) |
| Mood menu preview | [Helper](mood-check/check.cpp) · [Menu in Auto](mood-check/menu-auto.png) · [Menu with manual style](mood-check/menu-manual.png) |

The reports record the commit that was examined. The versioned tables keep the results of that review. Regenerating them with another commit lets you check what has changed. Use Git to compare the new output with the published tables.

## Reproduction on Windows

From the repository root, with Node.js and the Visual Studio 2022 compiler for x64. The scripts use Community by default. For the engine helpers, you can also launch the commands from an x64 Developer Command Prompt of another Visual Studio edition.

### Tension and Brightness

The internal sweep and the local edit can be run without the external materials.

```powershell
cmd /c docs\audits\run-engine.cmd
cmd /c docs\audits\local-edit\run.cmd
```

To rebuild the Tension comparison, download the materials from the authors' repository and pin the revision used in the report. The audit code uses `docs/audits/materials` by default. You can use an existing checkout by assigning its absolute path to `CHORDSLOTS_AUDIT_MATERIALS`.

```powershell
git clone https://github.com/merismeris/tonal-tension-TIS.git docs/audits/materials
git -C docs/audits/materials checkout --detach a5ca4a211e4ab34dcc9b7ba4f69cb863d801494f
Push-Location docs/audits
node stimuli.mjs --inspect | Set-Content source-inventory.txt
node check-audio.mjs | Set-Content audio-check.txt
& .\inspect-exp2.ps1 | Set-Content exp2-inventory.txt
node analyze.mjs | Set-Content tables.md -Encoding utf8
Pop-Location
```

The comparison only publishes aggregated tables. See the report for the license details of the original materials.

### Energy and the Mood menu

They need a Release x64 build of `chordslots_plugin_tests` generated with CMake and Visual Studio. The helpers reuse its compile options and its libraries.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target chordslots_plugin_tests
& ./docs/audits/energy/run.ps1
& ./docs/audits/mood-check/run.ps1
```

Pass `-BuildDir` to the scripts to reuse another build folder. The menu capture uses the JUCE editor without a window. To check a plugin loaded in a DAW, follow the integration task in the [general plan](../GENERAL-PLAN.md).
