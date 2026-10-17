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
| µmax de *Paramecium* en cultivo sin limitación | **PASA** | µ simulado 0,024/h contra 0,026/h publicado (a 20 °C), dos duplicaciones, con la ingestión medida de la especie (antes 0,026 con el caudal general, 4× sobrealimentado). Cultivo: placa de ~860 µm, 2 individuos, sin fagos ni Bdellovibrio, escala de tiempo 600. |
| µmax de *Amoeba proteus* | pendiente | Su µmax publicado es con *Tetrahymena* como presa (cientos por división); una placa chica no tiene presas protistas suficientes. Hace falta un cultivo con presa ciliada abundante (o *Tetrahymena* como especie). |
| Exclusión competitiva de Gause (*P. aurelia* vs *P. caudatum*) | sin resultado | Las dos especies existen (`ParameciumAurelia`). En la placa no da un resultado comparable: con la comida limitada (`organicInflow` 5) unos 50 paramecios agotan las bacterias y se amontonan en 0,86 mm; Gause tenía cientos por 0,5 cm³. Necesita el modo cultivo (ver «La escala de volumen»). Ajustes propios a sus datos (gauseR): *P. caudatum* r = 0,946 ± 0,109/día, K = 203; *P. aurelia* r = 0,707 ± 0,090/día, K = 222; mezcla, día 25: *aurelia* 140 contra *caudatum* 23. |
| Depredador–presa (*Didinium*–*Paramecium*) sin refugio | pendiente | Datos de Gause fig. 32 disponibles. |
| Tilman (*Asterionella* vs *Cyclotella*, P y Si) | **PASA** | Cultivo en lote, columna de agua de 2 cm. Con P limitante gana *Asterionella* (0,71 de la biomasa); con Si limitante, *Cyclotella* (*Asterionella* 0,22). Con el modelo de Droop (antes 0,52 y 0,47: la K de crecimiento publicada se usaba en la captación y la limitación quedaba débil). En lote no hay exclusión completa (Tilman usó cultivos semicontinuos). Constantes de Monod **sin verificar** contra Tilman & Kilham (1976). |
| Jennings (acumulación por CO₂) | **PASA** | 20 *Paramecium* en una placa de ~1,9 mm con el gotero de CO₂ abierto en el centro (2 mM/s; ~10 mM y pH 5,3 en la fuente). Fracción a menos de 0,2 radios (4 % del área): 0,081 con CO₂ contra 0,031 sin él (el test pide > 1,5× el área y > 2× el control). La densidad crece hacia la fuente (3,4× la media en el centro). Resultado cualitativo: Jennings (1906) no publicó conteos. Con ácido más fuerte vio un anillo alrededor de la burbuja; acá no se modela la evitación del ácido fuerte. |
| µmax de *Paramecium* en el cultivo bien mezclado (ingestión publicada) | **PASA** | 0,023/h contra 0,026/h a 20 °C, con bacterias saturantes (~10⁹/mL) y la ingestión medida (ver «Balance de energía de *Paramecium*»). |
| Monod: µ en función del P externo (*Asterionella*) | **PASA** (test básico) | Modelo de Droop desde el 15/10/2026: en equilibrio, µ = µmax·S/(K+S) con la K publicada, con un error menor al 3 % (`tests/test_nutrients.cpp`, sin `World`). En la placa, la célula ve menos P que la media (pozo de consumo: 0,028 contra 0,04 µM) y con ese valor local la curva coincide (0,0142 contra 0,0143 /h). |

## Balance de energía de *Paramecium* (cultivo bien mezclado, fase P1)

El cultivo bien mezclado (`game/culture`) usa la ingestión y el aclaramiento publicados de
*P. caudatum* (Applied Water Science 2013, doi:10.1007/s13201-013-0130-1: ~26·10³ bacterias por
individuo y por hora a 25 °C, ~106 nL/h). Con ellos, el primer resultado no alcanzaba el µmax de
Gause (0,007/h contra 0,026/h a 20 °C). Las causas, en orden:

1. **La masa de las células alargadas estaba inflada** (CORREGIDO, 16/10/2026). Salía de la esfera
   de la misma sección vista desde arriba (`cellMassPmol` del área 2D): un paramecio de 180 × 50 µm
   pesaba como una esfera de 95 µm (~4,5·10⁵ µm³), casi el doble que el esferoide real
   (~2,4·10⁵ µm³); *Asterionella*, 3,2 veces. Ahora la masa es la del esferoide de semiejes mayor,
   menor y vertical (`espesor` en `[ser.forma]`; por defecto, el menor) por la materia seca relativa
   (`cellMassScale`). Las células humanas, cuyo factor ya traía su volumen medido, se convirtieron
   para conservar la misma masa.
2. **Las células limitadas por la comida no se dividían nunca** (CORREGIDO): el metabolismo invertía
   en biomasa todo lo que pasaba del colchón de reservas (0,1 de la biomasa) y dividirse pedía
   reservas por 0,1 de la biomasa; la célula crecía sin límite pasado el doble. Ahora, con el tamaño
   de división, deja de invertir y junta las reservas de la citocinesis (punto de control del ciclo).
3. **La placa sobrealimentaba a los paramecios** (CORREGIDO): su caudal máximo era el general del
   citostoma (30 bacterias/s, sin fuente: ~108·10³/h, cuatro veces lo medido). Ahora la placa usa la
   ingestión medida de la especie cuando la tiene (la misma que el cultivo).

Con los tres: µ = 0,023/h en el cultivo y 0,024/h en la placa, contra 0,026/h publicado (las dos
dentro del 25 % sin ajustar nada). La cuenta a mano: un paramecio de ~400 pmol gasta ~6 pmol/h en
mantenerse y nadar y crecer a 0,026/h pide ~17 pmol/h más (rendimiento 0,6): ~10·10³ bacterias/h,
que la ingestión publicada (18·10³/h a 20 °C) cubre con bacterias abundantes.

**Gases con Q10** (CORREGIDO): en la placa, el O₂ que respira una célula y el CO₂ que libera se
dividían por `bioScale` (compresión × Q10): a 30 °C la célula respiraba en el agua lo mismo que a
20 °C. Ahora se deshace solo la compresión (`bioTimeScale`): con calor respira más rápido, como el
cultivo.

## Gause en el cultivo (fase P1, paso 5)

Protocolo de Gause (1934, cap. V): tubos de 0,5 cm³ de medio salino de Osterhout a 26 °C; cada
día se centrifuga, se cambia el medio y se agrega una ración fija de bacterias (en el modelo,
5·10⁷/mL). Tests en `tests/test_gause.cpp`:

| Test | Estado | Nota |
|---|---|---|
| Monocultivos: meseta | **PASA** | *P. caudatum* ~220–250 por tubo, *P. aurelia* ~550–620 (más chica: más individuos con la misma comida). Gause: K = 203 y 222 en volumen relativo. |
| Cultivo mixto: exclusión competitiva | **PASA** | Al día 60, una de las dos queda por debajo del 20 % en 4 de 5 réplicas. |
| Gana *P. aurelia* | **NO PASA** (`should_fail`) | Gana la que se adelanta al principio (2 o 3 de 5 réplicas). La ventaja de *P. aurelia* sale solo de la alometría de la alimentación (~20 % más aclaramiento por biomasa: Hansen et al. 1997) y no alcanza contra el µmax mayor de *P. caudatum*. Gause atribuyó parte de su resultado a la sensibilidad de *P. caudatum* a los desechos del medio, que el modelo no tiene. |
| *Didinium* sin refugio | **PASA** | *Didinium* se multiplica comiendo y extingue a los paramecios; después no queda ninguno activo (se enquista: Beers 1927). Gause vio que morían; con quistes, el cultivo igual queda sin depredadores activos. |
| *Didinium* con inmigración (oscilaciones) | pendiente | |

*P. aurelia* no tiene mediciones propias de la alimentación: su ingestión máxima y su aclaramiento
salen de los de *P. caudatum* con el exponente del zooplancton (V^0,75–0,77). ESTIMACIÓN.

## Quimiostato (fase P1, paso 7)

| Test | Estado | Nota |
|---|---|---|
| Monod: sustrato residual | **PASA** | Bacterias con D = 0,5/h: S* = 0,652 µM contra K (D + m) / (µmax − D − m) = 0,647. Antes del paso implícito para el sustrato, en cada paso de 1 min las bacterias se comían todo lo que entraba (S ≈ 0). |
| Tilman en quimiostato: P limitante | **PASA** | D = 0,25/día (Tilman 1977), 60 días: *Cyclotella* se extingue (0 contra 250 *Asterionella*). |
| Tilman en quimiostato: Si limitante | **PASA** | *Asterionella* se extingue (0 contra 1366 *Cyclotella*). Exclusión completa, como en los cultivos semicontinuos de Tilman (en lote solo se veía la tendencia). |

## Antibióticos: la CIM que emerge (fase P1, paso 11)

Los reactivos (`reactivos.toml`, `game/reagent`) usan la función farmacodinámica que Regoes et al.
(2004, Antimicrob. Agents Chemother. 48: 3670-3676) ajustaron a curvas de muerte de *E. coli*
(ψmax, ψmin, zMIC y κ), con la muerte proporcional a lo que crece la bacteria (los β-lactámicos no
matan a las que no crecen). La CIM no entra a la simulación: sale de una dilución en caldo simulada
(Mueller-Hinton a 35 °C, 5·10⁵ bacterias/mL, 18 h, serie al doble, crecimiento visible = 10⁷/mL,
ESTIMACIÓN del umbral) y se compara con la que el mismo trabajo midió por dilución.

| Test | Estado | Nota |
|---|---|---|
| CIM de la ampicilina | **PASA** | 4 µg/mL contra 8 publicada (una dilución: la reproducibilidad de la prueba es ±1). |
| CIM de la tetraciclina | **PASA** | 1 µg/mL contra 1. |

## Calibración cruzada placa ↔ cultivo (fase P1, paso 2)

`measureFunctionalResponse` (`game/calibration`, `bio_bench --respuesta-funcional paramecium`) mide
cuántas bacterias come *Paramecium* en una placa de 0,86 mm a 20 °C y lo compara con la respuesta
funcional del cultivo (tipo II con los datos). Tres tests de validación (~10⁵, ~10⁶ y ~10⁹
bacterias/mL) piden que coincidan dentro de un factor 2.

Lo que apareció al medir:

1. **La placa olvidaba la tercera dimensión de las presas chicas.** Toda bacteria superpuesta con la
   boca en la vista cenital se tragaba, como si estuviera a la misma altura que el ciliado. Ahora
   cada bacteria tiene una altura en la película (`World::filmHeight`, que cambia al azar cada vez
   que la cruza nadando) y la boca solo alcanza a las que pasan a su altura; la ameba, a las que
   nadan a la altura de su cuerpo y a las de la biopelícula.
2. **El aclaramiento publicado es a alta densidad.** El estudio de la ingestión (106 nL/h) lo calcula
   a ~2,5·10⁸ bacterias/mL, donde la ingestión ya está casi saturada; usado como pendiente inicial,
   la media saturación quedaba en 2,5·10⁸/mL y *Paramecium* no podía crecer en los cultivos de Gause.
   El aclaramiento con pocas presas sale de la escala general del zooplancton (~10⁵ volúmenes
   corporales por hora en ciliados; Hansen et al. 1997, Kiørboe 2011): ~24 µL/h para *P. caudatum*.
   Es consistente con el estudio: a 2,5·10⁸/mL la ingestión queda en 0,996 de la máxima y a ~10⁶/mL
   cae a la mitad, como informa para las concentraciones bajas. ESTIMACIÓN.
3. **El radio de captura de bacterias de la boca** (6 µm, sin fuente) se separó del alcance para
   células y detritos y se **calibró** con esa respuesta funcional: `bacteriaCaptureRadius` = 75 µm
   (la corriente oral arrastra el agua de alrededor del cuerpo).

Resultado (`bio_bench --respuesta-funcional paramecium`):

| bacterias/mL | placa [1/h] | cultivo [1/h] | razón |
|---|---|---|---|
| 7,4·10⁴ | 901 | 1595 | 0,56 |
| 1,1·10⁵ | 1621 | 2257 | 0,72 |
| 3,5·10⁵ | 4053 | 5664 | 0,72 |
| 1,9·10⁶ | 10329 | 13069 | 0,79 |
| 1,4·10⁷ | 18375 | 17412 | 1,06 |
| 8,5·10⁸ | 18375 | 18368 | 1,00 |

A baja densidad la placa come algo menos: los paramecios agotan las bacterias de alrededor (un pozo
de consumo que el cultivo bien mezclado no tiene). Pendiente: una respuesta funcional medida a varias
densidades para *P. caudatum* (hoy hay un solo punto publicado).

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

Elegido (15/10/2026, "lo más realista"): **refugio en la biopelícula**. Las bacterias que chocan
contra el vidrio pueden adherirse; las adheridas no nadan, crecen con la comida local y sus hijas
quedan en la biopelícula; se sueltan de a poco. Los filtradores no las alcanzan y las amebas sí
(Matz y Kjelleberg 2005, Trends Microbiol. 13: 302-307). La probabilidad de adhesión (0,01 por
choque) y el desprendimiento (0,2/h) son ESTIMACIONES.

Resultado en la gota del Charco (seed 66, 6 h biológicas): la biopelícula se forma rápido (3 480
adheridas, ~5 %, al cabo de 1 h); la epidemia de fagos igual mata al 99,9 % (la biopelícula no
protege de los fagos en el modelo), pero ya no hay extinción: quedan 16 resistentes, casi todas
adheridas, y desde ahí la población vuelve a crecer (16 → 176 en 3 h, duplicándose cada ~50 min).
Mientras tanto los paramecios bajan de 17 a 3 por falta de bacterias. El test de 30 min sigue
en `skip`: su placa no tiene materia orgánica, y sin comida las bacterias no crecen igual.

Otras opciones que quedan: densidades iniciales de protistas realistas para el volumen de la
placa; una columna de agua más profunda para las bacterias (como en Tilman); refugios para las
bacterias (biofilm, sedimento) y pérdida de fagos (adsorción a partículas); la resistencia
fenotípica a *Bdellovibrio* ya propuesta; o el modo cultivo para las dinámicas de población.

## Monod y Droop (elegido: Droop)

Antes, la K de Tilman y Kilham (1976), que es de **crecimiento**, se usaba como K de **captación**
con una captación máxima fija de 0,3 cuotas/h (~9× µmax): con P = K, *Asterionella* crecía al
82 % de µmax en vez del 50 %. Ahora (`bio/metabolism`):

- cuota mínima Q₀ (`*Ratio`) y máxima Qmax = L·Q₀ (`luxuryQuota`, L = 1,5);
- crecimiento de Droop: µ = µ∞(1 − Q₀/Q), µ∞ = µmax·L/(L−1) (a Qmax crece a µmax);
- captación de Monod con Vm = L·µmax·Q₀ y Ku = L·K.

En equilibrio (µ·Q = captación) eso da exactamente µmax·S/(K+S) (derivación propia; la relación
general entre constantes de captación y de crecimiento está en Morel 1987, J. Phycol. 23:
137-150). Después de agotarse el nutriente, la célula sigue creciendo con su reserva. Sin
verificar: L = 1,5 es un valor bajo para el P (en algas, Qmax/Q₀ llega a 5–50; Droop 1973 da
valores para B₁₂ y P); se dejó así para no cambiar la estequiometría del resto del modelo.

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

- Decidir cómo sostener a las bacterias en la gota («Colapso de las bacterias») y reactivar el
  test de 30 min.
- Verificar contra las publicaciones las temperaturas de Gause y los valores de Tilman.
- Uniones de filamentos (CORREGIDO, 15/10/2026): el 0,7–0,8 de `bio_bench` no era del resolvedor.
  Un filamento que crecía más que su lugar se pandeaba doblándose **en las uniones**: eran dos
  enlaces cerca de la punta redondeada (un perno de ~5 µm que giraba como bisagra) y la
  fragmentación cortaba enlaces sueltos (paredes a medias). Ahora la unión es la pared transversal
  entera (5 enlaces a lo ancho y 2 cruzados) y se corta entera. *Spirogyra* creciendo 20 h en la
  placa: desvío medio de las uniones 0,40 → 0,06 y el filamento se curva repartido entre las
  células.