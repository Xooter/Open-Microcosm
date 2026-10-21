# Cyanobacteria

Filamentous cyanobacteria are bacteria, so they live in the environmental tier as **chains of
particles**, not soft bodies (`bio/cyanobacteria`). Each bead is a cell (~5 µm).

## *Anabaena* / *Dolichospermum*-type filaments

- Photosynthesis with the local light and phosphorus; when a cell doubles its mass it divides
  inside the chain (intercalary growth).
- About every 10 cells one differentiates into a **heterocyst** (thick wall, does not divide; fixes
  N₂ with nitrogenase). O₂ inhibits nitrogenase even through the thick wall, so heterocysts fix more
  where neighbouring bacteria consume O₂ (mutualism). Fixed N is released as ammonium: it is the only
  process that changes the total nitrogen (`World::totalNitrogen()`).
- **Akinetes**: large, thick-walled resting cells formed under stress, like heterocysts; they
  survive the filament.
- Long filaments fragment (hormogonia) and glide slowly with reversals. Almost nothing eats a long
  filament: that is why they form blooms.
- **Death** (cyanophages, senescence): the dead cell cuts the filament (necridium) and releases
  **microcystin** (~0.1–1 % of dry weight), a toxin harmful to consumers (`toxin_` field, decaying
  with `toxin_decay`).

## *Oscillatoria*

Trichome without heterocysts (does not fix N₂), with short, wide cells; it glides faster, rotating
about its axis, so the advancing tip oscillates from side to side.

## In the biomes

`nitrogen_fixing_filaments`, `oscillatoria_filaments` and `cyanobacteria_temperature` (thermal range)
in each biome ([biomes](../content/biomes.md)); the hot spring has thermophilic cyanobacteria.

## Code

`src/bio/cyanobacteria.*`, `src/game/world_chem.cpp`.

## Tests

`tests/test_ecosystem.cpp`, `tests/test_algae.cpp`.

## Limitations

- Generic filaments, not named species with their own data (*Microcystis*, *Dolichospermum*,
  *Nostoc*: see [candidates](../design/candidates.md#18-cyanobacteria)).
- No gas vesicles or buoyancy (no vertical axis); no colonies with mucilage.
- Stains do not reach them: [bug](../bugs/stains-missing-targets.md).
