# Validación científica

Qué se compara contra resultados publicados, con qué tolerancia y cómo está hoy. Los valores y
sus fuentes viven en `assets/species/fisiologia.toml`; los tests en `tests/test_validation.cpp`
(`ctest -L validacion`).

## Datos de fisiología cargados

| Especie | µmax publicado | Temp. | Fuente | Estado |
|---|---|---|---|---|
| *Paramecium* (*P. caudatum*) | 0,946/día | 26 °C (sin verificar) | Gause 1934, fig. 22 (ajuste propio a los datos de gauseR) | estimación: verificar temperatura |
| *Amoeba proteus* | 44 h de generación | 20 °C | Rogerson 1980, Can. J. Zool. | medición directa |
| *Didinium nasutum* | 0,93/día | 26 °C (sin verificar) | Gause 1934, fig. 32 | estimación |
| *Chlamydomonas reinhardtii* | 20 h (clorofila) | ~25 °C (sin verificar) | PNAS 1997 | estimación (no es conteo de células) |
| *Euglena gracilis* | 0,047/h | 30 °C | cultivo en digerido al 10 % | estimación (medio no estándar) |

Las demás especies usan el valor general del metabolismo (`metabolismo.crecimiento_maximo`,
0,15/h): **falta cargarlas con fuente.**

Ajustes propios a los datos originales de Gause (1934) digitalizados en gauseR (Clark et al.):
*P. caudatum* r = 0,946 ± 0,109/día, K = 203; *P. aurelia* r = 0,707 ± 0,090/día, K = 222
(volumen relativo); en el cultivo mixto *P. aurelia* excluye a *P. caudatum* (140 contra 23 al día
25). *Didinium* con presa abundante: +0,93/día; sin presa: −0,51/día.

## Estado de los tests de validación

| Test | Estado | Nota |
|---|---|---|
| µmax de *Paramecium* en cultivo sin limitación | **PASA** | µ simulado 0,026/h contra 0,026/h publicado (a 20 °C), dos duplicaciones. Cultivo: placa de ~860 µm, 2 individuos, sin fagos ni Bdellovibrio, escala de tiempo 600. |
| µmax de *Amoeba proteus* | pendiente | Su µmax publicado es con *Tetrahymena* como presa (cientos por división); una placa chica no tiene presas protistas suficientes. Hace falta un cultivo con presa ciliada abundante (o *Tetrahymena* como especie). |
| Exclusión competitiva de Gause (*P. aurelia* vs *P. caudatum*) | sin resultado | Las dos especies existen (`ParameciumAurelia`). En la placa no da un resultado comparable: con la comida limitada (`organicInflow` 5) unos 50 paramecios agotan las bacterias y se amontonan en 0,86 mm; Gause tenía cientos por 0,5 cm³. Necesita el modo cultivo (ver «La escala de volumen»). Ajustes propios a sus datos (gauseR): *P. caudatum* r = 0,946 ± 0,109/día, K = 203; *P. aurelia* r = 0,707 ± 0,090/día, K = 222; mezcla, día 25: *aurelia* 140 contra *caudatum* 23. |
| Depredador–presa (*Didinium*–*Paramecium*) sin refugio | pendiente | Datos de Gause fig. 32 disponibles. |
| Tilman (*Asterionella* vs *Cyclotella*, P y Si) | **PASA (débil)** | Cultivo en lote, columna de agua de 2 cm. Con P limitante gana *Asterionella* (0,52 de la biomasa); con Si limitante, *Cyclotella* (*Asterionella* 0,47). La dirección se invierte con la proporción de recursos, como en Tilman (1977), pero el margen es chico: en lote no hay exclusión completa (Tilman usó cultivos semicontinuos con dilución). Constantes de Monod **sin verificar** contra Tilman & Kilham (1976). |
| Jennings (acumulación por CO₂) | pendiente | Ya hay comportamiento `acidTrap`; falta el test cuantitativo. |

## Problemas del modelo encontrados al validar (fase MVP)

1. **Las bacterias se extinguían por Bdellovibrio, no por hambre.** (Primero lo atribuimos, sin
   comprobarlo, a una muerte sincrónica por hambre; los contadores mostraron 177 000 estallidos de
   Bdellovibrio hasta no dejar ninguna bacteria, con materia orgánica de sobra.) Nadando a
   100 µm/s, en una placa de 0,86 mm recorren todo y encuentran a cada bacteria. En la realidad
   la presa no se extingue: una fracción desarrolla resistencia fenotípica transitoria (Shemesh y
   Jurkevitch 2004). **Propuesta pendiente:** modelar esa resistencia. Los cultivos de
   validación no llevan Bdellovibrio (`WorldConfig::bdellovibrioFraction = 0`).
   - Igual se corrigió el ayuno: la bacteria sin comida suficiente baja su mantención a 0,2 de la
     normal y deja de crecer (fase estacionaria; Schink et al. 2019, Cell Systems 9: 64-73).
     Antes gastaba la mantención completa hasta morir.
2. **Fagos en poblaciones chicas:** con ~20 000 bacterias y la frecuencia real de mutantes
   resistentes casi seguro no hay ninguna resistente y la epidemia lítica las extingue. Plausible
   para un volumen tan chico; los cultivos de validación no llevan fagos (como en el laboratorio).
3. **NaN en el cuerpo blando bajo compresión extrema** (CORREGIDO): la causa era `limitApproach`,
   que corregía dos veces el acercamiento de cada par; encadenado entre células apretadas, las
   velocidades llegaban a ~10⁵ µm/s. Además hay una red de seguridad (`recoverNonFinite`,
   `fallas_numericas` en la exportación). Lo que se vio: diez paramecios de ~180 µm en una placa
   de 350 µm: uno quedó estirado contra el vidrio (anillo de 220 puntos, caja de 240 × 247 µm) y
   sus posiciones se volvieron NaN. Tras la corrección, el soak denso da 0 fallas numéricas.
4. Tras una lisis en un volumen chico la materia orgánica llega a cientos de mM (conservación
   correcta, pero conviene revisar si el volumen de la película es el que corresponde).

5. **Cruces de la membrana de la ameba al fagocitar** (test largo «la membrana de la ameba no se
   cruza al comer presas de distinta forma»): es un error latente de la copa fagocítica que
   aparece o desaparece según la trayectoria. Con el µmax de Rogerson fallan los casos "tres" y
   "paramecio"; sin él (crecimiento rápido de antes), el caso "punta" (76 cruces). Antes pasaba
   por casualidad. Falta revisar la mecánica de la copa (labios, cierre, remallado).

6. **Anillos que crecían sin límite al dividirse** (CORREGIDO): la hija se remuestreaba con el
   perímetro de su geometría del momento; una hija estirada nacía con miles de puntos y sus hijas
   más (300 → 5000 puntos, cruzando toda la placa). Ahora: perímetro de la forma de su especie.
7. **Hijas degeneradas** (CORREGIDO): un surco que cortaba casi en el borde dejaba una mitad sin
   área y la hija nacía en NaN (Cyclotella en cultivo denso). Cada hija tiene al menos el 20 % del
   área total.
8. **Inóculos sin nutrientes** (CORREGIDO): la célula sembrada tomaba su cuota del agua de
   alrededor y en agua pobre nacía sin P ni Si. Ahora la trae de afuera (entrada externa
   registrada); los nacimientos internos siguen tomándola del agua.

## La escala de volumen (lección principal)

Los experimentos clásicos de poblaciones son de mililitros (Gause: 0,5 cm³; Tilman: cultivos
con 10⁴–10⁵ células/mL). La placa simulada es una película de ~0,1 mm sobre ~0,6 mm²: unas
10 000 veces menos volumen. Con concentraciones reales, **toda la placa tiene menos fósforo que
una sola diatomea**, y una decena de paramecios ya la llena. Para validar experimentos de
población:

- la química puede representar una columna de agua más profunda y bien mezclada (`gapHeight`):
  así se hizo con Tilman;
- la competencia por bacterias (partículas en 2D) no se arregla así: hay que limitar la comida
  para que la población de equilibrio quepa en la placa (Gause), o una placa más grande;
- un "modo cultivo" bien mezclado (sin cuerpos blandos, solo metabolismo y química) permitiría
  validar dinámicas de población a escala de mL. Propuesta pendiente.

## Pendientes

- Verificar contra las publicaciones las temperaturas de Gause y los valores de Tilman.
- Uniones de filamentos: se estiran 0,7–0,8 respecto del reposo en CPU y GPU (`bio_bench`).
- Test largo nuevo que falla tras el cambio de µmax: «la membrana de la ameba no se cruza al
  comer presas de distinta forma» (casos "tres" y "paramecio"): la nueva trayectoria muestra un
  cruce de membrana latente.
