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
| µmax de *Paramecium* en cultivo sin limitación | **PASA** | µ simulado 0,0261/h contra 0,0260/h publicado (a 20 °C). Cultivo: placa de ~860 µm, 2 individuos, sin fagos, escala de tiempo 600; ~2,5 min. |
| µmax de *Amoeba proteus* | pendiente | Su µmax publicado es con *Tetrahymena* como presa (cientos por división); una placa chica no tiene presas protistas suficientes. Hace falta un cultivo con presa ciliada abundante (o *Tetrahymena* como especie). |
| Exclusión competitiva de Gause (*P. aurelia* vs *P. caudatum*) | pendiente | Falta la especie *P. aurelia*. |
| Depredador–presa (*Didinium*–*Paramecium*) sin refugio | pendiente | Datos de Gause fig. 32 disponibles. |
| Tilman (*Asterionella* vs *Cyclotella*, P y Si) | pendiente | Faltan las especies y las medias saturaciones por especie (Tilman & Kilham 1976, J. Phycol. 12: 375–383: **valores sin verificar**). |
| Jennings (acumulación por CO₂) | pendiente | Ya hay comportamiento `acidTrap`; falta el test cuantitativo. |

## Problemas del modelo encontrados al validar (fase MVP)

1. **Las bacterias mueren todas a la vez cuando la comida no alcanza.** La muerte es un umbral
   fijo de masa (`masa < 0,5`) y la mantención es proporcional a la masa: cuando la población
   supera lo que sostiene el aporte de materia orgánica, todas adelgazan al mismo ritmo y cruzan
   el umbral casi juntas (120 000 → 0 en pocas horas), aun con alimento entrando. Las bacterias
   reales entran en fase estacionaria (bajan su mantención) y mueren de a poco. **Decisión de
   modelado pendiente.**
2. **Fagos en poblaciones chicas:** con ~20 000 bacterias y la frecuencia real de mutantes
   resistentes casi seguro no hay ninguna resistente y la epidemia lítica las extingue. Plausible
   para un volumen tan chico; los cultivos de validación no llevan fagos (como en el laboratorio).
3. **NaN en el cuerpo blando bajo compresión extrema:** diez paramecios de ~180 µm en una placa
   de 350 µm: uno quedó estirado contra el vidrio (anillo de 220 puntos, caja de 240 × 247 µm) y
   sus posiciones se volvieron NaN; el NaN pasa al metabolismo. El motor no debería producir NaN
   nunca: falta encontrar la operación (XPBD, división o contacto con el vidrio) y una red de
   seguridad.
4. Tras una lisis en un volumen chico la materia orgánica llega a cientos de mM (conservación
   correcta, pero conviene revisar si el volumen de la película es el que corresponde).

## Pendientes

- Verificar contra las publicaciones las temperaturas de Gause y los valores de Tilman.
- Uniones de filamentos: se estiran 0,7–0,8 respecto del reposo en CPU y GPU (`bio_bench`).
- Test largo nuevo que falla tras el cambio de µmax: «la membrana de la ameba no se cruza al
  comer presas de distinta forma» (casos "tres" y "paramecio"): la nueva trayectoria muestra un
  cruce de membrana latente.
