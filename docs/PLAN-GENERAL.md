# Plan general de ChordSlots

Actualizado el 18 de noviembre de 2026. Este documento reúne el estado de la revisión del VST, el orden de trabajo y las decisiones pendientes. Los informes enlazados conservan los métodos y resultados.

La [base de investigación](BASE-DE-INVESTIGACION.md) recoge la evidencia y las referencias que guiaron las auditorías y decisiones de diseño.

Este plan incorpora una [revisión crítica del motor musical](#revisión-crítica-del-motor-musical) del 18 de noviembre de 2026. Sus hallazgos y tareas (R1–R9) están pendientes de priorizar y se contrastan con literatura, no solo con el criterio de quien desarrolla el VST.

## Objetivo

Conseguir que cada control del VST tenga un efecto comprensible y predecible al editar un loop. Usaremos las pruebas del motor y del MIDI para medir qué hace el producto y la escucha externa para decidir qué efectos se perciben y cuáles merece la pena controlar desde la interfaz.

El alcance de este plan es Harmony, sus controles de interpretación asociados, los presets y la verificación del VST en un DAW.

## Estado actual

| Área | Hecho | Pendiente |
|---|---|---|
| **Tensión** | Comparación con respuestas de oyentes publicadas. Se usaron 11 progresiones y 63 posiciones. El modelo contextual no mostró una ventaja consistente frente a la referencia estática con voicing. | Probar qué aporta el contexto al **generar y editar** acordes. El cierre circular se ha medido como diagnóstico sobre secuencias existentes. [Informe](audits/INFORME.md) |
| **Brillantez** | Barrido por escalas y edición de un slot. La puntuación armónica aumentó en 195 de 240 subidas frente a una regeneración equivalente, también con centro fijo. El registro real y la tensión pueden variar al cambiar de acorde. | Probar **color armónico frente a registro** con oyentes. Ampliar la comprobación de edición a configuraciones más cercanas al uso habitual cuando toque verificar el VST. [Informe](audits/local-edit/INFORME.md) |
| **Energía** | Auditoría con acordes fijos en Piano, Close y Rootless. Se comprobó la respuesta de velocidad y densidad. El MIDI del procesador con transporte simulado y el Play interno coincidieron con el SMF en 288 casos por ruta. | Confirmar la integración en un DAW. La muestra actual no aporta un motivo para recalibrar la barra. [Informe](audits/energy/INFORME.md) |
| **Match y ayuda** | Diff de textos revisado. Match compara Brillantez y Tensión. La ayuda de los carriles describe el efecto de cada control. | Ver los tooltips en una ventana real. [README](../README.md) |
| **Moods** | Vista previa del preset validada en el editor JUCE y en render. Se verificaron Auto, Free manual y Custom. | Avisar de que elegir un preset también ejecuta un Roll. Comprobar el menú en un DAW y medir si los presets generan resultados distintos. [Comprobación](audits/mood-check/check.cpp) |

La revisión registró un build Release del VST3 y cinco pruebas de CTest aprobadas. La comprobación interactiva en un host sigue pendiente.

## Orden de trabajo

Trabajar una tarea principal cada vez. Si una tarea depende de acceso externo, se puede seguir por la siguiente que esté lista.

| Orden | Tarea | Se da por hecha cuando… |
|---|---|---|
| **1** | **Cerrar el cambio de Mood.** Añadir a la cabecera que aplicar el preset reemplaza las curvas y genera una progresión. Revisar el texto y el ancho del menú en el render. | La advertencia se lee antes de elegir y las comprobaciones de Auto, manual y Custom siguen pasando. La apertura dentro del DAW queda registrada en la tarea 4. |
| **2** | **Comparar Tensión contextual y estática como controles del generador.** Usar las mismas bases, semillas y objetivos para Roll y edición de un slot. Incluir curvas ascendentes, descendentes y con resolución, y examinar aparte el salto del último al primero. | Hay un informe reproducible sobre seguimiento del objetivo, localización de picos y caídas, y cambios colaterales. Ambos generadores se comparan con referencias de puntuación comunes. El informe indica si conviene conservar el contexto, investigar un fallo concreto o recabar más evidencia. |
| **3** | **Auditar la diferenciación de los moods.** Empezar por Dark / Tense y Epic / Euphoric / Uplifting. Igualar primero interpretación y después, en pruebas separadas, escala y estilo efectivo o curvas objetivo. | Sabemos qué aporta cada grupo de ajustes, qué presets se solapan en las condiciones probadas y qué diferencias dependen de la percepción del productor. |
| **4** | **Verificar el VST3 en un DAW.** Abrir la versión compilada, revisar tooltips y menú, editar slots con curvas variadas, comprobar bloqueos y Undo, y capturar MIDI en otra pista para compararlo con Save y Drag Master en condiciones equivalentes. | Se registra el host, la versión, los casos probados y las diferencias reproducibles. Se puede ejecutar en cuanto haya acceso a un DAW. |
| **5** | **Realizar la escucha de Brillantez.** Preparar pares de cambio armónico con registro igualado y de cambio de registro con los mismos acordes. Recoger por separado percepción y preferencia de uso. | Hay respuestas registradas por persona y ejemplo, y una decisión provisional sobre Brillantez combinada, Color armónico o controles separados. [Protocolo y ficha de sesión](PLAN-DE-CONTINUIDAD.md#protocolo-de-escucha-con-productores) |

### Criterio para la próxima auditoría de Tensión

Separar el **modelo que elige los acordes** del **modelo que puntúa el resultado**. De lo contrario, cada variante podría salir favorecida por su propia métrica. Evaluar ambas salidas con el mismo cálculo estático con voicing y el mismo cálculo contextual, e informar de ambos resultados. Mantener los pesos actuales durante la comparación. Tratar la discrepancia de cierre circular como diagnóstico del motor, ya que las respuestas publicadas no cubren esa vuelta del loop.

## Revisión crítica del motor musical

Una lectura de `Theory.h`, `Metrics.h`, `SpiralTension.h`, `Generator.h` y `Corpus.h` encontró puntos donde la salida numérica del motor puede diferir de lo que oye un músico. Las cifras de "Evidencia" salen de una sonda reproducible sobre `tensionOf` y `chordMetrics` (estimación por símbolo de acorde, sin voicing ni contexto) y de `buildCandidates`. No sustituyen a una auditoría con el generador completo. Las fuentes están resumidas en la [base de investigación](BASE-DE-INVESTIGACION.md#contraste-añadido-por-la-revisión-crítica).

### Hallazgos

| Id | Hallazgo | Evidencia | Contraste con la literatura |
|---|---|---|---|
| **H1** | **La Tensión no modela la resolución del tritono ni la atracción entre notas.** Con acordes funcionales la jerarquía es en general razonable (18 de 19 aserciones convencionales pasan en R1b). El fallo se concentra donde el tritono debería dominar. La función tonal es una tabla por grado que ignora la calidad. | En Do mayor, Gmaj7 (0.76) queda por encima de G7 (0.74), que es la única aserción convencional que falla. Cmaj7 y Cm puntúan igual (0.41). ♭VII cuenta como dominante. La [auditoría de Tensión](audits/INFORME.md) apunta en la misma dirección en dos progresiones, con muy pocas posiciones: en P7 la media humana sube de 0.446 a 0.777 en D7/C→A7 y el motor baja, y en P9 sube de 0.602 a 0.818 en E→E7/D y el contextual cae levemente. No bastan para probar nada por sí solas. | Lerdahl y Krumhansl (2007) combinan distancia en el espacio tonal, tensión de superficie (disonancia) y **atracción melódica**. Aquí solo hay los dos primeros, de forma aproximada. Bigand, Parncutt y Lerdahl (1996) ya separaban disonancia, función y movimiento. |
| **H2** | **Solapamiento parcial en la disonancia.** La aspereza (Sethares) y la norma del vector TIV se suman a partes iguales y ambas dependen del contenido interválico. La medición de R3 las muestra correlacionadas pero no redundantes, así que mantener ambas es razonable. | `typeDissonance` = 0.5·aspereza + 0.5·disonancia armónica. En los 32 tipos, Pearson 0.75 y Spearman 0.63. Dim7 es el mayor desacuerdo (puesto 29 en aspereza y 5 en TIV). | Harrison y Pearce (2020) separan aspereza, armonicidad y familiaridad como factores distintos. Habría que comprobar si aquí aportan información independiente. |
| **H3** | **Los pesos y calibraciones no están ajustados a datos.** Brillantez usa 0.55/0.25/0.20 y Tensión 0.45/0.35/0.20. Las constantes de `Calibration` se fijaron "inspeccionando la tabla del CLI". | Cinco tipos saturan en 0 o 1 (R4, sobre 46 080 combinaciones): dim (85 %) y m7b5 (68 %) en 0, aug (85 %) y maj7#11 (15 %) en 1, y quartal (5 %) en 0. Además, tipos sin relación empatan a tres decimales (sus4, madd9, m13, 7#9 y 9sus4 en 0.419). El [barrido de Brillantez](audits/INFORME.md) ya observa el recorte: H=0 en 25 de 80 casos en mayor, 22 en dórico y 21 en lidio en el extremo bajo de Roll, y lo describe como "recortes de la métrica". | Eerola, Friberg y Bresin (2013) tratan la expresión como suma de señales con pesos empíricos. Farbood (2012) estima con datos de oyentes cuánto aporta cada parámetro a la tensión. Temperley y Tan (2013) contradicen un orden estrictamente creciente en la línea de quintas (jónico sobre lidio). |
| **H4** | **Brillantez mezcla tres constructos y es circular con el registro.** Mezcla calidad del acorde, color en la tonalidad y registro. El objetivo de Brillantez también fija el centro del voicing (`57 + 14·targetB`), con lo que el registro cuenta dos veces. | `centreForBrightness` en `Voicing.h` y `registerBrightness` en `Metrics.h`. La [auditoría](audits/INFORME.md) cuantifica el efecto: con el centro móvil, el registro superior sube entre 3.32 y 5.35 semitonos de media entre los objetivos 0.1 y 0.9 en Roll, y con el centro fijo cambia entre −0.70 y +1.35. | Smit et al. (2020) encuentran efectos de modo y de altura media sobre la valencia de cadencias. Es el motivo de la tarea 5, que debe ajustar también los pesos, no solo decidir el control. |
| **H5** | **El grupo de candidatos y el prior se contradicen en menor.** Con Adventure bajo, el V mayor no entra en la escala Minor: el grupo admite 0 notas fuera de escala por debajo de 0.2 y la escala Minor es el eólico natural. El prior, en cambio, puntúa V→i. En el clúster menor, V y V7 suman cerca del 6.5 % de las transiciones, resuelven a la tónica el 65 % y el 85 % de las veces, y aparecen al menos una vez en 94 de las 173 canciones (54 %). | En La menor con Adventure 0.1: 66 candidatos, con Em y sin E ni E7. A 0.3 aparecen ambos. Recuento de R5 sobre 17 589 transiciones: V 3.67 %, V7 2.84 %, v 4.27 %, ♭VII 13.71 %. Recuento por canción de R5b: V en 58, V7 en 58, cualquiera de los dos en 94, ♭VII en 114 y v en 64. La mediana de V o V7 es el 10 % de los acordes de esas canciones. | de Clercq y Temperley (2011), base del corpus RS200, tratan la armonía del rock como práctica propia y no como extensión de la clásica. Que falte V puede ser coherente con el estilo PopRock y no serlo con jazz, clásico o un mood como Epic. Vuvan y Hughes (2019) muestran que la expectativa de V–I frente a ♭VII–I depende del estilo. **Pendiente:** el recuento de R5 es por transiciones, no por canciones. Falta saber cuántas canciones en menor usan V o V7. |
| **H6** | **El prior de transiciones es grueso y de un género.** Agrupa los acordes en 5 clases, así que Imaj7→IV y I→IV comparten estadística. El clúster "SoulRnb" se define por "≥30 % de acordes extendidos". Los datos son pop y rock de 1958–1991 y el producto apunta a productores (808, Hard, Dreamy). Minor tiene 173 anotaciones. La medición de R6 confirma que la agrupación oculta diferencias reales (ver resultados). | `QualityClass` (5 valores) y `tools/build_corpus.py`. | Eitel et al. (2024) modelan secuencias con PPM sobre símbolos más ricos. de Clercq y Temperley (2011) acotan el corpus al rock. Chordonomicon, ya listado en la base, cubre más géneros. Revisar procedencia y calidad antes de usarlo. |
| **H7** | **El contextual está activo por defecto sin evidencia.** `kUseContextualTension = true` se usa en 13 puntos de `Generator.h`: curvas de Epic y Hard, costes de selección y alternativas, `slot.tension`, puntuación de ajuste y refresco al dividir o quitar slots. Un bloque de `core_tests.cpp` se omite si la constante es falsa. El comentario del código pide cambiarla solo tras la escucha ciega, y la auditoría no mostró ventaja. | `Metrics.h` y `Generator.h`, definición de `moods()`. | Es la pregunta de la tarea 2. La constante y las curvas deberían decidirse juntas. |
| **H8** | **Convención implícita en los predicados de intervalos, sin fallos hoy.** Las extensiones superiores a 12 se ignoran (`hasMajorThird` busca 4, no 16). Con el vocabulario actual no hay discrepancias en 32 tipos. Solo maj7#11 y quartal cambiarían de clasificación si se redujera módulo 12. Quartal se etiqueta como sus por tener cuarta y no tercera. Faltan dominantes alterados (7#5, 7b13, 7#11). | `Theory.h`. Comprobado con un verificador independiente. Conviene una prueba antes de añadir tipos nuevos. |
| **H9** | **El generador casi nunca produce una dominante en tonalidades menores, ni siquiera cuando es diatónica.** El prior del corpus pesa entre 0.005 y 0.016 por transición, mientras que alejarse de los objetivos de Brillantez y Tensión cuesta del orden de 0.2. Un V mayor es "brillante" por su calidad, de modo que choca con los moods menores y oscuros. El usuario solo obtiene V o V7 si sube a la vez Brillantez y Tensión del último slot y Adventure ≥ 0.3. | Sonda sobre 300 semillas y 4 slots en Do menor. Con los objetivos por defecto, V/V7/V9 son el 0 % de los acordes en todas las Adventure de 0.1 a 0.7. Mood Tense (armónico menor, V diatónico): 0.1 %. Uplifting, Epic, Melancholic, Dark y Hard: 0 %. El corpus tiene V o V7 en el 54 % de las canciones menores y en cerca del 6.5 % de las transiciones. Con el último slot en Brillantez 0.7 y Tensión 0.8, aparece en el 13 % (Adventure 0.3) y el 28 % (0.6), y domina ♭VII. G tiene coste unario 0.204 frente a 0.009 del mejor acorde. | Es una cuestión de ingeniería, sin fuente. |

### Tareas propuestas

No alteran el orden de las tareas 1–5. R2 y R4 amplían las tareas 2 y 5 con los mismos informes y protocolos. El orden sugerido es R1, R3, R5, R2/R4, R6, R7 y R8. R10 depende de listeners y se agrupa con la tarea 5.

| Id | Tarea | Se da por hecha cuando… |
|---|---|---|
| **R1** | **Pruebas de conformidad teórica.** Convertir hechos de un manual de armonía estándar en aserciones: cifrados romanos, jerarquía de tensión (una dominante con séptima frente a la misma raíz con maj7), nombres de acordes por tonalidad, calidades. Una persona con formación teórica elige las aserciones. | Hay una lista revisada de aserciones, con las que fallan registradas y sin exigir arreglarlas todavía. |
| **R2** | **Función y atracción en Tensión.** Probar un término de tritono o de tono sensible y una función que dependa de la calidad, dentro de la comparación de la tarea 2 y sin retocar los demás pesos. | La ablación se informa con los datos de Navarro-Cáceres y con el conjunto de Barratt-Fitzgerald et al. como segundo contraste externo. |
| **R3** | **Colinealidad de H2.** Calcular la correlación entre aspereza y disonancia TIV en los 32 tipos y 12 raíces, y comparar la tensión con uno solo de los términos. | Hay un informe que decide mantener ambos, fusionarlos o quitar uno, con la correlación como justificación. |
| **R4** | **Pesos y saturación de Brillantez.** Ajustar los pesos con las respuestas por pares de la tarea 5 en lugar de fijarlos a mano. Revisar la saturación de Caug, Cdim y Cm7b5. | Los pesos tienen un intervalo de incertidumbre y no hay tipos de acorde saturados en los extremos sin motivo. |
| **R5** | **Dominante en menor (H5).** Contar V y V7 en las canciones en menor del corpus. Decidir si el grupo de candidatos incluye las notas de la menor armónica para acordes de función dominante, o si depende del estilo. | La decisión queda documentada con ese recuento, y la prueba de candidatos del CLI la refleja. |
| **R6** | **Prior de transiciones (H6).** Medir si separar maj/maj7 y min/min7 cambia el prior. Evaluar Chordonomicon para géneros ausentes (solo con permiso de los autores). Valorar POP909 (MIT, 909 canciones con calidades, orden y tonalidad, 401 en menor) como tercera fuente para el prior en menor; antes hay que cerrar los derechos de las canciones de origen. Renombrar los clústeres si no son estilos. | Se informa el efecto de cada cambio y la atribución sigue en `DATA_LICENSES.md`. |
| **R7** | **Panel ciego de jueces.** Varias personas, incluida al menos una con formación teórica, valoran a ciegas progresiones de ChordSlots, progresiones aleatorias y progresiones reales del corpus. | Hay una tabla con acuerdo entre jueces y una conclusión sobre qué condiciones se distinguen. |
| **R8** | **Valor por defecto del contextual (H7).** Fijar `kUseContextualTension` y las curvas de Epic y Hard según el resultado de la tarea 2. | La constante y las curvas se cambian juntas en un solo diff con el informe enlazado. |
| **R9** | **Informes de auditoría dentro del repositorio.** Hecha en `e39b0ca`: los informes, scripts y tablas están en [docs/audits](audits/README.md). Las cifras citadas en este plan (11 progresiones, 63 posiciones, 195 de 240, 288 casos) se comprobaron en los informes. Los informes examinaron el commit `7dc1ef0`. El núcleo (`core/`) no ha cambiado desde entonces. | Hecha. Los scripts de Windows (`.cmd`, `.ps1`) no se han probado en Linux. |
| **R10** | **Prueba de pares para el término de tritono (necesita oyentes).** Acordes aislados y cadencias cortas con el mismo instrumento, tempo y registro: G7 frente a Gmaj7, dim frente a m7b5, V7 frente a IV, V7 frente a V. Preguntas: cuál suena más tenso, cuál pide resolver, y sin diferencia o no sé. Se agrupa con la sesión de la tarea 5 para reclutar una sola vez, y admite oyentes que no son productores. | Hay respuestas por persona y par, y un peso de `kTritoneWeight` elegido a partir de ellas (probablemente entre 0.04 y 0.06) o la decisión de dejarlo en 0. Los estímulos y la hoja de respuestas se pueden preparar antes de reclutar. |
| **R11** | **Dominante en menor (H9).** Probar dos opciones detrás de constantes con valor por defecto que no cambie nada: (a) Brillantez que tenga en cuenta el contexto para acordes de función dominante en tonalidades menores; (b) un término de cadencia que favorezca la dominante antes de la vuelta del loop. Medir la proporción de V/V7 generada frente al corpus y la sensibilidad a los objetivos. | Hay una tabla con la proporción de V-familia por mood y Adventure frente al 6.5 % del corpus, y el efecto en las aserciones de R1 y R1b y en `eval`. Decisión documentada sobre si el producto debe ofrecer cadencias por defecto o solo bajo petición. |

### Resultados preliminares de R1, R3, R4 y R5

Cuatro análisis de solo lectura se ejecutaron el 18 de noviembre de 2026 con scripts en el espacio de trabajo local. Ninguno modificó el repositorio. Los números de R3 y R5 se recalcularon de forma independiente a partir de sus ficheros de salida. Los de R4 coinciden con la sonda inicial.

| Id | Resultado | Qué falta |
|---|---|---|
| **R1** | 44 aserciones sobre ortografía, cifrado romano con inversiones, intervalos de los tipos de acorde y función tonal: 44 pasan y 3 discutibles también. Es evidencia débil: las expectativas se escribieron con el código a la vista y ninguna cubre Tensión ni Brillantez, que es donde están los hallazgos. | Revisión por una persona con formación teórica. Añadir aserciones de jerarquía de tensión que hoy fallarían (H1). Mover vi como tónica y vii° como dominante a discutibles. |
| **R3** | Aspereza y disonancia TIV están correlacionadas, no son colineales (ver H2). | Decidir con escucha si compensa ajustar el reparto 50/50. |
| **R4** | Saturación limitada a cinco tipos de acorde, más empates entre tipos distintos (ver H3). | Ajustar `brightLo` y `brightHi` o los pesos en R4 completa, con las respuestas de la tarea 5. |
| **R5** | Los recuentos del clúster menor están en H5. Son transiciones con repeticiones colapsadas, sin ponderar por duración, con la tónica inferida por heurística. Los archivos brutos están en `chordslots-data` y permiten repetir el recuento por canción. | Decidir la política del grupo de candidatos en menor y repetir el recuento a nivel de canción. |

### Resultados preliminares de R1b, R5b, R6 y de la auditoría de H7 y H8

| Id | Resultado | Qué falta |
|---|---|---|
| **R1b** | 19 aserciones convencionales de tensión y brillo: 18 pasan y falla T1 (G7 por debajo de Gmaj7). De tres discutibles, fallan dos: Caug en el techo de Brillantez y Cdim igual a Cm7b5 en el suelo. La ordenación de V7 frente a IV, de Bdim frente a Am y de E7 frente a Am en La menor es correcta. | Revisión por una persona con formación teórica. La comparación ♭VII frente a V fue elección del agente y no cuenta como evidencia. |
| **R5b** | Reproduce el corpus (173 canciones menores, 139 de Billboard y 34 de RS200, sumas de verificación coincidentes). V o V7 aparecen en 94 canciones. Ver H5. | Decidir la política del grupo de candidatos. |
| **R6** | Sobre 1 090 canciones y 99 389 transiciones, el 34.6 % de las transiciones implica un acorde de séptima o extendido (el 39.6 % en Billboard y el 12.1 % en RS200). Al separar triada y familia de séptima, 9 de las 10 fuentes más frecuentes superan 0.1 bits de divergencia Jensen-Shannon (entre 0.08 y 0.33) frente a un suelo de ruido de 0.003 a 0.04. Las más altas son V mayor (0.325), ♭III mayor (0.275) y ♭VII mayor (0.235). | El suelo de ruido supone muestreo independiente entre transiciones. Las canciones se agrupan por estilo, así que el ruido real es mayor. Repetir con remuestreo por canción antes de decidir. **Resuelto después con una prueba de predicción en canciones no vistas y POP909 como tercera fuente (ver "Avance de R6"): separar no mejora entre corpus, así que se mantiene el prior de 5 clases.** |
| **H7** | Lista de los 13 puntos y de los efectos en pruebas (ver H7). La lectura es estática: no se invirtió la constante ni se ejecutó `eval`, así que se desconoce cuánto se mueven las líneas base. | Medir el efecto en `eval` de Epic y Hard antes de R8. |
| **H8** | Sin discrepancias en 32 tipos (ver H8). | Prueba que falle si se añade un tipo cuya clasificación depende de la convención. |

### Avance de R2: término de tritono

La rama `r2-tritone-term` añade `kTritoneWeight` en `Metrics.h`, con valor 0, y `hasChordTritone`, que marca los acordes con un tritono entre sus notas propias (séptimas de dominante, dim, m7b5, dim7; no cuenta el ♯11 de maj7#11). Con peso 0 los valores no cambian: las cuatro pruebas de CTest del núcleo, incluida `eval`, pasan.

Barrido sobre una copia de los encabezados con el motor de la auditoría, reproducido en Linux con las mismas tablas publicadas (11 progresiones, Pearson medio con la región estable):

| Peso | G7 frente a Gmaj7 | Estático con voicing | Contextual |
|---|---|---|---|
| 0.00 | 0.743 frente a 0.760 | 0.686 | 0.665 |
| 0.04 | 0.783 frente a 0.760 | 0.696 | 0.676 |
| 0.06 | 0.803 frente a 0.760 | 0.699 | 0.681 |
| 0.10 | 0.843 frente a 0.760 | 0.704 | 0.687 |

El Pearson sube de forma monótona en los tres modelos, pero el aumento es de unos 0.02 con solo 11 progresiones y una cohorte. Los datos no permiten elegir el peso, y el aumento sin tope hasta 0.10 sugiere que se ajusta a estos estímulos. Por eso el valor por defecto sigue en 0. **Pendiente:** fijar el peso con un criterio razonado (separar G7 de Gmaj7 por una diferencia comparable a la de disonancia entre ambos, de 0.04 a 0.06) y validarlo con datos independientes antes de activarlo. El conjunto de Barratt-Fitzgerald et al. se examinó y no sirve para esto (ver la [base de investigación](BASE-DE-INVESTIGACION.md)): sus estímulos manipulan la preparación de la disonancia y las modulaciones, y no hay MIDI. Quedan como opciones una prueba de pares con oyentes sobre acordes aislados y en cadencias (G7 frente a Gmaj7, dim frente a m7b5) y la revisión de una persona con formación teórica. El cambio también mueve Bdim, Cdim y los moods, así que habrá que volver a ejecutar `eval`.

### Avance de R11: contexto de Brillantez para la dominante en menor

La rama `r2-tritone-term` añade `kMinorDominantContext` en `Metrics.h`, con valor 0, y `contextQualityBrightness`, que acerca el término de calidad de un acorde con tercera mayor sobre el grado 7 de una escala con tercera menor al de una tríada menor. Se usa en la selección de candidatos y en la Brillantez dibujada. Con 0 los valores no cambian y las cuatro pruebas de CTest del núcleo pasan. Con 1.0 cambian 84 de 3 840 valores de Brillantez, todos en raíz 7 con tercera mayor, y ninguno en Mayor, Lidio o Mixolidio. La Brillantez de V baja de 0.80 a 0.44 y la de V7 de 0.62 a 0.39.

Proporción de V, V7 y V9 entre los acordes generados (300 semillas, 4 slots, Do, escala propia de cada mood):

| Valor | Tense (armónico menor) | Epic | Uplifting | Melancholic | Dark | Hard |
|---|---|---|---|---|---|---|
| 0.00 | 0.1 % | 0.0 % | 0.0 % | 0.0 % | 0.0 % | 0.0 % |
| 0.50 | 1.7 % | 0.2 % | 0.3 % | 0.0 % | 0.0 % | 0.0 % |
| 0.75 | 5.2 % | 0.4 % | 0.1 % | 0.0 % | 0.0 % | 0.0 % |
| 1.00 | 9.2 % | 0.9 % | 0.0 % | 0.1 % | 0.0 % | 0.0 % |

Con los objetivos por defecto no hay ningún cambio a ningún valor. La referencia del corpus es cerca del 6.5 % de las transiciones. Tense con 0.75 llega a 5.2 % sin mover ♭VII ni la tónica más de un punto. **El cambio ayuda solo a Tense.** En las escalas con tercera menor natural (Epic, Uplifting, Melancholic, Hard) hay un segundo obstáculo: el tono sensible de V cuenta como nota fuera de escala y recibe la penalización de Adventure. En Epic, el último slot da a G un coste de 0.054 frente a 0.005 del mejor acorde, y el término de notas fuera de escala aporta unos 0.034. Las comparaciones acorde a acorde con la misma semilla no sirven para medir localidad: el muestreo es conjunto y un coste distinto cambia toda la secuencia.

**Opción (c): tono sensible como nota de la escala.** `kMinorLeadingToneInScale` en `Theory.h`, desactivada, hace que la tercera mayor de un acorde V-familia no cuente como nota fuera de escala en las escalas con tercera menor. Con ella activada, V y V7 entran en el grupo de candidatos con Adventure 0.1 y 0.15 (72 candidatos en lugar de 66), lo que resuelve la incoherencia original de H5. Una prueba comprueba que no cambia ningún otro acorde ni las escalas mayores.

Proporción de V, V7 y V9 con el tono sensible activado (300 semillas, escala propia de cada mood; ref. del corpus: cerca del 6.5 %):

| Contexto de Brillantez | Tense | Epic | Uplifting | Melancholic | Dark | Hard |
|---|---|---|---|---|---|---|
| 0.00 | 0.1 % | 0.8 % | 0.8 % | 0.0 % | 0.0 % | 0.2 % |
| 0.50 | 1.7 % | 3.2 % | 6.9 % | 0.0 % | 0.0 % | 6.2 % |
| 0.75 | 5.2 % | 5.0 % | 4.0 % | 0.0 % | 0.0 % | 16.4 % |
| 1.00 | 9.2 % | 9.2 % | 0.5 % | 0.8 % | 0.0 % | 18.7 % |

La proporción de tónica se mantiene a menos de un punto de la base en Tense, Epic y Uplifting en todos los valores salvo uno. Ningún ajuste sirve a todos los moods: **0.5** acerca Uplifting, Epic y Hard al corpus y deja Tense bajo; **0.75** pone Tense, Epic y Uplifting en la banda, pero lleva Hard al 16 %. Melancholic y Dark no cambian. Con los objetivos por defecto no hay efecto, y con el tono sensible activado y el contexto en 1.0, el último slot con objetivos altos produce V en más del 90 % de los casos, que es excesivo. **Estado:** las dos constantes siguen desactivadas. Activarlas mueve las líneas base de `eval` y los resultados de varios moods, y el 6.5 % es una referencia global del corpus, no un objetivo por mood. Antes de activar (tono sensible activado y contexto cerca de 0.5) conviene que una persona con formación teórica escuche progresiones con y sin el cambio, dentro de la sesión de R10.

### Contraste de R11 con un segundo corpus: POP909

Para saber si el 6.5 % de V/V7 en menor es una rareza de Billboard y RS200, se midió lo mismo en **POP909** (909 canciones de pop con acordes y tonalidad, 401 en menor; licencia MIT en el repositorio). Todas las cifras son del mismo tipo: acordes con repeticiones consecutivas colapsadas, grado respecto a la tónica de la canción, clases de calidad del prior (mayor incluye maj7 y maj6; menor incluye min7; dom7 es 7; sus y aug quedan aparte). La tonalidad de cada canción es la de mayor duración. Se repitió con la primera tonalidad (407 canciones en menor) y las cifras cambian en menos de 0.3 puntos. Las cifras de POP909 las calculó primero un subagente y después se recalcularon con un script propio, con diferencias de uno o dos puntos que vienen de cómo se elige la tonalidad. Billboard y RS200 usan el mismo código que `tools/build_corpus.py`.

| Canciones en menor | Billboard (139) | RS200 (34) | Billboard + RS200 (173) | POP909 (401) |
|---|---|---|---|---|
| V mayor o V7, % de acordes | 6.0 | 8.1 | **6.5** | **5.2** |
| v menor, % de acordes | 4.9 | 2.9 | 4.4 | 9.1 |
| ♭VII mayor, % de acordes | 13.2 | 14.9 | 13.6 | 15.5 |
| Entre los acordes del grado 7: mayor o dominante / menor | 55 / 45 % | 74 / 26 % | 60 / 40 % | 36 / 64 % |
| Canciones con V mayor o V7 al menos una vez | 53 % | 59 % | 54 % | 77 % |
| Canciones con ♭VII | 65 % | 71 % | 66 % | 95 % |

Referencia en mayor (V o V7): 16.2 % en Billboard y RS200, 19.4 % en POP909.

**Qué se concluye.** (1) El 6.5 % no es una rareza de un corpus: POP909 da 5.2 %, de modo que la referencia razonable en menor es de **5 a 6.5 %** de los acordes. (2) V sigue siendo minoritario en menor y ♭VII es el acorde del grado alto más frecuente en ambos (13 a 16 %). Eso coincide con la descripción del rock en menor de de Clercq y Temperley (2011) y con la idea de Nobile (2016, citado en una revisión automática; no se leyó la fuente) de un menor "basado en VI" donde ♭VII y IV asumen función dominante, y es lo que se esperaría de una escala menor natural, no clásica. (3) En POP909 el grado 7 es menor (v) en casi dos de cada tres casos, así que la mejora no consiste en poner más V mayor. (4) En el contraste, **0.75 con el tono sensible activado es demasiado**: Hard llega al 16.4 % de V-familia, más del doble de cualquier corpus. Con 0.5, Uplifting (6.9 %), Epic (3.2 %) y Hard (6.2 %) quedan en la banda o por debajo. Esto confirma la recomendación de 0.5 si se activa, sin cambiar que las constantes siguen desactivadas.

**Límites.** POP909 son arreglos de piano de canciones pop chinas, no de los géneros de productores. Sus tonalidades salen del audio y pueden confundir relativas mayor y menor (148 canciones cambian de tonalidad). La definición de "canción en menor" de Billboard y RS200 es la de `build_corpus.py` y la de POP909 es la tonalidad anotada, así que las comparaciones de un punto no son concluyentes. **Pendiente:** medir la proporción de v menor y de ♭VII en el generador por mood, que no se midió; sin eso no se sabe si el generador acierta en el grado 7 menor ni en ♭VII, solo que le falta V mayor. Los scripts quedaron en el scratchpad de la sesión y no están en el repositorio.

### Medición del generador frente al corpus en menor (V mayor, v menor, ♭VII)

Pendiente del contraste anterior. Se generaron 600 semillas por mood (4 slots, tónica Do, escala propia de cada mood, repeticiones colapsadas) y se clasificó cada acorde por sus intervalos (con tercera mayor y séptima menor: dominante; con tercera mayor sin ella: mayor; con tercera menor: menor). Se compararon la base (constantes por defecto) y las dos variantes con el tono sensible activado. Referencia del corpus en menor: V mayor o V7 5.2 a 6.5 %, v menor 4.4 a 9.1 %, ♭VII mayor 13.6 a 15.5 %, tónica cerca del 26 %, y todo el grado 7 entre el 11 % (Billboard y RS200) y el 14 % (POP909).

Moods con escala de tercera menor, base:

| Mood | Escala | V mayor/dom | v menor | ♭VII mayor | Tónica | Grado 7 total |
|---|---|---|---|---|---|---|
| Uplifting | Menor | 0.2 % | 0.0 % | 22.3 % | 26.2 % | 0.2 % |
| Epic | Menor | 0.0 % | 17.8 % | 20.9 % | 25.2 % | 17.8 % |
| Melancholic | Menor | 0.0 % | 17.7 % | 0.5 % | 26.9 % | 17.7 % |
| Hard | Menor | 0.0 % | 23.6 % | 5.1 % | 25.2 % | 23.6 % |
| Dark | Frigio | 0.0 % | 5.2 % | 0.0 % | 25.7 % | 5.2 % |
| Tense | Armónico menor | 8.5 % | 6.8 % | 0.0 % | 13.2 % | 15.3 % |

Con el tono sensible activado (V mayor/dom, v menor): contexto 0.5, Uplifting 6.4 y 0.0, Epic 3.8 y 16.0, Hard 7.7 y 22.7, Tense 11.1 y 6.4; contexto 0.75, Uplifting 3.9 y 0.0, Epic 6.4 y 13.8, Hard 17.3 y 22.0, Tense 18.0 y 5.0. Los demás moods casi no cambian. Euphoric (Mayor) tiene 24.3 % de V, por encima del 16 a 19 % del corpus en mayor.

**Lo que se concluye.** (1) El problema no es que falte el grado 7, sino que **el grado 7 es casi siempre menor**: Epic, Melancholic y Hard generan entre 17.7 y 23.6 % de v menor, de dos a cinco veces el corpus (4.4 a 9.1 %), con un total del grado 7 por encima del 11 a 14 % del corpus. (2) La proporción de tónica (25 a 27 %) coincide con el corpus. (3) ♭VII, el acorde más característico del menor en el corpus, sale poco en Melancholic (0.5 %), Hard (5.1 %), Dark y Tense (0 %), y sale algo más de lo normal en Uplifting y Epic (21 a 22 %). Parte de esto puede ser intencionado, porque un mood oscuro elige acordes menores, pero no se ha contrastado con oyentes. (4) La opción (c) **añade V sin quitar v**: en Hard el total del grado 7 pasa de 23.6 % a 30.4 % con 0.5 y a 39.3 % con 0.75, y lo que cede es ♭VII, no el v. En Uplifting, en cambio, con 0.5 se obtiene 6.4 % de V y nada de v, que es la banda del corpus. Por eso la recomendación pasa a ser más matizada: **activar el tono sensible con contexto 0.5 solo ayuda claramente a Uplifting (y algo a Epic)**; para Hard y Epic el defecto es el exceso de v menor, y eso no lo resuelve esta opción.

**Corrección a una cifra anterior.** La tabla de R11 dice que Tense genera 0.1 % de V-familia en la base. Esa cuenta incluía solo V, V7 y V9 sin alteraciones. Tense genera sobre todo dominantes alteradas (V7♯9 4.2 %, V7♭9 4.2 %, V7sus4 1.7 %), que suman el 8.5 % de este recuento. En el corpus esas variantes se agrupan en la clase dominante, pero no se pueden comprobar sus alteraciones con el prior de 5 clases. Las cifras de las tablas anteriores de V-familia son por tanto "V simple", y esta sección usa "cualquier V mayor o dominante".

**Pendiente.** Decidir si conviene reducir el v menor. El mecanismo está en la sección siguiente. Es una decisión de producto y se debe oír antes de cambiar nada. Los scripts quedaron en el scratchpad de la sesión.

### Mecanismo del exceso de v menor y de la falta de ♭VII

Se listaron, para cada slot de Epic, Melancholic, Hard y Uplifting (Do menor, sin muestreo), el coste unario de los acordes clave (`dB² + dT²` más la penalización de Adventure) y su posición en el ranking, y se contó en qué slot sale cada acorde en 600 semillas. La hipótesis previa (Brillantez objetivo baja) era incompleta: el factor es la **combinación de Brillantez baja y Tensión alta en el último slot**.

- **El último slot pide "oscuro pero tenso".** Las curvas de Epic, Melancholic y Hard acaban con Brillantez 0.28 a 0.45 y Tensión 0.60 a 0.64. Gm (v) tiene Brillantez 0.33 y Tensión 0.65, casi exactamente eso: es el primero del ranking en esos tres moods (coste 0.005 a 0.015). La Tensión de v es alta porque el término de atracción trata la raíz del grado 7 como dominante sea cual sea su calidad. G (V) tiene Brillantez 0.80, y G7 tiene 0.62 con Tensión 0.78, así que ambos quedan lejos (rank 17 a 144; coste 0.06 a 0.2).
- **Resultado en el muestreo.** v menor en el último slot: Epic 71 %, Melancholic 58 %, Hard 36 % (y otro 51 % en el slot 2 de Hard, de nuevo con Brillantez 0.35 y Tensión 0.54). Casi todo el exceso del grado 7 menor sale de esos slots.
- **♭VII queda penalizado por la calidad mayor.** Bb mayor tiene Brillantez 0.67 sin importar que sea diatónico en menor. Con objetivos de 0.22 a 0.45 (Melancholic, Hard) cuesta de 0.05 a 0.3 y casi nunca gana (0.5 % y 10 % en el último slot). Solo sale cuando el objetivo es alto: Epic 61 % en el slot 2 (Brillantez objetivo 0.66) y Uplifting 62 % en el slot 3. Lo mismo pasa con ♭VI y ♭III (Brillantez 0.57 y 0.62).
- **Es la misma carencia que H9, ampliada.** La Brillantez de `qualityBrightness` es independiente de la tonalidad: un acorde mayor siempre es brillante. `contextQualityBrightness` corrige solo el grado 7. Los acordes mayores diatónicos de una tonalidad menor (♭III, ♭VI, ♭VII) tienen el mismo problema, y ♭VII es el acorde alto más frecuente del corpus en menor (13 a 16 %). Eso encaja con la discusión de la Brillantez como medida independiente del contexto (H3, H4) y es coherente con Temperley y Tan (2013), que muestran que el brillo percibido depende del modo de la escala (no solo de la calidad de cada acorde); su estudio es de modos, no de acordes, así que es un apoyo indirecto.

**Límites.** Los costes de arriba son unarios y usan la Brillantez y la Tensión estáticas, sin el contexto ni el muestreo conjunto, así que dan el orden, no las probabilidades exactas; el patrón coincide con el muestreo real. No se probó ninguna corrección. **Opciones para decidir** (ninguna implementada): (a) extender `contextQualityBrightness` a ♭III, ♭VI y ♭VII en escalas menores, que bajaría su Brillantez y los dejaría competir con v; (b) bajar la Tensión de v menor cuando V mayor no está disponible; (c) no tocar nada y aceptar que los moods oscuros resuelven en v. Es una decisión de sonido, así que va con la prueba de oyentes de R10 y la tarea 5 (Brillantez).

### Prototipo de la opción (a): Brillantez con contexto para ♭III, ♭VI y ♭VII

`kMinorDiatonicMajorContext` en `Metrics.h`, con valor 0 (desactivada), generaliza `contextQualityBrightness`: un acorde con tercera mayor sobre ♭III, ♭VI o ♭VII, en una escala con tercera menor y con su tríada (fundamental, tercera mayor y quinta) dentro de la escala, acerca el término de calidad al de una tríada menor, igual que ya se hace con V. La escala menor armónica no tiene ♭VII, así que ♭VII no cuenta en ella, y las escalas con tercera mayor no cambian. Con 0 los valores no cambian: las pruebas del núcleo pasan (157 626 comprobaciones, 0 fallos) y `eval` da 128 métricas sin diferencias. Las pruebas nuevas comprueban que el valor por defecto es la identidad en los 32 tipos, 12 raíces y todas las escalas, y que con 1.0 solo cambian acordes con tercera mayor sobre los grados 3, 8 y 10 de escalas con tercera menor, y solo a la baja.

**Medición.** 600 semillas por mood, 4 slots, tónica Do, escala propia de cada mood, repeticiones colapsadas, mismo script que la sección anterior (la base reproduce sus cifras). Referencia del corpus en menor: V mayor o dominante 5 a 6.5 %, v menor 4 a 9 %, ♭VII 13 a 16 %, tónica cerca del 26 %. Cada celda es V / v / ♭VII / tónica en %.

| Mood | Base | Contexto 0.5 | Contexto 0.75 | 0.5 + tono sensible | 0.75 + tono sensible |
|---|---|---|---|---|---|
| Uplifting | 0.2 / 0.0 / 22.3 / 26.2 | 0.2 / 0.0 / 21.0 / **34.0** | 1.1 / 0.2 / **7.5** / **39.9** | 5.6 / 0.0 / 17.0 / **34.8** | 16.9 / 0.2 / 2.9 / 40.8 |
| Epic | 0.0 / 17.8 / 20.9 / 25.2 | 1.1 / 9.5 / 19.8 / 25.8 | **3.5 / 4.5 / 15.5 / 26.4** | 10.9 / 5.9 / 17.5 / 25.2 | 18.8 / 1.7 / 17.7 / 25.8 |
| Melancholic | 0.0 / 17.7 / 0.5 / 26.9 | 0.0 / 13.4 / 3.6 / 27.0 | 0.0 / **8.0** / 8.5 / 29.7 | = (V no entra) | = |
| Hard | 0.0 / 23.6 / 5.1 / 25.2 | 0.0 / 14.3 / 20.6 / 25.0 | 0.0 / 14.0 / 24.0 / 24.5 | 0.0 / 14.5 / 20.5 / 25.0 | 0.0 / 13.9 / 24.1 / 24.5 |
| Dark (frigio) | 0.0 / 5.2 / 0.0 / 25.7 | 0.0 / 3.9 / 0.0 / 25.1 | 0.0 / 3.4 / 0.0 / 23.3 | = | = |
| Tense (armónico menor) | 8.5 / 6.8 / 0.0 / 13.2 | 7.9 / 6.4 / 0.0 / 12.1 | 7.8 / 6.7 / 0.0 / 11.7 | = | = |

(Valores intermedios 0.25 y 1.0 en las salidas de la sesión; con 1.0 Uplifting casi no genera ♭VII, 0.2 %, y la tónica sube a 42.5 %.)

**Qué mejora.** (1) **Epic** y **Melancholic**: con 0.75 el v menor baja de 17.8 y 17.7 % a 4.5 y 8.0 %, dentro de la banda del corpus, y ♭VII sube a 15.5 % (Epic, en banda) y a 8.5 % (Melancholic, de 0.5 %). La tónica se mantiene (26.4 %) en Epic y sube tres puntos en Melancholic. Es el efecto esperado según el mecanismo: el último slot "oscuro y tenso" deja de resolver en Gm porque Bb y Ab ya compiten. (2) **Dark** baja algo el v (5.2 a 3.4 %) y sigue sin generar ♭VII (Si♭ mayor sí es diatónico en frigio; no se investigó por qué no sale). (3) **Hard** reduce el v de 23.6 a 14 %, pero ♭VII pasa de 5.1 % a 20 a 24 %, por encima del corpus (13 a 16 %), y el v sigue alto.

**Qué empeora.** **Uplifting**: con 0.5 la tónica sube de 26 a 34 % y con 0.75 a casi 40 %, y ♭VII cae a 7.5 %. Uplifting pide Brillantez alta en los últimos slots y ♭VII era su acorde brillante natural; al bajarle la Brillantez, el generador vuelve a la tónica. Es decir, las curvas de los moods menores se escribieron con la Brillantez sin contexto y el cambio las desplaza de forma distinta según el mood. **Tense** casi no cambia, aunque pierde un punto de tónica.

**Combinación con el tono sensible** (`kMinorLeadingToneInScale`). Se suman sin interferir, porque tocan acordes distintos (V frente a ♭III, ♭VI y ♭VII). Con 0.5 y tono sensible, Uplifting llega a V 5.6 % sin v y ♭VII 17 % (el mejor de sus resultados, aunque con la tónica en 34.8 %) y Epic a V 10.9 % y v 5.9 %, con V algo por encima de la banda. Con 0.75 y tono sensible, Epic llega a V 18.8 % y Uplifting a 16.9 %, de dos a tres veces el corpus. Con una medida tosca (suma de puntos fuera de banda en V, v, ♭VII y tónica, sobre los seis moods menores) el mejor valor sin tono sensible es 0.75 (95.3 frente a 129.3 de la base) y con tono sensible es 0.5 (91.9 frente a 124.1). Esa suma está dominada por Dark y Tense, que no cambian, y mezcla métricas de escalas distintas, así que solo sirve para ordenar.

**Conclusión.** **No hay un valor global bueno para todos los moods.** El contexto de 0.75 deja a Epic y Melancholic casi en el corpus y es una mejora clara, pero perjudica a Uplifting, y Hard queda con el exceso de ♭VII. Si se activa, la salida limpia es que el valor dependa del mood (por ejemplo 0.75 en Epic y Melancholic, 0 en Uplifting, 0.5 en Hard) o que se retoquen a la vez las curvas de Uplifting. Eso añade un parámetro por mood, y es una decisión de producto que depende de cómo suenen los moods, no del corpus. **Necesita escucha:** (i) si Epic y Melancholic con ♭VII y ♭VI en el último slot suenan "épicos" o "melancólicos" en lugar de neutros; (ii) si el Hard con tanto ♭VII sigue sonando duro. La constante sigue en 0. **Contraste con la bibliografía:** de Clercq y Temperley (2011) describen ♭VII como el acorde propio del rock en menor y Nobile (2016, no leído) un menor "basado en VI" donde ♭VII y IV hacen de dominante. Temperley y Tan (2013) apoyan de forma indirecta que el brillo depende del modo y no solo del acorde, pero su estudio es de modos en melodías, no de acordes. Ningún estudio revisado mide cuánto más oscuro suena Bb mayor dentro de Do menor que fuera de él, así que el valor del contexto (0.5 o 0.75) sale de las proporciones del corpus y no de datos perceptivos. Los scripts quedan en `/home/vant/chordslots-data/session-scripts/` (`probe2.cpp` y `run.sh` de esta sesión se copiaron allí).

### Avance de R6: separar maj/maj7 y min/min7 con POP909 como tercera fuente

Medición solamente: POP909 no se usa en el prior ni se distribuye (los datos brutos están en `chordslots-data/raw/pop909`, fuera del repositorio, y los derechos de las canciones de origen siguen sin cerrar). Se midió con el código de `tools/build_corpus.py` para las 1 090 canciones de Billboard y RS200 (tokens de grado y calidad, repeticiones colapsadas, bandera "extendido" de ese script) y con la misma regla para las 909 de POP909 (acordes de `chord_midi.txt` pasados por `harte_chord`; tonalidad de mayor duración). La pregunta cambió respecto a la medición anterior de R6 (divergencia de Jensen-Shannon frente a un suelo de ruido): ahora se pregunta si separar el **contexto** (el acorde de origen) en simple frente a séptima mejora la predicción del acorde siguiente en canciones **no vistas**. El modelo es el del prior (matriz de transición suavizada con la marginal, beta 200); el modelo separado se suaviza con el no separado como respaldo, y el error es la entropía cruzada en bits por transición, con intervalo bootstrap por canción.

Tasa de acordes "extendidos" entre los tokens mayor y menor: Billboard 8 % de los mayores y 44 % de los menores; RS200 1.5 % y 9 %; POP909 5.3 % y 13 %. **La tasa depende mucho de la fuente y de la convención de anotación**, no solo de la música.

| Prueba (ganancia de bits por transición; positivo = separar mejora) | Ganancia | IC 95 % |
|---|---|---|
| Validación en 10 partes, Billboard+RS200 | −0.008 | [−0.020, +0.002] |
| Validación en 10 partes, POP909 | +0.013 | [+0.010, +0.017] |
| Billboard+RS200, solo menor / solo mayor | −0.061 / +0.004 | [−0.110, −0.017] / [−0.005, +0.012] |
| POP909, solo menor / solo mayor | +0.007 / +0.014 | [−0.001, +0.014] / [+0.008, +0.020] |
| Entrenar en Billboard+RS200, probar en POP909 | **−0.104** | [−0.113, −0.094] |
| Entrenar en POP909, probar en Billboard+RS200 | **−0.049** | [−0.063, −0.038] |
| Entrenar en Billboard, probar en RS200 / al revés | +0.005 / −0.164 | [−0.019, +0.027] / [−0.185, −0.145] |
| Cruzada con beta 800 (más suavizado) | −0.014 y −0.023 | [−0.021, −0.008] y [−0.037, −0.013] |

**Qué se concluye.** Separar mejora algo dentro de POP909 (+0.013 bits) y empeora dentro de Billboard+RS200, y **entre corpus empeora en todos los casos** (la única excepción, de Billboard a RS200, es cero dentro del intervalo). Con más suavizado la pérdida se reduce, pero nunca se vuelve ganancia. Es coherente con que la distinción "triada o séptima" refleje sobre todo cómo anota cada fuente (Billboard anota séptimas con frecuencia, POP909 sale de una extracción automática en arreglos de piano, RS200 usa cifrado romano) y no una regla de transición transferible. La literatura apunta en la misma dirección: Koops et al. (2019, *Annotator subjectivity in harmony annotations of popular music*, J. New Music Research; no leído completo) encuentran desacuerdos entre anotadores sobre todo en las extensiones del acorde, y Humphrey y Bello (2015) discuten el efecto del vocabulario de acordes en los sistemas de estimación. **Decisión:** no separar maj/maj7 ni min/min7 en el prior; el 34.6 % de transiciones con séptima se mantiene como el aviso de H6 (el prior mezcla convenciones), pero la separación no se justifica con estos datos. Que el generador elija maj7 frente a mayor sigue dependiendo de Brillantez y Tensión, no del prior. **Límites:** la salida cruzada es dominio contra dominio (los errores cruzados son de 3.8 a 4.0 bits frente a 3.3 dentro del corpus), solo se probó la separación del acorde de origen, no la del destino, y POP909 son arreglos de piano de canciones pop chinas. El script es `r6_split.py` en `session-scripts`.

### Cómo validar sin depender de una sola persona

Quien desarrolla el VST no es productor profesional y tampoco hace falta serlo para la mayor parte de las pruebas. Conviene separar dos preguntas:

- **Percepción** (¿suena más tenso o más luminoso?). Bigand y Poulin-Charronnat (2006) revisan que los oyentes sin formación perciben tensión, expectativa y estructura de forma similar a los músicos. Estas pruebas admiten oyentes no productores, lo que facilita reclutar. Lerdahl y Krumhansl (2007) y Farbood (2012) recogieron sus datos de oyentes con diseños de ese tipo.
- **Uso** (¿qué control querría tener al editar un loop?, ¿este preset suena a su nombre?). Aquí sí pesa la experiencia de producción. Conviene recoger esa información de varias personas y registrarla por separado.
- **Calidad de las salidas.** Collins et al. adaptan la Consensual Assessment Technique (Amabile) para que jueces independientes y ciegos valoren la salida de modelos musicales. R7 aplica ese diseño.

Registrar en cada prueba quién participó, su formación musical y su experiencia de producción, para no mezclar poblaciones al interpretar los resultados.

## Dependencias y decisiones

| Dependencia | Estado y efecto |
|---|---|
| **Participantes** | Pendiente de reclutamiento. Es posible preparar las fichas y los estímulos antes de la sesión. Las pruebas de percepción pueden incluir oyentes que no son productores. Las de uso necesitan productores. |
| **Revisor con formación teórica** | Necesario para R1 y R7. Sin esta persona, esas tareas quedan limitadas al criterio de quien desarrolla el VST. |
| **Acceso interactivo a un DAW** | Un intento de abrir ChordSlots mediante el control de Windows fue denegado. La tarea de integración queda pendiente hasta disponer de una vía de prueba. |
| **Material exacto del estudio de Navarro** | La correspondencia de P3/e3 y partes de la reconstrucción original siguen abiertas. Resolverlas mejoraría la réplica externa. La auditoría del generador puede avanzar con lo ya disponible. |

Las decisiones abiertas son los pesos de Tensión, la separación de color y registro, la posible opción de Cierre y qué presets conservar o rediseñar. La ubicación de Energía también es una decisión de interfaz. La auditoría actual documenta su funcionamiento MIDI para los casos probados.

## Cómo mantener el plan

1. Antes de cada encargo, identificar una fila de este plan, la pregunta que responde y su criterio de cierre.
2. Al terminar, enlazar el diff o informe y registrar qué se verificó. Una prueba pendiente debe indicar qué le falta para poder ejecutarse.
3. Revisar el resultado antes de abrir el siguiente encargo. Mantener los pasos de los experimentos en sus informes y el protocolo de productores en el plan de continuidad.

**Próximo encargo:** cerrar el aviso del menú Mood. **Siguiente auditoría:** Tensión en el generador.
