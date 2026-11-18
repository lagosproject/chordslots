# Energía con acordes fijos en ChordSlots

18 de noviembre de 2026. Checkout `chordslots-source`, rama `main` siguiendo `origin/main`, commit `7dc1ef0126d06c10c1616ff86d0acdb31cf30975`. El árbol inicial estaba limpio. El diff de producto modifica tres archivos con textos en inglés.

## Qué describe la ayuda

- **Match.** Para cada slot se calculan `dB = brightness − targetB` y `dT = tension − targetT` a partir de los valores medidos en la progresión actual. Su puntuación es `clamp(1 − 2 × sqrt((dB² + dT²) / 2), 0, 1)`. Se promedian los slots con el mismo peso, con independencia de su duración, y el editor redondea el porcentaje a un entero. Energía carece de un término propio en el cálculo. Puede mover Match indirectamente si cambia las notas que se usan para medir Brillantez o Tensión. [`Generator.h`](../../../core/include/chordslots/Generator.h#L1158) · [`PluginEditor.cpp`](../../../plugin/PluginEditor.cpp#L1180)
- **Brillantez y Tensión.** Al soltar se llama a `rerollSlots` con la máscara de slots editados. Los objetivos orientan la regeneración. Brillantez combina calidad, color relativo a la tonalidad y registro superior. Tensión combina la tensión local del voicing, la distancia de su centro armónico al centro de la tonalidad y el desplazamiento del centro desde el acorde anterior, incluido el enlace del último al primero. [`LanesView.cpp`](../../../plugin/ui/LanesView.cpp#L151) · [`Generator.h`](../../../core/include/chordslots/Generator.h#L953) · [`Metrics.h`](../../../core/include/chordslots/Metrics.h#L265)
- **Energía.** El arrastre escribe `targetE` y marca el mood como personalizado. Al soltar, `applyEnergy` recorre todos los slots desbloqueados con voicing, aunque el arrastre haya tocado uno. Conserva la identidad del acorde y usa su voicing anterior como referencia. Roll entra desde `rollButton.onClick` a `processor.edit` y `cs::roll`, que prepara cortes, selecciona acordes y asigna voicings, con optimización de conducción de voces cuando está habilitada. Con la capa Voicing bloqueada, un acorde intacto en Roll actualiza únicamente su velocidad base. [`LanesView.cpp`](../../../plugin/ui/LanesView.cpp#L105) · [`Generator.h`](../../../core/include/chordslots/Generator.h#L1007) · [`Generator.h`](../../../core/include/chordslots/Generator.h#L1361) · [`Generator.h`](../../../core/include/chordslots/Generator.h#L1702)

## Diseño de la prueba

Piano, Close y Rootless, cada uno con las semillas 101–108. Para cada par estilo-semilla se generó con Roll una progresión de cuatro slots equidistantes en dos compases de 4/4, tónica C, menor para semillas impares y mayor para pares, transiciones Free, ritmo bloqueado y Bass Motion en 0. Los objetivos B son 35/45/55/65 %, T 30/40/50/60 % y E 50 % en todos los slots. Se guardaron la progresión y el RNG tras ese Roll. Cada nivel parte de una copia nueva de ambos y cambia solo E del slot 2 en la edición. Se invocó el mismo punto de entrada del motor que usa el editor al soltar. El gesto y el repintado se siguieron en código.

Se probaron 10, 25, 50, 59, 60, 61, 75, 84, 85, 86, 95 y 100 %. El control 50 % pasa por la misma operación. Después se repitió el barrido con Roll y los bloqueos Roots y Colors activados para congelar el acorde completo. Se comprobaron raíz, tipo, intervalo de bajo simbólico, cortes y objetivos B/T en todos los slots y niveles. Las 24 bases tenían bajo simbólico en posición fundamental. La prueba adicional de inversión toma la semilla 101 de cada estilo, prepara A♭/C en el slot 2 y clona la nueva base y su RNG antes de editar a 10, 50 o 95 %.

## Edición de Energía

Cada celda de densidad es un rango de número de notas entre las ocho semillas del estilo. Las duplicaciones son notas adicionales de una clase de altura ya presente, contando también el bajo.

| Estilo | 10–60 % | 61–85 % | 86–100 % | Adiciones entre 50 y 95 % |
| --- | ---: | ---: | ---: | --- |
| Piano | 5–6 | 6–7 | 7–8 | 8/8 añadieron una tónica del acorde arriba y una octava inferior del bajo |
| Close | 3–6 | 4–7 | 5–8 | 8/8 añadieron la nota alta y la octava inferior |
| Rootless | 2–5 | 2–5 | 3–6 | 7/8 añadieron la octava inferior |

| Energía (%) | 10 | 25 | 50 | 59 | 60 | 61 | 75 | 84 | 85 | 86 | 95 | 100 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Velocidad base MIDI | 53 | 64 | 83 | 89 | 90 | 91 | 101 | 108 | 109 | 110 | 116 | 120 |

En los tres estilos `assembleVoicing` calcula `round(45 + 75 × E)` y limita a 1–127. Por encima de 0.60, Piano y Close pueden añadir la raíz por encima de la voz más alta. El registro disponible, las restricciones de intervalo y el máximo de notas pueden impedirlo. Rootless omite esa regla. Por encima de 0.85, el motor puede añadir el bajo una octava por debajo si la nueva nota queda en MIDI 24 o más y hay capacidad. Rootless-101 conserva cuatro notas aunque sube de velocidad 53 a 116 entre 10 y 95 %, porque su bajo principal MIDI 32 produciría MIDI 20. [`Voicing.h`](../../../core/include/chordslots/Voicing.h#L1119)

En los 288 casos de edición, los vecinos conservaron notas y velocidades respecto de la base. Al pasar de 50 a 95 %, Piano, Close y Rootless conservaron todas las notas anteriores del slot editado. El bajo principal mantuvo su altura en las 24 parejas. Con octava grave, la nota mínima del slot bajó 12 semitonos. A 95 %, el máximo salto absoluto entre bajos principales consecutivos en cada estilo fue de 10 semitonos, contando el cierre del loop. Al seguir la nota más grave de cada slot, el máximo fue de 21 semitonos. En la prueba A♭/C se conservó el intervalo simbólico de 4 y el bajo principal MIDI 36 en los tres estilos. A 95 % apareció también MIDI 24.

La Tensión calculada del slot 3 cambió entre 50 y 95 % en 8/8 Piano, 8/8 Close y 7/8 Rootless, aun con su MIDI intacto. El slot 3 usa el voicing anterior para calcular el término de movimiento. En Piano-101 pasó de 0.519752 a 0.520260. El comentario de `mouseUp` que promete que todo lo demás queda exactamente igual es demasiado amplio. El recálculo contextual explica este caso y el código de Energía tampoco limita el revoicing a la máscara. En la muestra no se reprodujo una alteración de notas vecinas mediante la edición de Energía.

## Roll con acordes fijos

Los acordes, bajos simbólicos, cortes y objetivos B/T se conservaron en los 288 Rolls emparejados. Los voicings vecinos sí cambiaron aunque sus objetivos E siguieran al 50 %. Se cuenta el número de slots vecinos con notas o velocidad distintas respecto al Roll de control, sobre 24 por estilo y nivel.

| Estilo | Notas medias en el slot 2 a 10 / 50 / 95 % | Vecinos distintos a 61 / 95 % | Bajo principal del slot 2 distinto a 95 % |
| --- | --- | --- | ---: |
| Piano | 5.25 / 5.25 / 7 | 11 / 10 | 1 de 8 |
| Close | 4 / 4 / 6 | 13 / 13 | 0 de 8 |
| Rootless | 3.625 / 3.625 / 4.375 | 0 / 2 | 1 de 8 |

El Roll de control también cambió un vecino con Close y otro con Rootless respecto a sus respectivas bases. Comparar niveles contra su Roll de control separa esos cambios de los asociados al cambio de E. Por ejemplo, Piano-101 en Roll cambia el slot 3 de `44 51 60 63 70 75` a `44 51 60 63 75 82` entre 50 y 95 %. En la edición local, ese vecino conserva `44 51 60 63 70 75`. El Roll genera con referencias entre slots y puede optimizar la conducción del ciclo completo. Su resultado no equivale a editar una curva sobre el voicing existente.

## Salida MIDI y preescucha

Se ejecutó el procesador JUCE con transporte simulado de host y con el transporte interno que activa Play. Se capturaron los eventos de `processBlock` y se constató audio de síntesis finito con pico mayor que cero. El sintetizador recibe ese mismo búfer MIDI al final de `processBlock`. Para exportación se ejecutaron `createMidiFile`, escritura SMF y lectura del archivo serializado. Save usa el mismo exportador y el mismo formato SMF según el número de pistas. También se siguió por código el arrastre Master hacia `writeTempMidiFile`. [`PluginProcessor.cpp`](../../../plugin/PluginProcessor.cpp#L1746) · [`PluginEditor.cpp`](../../../plugin/PluginEditor.cpp#L1321)

Para cada modo de motor, estilo y semilla se capturaron 10, 50 y 95 %, con Split apagado y encendido. Condiciones comunes de 120 BPM, 48 kHz, bloques de 256 muestras, gate 80 %, Sustain en bajo y comping, y Strum, Accent y Humanize a cero. Se emparejaron note-on y note-off por canal y nota, se convirtieron las muestras a ticks de 480 PPQ y se compararon altura, canal, velocidad, inicio y duración.

| Comparación con el SMF del mismo modo | Casos | Diferencias de notas/canal o velocidad | Diferencia máxima de inicio y duración |
| --- | ---: | --- | --- |
| Transporte de host simulado | 288 | 0 | 0 ticks en ambos |
| Play interno con el host detenido | 288 | 0 | 0 ticks en ambos |

Con Split apagado, los note-on del slot 2 a 95 % sumaron 58 en Piano, 48 en Close y 36 en Rootless sobre las ocho semillas de edición. Con Split encendido fueron 50, 40 y 29. El bajo de Sustain en Ch 2 es monofónico y emite `primaryBass`. Se pierde la duplicación grave, de forma coherente en vivo, Play y exportación. Todas las notas emitidas del slot 2 en esa condición conservaron velocidad 116. El patrón de bajo, comping y controles de dinámica del plugin pueden transformar por separado el voicing y la velocidad base antes de emitir MIDI. [`Generator.h`](../../../core/include/chordslots/Generator.h#L1814) · [`PluginProcessor.cpp`](../../../plugin/PluginProcessor.cpp#L1782)

Ejemplo reproducible, Piano-101, A♭ mayor en slot 2. El motor da `44 51 60 63 72 75` a 50 %, añade MIDI 80 a 61 % y añade MIDI 32 a 86 %. A 95 %, el MIDI combinado emite las ocho notas en Ch 1. Split emite `51 60 63 72 75 80` en Ch 1 y `44` en Ch 2. En ambos archivos las notas arrancan en el tick 960 y duran 768 ticks (0.8 s), con velocidad 116.

La comprobación ejecutada llega al procesador y al SMF. Queda pendiente cargar el VST3 compilado en un DAW, pasar el cursor por Match y los tres carriles para verificar el tooltip visible, capturar el MIDI en una segunda pista y repetir Save/Drag Master en el host. La prueba de Play comprueba los eventos que recibe el sintetizador, sin atribuirles una energía percibida. Las audiciones del selector de acordes y de patrones quedan fuera de esa comparación del loop.

## Comandos y archivos

Para repetir los ensayos, sigue las [instrucciones de reproducción](../README.md).

- [Captura C++](energy.cpp), [script de ejecución](run.ps1) y [resumen Node.js](summarize.mjs). El script toma flags y bibliotecas del proyecto Release del test de plugin y usa su host simulado.
- `slots.csv` y `slash-bass.csv`. Incluyen MIDI base del motor, métricas, objetivos, límites temporales y bajo simbólico. [Resumen por estilo, modo y nivel](summary.csv) y `slot-comparisons.csv`, con duplicaciones, saltos del bajo principal y saltos de la nota más grave.
- `midi-events.csv`, [paridad por caso](midi-parity.csv) y SMF de ejemplo `piano-101-combined.mid` y `piano-101-split.mid`.
- `build-shared.log`, `build-vst3.log`, `build-plugin-tests.log`, `ctest.log` y `reproducibility.log`.

## Decisión de producto

La ayuda puede describir Energía como control de velocidad y densidad dependiente del voicing. Los tramos planos de densidad observados conservan una respuesta de velocidad. La diferencia de octava de bajo entre Combined y Split responde al carácter monofónico de Split. Si se quiere que Split conserve las dobles octavas, eso cambiaría su contrato y merece una decisión aparte. La muestra no aporta un fallo de calibración reproducible. Las métricas documentan notas y dinámica MIDI. La valoración de la energía percibida requeriría escucha.
