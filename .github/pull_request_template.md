<!-- Thank you for contributing. Read CONTRIBUTING.md and AGENTS.md first.
     One logical change per pull request. Delete the sections that do not apply. -->

## What changes and why

<!-- What the change does and the problem or biology behind it. Link the issue or the
     docs/bugs/ file it closes (e.g. "Closes #12", "Fixes docs/bugs/drop-bacteria-collapse.md"). -->

## Sources

<!-- Every measured value or mechanism needs its published source (author, year, journal), in
     [physiology.<id>.*] or in a comment. Estimates are marked as such and explained.
     Verify citations yourself: AI assistants invent them. -->

- 

## How it was tested

- [ ] `tools/ci.sh` passes (build without warnings, translations, doc checks, fast tests, short soak)
- [ ] `tools/ci.sh --todo` passes (needed when simulation code changes: long and validation suites)
- [ ] New or changed behaviour has a test (`rapidos`, `largos` or `validacion`)
- [ ] Optimisation only: `microcosm_bench` checksums match old vs new, built with `-DBIO_STRICT_FP=ON`
- [ ] Tried in the app (describe what you looked at; screenshots or a scene are welcome)

<!-- Results, numbers, comparison with the literature: -->

## Checklist

- [ ] Only real species and mechanisms; simplifications are commented with what the real organism does
- [ ] Determinism kept: randomness only from `Rng` (new stream for a new mechanism), every change to
      the world goes through an intervention, no allocations in hot loops
- [ ] Per-species parameters live in data (TOML), not in code
- [ ] Documentation updated in this PR: `docs/mechanics/`, the being's file
      (`tools/gen_being_docs.py`), `docs/content/`, `docs/validation/` or `docs/bugs/` as needed
- [ ] New UI text goes through `ui::tr` and has its English translation in `assets/lang/en.txt`
- [ ] No new library (or it was approved in an issue and is GPLv3-compatible)
- [ ] I understand and stand behind this change (AI assistance is fine; mentioning it is optional)
