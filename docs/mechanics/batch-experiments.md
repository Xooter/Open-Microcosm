# Batch experiments

Replicates with different seeds and swept factors, headless (`game/experiment`, `microcosm_batch`,
Experiment panel). Without replicates there are no statistics, and between machines the promised
reproducibility is statistical ([reproducibility](reproducibility.md)).

## File

An experiment is a TOML file ([user files](../content/user-files.md#experiment-dataopenmicrocosmexperimentstoml)):

```toml
[experiment]
name = "Si sweep"
kind = "culture"        # or "plate"
replicates = 5
seed = 1
hours = 240
sample_h = 6
threads = 0             # 0: one per physical core

[culture]
vessel = "erlenmeyer_250"
medium = "wc_guillard"
temperature = 20
light = 1.0
program = []
events = ["0 inoculate 100 asterionella", "0 inoculate 100 cyclotella"]

[[factor]]
key = "solute:silicate"
values = [5, 20, 80]
```

Factors: in a culture, `temperature`, `light`, `bacteria` (per mL at the start), `solute:<key>` (µM at
the start), `inoculate:<being id>` (individuals) or `reagent:<id>`; on a plate, `temperature`,
`solute:<key>` or a key of the parameter registry. Keys saved with the old Spanish names are still
read (`game/legacy_names`). Each combination of levels runs `replicates` times with seeds derived
from the experiment's seed. Time is laboratory time.

## Running

- `microcosm_batch <file> [--salida <dir>] [--hilos N] [--replicas N] [--paquete <pack>]`.
- The Experiment panel runs the same thing inside the app.
- One run per thread (OpenMP with 1 thread inside each run); registries are preloaded.

## Output

- Long format (one row per run, sample time and column).
- Summary with mean, SD and 95 % CI per factor level.
- JSON with the full provenance.

## Dose–response

A batch over a reagent's concentration (`reagent:<id>`) gives the curve from which `fitDoseResponse`
computes the IC50 and `minimumInhibitory` the MIC ([reagents](reagents.md)).

## Code

`src/game/experiment.*`, `src/game/dose_fit.*`, `bench/microcosm_batch.cpp`,
`src/ui/panels/experiment_panel.cpp`.

## Tests

`tests/test_experiment.cpp`, `tests/test_reagents.cpp`.
