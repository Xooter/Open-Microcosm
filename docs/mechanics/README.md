# Mechanics

One document per important mechanism of BioGame. Each one explains **what is simulated**, the
**real biology or physics** behind it (with sources), the **data** that parametrise it, **where the
code lives**, which **tests** cover it and its **known limitations**.

When a feature is added or changed, its document here is updated in the same commit (documentation
policy in `AGENTS.md`). A new mechanism gets a new file and a line in this index.

## Physical core

| Document | Covers |
|---|---|
| [Time and units](time-and-units.md) | µm and seconds, real-time physics, `kBioTimeScale`, dry mass, Q10. |
| [Fluid and dish](fluid-and-dish.md) | Stokes–Brinkman water with FFT, the glass well, toroidal mode. |
| [Chemistry and light](chemistry-and-light.md) | Dissolved fields, diffusion and advection, carbonate system and pH, gases, Beer–Lambert light. |
| [Soft bodies](soft-bodies.md) | Inertia-free XPBD, constraints, contacts, plastic membrane, topology. |
| [Shape changes](shape-changes.md) | Silhouettes and processes, rest shapes that change in life (contraction, metaboly, semicells, neck, somersaults). |
| [Performance and GPU](performance-and-gpu.md) | LOD, GPU solver, profiling, `bio_bench`, performance rules. |

## Cell biology

| Document | Covers |
|---|---|
| [Metabolism and nutrients](metabolism-and-nutrients.md) | Respiration, fermentation, photosynthesis, Droop quotas, Liebig, stress, single physiology. |
| [Life cycle and division](life-cycle-and-division.md) | Growth, checkpoint, mitosis, budding, cysts, death and lysis, spheroid mass. |
| [Genome and organelles](genome-and-organelles.md) | Quantitative genes, mutation, conjugation, physical organelles, defences. |
| [Locomotion](locomotion.md) | Cilia, flagella, pseudopods, crawling, gliding, helical swimming, controlling a being. |
| [Feeding and phagocytosis](feeding-and-phagocytosis.md) | Feeding modes, vacuoles, digestion, film height, phagocytic cup, mouth width. |

## Behaviour and ecology

| Document | Covers |
|---|---|
| [Ecosystem and behaviour](ecosystem-and-behaviour.md) | Species as data, perception, autopilot, predation, flight, the drop food web. |
| [Behavioural responses](behavioural-responses.md) | Phobic responses (M1), mechanoreception (M2), avoiding reaction, acid trap, phototaxis, jumps. |
| [Chemical signals](chemical-signals.md) | Released signals (M3), induced morphs (M4), cAMP relay, chemotaxis, killer toxins. |
| [Stalks and colonies](stalks-and-colonies.md) | Stalks as elastic rods (M9), branched colonies, filaments, star and sliding colonies, volvocines. |
| [Symbioses](symbioses.md) | Endosymbiotic algae, kleptoplasts, lichen, nitrogen-fixing mutualism. |

## Other organisms

| Document | Covers |
|---|---|
| [Bacteria and phages](bacteria-and-phages.md) | Bacterial particles, strains, run and tumble, biofilm, quorum sensing, phages, *Bdellovibrio*. |
| [Cyanobacteria](cyanobacteria.md) | Filaments, heterocysts and N₂ fixation, akinetes, *Oscillatoria*, microcystin. |
| [Fungi](fungi.md) | Chytrids, *Saprolegnia*, *Allomyces*, lichen mycobiont, yeasts. |
| [*Physarum*](physarum.md) | Tube network, Kirchhoff flow, Tero adaptation, slime, oat flakes, sclerotium. |

## Cells of multicellular organisms

| Document | Covers |
|---|---|
| [Animal cells](animal-cells.md) | Adhesion, crawling, cell cycle, contact inhibition, glucose/glutamine, Warburg, apoptosis, fMLP. |
| [Prepared plates and tissues](prepared-plates-and-tissues.md) | Cell culture plates, slides, plant tissues, osmosis, erythrocytes, fixed smears. |
| [Sperm and CASA](sperm-and-casa.md) | Human sperm swimming, WHO classes, CASA kinematics, vitality, HOS, capacitation, morphology. |

## Laboratory

| Document | Covers |
|---|---|
| [Well-mixed culture](well-mixed-culture.md) | Individual-based culture, encounter theory, chemostat, passages, sample drop. |
| [Programs](programs.md) | Ramps, cycles, sines and events by laboratory time. |
| [Batch experiments](batch-experiments.md) | Replicates and factors, `bio_lote`, statistics, provenance. |
| [Reagents](reagents.md) | Pharmacodynamic antibiotics, emergent MIC, signal reagents. |
| [Stains](stains.md) | Rule-based stains, Gram retention, vital and fixing stains, preview. |
| [Lessons, glossary and notes](lessons-glossary-notes.md) | Protocols with steps and tested solutions, glossary chips, anchored notes. |

## Measurement and reproducibility

| Document | Covers |
|---|---|
| [Measurement](measurement.md) | Mass budget and flux ledger, time series, growth fit, regions, limitation map, export. |
| [Reproducibility](reproducibility.md) | Parameter registry, interventions, scenes, determinism, provenance. |

## Presentation

| Document | Covers |
|---|---|
| [Rendering and optics](rendering-and-optics.md) | Bright field, microscope post-process, semantic zoom, body plans, photo and time-lapse. |
| [Interface](interface.md) | `App`, dockable panels, tools, commands, widgets, languages, accessibility, options. |
