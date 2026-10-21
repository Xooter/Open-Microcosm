#!/usr/bin/env python3
"""Revisa los links relativos de la documentación (*.md del repo): que el archivo exista y, si el
link lleva #ancla, que el encabezado exista (con la regla de anclas de GitHub). Lo corre tools/ci.sh.

  tools/check_doc_links.py
"""
import re
import sys
import unicodedata
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SKIP = ("build", ".agents", ".claude", ".git")


def anchor(heading):
    # GitHub: minúsculas, sin puntuación (salvo - y _), espacios → guiones.
    h = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", heading.strip().lower())
    h = "".join(c for c in h if c in " -_" or unicodedata.category(c)[0] in "LN")
    return h.replace(" ", "-")


def markdown_files():
    for path in ROOT.rglob("*.md"):
        rel = path.relative_to(ROOT)
        if rel.parts[0] in SKIP or "_deps" in rel.parts:
            continue
        yield path


def main():
    files = list(markdown_files())
    anchors = {}
    for path in files:
        text = path.read_text(encoding="utf-8")
        anchors[path] = {anchor(m.group(1)) for m in re.finditer(r"^#+\s+(.*)$", text, re.M)}
    problems = []
    for path in files:
        text = re.sub(r"```.*?```", "", path.read_text(encoding="utf-8"), flags=re.S)
        for m in re.finditer(r"\]\(([^)\s]+)\)", text):
            target = m.group(1)
            if re.match(r"[a-z]+:", target):
                continue
            file_part, _, anc = target.partition("#")
            dest = (path.parent / file_part).resolve() if file_part else path
            rel = path.relative_to(ROOT)
            if not dest.exists():
                problems.append(f"{rel}: no existe {target}")
            elif anc and dest.suffix == ".md" and anc not in anchors.get(dest, set()):
                problems.append(f"{rel}: no existe el ancla {target}")
    for p in problems:
        print(p, file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
