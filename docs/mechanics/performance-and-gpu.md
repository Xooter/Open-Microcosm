# Performance and GPU

Target: **60 FPS** with the content of the current phase on mid-range desktop hardware. A change
that alters the physical result is not accepted as an optimisation: **truthfulness over speed**.

## Level of detail (LOD)

`World::setFocus` defines three levels:

| Level | Where | Treatment |
|---|---|---|
| Detail | Near the view | Full XPBD and contacts. |
| Far | Farther away | 1 of every `lodStride` XPBD sub-steps with a longer step (rates [1/s] keep their meaning). |
| Statistical | Off screen | Moved as solids, no XPBD or contacts. |

Division, phagocytosis, conjugation and held prey are always in detail. Without a focus (tests)
everything is detailed. LOD changes the trajectory: see [reproducibility](reproducibility.md).

## GPU solver

XPBD and contacts can run on the GPU (`src/gpu/gpu_substep_solver.*`, OpenGL 4.3 compute shaders in
`assets/shaders/compute/`, `solve.comp`), deterministically: each engine is deterministic on its own, but
GPU rounding differs from the CPU's, so the two engines give different trajectories.
`BIO_GPU_TIMING=1` reports each kernel's time with timestamps. If the GPU does not support compute
shaders (for example macOS, limited to OpenGL 4.1) the kernels fail to compile and the CPU engine is
used.

## Measuring

- `microcosm_bench` runs a fixed drop headless and prints ms/step, the census and a state checksum.
  Options: `--wildlife 3` (~700 cells), `--gpu`, `--biome`, `--seed`, `--steps`, `--threads`,
  `--shot`, `--stain <id>` (with `--render`: a factory stain), `--timelapse <bio_min>`, `--casa`, `--respuesta-funcional <id>`, `--paquete`,
  `--scene`. By default one thread per physical core, like the app (with SMT, 16 threads are slower
  than 8).
- Preset `lite`: times per `PROFILE_SCOPE` without Tracy; preset `profile`: Tracy 0.11.
- An optimisation that must not change results is verified by comparing checksums of the old and
  new versions built with `-DBIO_STRICT_FP=ON`: with `-ffast-math` any code change may reassociate
  operations and alter the last bits, and the dynamics amplify it.

## Rules

- No memory allocation per frame in hot loops: buffers are reused.
- Parallelise with OpenMP only when each iteration writes its own index; with crossed writes, use
  per-thread buffers and a reduction.
- Every new simulation function gets a `PROFILE_SCOPE`. Measure with Tracy before optimising.
- Contacts consider only membrane points near another body; bacteria feeding uses a grid; eaten
  items are marked and removed together at the end of the step.

## Reference numbers

- Phase 19: 551 cells in 16.1 ms per frame; `limitApproach` 8.5× faster.
- `microcosm_bench --wildlife 3`, 300 steps (19/10/2026): puddle 43.1 ms/step (875 cells), pond
  46.7 ms/step (927 cells).

## Pending

Roadmap phase I: `limitApproach` (~8 ms, serial), GPU wait (`collect`, `pairs` kernel), chemistry +
FFT, `applyMotion`, `resolvePenetrations`, `perceive`.
