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
| **H8** | **Convención implícita en los predicados de intervalos, sin fallos hoy.** Las extensiones superiores a 12 se ignoran (`hasMajorThird` busca 4, no 16). Con el vocabulario actual no hay discrepancias en 32 tipos. Solo maj7#11 y quartal cambiarían de clasificación si se redujera módulo 12. Quartal se etiqueta como sus por tener cuarta y no tercera. Faltan dominantes alterados (7#5, 7b13, 7#11). | `Theory.h`. Comprobado con un verificador independiente. Conviene una prueba antes de añadir tipos nuevos. | Es una cuestión de ingeniería, sin fuente. |

### Tareas propuestas

No alteran el orden de las tareas 1–5. R2 y R4 amplían las tareas 2 y 5 con los mismos informes y protocolos. El orden sugerido es R1, R3, R5, R2/R4, R6, R7 y R8.

| Id | Tarea | Se da por hecha cuando… |
|---|---|---|
| **R1** | **Pruebas de conformidad teórica.** Convertir hechos de un manual de armonía estándar en aserciones: cifrados romanos, jerarquía de tensión (una dominante con séptima frente a la misma raíz con maj7), nombres de acordes por tonalidad, calidades. Una persona con formación teórica elige las aserciones. | Hay una lista revisada de aserciones, con las que fallan registradas y sin exigir arreglarlas todavía. |
| **R2** | **Función y atracción en Tensión.** Probar un término de tritono o de tono sensible y una función que dependa de la calidad, dentro de la comparación de la tarea 2 y sin retocar los demás pesos. | La ablación se informa con los datos de Navarro-Cáceres y con el conjunto de Barratt-Fitzgerald et al. como segundo contraste externo. |
| **R3** | **Colinealidad de H2.** Calcular la correlación entre aspereza y disonancia TIV en los 32 tipos y 12 raíces, y comparar la tensión con uno solo de los términos. | Hay un informe que decide mantener ambos, fusionarlos o quitar uno, con la correlación como justificación. |
| **R4** | **Pesos y saturación de Brillantez.** Ajustar los pesos con las respuestas por pares de la tarea 5 en lugar de fijarlos a mano. Revisar la saturación de Caug, Cdim y Cm7b5. | Los pesos tienen un intervalo de incertidumbre y no hay tipos de acorde saturados en los extremos sin motivo. |
| **R5** | **Dominante en menor (H5).** Contar V y V7 en las canciones en menor del corpus. Decidir si el grupo de candidatos incluye las notas de la menor armónica para acordes de función dominante, o si depende del estilo. | La decisión queda documentada con ese recuento, y la prueba de candidatos del CLI la refleja. |
| **R6** | **Prior de transiciones (H6).** Medir si separar maj/maj7 y min/min7 cambia el prior. Evaluar Chordonomicon para géneros ausentes. Renombrar los clústeres si no son estilos. | Se informa el efecto de cada cambio y la atribución sigue en `DATA_LICENSES.md`. |
| **R7** | **Panel ciego de jueces.** Varias personas, incluida al menos una con formación teórica, valoran a ciegas progresiones de ChordSlots, progresiones aleatorias y progresiones reales del corpus. | Hay una tabla con acuerdo entre jueces y una conclusión sobre qué condiciones se distinguen. |
| **R8** | **Valor por defecto del contextual (H7).** Fijar `kUseContextualTension` y las curvas de Epic y Hard según el resultado de la tarea 2. | La constante y las curvas se cambian juntas en un solo diff con el informe enlazado. |
| **R9** | **Informes de auditoría dentro del repositorio.** Hecha en `e39b0ca`: los informes, scripts y tablas están en [docs/audits](audits/README.md). Las cifras citadas en este plan (11 progresiones, 63 posiciones, 195 de 240, 288 casos) se comprobaron en los informes. Los informes examinaron el commit `7dc1ef0`. El núcleo (`core/`) no ha cambiado desde entonces. | Hecha. Los scripts de Windows (`.cmd`, `.ps1`) no se han probado en Linux. |

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
| **R6** | Sobre 1 090 canciones y 99 389 transiciones, el 34.6 % de las transiciones implica un acorde de séptima o extendido (el 39.6 % en Billboard y el 12.1 % en RS200). Al separar triada y familia de séptima, 9 de las 10 fuentes más frecuentes superan 0.1 bits de divergencia Jensen-Shannon (entre 0.08 y 0.33) frente a un suelo de ruido de 0.003 a 0.04. Las más altas son V mayor (0.325), ♭III mayor (0.275) y ♭VII mayor (0.235). | El suelo de ruido supone muestreo independiente entre transiciones. Las canciones se agrupan por estilo, así que el ruido real es mayor. Repetir con remuestreo por canción antes de decidir. |
| **H7** | Lista de los 13 puntos y de los efectos en pruebas (ver H7). La lectura es estática: no se invirtió la constante ni se ejecutó `eval`, así que se desconoce cuánto se mueven las líneas base. | Medir el efecto en `eval` de Epic y Hard antes de R8. |
| **H8** | Sin discrepancias en 32 tipos (ver H8). | Prueba que falle si se añade un tipo cuya clasificación depende de la convención. |

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
