# Lessons: `lessons/<id>.toml`

A `[[lesson]]` is a protocol with steps ([lessons and protocols](../mechanics/lessons-glossary-notes.md)).
There is no score: the report is marked by the teacher. Engine: `game/lesson`.

| Key | Meaning |
|---|---|
| `id`, `title`, `level`, `goal`, `source`, `legacy_ids` | Identity and reference. |
| `plate` | Prepared plate id to set up, **or** |
| `biome` + `seed` + `content` | A drop (`content`: 0 complete, 1 lifeless, 2 empty), **or** |
| `culture` | The text of a culture scene. |
| `setup` | Interventions at the start, as in a scene (`"step type active index x y v0..v5 [key]"`). |
| `controls` | Keys of the parameter registry the student sees. |
| `[[lesson.locked]]` | `key`, `reason`: parameters fixed by the protocol (they can be unlocked; it is recorded and `runJson` marks `fuera_de_protocolo`). |
| `[[lesson.actions]]` | `text`, `event`: a named action (a plate intervention or a culture operation, `"op value [key]"`). |
| `[[lesson.step]]` | `text`; optional `goal`, `question`, `prediction` (below). |
| `solution` | Interventions or operations with their step that meet every goal. |
| `solution_hours` | How long the solution runs. |

Step fields:

- `goal` = {`kind`, `key`, `op`, `value`}: a named condition.
- `question` = {`text`, `options`, `correct`, `explanation`}: the explanation is shown on answering.
- `prediction` = {`text`, `quantity` = {`kind`, `key`}, `unit`}.

Condition kinds (`kind`): `time_h`, `population`, `bacteria`, `concentration`, `ph`, `plasmolysis`,
`hemolysis`, `near_center`, `fraction`. A new condition is added in `lessonQuantity`
(`game/lesson.cpp`); the data choose which one to use.

`tests/test_lessons.cpp` runs every lesson's `solution`: if a model change breaks a lesson, the test
reports it.

Factory lessons: `jennings_co2`, `onion_plasmolysis`, `osmotic_fragility`, `gause_competition`,
`tilman_chemostat`, `spermiogram`.
