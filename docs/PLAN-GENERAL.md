# Plan general de ChordSlots

Actualizado el 18 de noviembre de 2026. Este documento reúne el estado de la revisión del VST, el orden de trabajo y las decisiones pendientes. Los informes enlazados conservan los métodos y resultados.

## Objetivo

Conseguir que cada control del VST tenga un efecto comprensible y predecible al editar un loop. Usaremos las pruebas del motor y del MIDI para medir qué hace el producto y la escucha externa para decidir qué efectos se perciben y cuáles merece la pena controlar desde la interfaz.

El alcance de este plan es Harmony, sus controles de interpretación asociados, los presets y la verificación del VST en un DAW.

## Estado actual

| Área | Hecho | Pendiente |
|---|---|---|
| **Tensión** | Comparación con respuestas de oyentes publicadas. Se usaron 11 progresiones y 63 posiciones. El modelo contextual no mostró una ventaja consistente frente a la referencia estática con voicing. | Probar qué aporta el contexto al **generar y editar** acordes. El cierre circular se ha medido como diagnóstico sobre secuencias existentes. El informe detallado está en `audit-curves/INFORME.md` del espacio de trabajo. |
| **Brillantez** | Barrido por escalas y edición de un slot. La puntuación armónica aumentó en 195 de 240 subidas frente a una regeneración equivalente, también con centro fijo. El registro real y la tensión pueden variar al cambiar de acorde. | Probar **color armónico frente a registro** con oyentes. Ampliar la comprobación de edición a configuraciones más cercanas al uso habitual cuando toque verificar el VST. El informe local está en `audit-curves/local-edit/INFORME.md`. |
| **Energía** | Auditoría con acordes fijos en Piano, Close y Rootless. Se comprobó la respuesta de velocidad y densidad. El MIDI del procesador con transporte simulado y el Play interno coincidieron con el SMF en 288 casos por ruta. | Confirmar la integración en un DAW. La muestra actual no aporta un motivo para recalibrar la barra. El informe local está en `audit-curves/energy/INFORME.md`. |
| **Match y ayuda** | Diff de textos revisado. Match compara Brillantez y Tensión. La ayuda de los carriles describe el efecto de cada control. | Ver los tooltips en una ventana real. [README](../README.md) |
| **Moods** | Vista previa del preset validada en el editor JUCE y en render. Se verificaron Auto, Free manual y Custom. | Avisar de que elegir un preset también ejecuta un Roll. Comprobar el menú en un DAW y medir si los presets generan resultados distintos. El auxiliar local está en `audit-curves/mood-check/check.cpp`. |

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

## Dependencias y decisiones

| Dependencia | Estado y efecto |
|---|---|
| **Participantes** | Pendiente de reclutamiento. Es posible preparar las fichas y los estímulos antes de la sesión. |
| **Acceso interactivo a un DAW** | Un intento de abrir ChordSlots mediante el control de Windows fue denegado. La tarea de integración queda pendiente hasta disponer de una vía de prueba. |
| **Material exacto del estudio de Navarro** | La correspondencia de P3/e3 y partes de la reconstrucción original siguen abiertas. Resolverlas mejoraría la réplica externa. La auditoría del generador puede avanzar con lo ya disponible. |

Las decisiones abiertas son los pesos de Tensión, la separación de color y registro, la posible opción de Cierre y qué presets conservar o rediseñar. La ubicación de Energía también es una decisión de interfaz. La auditoría actual documenta su funcionamiento MIDI para los casos probados.

## Cómo mantener el plan

1. Antes de cada encargo, identificar una fila de este plan, la pregunta que responde y su criterio de cierre.
2. Al terminar, enlazar el diff o informe y registrar qué se verificó. Una prueba pendiente debe indicar qué le falta para poder ejecutarse.
3. Revisar el resultado antes de abrir el siguiente encargo. Mantener los pasos de los experimentos en sus informes y el protocolo de productores en el plan de continuidad.

**Próximo encargo:** cerrar el aviso del menú Mood. **Siguiente auditoría:** Tensión en el generador.
