# Security policy

Open Microcosm is a desktop application and a headless library. It has no network service and no
accounts. Its attack surface is the **files it reads**, which users exchange with each other:
content packs (`.biopack` and pack folders), plates (`.plate`), scenes, culture scenes
(`.culture`), stains (`.stain`), experiments and lessons (TOML), and the options and layout files in
the user data folder.

## Supported versions

| Version | Supported |
|---|---|
| Latest release | ✅ |
| `main` branch | ✅ |
| Older releases | ❌ (please update) |

## Reporting a vulnerability

**Do not open a public issue.** Report it privately through GitHub:
*Security → Report a vulnerability* in this repository
([direct link](https://github.com/Xooter/Open-Microcosm/security/advisories/new)).

Please include:

- the version or commit (`Help > About`, or `git rev-parse --short HEAD`) and the platform;
- the file that triggers it, or the steps to build one, and how it was opened (Content panel import,
  *Load plate*, `microcosm_bench --paquete`, …);
- what happens (crash, memory corruption, a file written outside the data folder, a hang, …) and,
  if you have one, a sanitizer trace (build with `-fsanitize=address,undefined`).

This is a volunteer project. We aim to acknowledge reports within **7 days** and to agree on a fix and
disclosure date with you. Fixes are released as a new version and published as a GitHub security
advisory, crediting the reporter unless you prefer otherwise.

## In scope

- Memory-safety bugs (out-of-bounds access, use after free, overflows) triggered by a crafted file.
- Writing, overwriting or deleting files outside `<data>/openmicrocosm/`, for example through ids or
  paths in a pack, plate or scene. Ids are limited to `[a-z0-9_]`, so a bypass of that limit
  counts.
- Bypassing the content-pack limits (8 MB, ids) to exhaust memory or disk, or to hang the program
  when a file is loaded.
- Problems in the released packages themselves (for example a binary that loads libraries from an
  unexpected location).

## Out of scope

- Results that differ from the biology, wrong data or missing sources. Those are model bugs: open a
  normal issue with the bug template.
- Crashes caused by editing the program's own files in its install folder, or by running it with
  modified binaries.
- Slow simulations caused by large but legitimate settings (huge populations, many steps per frame).
- Vulnerabilities in third-party libraries (raylib, GLFW, Dear ImGui, ImPlot, EnTT, toml++) that
  do not affect Open Microcosm. Report those upstream. If one does affect us, report it here as well
  so we can update the pinned version.
