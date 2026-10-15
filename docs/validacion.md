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
| µmax de heterótrofas en cultivo sin limitación (*Paramecium*, *Amoeba*) | **FALLA** | La población decrece (µ ≈ −0,018/h) o se extingue en vez de crecer a su µmax. Investigar: ¿el cultivo no es realmente sin limitación (comida, reservas iniciales) o hay un desbalance del modelo a escala de tiempo 600? Tarda demasiado (~1 h). |
| Exclusión competitiva de Gause (*P. aurelia* vs *P. caudatum*) | pendiente | Falta la especie *P. aurelia*. |
| Depredador–presa (*Didinium*–*Paramecium*) sin refugio | pendiente | Datos de Gause fig. 32 disponibles. |
| Tilman (*Asterionella* vs *Cyclotella*, P y Si) | pendiente | Faltan las especies y las medias saturaciones por especie (Tilman & Kilham 1976, J. Phycol. 12: 375–383: **valores sin verificar**). |
| Jennings (acumulación por CO₂) | pendiente | Ya hay comportamiento `acidTrap`; falta el test cuantitativo. |

## Pendientes

- Verificar contra las publicaciones las temperaturas de Gause y los valores de Tilman.
- Uniones de filamentos: se estiran 0,7–0,8 respecto del reposo en CPU y GPU (`bio_bench`).
- Test largo nuevo que falla tras el cambio de µmax: «la membrana de la ameba no se cruza al
  comer presas de distinta forma» (casos "tres" y "paramecio"): la nueva trayectoria muestra un
  cruce de membrana latente.
