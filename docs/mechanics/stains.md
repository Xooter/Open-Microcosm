# Stains

Laboratory stains as **data, not code per dye** (`game/stain`, `render/cell_renderer`, Stains
panel). A stain (`StainSpec`) is a list of rules {target, response, colours, intensity}.

## Targets

Real compounds or structures (`StainTarget`): nucleus, nucleoid, starch, glycogen, paramylon, acidic
compartments with their pH, gram-positive and gram-negative walls, chlorophyll, cytoplasm, bacteria,
damaged membrane…

## Responses

| Response | Behaviour |
|---|---|
| `Fixed` | The target takes the colour. |
| `PhIndicator` | Turns with the pH (neutral red, Congo red in digestive vacuoles). |
| `GramRetention` | Loses the dye with decolouriser time: the thin wall in ~3 s, the thick one in ~60 s; over-decolourising turns a gram-positive pink. |
| `DamagedMembrane` | Exclusion dyes (eosin, trypan blue) do not enter a living cell and do enter a dead or late-apoptotic one. |

## Factory stains

Gram, Lugol, DAPI (fluorescence, dark background), neutral red, methylene blue, Congo red, Giemsa
(blood: violet nuclei, pink haemoglobin), aceto-orcein (chromosomes), eosin–nigrosin (sperm
vitality) and trypan blue (cultures).

## Real biochemistry in the simulation

`speciesChemistry(id)` gives the storage (`Storage`: starch, glycogen, paramylon, chrysolaminarin)
and the wall (`CellWall`) of each species: only true starch (chlorophytes) turns blue-black with
Lugol and *Euglena* (paramylon) does not react. `Bacteria::strain` 2 is *Bacillus subtilis*
(gram-positive); the others are gram-negative.

## Render-only

The render asks `evaluateStain` for each target it draws; stains do not touch `World` or `Rng`.
`vital = false` marks stains that really fix or kill (Gram, Lugol, DAPI): for those the panel offers
**Fix the sample** (`World::fix`), which kills everything without lysis and stops `step`.

## Preview

`ui::StainPreview` draws a fixed test specimen (a small `World`) with the **same** `Renderer` as the
plate (`Renderer::drawWorldTo` into a texture), unstained and stained, in cell, detail and bacteria
views.

## Structures that are not soft-bodied cells

Hyphae, cyanobacterial filaments and oat flakes take their colour from the stain too
(`stainedColor` in `render/stain_color.hpp`): in bright field the strongest dye among their
targets, in fluorescence the sum of their emissions (what has no target is not seen).

| Structure | Targets | Real basis |
|---|---|---|
| Cyanobacteria (*Anabaena*, *Oscillatoria*) | gram-negative wall, glycogen, nucleoid, chlorophyll | Gram-negative envelope; "cyanophycean starch" is an α-glucan of the glycogen type (reddish brown with Lugol); chlorophyll *a* fluoresces red. |
| Hyphae of true fungi (chytrid, *Allomyces*, mycobiont) | cytoplasm, glycogen | Fungi store glycogen (`FungusInfo::glycogen`). |
| Hyphae of *Saprolegnia* | cytoplasm | An oomycete: it stores mycolaminarin, a β-glucan that iodine does not stain, and its cellulose wall does not turn blue with Lugol alone. |
| Oat flakes | starch, cytoplasm | Endosperm starch: blue-black with Lugol. |

`microcosm_bench --render --stain <id> --shot <png>` draws a headless frame with a factory stain.

## Adding a target

A value of `StainTarget`, its evaluation in the render and, if needed, a datum in
`SpeciesChemistry`.

## User stains

Saved in `<data>/openmicrocosm/stains/*.stain` (versioned `key=value` text, `stain_version`).

## Code

`src/game/stain.*`, `src/render/stain_color.hpp`, `src/render/renderer.cpp`, `src/render/cell_renderer.cpp`, `src/render/tissue_renderer.cpp`,
`src/ui/panels/stain_panel.cpp`.

## Tests

`tests/test_stain.cpp`.

## Limitations

- Sporangia, resting sporangia, zoospores and the *Physarum* plasmodium keep their natural colour
  (and are not drawn in fluorescence).
- India ink for capsules and stains on the live plate with real-time exposure are pending (roadmap C).
