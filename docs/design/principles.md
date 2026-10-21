# Design principles

The design decisions that still hold. They come from the original gameplay document, whose
"game" choices (roguelite run, victory by biomass, encyclopedia) were discarded on 15/10/2026 and
17/10/2026, when Open Microcosm became a laboratory. What was discarded is listed in the
[roadmap](../roadmap.md#3-discarded-).

## What Open Microcosm is

A **sandbox laboratory**: there is no run, score or victory. The user prepares a plate, observes it
and intervenes. Every behaviour emerges from a physical particle simulation, not from animations.
The same core serves research (reproducible, measurable, headless runs) and teaching (the
microscope image, inspector cards, guided lessons).

## Decisions

| Topic | Decision | Notes |
|---|---|---|
| Realism | **Strict realism.** Only real species and mechanisms. Time scales are compressed but keep the proportions between processes and species. | See "Guiding principle" in `AGENTS.md`. |
| Control | **Physical locomotion by cell type.** Flagellum gives impulses, cilia glide continuously, amoebae extend pseudopods by click and drag. Low Reynolds number: no inertia. | Controlling a being is optional (Control tool). |
| Feeding | **Physical phagocytosis.** Eating means wrapping the prey with the deformable membrane until a vacuole closes. | [Feeding and phagocytosis](../mechanics/feeding-and-phagocytosis.md). |
| Representation | **Hybrid fluid + soft body.** Water and chemistry are continuous grids; cells are particle soft bodies; bacteria and detritus are simple particles. | [Soft bodies](../mechanics/soft-bodies.md), [fluid](../mechanics/fluid-and-dish.md). |
| Level of detail | Near cells in full detail, far ones simplified, off-screen ones statistical. | [Performance](../mechanics/performance-and-gpu.md). |
| Visuals | Soft optical membrane edge with halo, visible cyclosis, Brownian jitter, lysis that disperses the content, digestive vacuoles that change colour. | [Rendering](../mechanics/rendering-and-optics.md). |
| Visual style | **Bright field** by default. Phase contrast, dark field and fluorescence are planned as interchangeable lenses. | Roadmap phase C. |
| Focal plane | Discarded: everything happens in one plane. | |
| Metabolism | Simplified but real: respiration, fermentation, photosynthesis, limited by nutrients. | [Metabolism](../mechanics/metabolism-and-nutrients.md). |
| Mutualism and parasitism | Nitrogen-fixing cyanobacteria, lichen, phages, *Bdellovibrio*, chytrids, quorum sensing. | [Ecosystem](../mechanics/ecosystem-and-behaviour.md). |
| Reproduction | Each kind of life reproduces its own way (fission, conjugation, budding, cysts, colonies, autospores). The controlled being pays the highest division cost. | [Life cycle](../mechanics/life-cycle-and-division.md). |
| Organelles | Physical organelles that take up space; the genome mutates on mitosis and recombines on conjugation. | [Genome](../mechanics/genome-and-organelles.md). |
| Variety | Procedural drops by seed, real biomes (puddle, pond, moss, termite gut, hydrothermal spring, sea ice) and prepared plates. | [Biomes](../content/biomes.md). |
| Multicellular animals | Out of scope until their simulation is realistic (rotifers, *Hydra*, copepod nauplii were added and removed). | |

## Real-life facts that shaped the design

1. At this scale there is no inertia (low Reynolds number).
2. Sizes vary enormously: bacterium ~1 µm, *Paramecium* ~200 µm.
3. Times vary enormously: a bacterium divides in ~20 min, a *Paramecium* about once a day. One
   constant (`units::kBioTimeScale`) compresses all of biology alike, see
   [time scales](../mechanics/time-and-units.md).
