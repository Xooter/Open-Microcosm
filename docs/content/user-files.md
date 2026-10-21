# User files

Files that are not part of a content pack: saved plates, scenes, cultures, programs, experiments and
the user data folder.

## Plates and scenes (`.plate`)

`PlateSpec` (`game/plate_spec`) is the source of truth for what is preconfigured: biome and seed,
content (`DropContent`), environment, nutrients, sediment, populations and geometry.
`parsePlate`/`serializePlate` write versioned `key=value` text (`plate_version`); missing keys keep
the biome's value. Floats are written with round-trip precision.

A **scene** is a `.plate` plus `event=step type …` lines (interventions) and `nota=` lines (notes).
Loading it and creating the plate replays the run ([reproducibility](../mechanics/reproducibility.md)).

## Culture scene (`.culture`, `<data>/openmicrocosm/cultures/`)

`key=value` lines: `culture_version`, `name`, `vessel`, `medium`, `temperature`, `light`, `seed`,
`gas_transfer` (optional: cm/h, overrides the vessel's), `program=` and `event=step op value [key]`.
Operations: `inoculate`, `bacteria`, `medium`, `dilution` (also the thinning), `concentration`,
`phages`, `sample`, `withdraw`, `receive`, `inoculate_density`, `chemostat`, `inflow_medium`,
`reagent`, `gas_transfer`.

## Program (`program=` in plates and cultures)

`target shape start end from to period cycle amount`.

- Shape: `ramp`, `cycle`, `sine`, `event`.
- Target: `temperature`, `light`, `solute:<solute>`, `parameter:<key>` or an action (`bacteria`,
  `medium` (culture only), `dilution` (in the culture and, as thinning, on the plate),
  `add:<solute>`, `inoculate:<id>`).
- Solutes: `o2`, `co2`, `phosphate`, `nitrogen`, `silicate`, `iron`, `b12`, `organic`.

See [programs](../mechanics/programs.md).

## Experiment (`<data>/openmicrocosm/experiments/*.toml`)

- `[experiment]`: `name`, `kind` (`culture` or `plate`), `replicates`, `seed`, `hours`, `sample_h`,
  `threads`.
- `[culture]` (`vessel`, `medium`, `temperature`, `light`, `program`, `events`) or `[plate]`
  (`scene`).
- `[[factor]]`: `key`, `values`.

See [batch experiments](../mechanics/batch-experiments.md).

## Stains (`.stain`)

User stains in `<data>/openmicrocosm/stains/*.stain`: versioned `key=value` text (`stain_version`). See
[stains](../mechanics/stains.md).

## User data folder

`<data>` is the platform's data folder (`userDataDir`, `core/paths`). Inside `<data>/openmicrocosm/`:
`packs/`, `plates/`, `cultures/`, `experiments/`, `stains/`, `photos/`, `exports/`,
`autosave.plate`, `layout.ini` and `options.txt` (a `progress.txt` left by older versions is
no longer read). The folder of the project's
previous name (`<data>/biogame/`) is renamed to `openmicrocosm/` the first time. Folders and files
with the old Spanish names (`paquetes/`, `cultivos/`, `experimentos/`, `fotos/`, `autoguardado.plate`) are renamed
automatically the first time, and the saved panel layout is migrated to the new ids.

Data saved with the old (Spanish) names is still read (`game/legacy_names`, `legacy_ids`) and saved
again with the new ones. An old pack is migrated with
`tools/migrate_content.py <old pack> <new folder>`; the names are in `tools/content_names.py`.
