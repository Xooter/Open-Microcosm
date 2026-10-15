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
| Jennings (acumulación por CO₂) | **PASA** | 20 *Paramecium* en una placa de ~1,9 mm con el gotero de CO₂ abierto en el centro (2 mM/s; ~10 mM y pH 5,3 en la fuente). Fracción a menos de 0,2 radios (4 % del área): 0,081 con CO₂ contra 0,031 sin él (el test pide > 1,5× el área y > 2× el control). La densidad crece hacia la fuente (3,4× la media en el centro). Resultado cualitativo: Jennings (1906) no publicó conteos. Con ácido más fuerte vio un anillo alrededor de la burbuja; acá no se modela la evitación del ácido fuerte. |
| Monod: µ en función del P externo (*Asterionella*) | **NO COINCIDE** | Quimiostato ideal (P fijo, Si y luz de sobra). Con P = K publicado (0,04 µM) el modelo crece al 82 % de µmax en vez del 50 %; satura a 4 K. Causa: la K de Tilman y Kilham (1976) es de **crecimiento** y el modelo la usa como K de **captación**, con una captación máxima de 0,3 cuotas/h (~9× µmax). Ver «Monod o Droop». |

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

5. **Cruces de la membrana de la ameba al fagocitar** (CORREGIDO; test largo «la membrana de la
   ameba no se cruza al comer presas de distinta forma»). Dos defectos de la copa, que aparecían
   o no según la trayectoria:
   - el avance de cada labio se medía con el ángulo envuelto a ±π desde el ancla; cuando el
     ancla se reubicaba cerca de un labio, el otro forro pasaba el antípoda, parecía estar atrás
     y la membrana nueva se insertaba 8 µm detrás del labio, doblada (caso "tres");
   - en un pseudópodo fino apretado contra la presa, el forro seguía por la punta hasta la cara
     de enfrente y la llevaba hacia la presa a través de la otra cara (caso "paramecio").
   Ahora el avance se acumula punto a punto, el forro termina donde la membrana da la vuelta y
   cada labio puede avanzar hasta encontrar al otro. La red de seguridad (`untangle`) ya no actúa
   en estos casos.

6. **Anillos que crecían sin límite al dividirse** (CORREGIDO): la hija se remuestreaba con el
   perímetro de su geometría del momento; una hija estirada nacía con miles de puntos y sus hijas
   más (300 → 5000 puntos, cruzando toda la placa). Ahora: perímetro de la forma de su especie.
7. **Hijas degeneradas** (CORREGIDO): un surco que cortaba casi en el borde dejaba una mitad sin
   área y la hija nacía en NaN (Cyclotella en cultivo denso). Cada hija tiene al menos el 20 % del
   área total.
8. **Inóculos sin nutrientes** (CORREGIDO): la célula sembrada tomaba su cuota del agua de
   alrededor y en agua pobre nacía sin P ni Si. Ahora la trae de afuera (entrada externa
   registrada); los nacimientos internos siguen tomándola del agua.

9. **Fijar la concentración media explotaba con el agua casi agotada** (CORREGIDO, al medir Monod):
   `setMeanConcentration` multiplicaba todo el campo por objetivo / media, incluidos los rizos
   negativos de la difusión espectral junto a un pozo de consumo y las celdas de vidrio (que suman
   como masa: ~1 % de error). Con la media casi en cero el factor era ~10⁶, el signo alternaba
   en cada paso y a los 220 pasos el fosfato era NaN (con NaN la captación nunca limita). Ahora
   toca solo el agua, recorta los negativos y, si no queda distribución, llena parejo.

## Colapso de las bacterias en la gota (test de 30 min)

El test «30 min sin jugador: ninguna especie se extingue» estaba en `skip` y nunca se había
corrido con el modelo actual. Corrido (seed 66, 50 000 bacterias): las bacterias pasan de 50 000
a 0 en 3 min (3 h biológicas); detrás caen los bacterívoros y las algas (comidas por los
depredadores y por quitridios) y crecen sin control *Candida* (13 → 815) y *Naegleria*
(6 → 361), que llenan la placa (el paso se vuelve muy lento: ~4 h para 30 min). Causas,
medidas con un diagnóstico por separado:

1. La configuración del test no tiene materia orgánica (sin detritos ni aporte): las bacterias no
   pueden crecer y el pastoreo las agota. Es coherente con esa configuración.
2. En la gota del Charco (con aporte orgánico), una epidemia de fagos lisa a 62 933 bacterias en
   ~100 s (de ~80 000 a ~120). Sobreviven ~45 resistentes (la frecuencia de mutación, 10⁻³).
3. Las resistentes, sanas y con 2–7 mM de materia orgánica, no se recuperan: el pastoreo les
   impone una mortalidad de ~0,05/s (estimada de la caída 50 000 → 31 000 en 20 s sin fagos) y
   su crecimiento máximo es 0,023/s (1,4/h biológica a ×60). Es la escala de volumen otra vez:
   20 paramecios en ~0,06 µL son ~3×10⁵ por mL, unas 300 veces más que un cultivo denso.

Opciones (decisión pendiente): densidades iniciales de protistas realistas para el volumen de la
placa; una columna de agua más profunda para las bacterias (como en Tilman); refugios para las
bacterias (biofilm, sedimento) y pérdida de fagos (adsorción a partículas); la resistencia
fenotípica a *Bdellovibrio* ya propuesta; o el modo cultivo para las dinámicas de población.

## Monod o Droop (decisión pendiente)

Medido con P externo constante (`setMeanConcentration` en cada paso), *Asterionella* a 20 °C:

| P [µM] | µ simulado [/h] | Monod publicado (µmax 0,0347, K 0,04) |
|---|---|---|
| 0,0025 | 0,0037 | 0,0020 |
| 0,01 | 0,0108 | 0,0069 |
| 0,04 | 0,0286 | 0,0174 |
| 0,16 | 0,0343 | 0,0278 |
| 1 | 0,0343 | 0,0334 |

El modelo es de cuota interna con estequiometría fija: capta con Monod (K de la especie,
0,3 cuotas/h) hasta 1,5 cuotas y crece mientras la reserva cubra la biomasa nueva. En equilibrio
µ(S) = mín(µmax, V·S/(K+S)): la K efectiva de crecimiento queda varias veces menor que la publicada.
Opciones:

1. **Monod de crecimiento** (como Tilman): el crecimiento se acota además por µmax·S/(K+S) con el S
   local. Reproduce las constantes publicadas tal cual; pierde el crecimiento con la reserva
   después de agotarse el nutriente (en lote, las células reales dan 1–3 divisiones más).
2. **Droop** (cuota celular): µ = µ∞(1 − Q₀/Q) con la reserva como Q y captación de Monod. Con la
   cuota máxima de 1,5 Q₀, µ∞ = 3 µmax, una captación máxima de 1,5 µmax·Q₀ y K de captación =
   1,5 K de crecimiento, el equilibrio da la K publicada (derivación propia; la relación general
   entre las constantes de captación y de crecimiento está en Morel 1987, J. Phycol. 23: 137-150);
   conserva la reserva.

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

- Elegir Monod o Droop para la limitación por nutrientes y agregar el test.
- Decidir cómo sostener a las bacterias en la gota («Colapso de las bacterias») y reactivar el
  test de 30 min.
- Verificar contra las publicaciones las temperaturas de Gause y los valores de Tilman.
- Uniones de filamentos (el 0,7–0,8 de `bio_bench`): no es un error del resolvedor. Un filamento
  aislado sin carga las mantiene a 2–7 % del reposo. El desvío aparece cuando el filamento crece
  más que el lugar disponible y se pandea: se dobla **en las uniones**, que tienen solo dos enlaces
  en las esquinas y funcionan como bisagras (uno se comprime y el otro llega a 2× su largo). En
  *Spirogyra* la pared transversal es compartida y el filamento se curva de manera repartida.
  Propuesta: rigidez a la flexión en la unión (enlaces cruzados o una restricción de ángulo entre
  los ejes de las dos células).
