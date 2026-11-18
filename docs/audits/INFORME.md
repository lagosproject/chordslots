# Auditoría de Tensión y Brillantez en ChordSlots

18 de noviembre de 2026. Motor analizado en `B:\Proyectos\Programacion\chordslots\chordslots-source`, rama **main**, commit **`7dc1ef0126d06c10c1616ff86d0acdb31cf30975`**. El checkout estaba limpio al inicio y al cierre. El auxiliar compila directamente los encabezados del motor. Toda la auditoría vive en `audit-curves`, junto al checkout.

## Resumen ejecutivo

- **Tensión.** Se recuperaron diez perfiles suficientemente largos por estímulo. La comparación usa once progresiones y 63 posiciones de acorde con audio y MIDI compatibles. El contextual mejora al estático con voicing en 3 de 11 progresiones con el criterio de estabilidad y en 4 de 11 al promediar la segunda mitad de cada ventana. El Pearson medio por progresión es 0,665 frente a 0,686 con estabilidad y 0,850 frente a 0,851 con la segunda mitad. La conclusión provisional es conservar la curva y sus pesos. Los datos recuperados aportan evidencia de seguimiento de contorno, con una ventaja contextual inconsistente.
- **Brillantez.** En el barrido completo, cada uno de los 480 slots emparejados cambia de acorde entre los objetivos 0,1 y 0,9, también con el centro fijo. La media del componente armónico aumenta en las seis escalas. Con el centro móvil, el registro superior sube entre 3,32 y 5,35 semitonos en promedio según la escala en Roll. En centro fijo, el cambio medio de altura superior aún oscila entre −0,70 y +1,35. Conservar provisionalmente la curva como control combinado de armonía y registro. El barrido mide la respuesta del motor. La luminosidad percibida queda para una fase con oyentes.

## 1. Tensión frente a oyentes

### Materiales abiertos y trazabilidad

Repositorio del estudio en rama `master`, commit `a5ca4a211e4ab34dcc9b7ba4f69cb863d801494f`. Los números P1–P12 siguen el artículo. [Materiales en la revisión auditada](https://github.com/merismeris/tonal-tension-TIS/tree/a5ca4a211e4ab34dcc9b7ba4f69cb863d801494f)

| Material | Contenido y vínculo |
|---|---|
| `ChordProgressionsMIDI/Progression1.mid` … `Progression12.mid` | Se abrieron los doce SMF y se leyeron los ataques y metadatos. Tienen 5–10 posiciones de cuatro notas. P3 y P9 son idénticos byte a byte en MIDI. Los MIDI declaran armadura de Do mayor y ataques cada 3.840 ticks con PPQ 960 y tempo 600.000 µs por negra, equivalentes a 2,4 s. Para alinear las respuestas se usó el reloj del audio del experimento. |
| `Experiment1/Interface/TensionWin/AUDIO/e0.wav` … `e12.wav` y `Tension.pd` | Se abrieron los 13 WAV de Windows y el parche de captura. Cada ensayo se guarda bajo su propio nombre WAV. Se comprobó e1→P1, etc., con espectros de ventanas interiores frente a las alturas MIDI. Para once estímulos la correspondencia es compatible. En e3 los ataques contienen alturas incompatibles con P3, sistemáticamente, mientras que e9 concuerda con el MIDI compartido por P3 y P9. Los WAV e3 y e9 tienen audio distinto. Se excluye P3 de la concordancia y se conserva su perfil humano de e3 y su diagnóstico MIDI por separado mediante `stimulusMatch=0`. e0 queda fuera de P1–P12. |
| `Experiment1/Results/p1.txt` … `p15.txt` | Se parsearon los 15 archivos de respuestas continuas individuales, con listas de posiciones del deslizador bajo etiquetas `AUDIO/eN.wav`. p1–p10 cubren las ventanas de análisis de todos los estímulos. p11–p13 tienen aproximadamente 7–13 muestras por estímulo. p14 y p15 carecen de muestras para e1–e12. El artículo describe 15 oyentes con al menos seis años de formación musical. El análisis local se basa en los diez archivos largos, sin suponer el motivo de los registros incompletos. |
| Perfiles medios y agregados del experimento 1 | La Figura 8 contiene medias con error estándar por acorde y la Tabla 3 contiene correlaciones por progresión. El repositorio inspeccionado aporta respuestas individuales, código y salidas de modelos. La media entregada se reconstruyó de las respuestas recuperadas. No se digitalizaron figuras. |
| `Experiment2/ResulstListeningTest.xlsx` | Se abrió el contenedor y la hoja `Sheet0`. Contiene filas con fecha y elecciones categóricas 1–6, con columnas explícitas P1–P12. Se contaron 91 filas con fecha numérica, 71 con los doce códigos válidos y 74–77 códigos por columna incluyendo filas parciales. Esos filtros sobre el libro difieren de los 73 oyentes que completaron el experimento según el artículo, que informa 51 con formación y 22 sin ella. El libro permite vincular cada elección con una P. La selección analítica exacta de filas del artículo queda pendiente. Las tablas 4–7 publican porcentajes, concordancia entre oyentes y comparaciones de categorías globales. Este libro se usa solo para el inventario. |
| `code/data.m`, `code/main_analysis.m`, `code/final_measures/TIS_select_candidates.m` | Se abrieron como texto. `data.m` contiene dos series de 12 bloques de salida, con indicadores `vs` (TIS), `vl` (Lerdahl) y totales. La segunda serie tiene totales ponderados distintos de las sumas iniciales. Se recupera su vector `vs_total` por orden y longitud de estímulo como referencia archivada. El código fuente muestra que la etapa TIS puede elegir inversiones para conducción de voces. Quedan sin verificar la procedencia exacta de los coeficientes de esa transcripción y su identidad con la versión final de Figura 8, así como la correspondencia de voicings internos del cálculo. |

**Licencias.** El artículo declara CC BY 4.0. No aparece licencia raíz ni concesión específica para MIDI, WAV, TXT, XLSX o código TIS propio en la revisión examinada. El Pure Data incluido trae licencia BSD mejorada y sus componentes Tcl/Tk tienen avisos propios. El lector MATLAB de MIDI contiene avisos de copyright de Ken Schutte. Esas licencias y avisos son de componentes concretos. La reproducción de esta auditoría descarga los originales desde su repositorio. [Artículo con aviso de licencia](https://pmc.ncbi.nlm.nih.gov/articles/PMC7712964/)

### Alineación y puntuación

1. El artículo (§4.2.4) describe min–max por perfil, diez muestras por acorde y media de una región contigua estable con diferencia entre muestras de hasta ±0,005 tras normalizar. En el parche publicado, `readsf~` arranca el WAV y `delay 2060` inicia `metro 200`. Los recuentos observados son coherentes con ese arranque y la duración de cada WAV. Por ello se toma la muestra 0 como ≈2,06 s y las diez primeras muestras como acorde 2. Se ignora la cola desde el final nominal del último acorde. El primer acorde queda sin observación utilizable en todos los perfiles comparados.
2. Se min–max normalizan las muestras que entran en esas ventanas. En cada ventana se toma el tramo contiguo más largo con diferencias ≤0,005, con empate a favor del más tardío y un mínimo de una muestra. El artículo no precisa desempate, tratamiento de tramos de una muestra ni alineación con el retraso del parche. Este resultado es una reconstrucción explícita del método, con una sensibilidad de media de las cinco muestras finales. Cada oyente pesa igual en la media por acorde. [Método publicado, secciones 4.1–4.2](https://www.mdpi.com/1099-4300/22/11/1291)
3. Se conserva cada nota MIDI, su octava, duplicación y orden temporal. Se fija la raíz según lectura armónica de cada acorde, se busca la calidad disponible de menor diferencia simétrica de clases y se desempata por orden del catálogo. Se usa la nota grave como inversión si existe en el tipo elegido. El CSV cuenta las clases discrepantes por slot. Por ejemplo, P7.2 usa símbolo D7/C con la quinta ausente en el estímulo, y P5.2 se aproxima con D7♯9 por distancia en el catálogo. El análisis de rugosidad y el centro espiral reciben las cuatro alturas originales incluso en estos casos. La referencia simbólica y las partes del contextual dependientes del símbolo sí absorben el error de representación.
4. Se usa Do mayor salvo el par MIDI P3/P9, donde se infiere La menor por Am inicial y el tramo Dm–Gm–E–E7. El artículo describe tonalidades de Do mayor o menor y las armaduras MIDI son uniformes, de modo que esa asignación tiene incertidumbre explícita. El CSV de sensibilidad de P9 también prueba La menor armónica, Do mayor y Do menor. En La menor, el estático con voicing supera al contextual (0,839 a 0,774). En Do mayor se invierte el orden (0,320 a 0,494). Se mantiene la altura original en ambas pruebas.
5. La referencia de la rama sin contexto es `tensionOf` (estática simbólica). Se añade `voicedTension` para aislar el valor del contexto dado el mismo voicing. El modelo activo combina **0,86 × estática con voicing + 0,09 × distancia a tonalidad + 0,05 × desplazamiento desde el acorde anterior**. Normaliza y recorta los dos últimos términos con techos 0,33 y 0,36. Sigue los pesos del commit sin recalibración. Para el primer acorde lineal se fija desplazamiento cero y se conserva la tonalidad. `measuredContextualTension` se usa directamente para el resto y en el diagnóstico circular. [Implementación contextual](../../core/include/chordslots/Metrics.h) · [Cálculo por slot](../../core/include/chordslots/Generator.h)

### Resultados medidos

Pearson entre las posiciones disponibles de cada progresión y la media humana reconstruida con la región estable. Mide concordancia de contorno. El tamaño de muestra por correlación es el número de acordes emparejados (4–9), con diez perfiles por media. Las correlaciones son descriptivas y están condicionadas a que cada fila representa pocos acordes y a que la misma cohorte evalúa todas las progresiones.

| P | Acordes MIDI | Perfiles íntegros | Acordes comparados | r estático simbólico | r estático con voicing | r contextual | r TIS archivado | Δ primer acorde en loop |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 6 | 10 | 5 | 0.704 | 0.771 | 0.746 | 0.816 | 0.000 |
| 2 | 10 | 10 | 9 | 0.695 | 0.748 | 0.728 | 0.804 | 0.014 |
| 3 | 6 | 10 | 0 | — | — | — | — | 0.028 |
| 4 | 6 | 10 | 5 | 0.629 | 0.645 | 0.515 | 0.626 | 0.013 |
| 5 | 7 | 10 | 6 | 0.951 | 0.941 | 0.943 | 0.815 | 0.014 |
| 6 | 5 | 10 | 4 | 0.565 | 0.548 | 0.488 | 0.861 | 0.014 |
| 7 | 6 | 10 | 5 | 0.408 | 0.464 | 0.539 | 0.717 | 0.000 |
| 8 | 10 | 10 | 9 | 0.499 | 0.528 | 0.519 | 0.848 | 0.000 |
| 9 | 6 | 10 | 5 | 0.875 | 0.839 | 0.774 | 0.805 | 0.028 |
| 10 | 6 | 10 | 5 | 0.965 | 0.945 | 0.933 | 0.952 | 0.013 |
| 11 | 6 | 10 | 5 | 0.672 | 0.684 | 0.680 | -0.212 | 0.023 |
| 12 | 6 | 10 | 5 | 0.481 | 0.433 | 0.450 | -0.880 | 0.000 |

La columna TIS compara con la salida archivada de la segunda serie de `data.m`. Es una referencia histórica de alcance distinto al contraste entre dos fórmulas de ChordSlots con el mismo voicing. El artículo informa un Pearson global de 0,750 para TIS con el ajuste de aquel estudio. El dato publicado usa otro conjunto de posiciones y perfiles que la reconstrucción local. [Ajuste y comparación del estudio, §4.2.5–4.2.6](https://www.mdpi.com/1099-4300/22/11/1291)

- **Subida y caída final, P5.** Del slot 5 al 6, la media humana sube de 0,606 a 0,725 y el contextual de 0,640 a 0,659. Al resolver G7→C en el slot 7, caen a 0,403 y 0,223. La referencia estática también reproduce ambos sentidos.
- **Pico en el lugar equivocado, P7.** La media sube de 0,446 a 0,777 en D7/C→A7. El contextual cae de 0,537 a 0,495 y el estático con voicing de 0,574 a 0,485. El contextual mejora el Pearson global de 0,464 a 0,539 y conserva la caída final G→C, con el máximo del motor desplazado a G.
- **Ascenso final, P9.** La media sube 0,602→0,818 con E→E7/D. La estática con voicing sube 0,769→0,824, mientras que el contextual cae levemente, 0,759→0,753. Es un ejemplo de cómo el desplazamiento y la distancia a tonalidad pueden reducir la concordancia.
- **Inversión y resolución, P12.** C/E recibe 0,535 y el siguiente C 0,336 en la media humana. El contextual detecta la bajada (0,196→0,190) con un cambio pequeño. Al final, F/C→C cae 0,549→0,171 en los oyentes y 0,475→0,192 en el contextual. El TIS archivado sube en ese cierre. Esa discrepancia merece verificar qué versión produjo la transcripción.
- **Cierre circular por separado.** La última columna es la diferencia en la puntuación del primer acorde al añadir último→primero respecto de la entrada lineal. En estos estímulos llega a +0,028, con máximo en P3/P9 por sus MIDI idénticos y el mismo contexto tonal inferido. Se trata de un diagnóstico del motor sin valoraciones humanas para esa transición.

**Inferencia y pendientes.** El grueso del seguimiento de contorno ya está en la fórmula estática. Los dos métodos de alineación cambian bastante el nivel absoluto de correlación, sobre todo en P4, P6 y P8. La ventaja contextual media sigue cerca de cero o por debajo de la referencia con voicing. Recuperar el MIDI exacto de e3, la extracción de la Figura 8 o el pipeline original de sincronización y la resolución de tonalidad de P9 permitiría reducir la incertidumbre. Haría falta validar también el comportamiento de principio de frase y de cierre circular con oyentes en una fase posterior.

## 2. Brillantez con centro fijo

### Flujo y diseño experimental

- El gesto escribe `Slot::targetB`. Al soltar, `LanesView::mouseUp` llama a `rerollSlots` para la máscara editada. El botón Roll usa `roll`. Ambos llegan a `generateChords`. La edición revocea los slots cuyo acorde ha cambiado vía `setChord`; Roll revocea y puede optimizar la conducción de voces global. [Carriles de la interfaz](../../plugin/ui/LanesView.cpp) · [Operaciones del motor](../../core/include/chordslots/Generator.h)
- El coste por candidato incluye `(cand.brightness - targetB)²`. La brillantez candidata usa registro neutro 0,5. Definimos **H** como ese valor calibrado. Antes de calibrar, el término estrictamente armónico `rawH` es 0,55 × calidad del acorde + 0,25 × color relativo a la tónica, ambos derivados de la línea de quintas. La calibración resta 0,24, divide por 0,50 y recorta a [0,1]. El coste también incorpora tensión y otras condiciones de generación. [Métricas](../../core/include/chordslots/Metrics.h) · [Selección](../../core/include/chordslots/Generator.h)
- El centro objetivo vale `57 + 14 × targetB` (58,4 / 64 / 69,6 MIDI). `makeVoicing` penaliza el alejamiento de la media del registro superior respecto al centro. La optimización circular usa el centro al construir/ordenar candidatos de voicing y puede revertir si cae el ajuste más de 0,02. La condición fija reemplaza exclusivamente esa fórmula por 64 en una copia auxiliar de los encabezados. El criterio de ajuste conserva los objetivos originales. [Selección de voicings](../../core/include/chordslots/Voicing.h) · [Optimización y medida](../../core/include/chordslots/Generator.h)
- El resultado que dibuja el carril de Brillantez es `Slot::brightness`: fórmula calibrada con un 20 % de registro real, calculado como `clamp((media MIDI superior − 48)/36)` sobre notas MIDI ≥48. La auditoría registra esa media y una segunda media sobre todas las notas almacenadas en el voicing, incluido el bajo. Los resultados son anteriores al procesamiento de groove e interpretación y reflejan los números MIDI efectivos del voicing. [Métrica de registro](../../core/include/chordslots/Metrics.h) · [Valor dibujado](../../plugin/ui/LanesView.cpp)

**Controles.** Do en seis escalas. Cada nivel usa las semillas 101–120, en pares por semilla y slot, con cuatro slots equidistantes en cuatro compases 4/4 y ritmo bloqueado. Estilo armónico Free, voicing Piano, Adventure 0,30, Variety 0,50, Tensión objetivo 0,40 y Energía objetivo 0,60. Voice Leading 0,50, Bass Motion 0, bajo Sustain, sin melodía, con repetición de frase desactivada y sin otros bloqueos. El primer barrido parte de una progresión nueva por nivel. El segundo simula la aplicación de una edición a los cuatro slots, partiendo de la misma progresión Roll de objetivo medio por semilla; usa la temperatura predeterminada de `rerollSlots` (0,004). No se simulan coordenadas de ratón. Hay 1.440 filas por operación y condición, 80 observaciones por escala y nivel. Los acordes seleccionados (raíz, tipo y bajo simbólico) coincidieron entre centro fijo y móvil en todas las filas emparejadas del mismo nivel y operación.

### Resultados medidos por escala

B/M/A son los objetivos 0,1/0,5/0,9. «Cambian» compara raíz, calidad e inversión entre B y A. Las flechas cuentan cambios de H para cada intervalo emparejado. «A en máximo» cuenta H igual a 1 entre 80 slots de nivel alto. Cada Δ es alto menos bajo. Se da en semitonos para altura MIDI y en la escala 0–1 para el carril de UI.

### Roll

| Escala | H bajo / medio / alto | Cambian B→A | ↑ / = / ↓ B→M | ↑ / = / ↓ M→A | A en máximo | MIDI superior Δ fijo / actual | MIDI total Δ fijo / actual | UI Δ fijo / actual |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Major | 0.183 / 0.495 / 0.821 | 80 | 79/1/0 | 80/0/0 | 0 | 0.16 / 4.28 | 0.37 / 3.68 | 0.64 / 0.68 |
| Minor | 0.206 / 0.519 / 0.755 | 80 | 80/0/0 | 78/2/0 | 0 | -0.51 / 3.74 | -0.26 / 3.59 | 0.54 / 0.59 |
| Dorian | 0.151 / 0.512 / 0.750 | 80 | 80/0/0 | 78/0/2 | 0 | -0.70 / 3.32 | -0.37 / 3.11 | 0.59 / 0.63 |
| Phrygian | 0.216 / 0.519 / 0.725 | 80 | 80/0/0 | 79/1/0 | 0 | -0.31 / 3.36 | -0.02 / 3.36 | 0.51 / 0.55 |
| Lydian | 0.160 / 0.503 / 0.838 | 80 | 79/1/0 | 80/0/0 | 1 | 1.11 / 4.50 | 0.60 / 3.43 | 0.69 / 0.73 |
| Harmonic Minor | 0.190 / 0.495 / 0.812 | 80 | 80/0/0 | 80/0/0 | 13 | 1.35 / 5.35 | 1.27 / 4.54 | 0.63 / 0.68 |

### Edición

| Escala | H bajo / medio / alto | Cambian B→A | ↑ / = / ↓ B→M | ↑ / = / ↓ M→A | A en máximo | MIDI superior Δ fijo / actual | MIDI total Δ fijo / actual | UI Δ fijo / actual |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Major | 0.189 / 0.464 / 0.837 | 80 | 80/0/0 | 80/0/0 | 0 | -0.57 / 3.12 | -0.36 / 2.77 | 0.64 / 0.68 |
| Minor | 0.185 / 0.531 / 0.749 | 80 | 80/0/0 | 80/0/0 | 0 | 0.04 / 2.42 | 0.10 / 2.21 | 0.56 / 0.59 |
| Dorian | 0.118 / 0.514 / 0.760 | 80 | 80/0/0 | 80/0/0 | 0 | -1.12 / 1.95 | -0.43 / 2.20 | 0.63 / 0.66 |
| Phrygian | 0.188 / 0.534 / 0.719 | 80 | 80/0/0 | 78/2/0 | 0 | 0.49 / 3.07 | 0.54 / 2.77 | 0.54 / 0.56 |
| Lydian | 0.131 / 0.492 / 0.842 | 80 | 80/0/0 | 80/0/0 | 0 | 0.02 / 2.65 | -0.22 / 2.07 | 0.71 / 0.74 |
| Harmonic Minor | 0.187 / 0.510 / 0.829 | 80 | 80/0/0 | 80/0/0 | 19 | 1.83 / 4.01 | 1.31 / 3.14 | 0.66 / 0.68 |

**Margen y saturación.** Todos los pools de candidatos de este ajuste alcanzan H=0 y H=1, pero ese intervalo por sí solo dice poco sobre el resultado alcanzable en una progresión. Lidio tiene el mayor recorrido medio en Roll (H +0,678) y frigio el menor (+0,509), con una media alta de 0,725. Menor armónica presenta el techo local más frecuente (13/80 en Roll y 19/80 en edición). En el extremo bajo de Roll, H=0 aparece en 25/80 en mayor, 5/80 en menor, 22/80 en dórico, 1/80 en frigio, 21/80 en lidio y 15/80 en menor armónica. Son recortes de la métrica para esos acordes. Los datos no establecen una meseta de toda la escala a partir de un objetivo concreto. Para ello quedaría medir niveles más cercanos y el efecto de otros ajustes de Adventure, estilos y curvas no planas.

**Ejemplos reproducibles.**

| Operación y parámetros | Resultado |
|---|---|
| Roll, Do menor armónica, semilla 101, slot 1 | Cm → A♭ → C. H 0,284 → 0,575 → 0,756. Entre bajo y alto, el centro fijo deja +0,50 semitonos en las voces superiores y +0,40 contando el bajo. En operación actual se obtienen +4,25 y +3,40, respectivamente. |
| Roll, Do dórico, semilla 108, slot 3, medio → alto | E♭6 → B♭add9, ΔH −0,0052. El rango armónico sin recorte también baja (−0,0026). |
| Roll, Do dórico, semilla 115, slot 2, medio → alto | E♭add9 → E♭, ΔH −0,0169 y Δ del término armónico sin recorte −0,0084. |

**Inferencia.** El generador utiliza sustancialmente la palanca armónica en estos ajustes. Las dos excepciones de dórico muestran que el óptimo de secuencia tiene margen para sacrificar el ajuste local de Brillantez aun cuando sube el objetivo. El mayor recorrido de la medida del carril con centro móvil coincide con una subida de alturas reales. Tomar una diferencia entre ambas condiciones como efecto del centro es válido para esta intervención emparejada. No equivale a una descomposición perceptiva del acorde.

## Reproducción

Para repetir los ensayos, sigue las [instrucciones de reproducción](README.md).

- Auxiliar C++ [engine.cpp](engine.cpp), compilación [run-engine.cmd](run-engine.cmd) y variante de [centro fijo](fixed-header.mjs).
- Tensión. [Lector de MIDI y respuestas](stimuli.mjs), [diagnóstico espectral del audio](check-audio.mjs), `source-inventory.txt`, `audio-check.txt`, `exp2-inventory.txt` y su [script](inspect-exp2.ps1). El diagnóstico espectral ordena magnitudes en frecuencias MIDI 32–83 en siete ventanas interiores por acorde; cada línea da notas MIDI esperadas, seis picos de la ventana central elegida y la peor posición de una nota esperada en cada ventana.
- Tensión. `stimuli.csv`, `tension-coverage.csv`, `tension-profiles.csv`, [comparaciones y sensibilidad temporal](tension-comparison.csv) y [sensibilidad a la tonalidad de P9](tension-key-sensitivity.csv).
- Brillantez. Datos por slot de `brightness-current.csv`, `brightness-fixed.csv`, `brightness-edit-current.csv` y `brightness-edit-fixed.csv`. Incluyen todas las notas MIDI, el centro nominal, la altura superior y total, H, `rawH` y el valor medido para UI.
- Brillantez. `brightness-pairs.csv`, `brightness-edit-pairs.csv`, [resumen Roll](brightness-summary.csv) y [resumen de edición](brightness-edit-summary.csv). [analyze.mjs](analyze.mjs) genera los resúmenes, compara los modelos de tensión y escribe las `tables.md`.

## Recomendación provisional

- **Tensión.** Mantener el nombre, el modelo y los pesos durante esta fase. Para defender el peso del contexto o cambiarlo, buscar una mejora reproducible frente a `voicedTension` en progresiones independientes, que mantenga el sentido de las subidas y resoluciones al variar la alineación y que haga predicciones verificables para último→primero. El pequeño diferencial observado no justifica recalibrar sobre estas once progresiones.
- **Brillantez.** Mantener provisionalmente la curva combinada. Una prueba de escucha con registro igualado y otra con voicings registralmente desplazados sobre los mismos acordes podrían cambiar la recomendación. Si los oyentes distinguen de manera estable uno de los dos efectos y atribuyen el otro a altura o color armónico, habría base para revisar el nombre o exponer dos controles. Si ambos contribuyen al juicio en los rangos usados, el control combinado gana respaldo perceptivo.
