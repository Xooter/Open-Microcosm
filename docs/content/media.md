# Culture media: `media/<id>.toml`

A `[[medium]]` is a published composition: `source` is mandatory. It is used by prepared plates
([plates.md](plates.md)), cultures and lessons. Reader: `game/medium_toml`.

| Key | Meaning |
|---|---|
| `id`, `name`, `description`, `source`, `legacy_ids` | Identity and reference. |
| `[medium.composition]` | The keys of `[biome.medium]` ([biomes.md](biomes.md), in µM unless stated otherwise), plus the ones below. |

Extra composition keys:

| Key | Unit | Meaning |
|---|---|---|
| `glucose` | µM | Dissolved organic carbon. |
| `glutamine` | µM | |
| `serum` | fraction | Fetal bovine serum (restriction point of animal cells). |
| `phenol_red` | mg/L | pH indicator. |
| `carbonate_pka` | — | Apparent pK′ of CO₂/HCO₃⁻. |
| `air_co2` | fraction | |

Temperature, CO₂ and light are not part of the medium but of the plate's environment.

Factory media: `dmem_high_glucose`, `distilled_water`, `saline_0_9`, `osterhout_gause` (Gause's
saline), `wc_guillard` (WC medium for algae), `mueller_hinton` (MIC), `seminal_plasma`,
`hos_solution` (hypo-osmotic test), `htf_hsa` (capacitating medium).
