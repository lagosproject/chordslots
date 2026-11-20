# Punto 1b · Generación y voicing editado

## Prueba controlada

La [sonda](edited-voicing.cpp) usa cuatro compases en 4/4 con cortes equidistantes, Fa mayor, Bb–F–Bb–F, estilo Close, Adventure 0,45, Voice Leading 0,5, Bass Motion 0, bajo Sustain, los objetivos predeterminados y semilla 73. El slot 2 es el editado. Cada copia cambia únicamente una de las dos inversiones vecinas mediante `invertChord`. El bajo de ambos Bb queda en 34 (Bb1). Las voces superiores pasan de 58–62–65 (Bb3–D4–F4) a 62–65–70 (D4–F4–Bb4).

Desde el directorio padre del código, en una consola de desarrollador de Visual Studio:

```bat
cl /nologo /EHsc /std:c++17 /O2 /I chordslots-source\core\include /Fo:chordslots-build\edited-voicing.obj /Fe:chordslots-build\edited-voicing.exe chordslots-source\docs\audits\edited-voicing.cpp
chordslots-build\edited-voicing.exe
```

Para repetir la medición original, compilar la misma sonda contra los encabezados previos al cambio y añadir `/DCHORDSLOTS_BEFORE_EDITED_VOICING`. Las notas MIDI listadas incluyen el bajo en primer lugar.

## Inspect

En la prueba inicial, las tres progresiones mostraron el mismo orden dentro de escala: Dm, Am, C5, Fadd11, Dmadd9, Fsus4. Solo la inversión anterior alteró las notas previsualizadas. Por ejemplo, Dm pasó de 38–57–62–65 a 38–62–65–69. La inversión siguiente dejó 38–57–62–65.

| Variante | Dm antes | Dm después | Fadd11 antes → después | C5 antes → después |
| --- | --- | --- | --- | --- |
| Ambos Bb de referencia | 38 57 62 65 | 38 57 62 65 | 29 57 58 60 65 → igual | 36 60 67 → igual |
| Bb anterior invertido | 38 62 65 69 | 38 62 65 69 | 29 60 65 69 70 → 29 58 60 65 69 | 36 60 67 → igual |
| Bb siguiente invertido | 38 57 62 65 | 38 62 65 69 | 29 57 58 60 65 → 29 58 60 65 69 | 36 60 67 → igual |

Después, con ambos Bb de referencia, el orden es Dm, Am, Fadd11, Dmadd9, C5, Fsus4. Con cualquiera de los Bb invertidos, es Dm, Am, Fadd11, C5, Dmadd9, Fsus4. En el caso de referencia, Fadd11 adelanta a C5 con una suma de distancias de 2,50 frente a 5,33. Con el vecino siguiente invertido, la nueva previsualización de Dm reduce la suma de 4,67 a 4,00.

El algoritmo conserva el coste armónico y añade una penalización de movimiento inferior a 0,02 para que solo las alternativas cercanas puedan intercambiar posiciones. Aplica el orden por separado a cada grupo y conserva la prioridad de escala. Sigue usando el cálculo actual del bajo. Las métricas de la fila, el clic, la pulsación mantenida y Replace reciben el mismo voicing. El cálculo se realiza fuera de `processBlock`. La búsqueda evita construir voicings para candidatos que ya quedan fuera por coste armónico. En la sonda Close, con ambos grupos, el cálculo duró aproximadamente 1,1–1,5 ms en Release.

## Roll

Se inmovilizaron los dos vecinos con el bloqueo individual. Para forzar la repetición se bloquearon raíces y colores. Con esos dos bloqueos libres, la semilla seleccionó Dm. El tercer caso bloqueó el slot editado. La tabla muestra la progresión con ambos Bb de referencia.

| Voicing Lock | Caso | Completo antes → después | Seleccionado antes → después |
| --- | --- | --- | --- |
| Off | F se repite | 41 57 60 65 → igual | 41 60 65 69 → 29 57 60 65 |
| Off | F cambia a Dm | 38 57 62 65 → igual | 38 57 62 65 → igual |
| Off | Slot bloqueado | 41 60 65 69 → igual | 41 60 65 69 → igual |
| On | F se repite | 41 60 65 69 → igual | 41 60 65 69 → igual |
| On | F cambia a Dm | 38 62 65 69 → igual | 38 57 62 65 → 38 62 65 69 |
| On | Slot bloqueado | 41 60 65 69 → igual | 41 60 65 69 → igual |

Equivalencias de esa tabla: 29 F1, 38 D2, 41 F2, 57 A3, 60 C4, 62 D4, 65 F4, 69 A4. La sonda imprime las 36 combinaciones, incluidas las dos variantes de inversión.

Roll completo ya aprovechaba los voicings bloqueados. Con Voicing Lock global desactivado y el mismo Dm seleccionado, cambiar el vecino anterior o el siguiente produjo D4–F4–A4 en lugar de A3–D4–F4, con mejor enlace en esa variante. Por eso se conservó el algoritmo de selección armónica de Roll. Ambos Roll comparten ahora la regla del bloqueo global. El reparto global de Roll completo sigue pudiendo producir un resultado distinto del reparto local con el bloqueo desactivado.

## Verificación en Release

- CTest completó las cinco pruebas de `chordslots-build`: core, performance, trigger, eval y plugin. La prueba de Inspect comprobó el orden por grupo, el MIDI de la escucha aislada y contextual, y el voicing y las métricas resultantes de Replace.
- El VST3 superó pluginval en nivel 5. En un host local que cargó el binario Release, el editor mostró Inspect al abrirlo desde la tarjeta del slot. El grupo externo apareció al activarlo y Replace actualizó el acorde, la lista y las flechas. Los tres cambios fueron visibles en la primera captura posterior a cada acción. La cifra en milisegundos corresponde a la sonda del motor.
