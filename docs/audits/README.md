# Auditorías de ChordSlots

| Auditoría | Informe |
|---|---|
| Tensión frente a oyentes y barrido de Brillantez | [Informe y tablas](INFORME.md) |
| Brillantez al editar un solo slot | [Informe y tablas](local-edit/INFORME.md) |
| Energía con acordes fijos y paridad MIDI | [Informe y tablas](energy/INFORME.md) |
| Vista previa del menú Mood | [Auxiliar](mood-check/check.cpp) · [Menú en Auto](mood-check/menu-auto.png) · [Menú con estilo manual](mood-check/menu-manual.png) |

Los informes registran el commit que se examinó. Las tablas versionadas conservan los resultados de esa revisión. Regenerarlas con otro commit permite comprobar qué ha cambiado. Usa Git para comparar el resultado nuevo con las tablas publicadas.

## Reproducción en Windows

Desde la raíz del repositorio, con Node.js y el compilador de Visual Studio 2022 para x64. Los scripts usan Community por defecto. Para los auxiliares del motor, también puedes lanzar los comandos desde un Developer Command Prompt x64 de otra edición de Visual Studio.

### Tensión y Brillantez

El barrido interno y la edición local se pueden ejecutar sin los materiales externos.

```powershell
cmd /c docs\audits\run-engine.cmd
cmd /c docs\audits\local-edit\run.cmd
```

Para reconstruir la comparación de Tensión, descarga los materiales desde el repositorio de los autores y fija la revisión usada en el informe. El código de la auditoría utiliza `docs/audits/materials` por defecto. Puedes usar un checkout existente asignando su ruta absoluta a `CHORDSLOTS_AUDIT_MATERIALS`.

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

La comparación solo publica tablas agregadas. Consulta el informe para los detalles de la licencia de los materiales originales.

### Energía y menú Mood

Necesitan un build Release x64 de `chordslots_plugin_tests` generado con CMake y Visual Studio. Los auxiliares reutilizan sus opciones de compilación y sus bibliotecas.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target chordslots_plugin_tests
& ./docs/audits/energy/run.ps1
& ./docs/audits/mood-check/run.ps1
```

Pasa `-BuildDir` a los scripts para reutilizar otra carpeta de build. La captura del menú usa el editor JUCE sin ventana. Para comprobar un plugin cargado en un DAW, sigue la tarea de integración del [plan general](../PLAN-GENERAL.md).
