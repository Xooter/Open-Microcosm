# Known bugs and model limitations

One file per issue, all following [`TEMPLATE.md`](TEMPLATE.md). The file name is the issue's ID.

- **bug**: the code does not do what it is meant to.
- **model-limitation**: the model lacks a real mechanism, so a result differs from the biology.
- **validation-gap**: a published result is not reproduced or not yet tested.

Reporting a new one: copy the template, fill every section (with exact steps to reproduce and the
commit where you saw it) and add a line below, in the table of its status. GitHub issues use the
same structure (`.github/ISSUE_TEMPLATE/bug_report.md`); an issue that is confirmed gets its file
here.

## Open

| ID | Type | Severity | Summary |
|---|---|---|---|
| [`gause-aurelia-does-not-win`](gause-aurelia-does-not-win.md) | validation-gap | major | In Gause's competition whichever *Paramecium* gets ahead wins, not *P. aurelia*. |
| [`drop-bacteria-collapse`](drop-bacteria-collapse.md) | model-limitation | major | Bacteria in a drop collapse within a few biological hours; the 30-min test is skipped. |
| [`bdellovibrio-no-prey-resistance`](bdellovibrio-no-prey-resistance.md) | model-limitation | major | *Bdellovibrio* can exterminate its prey: no phenotypic resistance. |
| [`organic-matter-after-lysis`](organic-matter-after-lysis.md) | model-limitation | minor | Organic matter reaches hundreds of mM after a lysis in a small plate. |
| [`diatom-division-geometry`](diatom-division-geometry.md) | model-limitation | minor | Long or cylindrical diatoms cannot divide; diatoms divide by a waist furrow. |
| [`stalks-and-tails-do-not-collide`](stalks-and-tails-do-not-collide.md) | model-limitation | minor | Stalks and sperm tails pass through cells. |
| [`inoculation-ignores-region`](inoculation-ignores-region.md) | bug | minor | Inoculating into a drawn region ignores its shape. |
| [`stains-missing-targets`](stains-missing-targets.md) | bug | minor | Hyphae, cyanobacteria and oat flakes do not respond to stains. |
| [`ciliate-ring-crossed-after-division`](ciliate-ring-crossed-after-division.md) | bug (needs reproduction) | minor | Swimming ciliates sometimes have a crossed ring after dividing. |
| [`inspector-values-untranslated`](inspector-values-untranslated.md) | bug | cosmetic | Numeric values in the inspector stay in Spanish. |

## Fixed

Fixed issues keep their file with `Status: fixed` and the commit. Issues fixed before this folder
existed are recorded in [validation](../validation/README.md#model-problems-found-while-validating-mvp-phase).

| ID | Fixed in | Summary |
|---|---|---|
| — | — | — |
