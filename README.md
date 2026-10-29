# chordslots (nombre provisional)

Generador de progresiones de acordes por huecos con duración libre,
candados y variaciones A/B. Proyecto open source en desarrollo.

Estado: **hito M1** (modelo de datos, escalas y resolución de acordes).
Aún no hay plugin; solo el motor y sus pruebas.

## Estructura

```
engine/include/chordslots/   cabeceras públicas
engine/src/                  implementación
engine/tests/                pruebas con Catch2
.github/workflows/ci.yml     compila y prueba en Windows, Linux y macOS
```

## Compilar y probar

Requisitos: un compilador C++17 y CMake 3.20 o superior.
Catch2 se usa del sistema si está instalado y, si no, se descarga solo.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --build-config Release --output-on-failure
```

## Reglas musicales fijadas en M1

- Un tick = 1/480 de negra. Las duraciones son múltiplos de un beat (480).
- La fundamental de cada acorde cae en MIDI 48..59 antes de inversiones y octava.
- La inversión n sube una octava las n notas más graves.
- Los nombres de nota usan sostenidos (sin bemoles todavía).
- Grado, inversión y octava fuera de rango se limitan al rango válido en
  `resolveChord`; `validate` los señala para que la interfaz pueda avisar.

## Licencia

Pendiente de decidir (propuesta: motor MIT, plugin AGPLv3).
