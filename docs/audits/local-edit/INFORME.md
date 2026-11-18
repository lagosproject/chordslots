# Brillantez de ChordSlots · edición de un solo slot

Extensión de la [auditoría de curvas](../INFORME.md), sobre `chordslots-source`, rama `main`, commit `7dc1ef0126d06c10c1616ff86d0acdb31cf30975`. Los CSV originales se conservaron. El auxiliar vive en `audit-curves/local-edit` y reutiliza el [generador auxiliar de encabezados con centro fijo](../fixed-header.mjs). Las bases se comprobaron contra el `../brightness-current.csv`.

## Diseño emparejado

- Do mayor, menor natural y dórico. Semillas 101–120. Mismos ajustes que en la auditoría original, entre ellos cuatro slots equidistantes en cuatro compases 4/4, estilo Free, voicing Piano, Adventure 0,30, Variety 0,50, Tensión objetivo 0,40, Energía objetivo 0,60, Voice Leading 0,50 y ritmo bloqueado. Cada combinación escala/semilla produce una sola base con Roll a Brillantez 0,5.
- Para cada ensayo se copia la base y el estado completo del RNG inmediatamente posterior al Roll. Para ensayar 0,4, se fija a 0,4 el objetivo de partida del slot en la copia, sin volver a generar la base. Así todos los ensayos de cada semilla y escala parten de los mismos acordes y voicings. La referencia regenera con el objetivo de partida del ensayo, de modo que hay controles 0,4→0,4 y 0,5→0,5. Las subidas son 0,4→0,6 y 0,5→0,7.
- Se ejecuta `rerollSlots` con un solo bit y su temperatura predeterminada, 0,004. Se ejecutan por separado slot 2 (interior, siguiente 3) y slot 4 (último, siguiente 1). Se compila una variante con el centro actual (`57 + 14 × objetivo`) y otra con el centro fijo en 64. En la base ambas usan 64. En el motor, la edición solo revocea el slot si cambia su acorde. Se registran las alturas efectivas, incluso si el centro objetivo ha cambiado.
- Se verificaron los 240 slots de las 60 bases en las cuatro combinaciones de centro y posición y en todas las copias, comparando acorde, notas MIDI, velocidad, Tensión y medidas armónicas y de registro. Los acordes y notas iniciales coinciden con el CSV original. Las cuatro serializaciones completas de los estados de RNG también coinciden. Cada fila agregada de la tabla usa 60 ensayos emparejados, 20 por escala. Hay 960 ejecuciones de `rerollSlots` contando ambos centros, posiciones, inicios y sus controles.

## Resultados medidos

`rawH` es el término armónico sin recorte, `0,55 × calidad + 0,25 × color tonal`. Una variación absoluta menor o igual a 0,0000001 se trata como empate. El registro es la media de las notas MIDI ≥48, en semitonos. Δ se calcula restando el estado que indica la comparación. La mediana de magnitudes usa el valor absoluto por ensayo, para mostrar el movimiento aunque los cambios firmados se compensen. ΔT se refiere a la Tensión calculada por el motor (0–1). La última columna da su magnitud mediana en el slot editado y en el siguiente.

| Slot | Centro | Inicio | Comparación | rawH ↑ | rawH = | rawH ↓ | Mediana Δ registro | Mediana magnitud registro | Mediana magnitud ΔT slot / sig. |
|---:|---|---:|---|---:|---:|---:|---:|---:|---:|
| 2 | Actual | 0,4 | Referencia vs base | 6/60 (10,0 %) | 8 | 46 | 0,00 | 2,25 | 0,049 / 0,006 |
| 2 | Actual | 0,4 | Subida vs base | 44/60 (73,3 %) | 11 | 5 | 0,00 | 1,77 | 0,050 / 0,007 |
| 2 | Actual | 0,4 | Subida vs referencia | 59/60 (98,3 %) | 1 | 0 | 0,50 | 2,50 | 0,076 / 0,006 |
| 2 | Actual | 0,5 | Referencia vs base | 26/60 (43,3 %) | 16 | 18 | 0,00 | 1,50 | 0,040 / 0,004 |
| 2 | Actual | 0,5 | Subida vs base | 50/60 (83,3 %) | 7 | 3 | 0,50 | 2,07 | 0,058 / 0,007 |
| 2 | Actual | 0,5 | Subida vs referencia | 41/60 (68,3 %) | 19 | 0 | 0,00 | 1,05 | 0,041 / 0,005 |
| 2 | Fijo | 0,4 | Referencia vs base | 6/60 (10,0 %) | 8 | 46 | 0,00 | 2,25 | 0,051 / 0,006 |
| 2 | Fijo | 0,4 | Subida vs base | 44/60 (73,3 %) | 11 | 5 | 0,00 | 2,02 | 0,053 / 0,007 |
| 2 | Fijo | 0,4 | Subida vs referencia | 59/60 (98,3 %) | 1 | 0 | -0,13 | 2,50 | 0,082 / 0,005 |
| 2 | Fijo | 0,5 | Referencia vs base | 26/60 (43,3 %) | 16 | 18 | 0,00 | 1,50 | 0,040 / 0,004 |
| 2 | Fijo | 0,5 | Subida vs base | 50/60 (83,3 %) | 7 | 3 | 0,00 | 2,48 | 0,058 / 0,007 |
| 2 | Fijo | 0,5 | Subida vs referencia | 41/60 (68,3 %) | 19 | 0 | 0,00 | 1,75 | 0,040 / 0,004 |
| 4 | Actual | 0,4 | Referencia vs base | 3/60 (5,0 %) | 14 | 43 | 0,00 | 1,65 | 0,063 / 0,006 |
| 4 | Actual | 0,4 | Subida vs base | 37/60 (61,7 %) | 17 | 6 | 0,00 | 1,00 | 0,049 / 0,007 |
| 4 | Actual | 0,4 | Subida vs referencia | 52/60 (86,7 %) | 8 | 0 | 0,93 | 2,50 | 0,060 / 0,006 |
| 4 | Actual | 0,5 | Referencia vs base | 22/60 (36,7 %) | 21 | 17 | 0,00 | 1,13 | 0,043 / 0,004 |
| 4 | Actual | 0,5 | Subida vs base | 47/60 (78,3 %) | 11 | 2 | 0,15 | 1,75 | 0,053 / 0,009 |
| 4 | Actual | 0,5 | Subida vs referencia | 43/60 (71,7 %) | 17 | 0 | 0,00 | 1,75 | 0,051 / 0,006 |
| 4 | Fijo | 0,4 | Referencia vs base | 3/60 (5,0 %) | 14 | 43 | 0,00 | 1,65 | 0,063 / 0,006 |
| 4 | Fijo | 0,4 | Subida vs base | 37/60 (61,7 %) | 17 | 6 | 0,00 | 1,27 | 0,053 / 0,007 |
| 4 | Fijo | 0,4 | Subida vs referencia | 52/60 (86,7 %) | 8 | 0 | 0,85 | 2,50 | 0,058 / 0,006 |
| 4 | Fijo | 0,5 | Referencia vs base | 22/60 (36,7 %) | 21 | 17 | 0,00 | 1,13 | 0,043 / 0,004 |
| 4 | Fijo | 0,5 | Subida vs base | 47/60 (78,3 %) | 11 | 2 | 0,00 | 2,50 | 0,057 / 0,009 |
| 4 | Fijo | 0,5 | Subida vs referencia | 43/60 (71,7 %) | 17 | 0 | 0,00 | 2,90 | 0,066 / 0,006 |

### Lectura de la comparación

- **Control armónico local.** Frente a su regeneración emparejada, el objetivo más alto produjo mayor `rawH` en 195/240 ensayos por condición de centro. Los otros 45 empataron. Las dos condiciones eligieron los mismos acordes en los 480 pares de resultados, contando referencias y subidas. El efecto armónico moderado persiste con el centro fijo.
- **El acorde que ya sonaba es otra referencia.** Una regeneración al mismo objetivo cambió el acorde en 186/240 ensayos con el comportamiento actual. Las subidas mejoraron `rawH` frente a la base en 178/240 ensayos, empataron en 46 y bajaron en 16. El control aumenta la probabilidad de encontrar un resultado armónicamente más brillante, con garantías limitadas sobre un acorde ya presente. En 0,5→0,7, Do menor fue especialmente propenso al empate frente a su referencia (suben 9/20 en slot 2 y 8/20 en slot 4). En mayor subieron 20/20 en ambas posiciones. Los desgloses de las tres escalas están en [summary.csv](summary.csv).
- **Registro y Tensión.** Frente a la referencia, la mediana firmada de altura superior va de 0 a +0,93 semitonos con centro móvil y de −0,13 a +0,85 con centro fijo. Las magnitudes medianas llegan a 2,90 semitonos incluso con centro fijo. La Tensión puede moverse aunque su objetivo siga en 0,40. Frente a la base, el mayor cambio absoluto observado fue 0,313 en el editado y 0,030 en el siguiente, considerando controles y subidas de ambos centros. En los 480 ensayos actuales la Tensión del siguiente cambió en 380, incluidos los casos de 4→1. En los 960 ensayos, ningún slot fuera de la máscara cambió de acorde ni de notas, y la Tensión de los slots distintos del editado y su sucesor permaneció igual dentro de la tolerancia indicada.

### Ejemplos reproducibles

| Escala, semilla, slot y subida | Referencia y resultado |
|---|---|
| Do mayor, 101, slot 2, 0,4→0,6 | La base y la referencia conservan Am, `rawH=0,35000`. Con la subida ambos centros eligen Cadd11, `rawH=0,45065`. Altura superior +0,95 semitonos y Tensión −0,05251 en el slot 2 y −0,01299 en el 3 respecto a la base. |
| Do dórico, 107, slot 4, 0,5→0,7 | La base tiene E♭add9, `rawH=0,45844`. Tanto la referencia como la subida eligen E♭, `rawH=0,45000`, en ambos centros. La altura superior sube +2,50 semitonos en ambos ensayos. La Tensión baja 0,12591 en el slot 4 y 0,00225 en el slot 1. La subida empata con su referencia y retrocede frente a la base. |

El contraste emparejado apoya conservar Brillantez como edición armónica local y registral. El gesto actúa como una preferencia sobre la regeneración y puede dar el mismo resultado armónico que regenerar al objetivo de partida. El alcance de esta prueba son curvas de fondo planas, una edición aislada y los ajustes citados. Los cambios en las métricas internas requieren una prueba de escucha para relacionarlos con luminosidad percibida.

## Repetición y archivos

Para repetir los ensayos, sigue las [instrucciones de reproducción](../README.md).

- [run.cmd](run.cmd), [local-edit.cpp](local-edit.cpp), [summarize.mjs](summarize.mjs).
- CSV por slot con antes/después de todos los acordes, notas y medidas. `slots-2-current.csv`, `slots-2-fixed.csv`, `slots-4-current.csv`, `slots-4-fixed.csv`.
- `edits.csv` contiene las diferencias de cada ejecución respecto a su base, incluidas las comprobaciones de otros slots. `paired.csv` contiene la diferencia subida menos regeneración para el mismo inicio, centro, slot, semilla y escala. [summary.csv](summary.csv) desglosa medianas, máximos y recuentos por escala.
- `rng-2-current.txt`, `rng-2-fixed.txt`, `rng-4-current.txt` y `rng-4-fixed.txt`. `original-checksums.json`.
