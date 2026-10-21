# Biomes: `biomes/<id>.toml`

A `[[biome]]` is a real environment with its chemistry, temperature and cast of beings. Each drop
plate comes from a biome with variations by seed (`makeDrop`, `game/biome`). The medium is read with
`game/medium_toml`, the same reader as the culture media.

| Key | Meaning |
|---|---|
| `id`, `name`, `description`, `legacy_ids` | Identity. |
| `bacteria`, `detritus` | Individuals in the reference drop (2048 × 1024 µm); scaled by area on other plates. |
| `nitrogen_fixing_filaments`, `oscillatoria_filaments` | Cyanobacterial filaments (with heterocysts / gliding). |
| `cyanobacteria_temperature` [°C] | Thermal range of the cyanobacteria. |
| `saprolegnia`, `chytrid_clusters`, `allomyces`, `lichens`, `physarum` | Fungi and moulds at the start. |
| `patchiness` | Initial patches of organic matter and detritus (0–1). |
| `base_weight` | Multiplier of the initial population of every being. |
| `weights` | Inline table: multiplier per being id. An `obligate_symbiont` appears only where the biome names it. |

## `[biome.medium]`

Chemistry and environment; missing keys keep the general freshwater value.

| Key | Unit | Meaning |
|---|---|---|
| `temperature` | °C | |
| `salinity` | PSU | |
| `light` | 0–1 | Lamp intensity. |
| `o2_exchange` | 1/s | Air–water exchange. |
| `anoxic` | bool | |
| `alkalinity` | µM | Bicarbonate. |
| `phosphate`, `nitrogen`, `silicate`, `iron`, `vitamin_b12` | µM | Dissolved nutrients. |
| `b12_source` | µM/h | B₁₂ release by bacteria. |
| `b12_decay` | 1/h | |
| `frustule_dissolution` | 1/h | |
| `phages` | virions/µm³ | |
| `phage_inactivation` | 1/h | |
| `toxin_decay` | 1/h | Microcystin. |
| `ahl_half_life` | h | Quorum-sensing autoinducer. |
| `organic_inflow` | µM/biological h | |
| `decomposition` | 1/biological s | |
| `detritus_inflow` | 1/s | |
| `nutrient_uptake` | 1/s | |
| `dust` | 1/s | |
| `osmolytes` | mOsm/L | Non-salt osmolytes (sucrose). |

## `[biome.sediment]`

Particles per reference drop, by mineral id ([minerals.md](minerals.md)), plus `grains` (sand: fixed
obstacles) and `grain_radius` [µm] (default [50, 120]).

## Factory biomes

| Id | Environment |
|---|---|
| `puddle` | Rain water over mud and leaves: warm, bright and very rich. Almost everything lives here. |
| `pond` | Still, temperate water with aquatic plants: *Volvox*, *Spirogyra* and *Stentor*. |
| `moss` | The water film between moss leaflets: cold, shaded and acidic. Testate amoebae and *Naegleria*. |
| `termite_gut` | Anoxic and dark, full of wood splinters. *Trichonympha* digests cellulose for the termite. |
| `hot_spring` | Water at 42 °C: almost no eukaryote tolerates it. Thermophilic cyanobacteria and *Naegleria*. |
| `sea_ice` | Brine channels inside the ice: −2 °C and twice the salt of the sea. Diatoms. |

Excerpt of `biomes/pond.toml`:

```toml
[[biome]]
id = "pond"
legacy_ids = ["estanque"]
name = "Estanque"
bacteria = 30000
detritus = 60
nitrogen_fixing_filaments = 4
cyanobacteria_temperature = [10.0, 35.0]
patchiness = 0.3
base_weight = 0.15
weights = { stentor = 0.22, spirogyra = 0.22, volvox = 0.3, candida = 0.04, naegleria = 0.0 }

[biome.medium]
temperature = 17.0
phosphate = 20.0
nitrogen = 300.0
silicate = 150.0
iron = 0.5
vitamin_b12 = 0.001
organic_inflow = 60.0

[biome.sediment]
clay = 160
quartz = 50
frustule = 30
grains = 2
```
