# Body plans: `body_plans/<id>.toml`

A `[[body_plan]]` lists which structures a type of cell has and how large they really are. The
semantic zoom and the inspector walk through it ([rendering](../mechanics/rendering-and-optics.md)).
Nothing is added because it "looks good": every structure must exist in the real organism. Parser:
`bio/body_plan.cpp`.

| Key | Meaning |
|---|---|
| `id`, `name` | Identity. |
| `morphology` | `eukaryote`, `rod`, `vibrio`, `tailed_phage`, `icosahedral`, `helical`. |
| `structures` | Array of structures (below). |

Each structure:

| Key | Meaning |
|---|---|
| `kind` | `plasma_membrane`, `outer_membrane`, `peptidoglycan`, `cell_wall`, `capsule`, `nucleoid`, `ribosomes`, `flagellum`, `pili`, `nucleus`, `nucleolus`, `mitochondrion`, `hydrogenosome`, `chloroplast`, `pyrenoid`, `eyespot`, `endoplasmic_reticulum`, `golgi`, `contractile_vacuole`, `food_vacuole`, `lysosome`, `storage_granule`, `acrosome`, `capsid`, `viral_genome`, `viral_envelope`, `tail`, `baseplate`, `tail_fibers`. |
| `source` | `simulated` (it has state in the simulation) or `declared` (it exists but is not simulated: drawn only in the model view, unless `brightfield`). |
| `arrangement` | `envelope`, `interior`, `polar`, `peritrichous`, `perinuclear`, `anterior`. |
| `length`, `width` [µm] | 0: given by the simulation. |
| `count` | Per cell. |
| `per_um3` | What is distributed by volume, such as ribosomes. |
| `brightfield` | Visible without staining. |
| `name` | Name in the inspector. |

Declared structures are placed with a hash of the stable id (`Bacteria::id`), never at random, so
the same being always looks the same.

Excerpt of `body_plans/ciliate.toml`:

```toml
[[body_plan]]
id = "ciliate"
name = "ciliado"
morphology = "eukaryote"
structures = [
    { kind = "plasma_membrane", source = "simulated", arrangement = "envelope", brightfield = true, name = "pelicula" },
    { kind = "nucleus", source = "simulated", arrangement = "interior", count = 1, brightfield = true, name = "macronucleo" },
    { kind = "nucleolus", source = "declared", arrangement = "interior", length = 1.5, width = 1.5, count = 1, brightfield = false, name = "nucleolos" },
    { kind = "contractile_vacuole", source = "simulated", arrangement = "interior", count = 1, brightfield = true, name = "vacuola contractil" },
]
```
