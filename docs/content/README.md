# Content packs

Open Microcosm's content is data (`game/content`): beings, body plans, minerals, biomes, media, vessels,
prepared plates, lessons, reagents and the glossary. Machine names (folders, files, keys, ids and
named values) are in **English**. Human-readable text (`name`, `description`, `source`,
definitions, lesson steps, comments) is in Spanish today and is translated by `assets/lang`; new
text may be written in English and existing text will be migrated over time.

Documentation of each format:

| Folder | Table | Format | Per-item documentation |
|---|---|---|---|
| `beings/` | `[[being]]` + `[physiology.<id>.*]` | [beings.md](beings.md) | [beings/](beings/README.md) (one file per being) |
| `body_plans/` | `[[body_plan]]` | [body_plans.md](body_plans.md) | — |
| `minerals/` | `[[mineral]]` | [minerals.md](minerals.md) | — |
| `biomes/` | `[[biome]]` | [biomes.md](biomes.md) | — |
| `media/` | `[[medium]]` | [media.md](media.md) | — |
| `vessels/` | `[[vessel]]` | [vessels.md](vessels.md) | — |
| `plates/` | `[[plate]]` | [plates.md](plates.md) | — |
| `lessons/` | `[[lesson]]` | [lessons.md](lessons.md) | — |
| `reagents/` | `[[reagent]]` | [reagents.md](reagents.md) | — |
| `glossary/` | `[[term]]` | [glossary.md](glossary.md) | — |
| — | scenes, programs, experiments, user data | [user-files.md](user-files.md) | — |

## A pack

A pack is a folder with its manifest and one folder per type, with **one file per definition**,
named after its id:

```
assets/content/base/
  pack.toml
  beings/paramecium.toml      [[being]] and its measured physiology ([physiology.paramecium.*])
  body_plans/ciliate.toml     [[body_plan]]
  minerals/clay.toml          [[mineral]]
  biomes/pond.toml            [[biome]]
  media/wc_guillard.toml      [[medium]]
  vessels/dish_35mm.toml      [[vessel]]
  plates/onion_epidermis.toml [[plate]]
  lessons/jennings_co2.toml   [[lesson]]
  reagents/ampicillin.toml    [[reagent]]
  glossary/chemostat.toml     [[term]]
```

- **Adding something means adding a file.** Each file holds a single definition and its name must
  be its id; otherwise the pack warns.
- Folders are loaded in dependency order (`core/pack_files`, `kPackFolders`). Inside a folder, what
  `[pack.order]` lists comes first and the rest follows by name. The indices of the factory beings,
  biomes and minerals are their load order: old scenes depend on it, so **that list is never
  reordered** and new items are appended at the end.
- A `.biopack` is the same thing in a single file: the `[pack]` table and the concatenation of the
  definitions, plus `[[scene]]` (`id`, `plate`: the text of a plate).
- User packs live in `<data>/openmicrocosm/packs/` (folder or `.biopack`). They are imported and
  exported from the Content panel; `microcosm_bench --paquete` loads them headless.
- Packs are data only and bounded (8 MB, ids `[a-z0-9_]`). Each registry keeps the source TOML of
  every definition so it can be exported unchanged.
- **Renamed ids:** `legacy_ids = ["old_id"]` keeps a previous id valid (saved plates, scenes and
  experiments name it). When read, they are rewritten with the current id.
- Readers use `core/toml_reader`: errors report their location and unknown keys produce a warning
  (visible in the Content panel).

### `pack.toml`

`[pack]`: `id` (lowercase letters, digits and `_`), `name`, `version`, `author`, `license`,
`min_program` (minimum program version). `[pack.order]`: a list of ids per folder.

## Rules for new content

- Only **real** species, cell types and mechanisms, with a published source for every measured value
  (see "Guiding principle" in `AGENTS.md`). An estimate is allowed when it is marked
  (`estimate = true`) and explained.
- No code per species: if a being needs behaviour the engine lacks, add a **named mechanism** that
  the data selects (a new key or a new value of an existing key) and document it here.
- Every new being needs its document in [`beings/`](beings/README.md) (see the documentation policy
  in `AGENTS.md`). `tools/gen_being_docs.py --check` fails otherwise.
