# Contributing to Open Microcosm

Thank you for wanting to help. Open Microcosm is a GPL-3.0-or-later research and teaching tool, and every
kind of contribution is welcome: organisms, mechanisms, lessons, validation against published
experiments, translations, bug reports, documentation and performance work.

Before you start, read [`AGENTS.md`](AGENTS.md): it describes the architecture, the rules every
change follows and the documentation policy. It is written for humans and for AI agents alike.

## The one rule: strict realism

Only **real** species and mechanisms. Every measured value (size, rate, half-saturation, speed…) is
loaded with its published source in `[physiology.<id>.*]` or in a comment; an estimate is marked as
such and explained. A test that does not reproduce the literature is **not tuned until it passes**:
the discrepancy is documented (in [`docs/validation/`](docs/validation/README.md) and, if it is a
defect, in [`docs/bugs/`](docs/bugs/README.md)) until the real mechanism is found.

## Ways to contribute

| You want to… | Start here |
|---|---|
| Report a bug or a result that differs from biology | Open an issue with the bug template; it mirrors [`docs/bugs/TEMPLATE.md`](docs/bugs/TEMPLATE.md). Attach the scene (Plate panel > *Save scene*) and the export (Ctrl+E) if you can. |
| Add an organism | A TOML file in `assets/content/base/beings/` ([format](docs/content/beings.md)), its document (`tools/gen_being_docs.py`, then complete it by hand) and a reproduction test in `tests/test_candidates.cpp`. Check [candidates](docs/design/candidates.md) for researched beings. |
| Add a mechanism | A **named mechanism** that the data select (no per-species code), a document in [`docs/mechanics/`](docs/mechanics/README.md) and tests. Plan medium and large features first. |
| Add a lesson, plate, medium or reagent | A TOML file in its folder ([formats](docs/content/README.md)); lessons carry a tested `solution`. |
| Validate against a published experiment | A test labelled `validacion` and its entry in [`docs/validation/`](docs/validation/README.md). |
| Translate the interface | `assets/lang/<code>.txt` (`key<TAB>translation`), checked with `tools/claves_tr.py --check`. |
| Distribute your own content | A content pack (folder or `.biopack`) in `<data>/openmicrocosm/packs/`; no C++ needed. |

## Workflow

1. Fork and create a branch from `main`.
2. Build and test:

   ```sh
   cmake --preset release && cmake --build --preset release
   tools/ci.sh            # build without warnings, translations, docs checks, fast tests, short soak
   tools/ci.sh --todo     # also the long and validation suites (when you touch simulation code)
   ```

3. Keep the code style: C++20, `.clang-format` (LLVM, 4 spaces, 100 columns), the naming rules of
   `AGENTS.md`, no warnings with `-Wall -Wextra -Wpedantic`.
4. Respect determinism and reproducibility: randomness only from `Rng` with a new stream for a new
   mechanism; every change to the world through an intervention; no allocations in hot loops.
5. An optimisation must not change results: compare `microcosm_bench` checksums of the old and new
   versions built with `-DBIO_STRICT_FP=ON`.
6. Update the documentation in the same change (see the policy in `AGENTS.md`). `tools/ci.sh` runs
   `tools/gen_being_docs.py --check` and `tools/check_doc_links.py`.
7. Open a pull request explaining what changes, why, the sources behind it and how you tested it.
   One logical change per pull request.

New libraries need approval first (open an issue): they must be GPLv3-compatible and are fetched
with `FetchContent` at a fixed version.

## Vibe coding

**Vibe coding is completely allowed.** You may write your contribution with AI assistants or coding
agents, in whole or in part. The same standard applies as for any other contribution: it builds
without warnings, the tests pass, biological data are real and sourced (verify citations yourself:
models invent them), the documentation is updated, and you understand and stand behind what you
submit. Mentioning that an AI helped is appreciated but optional.

## Licence of contributions

By contributing you agree that your contribution is licensed under the
[GNU General Public License v3.0 or later](LICENSE), the licence of the project, and you confirm that
you have the right to license it so (it is your work, or compatible free software whose licence and
attribution you keep). Assets (images, fonts, data) must carry a GPLv3-compatible licence and their
source.

## Code of conduct

This project follows the [Contributor Covenant](CODE_OF_CONDUCT.md). Be kind and constructive.
