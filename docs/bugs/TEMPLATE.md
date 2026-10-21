# <Short title in the present tense: what goes wrong>

<!--
Copy this file to docs/bugs/<slug>.md (lowercase, words separated by dashes, the slug is the ID)
and add a line to docs/bugs/README.md. Keep every section; write "Unknown" or "None" when there
is nothing to say. When the bug is fixed, set Status to `fixed`, fill "Fixed in" and move its line
in the index to "Fixed"; the file stays as a record.
-->

| Field | Value |
|---|---|
| **ID** | `<slug>` (same as the file name) |
| **Type** | `bug` (the code does not do what it is meant to) · `model-limitation` (the model lacks a real mechanism, so a result differs from the biology) · `validation-gap` (a published result is not reproduced or not tested) |
| **Status** | `open` · `investigating` · `needs-reproduction` · `fixed` · `wont-fix` |
| **Severity** | `critical` (crash, NaN, corrupt data, breaks reproducibility) · `major` (wrong scientific result or blocked feature) · `minor` (wrong in a corner case) · `cosmetic` (image or text only) |
| **Area** | Link to the mechanics document, e.g. [`soft-bodies`](../mechanics/soft-bodies.md) |
| **Affects** | Beings, plates, lessons, tests or tools involved |
| **Code** | Files or functions involved (`src/...`) |
| **Reported** | DD/MM/YYYY |
| **Last verified** | DD/MM/YYYY, commit `<short hash>` |
| **Fixed in** | Commit `<short hash>` (only when fixed) |

## Summary

One or two sentences: what happens and why it matters.

## Steps to reproduce

1. Exact commands, plate, seed and parameters (`bio_bench --biome pond --seed 66 ...`, a scene
   file, or the test name: `bio_tests -tc="..."`).
2. What to look at.

## Expected behaviour

What the code should do or, for a model limitation, what the real organism does, with its source.

## Actual behaviour

What happens instead, with numbers when possible.

## Evidence

Test output, `bio_bench` lines, checksums, screenshots in `docs/img/bugs/`.

## Cause

Known or suspected cause. Say clearly which one it is ("confirmed by ..." / "suspected").

## Proposed fix

Options with their cost and their effect on other results. Per `AGENTS.md`: do not tune
parameters to make a test pass; find the real mechanism.

## Workaround

How to avoid it today, if possible.

## References

Literature, related bugs, commits, roadmap items.

## History

- DD/MM/YYYY: reported (by whom or which test).
