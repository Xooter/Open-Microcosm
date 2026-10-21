# Reproducibility

A run can be repeated: **bit for bit** with the same binary, machine, engine and options;
**statistically** (replicates) between machines, because the compiler, `libm` and the GPU change.

## Determinism by seed

All randomness comes from `Rng` (`core/rng.hpp`), computed from (seed, stream, index, step), so it
does not depend on the thread that runs it. No `rand()` or `std::random_device` inside the
simulation. Parallel loops only write to their own index. The determinism test compares runs with 1
and 4 threads. A new random mechanism uses a new stream (so existing runs do not change).

## Parameter registry

`parameters()` (`game/parameters`): every adjustable parameter of the running plate is in **one
table** (stable key, unit, range, accessor). Panels draw their controls from it
(`ui/param_controls`) and `runJson` exports it whole. A new parameter is added there, not in a panel.

## Interventions

**Every action that changes the world** (tools, parameters, environment, solutes, the controlled
being, fixing, LOD focus, engine) goes through `App::intervene` / `interveneState` →
`Run::interventions()`: it is applied between steps and recorded with its step. **Never call `World`
directly from the interface.** What is held down (ink, dropper) is a brush that
`InterventionLog::beforeStep` applies **once per step**, not per frame: the result cannot depend on
the FPS.

## Scenes

A scene = `.plate` + `event=step type …` lines. Loading it and creating the plate replays the run
(`InterventionLog::schedule`); the export includes it. Plate panel > *Save scene*. Notes (`nota=`) follow the
events and are not interventions. A scene names everything by text id and lists its packs (`pack=`);
`missing` reports what it names and is not loaded. Floats in text files are written with round-trip
precision (`max_digits10` or `std::to_chars`): with 6 digits a plate read back is no longer the same.

## Options that change the trajectory

| Option | Changes the trajectory? |
|---|---|
| Cell physics on the GPU | **Yes**: GPU rounding differs from the CPU's (each engine is deterministic on its own). |
| LOD and far sub-steps | **Yes**: what is outside the view is solved with fewer sub-steps (biology does not change). |
| Threads | No. |
| Steps per frame | No: only how much is recovered per frame if the simulation falls behind. |
| Quality, render scale, optics, detail, VSync, FPS cap, MSAA | No: image only. |

## Provenance

`runJson` records the version, commit, compiler flags, engine, GPU, CPU, pack fingerprints and the
fingerprint of the interventions. A run that unlocked a lesson parameter is marked
`fuera_de_protocolo`.

## Autosave

The session is autosaved (`autosave.plate`); after a crash the app offers to recover it.

## Code

`src/core/rng.hpp`, `src/game/parameters.*`, `src/game/intervention.*`, `src/game/scene.*`,
`src/game/run.*`, `src/app/app.cpp` (`intervene`).

## Tests

`tests/test_scene.cpp`, `tests/test_run.cpp`, `tests/test_parameters.cpp`, `tests/test_plate_spec.cpp`.
