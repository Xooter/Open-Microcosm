# Glossary: `glossary/<id>.toml`

A `[[term]]` is a glossary entry ([glossary and notes](../mechanics/lessons-glossary-notes.md)).
Engine: `game/glossary`.

| Key | Meaning |
|---|---|
| `id` | Stable id. |
| `term` | The term as shown. |
| `definition` | Definition, with its `source`. |
| `source` | Reference. |
| `alias` | Other forms in the text, lowercase. They are in Spanish because they are searched in the Spanish texts (`ui::glossaryChips` matches both Spanish and the translation). |
| `related` | Related term ids. |
| `lesson`, `plate` | Where to see it: the term's "animation" is the simulation, not a separate drawing. |

The factory pack has 47 terms (Reynolds number, chemotaxis, Monod, chemostat, Q10, osmosis, Gram,
CASA, capacitation…), listed in `pack.toml`.
