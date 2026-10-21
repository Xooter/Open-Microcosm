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
| [`gause-displacement-too-slow`](gause-displacement-too-slow.md) | validation-gap | minor | *P. aurelia* wins Gause's competition but displaces *P. caudatum* more slowly than Gause saw. |
| [`starved-cultures-keep-dividing`](starved-cultures-keep-dividing.md) | model-limitation | minor | Food-limited cultures plateau in biovolume but keep dividing into smaller cells. |
| [`drop-bacteria-collapse`](drop-bacteria-collapse.md) | model-limitation | major | Bacteria in a drop collapse within a few biological hours; the 30-min test is skipped. |
| [`bdellovibrio-no-prey-resistance`](bdellovibrio-no-prey-resistance.md) | model-limitation | major | *Bdellovibrio* can exterminate its prey: no phenotypic resistance. |
| [`organic-matter-after-lysis`](organic-matter-after-lysis.md) | model-limitation | minor | Organic matter reaches hundreds of mM after a lysis in a small plate. |
| [`diatom-division-geometry`](diatom-division-geometry.md) | model-limitation | minor | Long or cylindrical diatoms cannot divide; diatoms divide by a waist furrow. |
| [`stalks-and-tails-do-not-collide`](stalks-and-tails-do-not-collide.md) | model-limitation | minor | Stalks and sperm tails pass through cells. |
| [`ciliate-ring-crossed-after-division`](ciliate-ring-crossed-after-division.md) | bug (needs reproduction) | minor | Swimming ciliates sometimes have a crossed ring after dividing. |

## Fixed

Fixed issues keep their file with `Status: fixed` and the commit. Issues fixed before this folder
existed are recorded in [validation](../validation/README.md#model-problems-found-while-validating-mvp-phase).

| ID | Fixed in | Summary |
|---|---|---|
| [`stains-missing-targets`](stains-missing-targets.md) | 21/10/2026 | Hyphae, cyanobacteria and oat flakes did not respond to stains. |
| [`inoculation-ignores-region`](inoculation-ignores-region.md) | 21/10/2026 | Inoculating into a drawn region ignored its shape. |
| [`gause-aurelia-does-not-win`](gause-aurelia-does-not-win.md) | 21/10/2026 | In Gause's competition whichever *Paramecium* got ahead won: the daily thinning of his protocol was missing. |
| [`inspector-values-untranslated`](inspector-values-untranslated.md) | 21/10/2026 | Numeric values in the inspector stayed in Spanish. |
