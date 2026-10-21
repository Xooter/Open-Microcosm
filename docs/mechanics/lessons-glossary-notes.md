# Lessons, glossary and notes

Teaching tools built on the same simulation.

## Lessons and protocols

A lesson is a **protocol with steps**, as data (`lessons/<id>.toml`, `[[lesson]]` in packs;
[format](../content/lessons.md)), run from the Lesson panel (`game/lesson`):

- what is prepared (a prepared plate, a drop of a biome or a culture scene);
- the controls the student sees (registry keys);
- the parameters the protocol fixes, with their reason: they appear locked (`App::protocolLocks`,
  `parameterControl`). They can be unlocked; it is recorded and `runJson` marks
  `fuera_de_protocolo`;
- named actions (a plate intervention or a culture operation);
- steps with an observable goal, a question or a prediction.

Goals are **named conditions** (`time_h`, `population`, `bacteria`, `concentration`, `ph`,
`plasmolysis`, `hemolysis`, `near_center`, `fraction`): the code provides the mechanism and the data
choose which. A new condition is added in `lessonQuantity`.

There is **no score**: questions show their explanation when answered and the report (Markdown) is
marked by the teacher. Predictions are compared with what was measured.

Each lesson carries its **solution** (interventions or operations with their step) and
`tests/test_lessons.cpp` runs it: if a model change breaks a lesson, the test reports it.

Factory lessons: Jennings (CO₂), onion plasmolysis, osmotic fragility, Gause competition, Tilman
chemostat and the semen analysis.

## Glossary

Terms as data (`glossary/<id>.toml`, `[[term]]`; [format](../content/glossary.md)): definition with a
source, `alias` (forms in the text, lowercase), related terms and where to see it (`lesson`,
`plate`): the term's "animation" is the simulation, not a separate drawing. `ui::glossaryChips` shows
as buttons the terms that appear in a text (the inspector card, lesson steps), in Spanish and in the
translation.

## Notes

User notes are anchored to a being (`Picked::Kind` + entity or stable id: the same when the scene is
replayed), to a place or to an instant. They live in `Run::notes()`, go into the scene (`nota=`) and
into the export, and **are not interventions**: they do not change the world or the fingerprint.

## Code

`src/game/lesson.*`, `src/game/glossary.*`, `src/game/notes.*`, `src/ui/panels/lesson_panel.cpp`,
`src/ui/panels/glossary_panel.cpp`, `src/ui/panels/notes_panel.cpp`.

## Tests

`tests/test_lessons.cpp`, `tests/test_glossary.cpp`, `tests/test_scene.cpp`.
