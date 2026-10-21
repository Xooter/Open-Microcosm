# Open Microcosm documentation

All documentation is in English. Start with the project [`README.md`](../README.md) for what Open Microcosm
is and how to build it, and with [`AGENTS.md`](../AGENTS.md) for the architecture, the rules and
the documentation policy that every contribution follows.

## Map

| Folder / file | What it holds |
|---|---|
| [`roadmap.md`](roadmap.md) | Where the project is going: phases, "done" criteria and decisions. |
| [`mechanics/`](mechanics/README.md) | One file per important mechanism of the simulator (physics, biology, laboratory tools). |
| [`content/`](content/README.md) | Content packs: the TOML format of each content type. |
| [`content/beings/`](content/beings/README.md) | One file per being in the catalogue: description, literature, what is not implemented, unique mechanics. |
| [`validation/`](validation/README.md) | Comparison against published results and the state of each validation test. |
| [`bugs/`](bugs/README.md) | One file per known bug or model limitation, following [`bugs/TEMPLATE.md`](bugs/TEMPLATE.md). |
| [`design/`](design/principles.md) | Design principles, the [idea backlog](design/ideas.md) and the [candidate beings and experiments](design/candidates.md). |
| `img/` | Screenshots and videos used by the README. |

## Mechanics

| Area | Documents |
|---|---|
| Physical core | [Time and units](mechanics/time-and-units.md) · [Fluid and dish](mechanics/fluid-and-dish.md) · [Chemistry and light](mechanics/chemistry-and-light.md) · [Soft bodies](mechanics/soft-bodies.md) · [Shape changes](mechanics/shape-changes.md) · [Performance and GPU](mechanics/performance-and-gpu.md) |
| Cell biology | [Metabolism and nutrients](mechanics/metabolism-and-nutrients.md) · [Life cycle and division](mechanics/life-cycle-and-division.md) · [Genome and organelles](mechanics/genome-and-organelles.md) · [Locomotion](mechanics/locomotion.md) · [Feeding and phagocytosis](mechanics/feeding-and-phagocytosis.md) |
| Behaviour and ecology | [Ecosystem and behaviour](mechanics/ecosystem-and-behaviour.md) · [Behavioural responses](mechanics/behavioural-responses.md) · [Chemical signals](mechanics/chemical-signals.md) · [Stalks and colonies](mechanics/stalks-and-colonies.md) · [Symbioses](mechanics/symbioses.md) |
| Other organisms | [Bacteria and phages](mechanics/bacteria-and-phages.md) · [Cyanobacteria](mechanics/cyanobacteria.md) · [Fungi](mechanics/fungi.md) · [*Physarum*](mechanics/physarum.md) |
| Multicellular cells | [Animal cells](mechanics/animal-cells.md) · [Prepared plates and tissues](mechanics/prepared-plates-and-tissues.md) · [Sperm and CASA](mechanics/sperm-and-casa.md) |
| Laboratory | [Well-mixed culture](mechanics/well-mixed-culture.md) · [Programs](mechanics/programs.md) · [Batch experiments](mechanics/batch-experiments.md) · [Reagents](mechanics/reagents.md) · [Stains](mechanics/stains.md) · [Lessons, glossary and notes](mechanics/lessons-glossary-notes.md) |
| Measurement and reproducibility | [Measurement](mechanics/measurement.md) · [Reproducibility](mechanics/reproducibility.md) |
| Presentation | [Rendering and optics](mechanics/rendering-and-optics.md) · [Interface](mechanics/interface.md) |

## Conventions

- Documents describe what the code **does today**. Plans go in the roadmap; known defects go in
  `bugs/`.
- Code identifiers are written as `code` and link to their file when useful
  (`src/game/world.cpp`). Units are always stated (µm, s, pmol, µM).
- Biological claims cite their source (author, year, journal). Simplifications say what the real
  organism does.
- Dates are written `DD/MM/YYYY`.
