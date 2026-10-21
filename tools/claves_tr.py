#!/usr/bin/env python3
"""Claves de traducción de Open Microcosm (fase P1, paso 9).

Junta los textos que la interfaz pasa por `ui::tr` (las claves son el texto en español): los
literales de src/ui y src/app, los rótulos del registro de parámetros y de la ficha, y los textos de
los paquetes de fábrica (seres, medios, recipientes, placas, biomas, minerales, lecciones, glosario).

    tools/claves_tr.py                     lista las claves (una por línea)
    tools/claves_tr.py --check assets/lang/en.txt
        avisa las claves sin traducir y las traducciones que no conservan los especificadores de
        formato (%d, %.1f, %s...): esas romperían el texto. Sale con 1 si hay alguna de estas.

Para encontrar lo que se arma en tiempo de ejecución: BIO_TR_FALTANTES=faltantes.txt ./microcosm, recorrer
los paneles y cerrar; quedan en el archivo las que `tr` no encontró.
"""
import glob
import os
import re
import sys

try:
    import tomllib
except ImportError:  # Python < 3.11
    tomllib = None

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')
SPEC = re.compile(r"%[-+#0]*[0-9*]*(?:\.[0-9*]+)?(?:hh|h|ll|l|z|j|t)?[diouxXeEfgGcspn%]")
SOURCES = ["src/ui/**/*.cpp", "src/app/*.cpp", "src/game/parameters.cpp", "src/game/mass_budget.cpp",
           "src/bio/metabolism.cpp", "src/game/inspect.cpp", "src/game/encyclopedia.cpp", "src/game/scale_path.cpp",
           "src/game/stain.cpp", "src/game/biome.cpp"]
# Campos de texto de los paquetes que se muestran.
TEXT_FIELDS = {"name", "description", "title", "goal", "text", "explanation", "reason", "definition",
               "level", "term"}
LIST_FIELDS = {"options", "alias"}


def unescape(s):
    return s.replace('\\"', '"').replace("\\n", "\n").replace("\\\\", "\\")


def looks_like_text(s):
    if len(s) < 2 or not re.search(r"[A-Za-zÁÉÍÓÚáéíóúñÑ]{2}", s):
        return False
    if s.startswith("##") or re.fullmatch(r"[a-z0-9_.:/\-]+", s):  # ids, claves, rutas
        return False
    if re.fullmatch(r"[%#][^ ]*", s):
        return False
    return True


def code_keys():
    keys = set()
    for pattern in SOURCES:
        for path in glob.glob(os.path.join(ROOT, pattern), recursive=True):
            text = open(path, encoding="utf-8").read()
            text = re.sub(r"//[^\n]*", "", text)
            # Literales seguidos en líneas distintas: C++ los concatena (la clave es el texto entero).
            text = re.sub(r'"[ \t]*\n[ \t]*"', "", text)
            for m in LITERAL.finditer(text):
                s = unescape(m.group(1))
                if "###" in s:
                    s = s.split("###")[0]
                if looks_like_text(s):
                    keys.add(s)
    return keys


def walk(node, keys):
    if isinstance(node, dict):
        for k, v in node.items():
            if k in TEXT_FIELDS and isinstance(v, str) and looks_like_text(v):
                keys.add(v)
            elif k in LIST_FIELDS and isinstance(v, list):
                keys.update(x for x in v if isinstance(x, str) and looks_like_text(x))
            else:
                walk(v, keys)
    elif isinstance(node, list):
        for x in node:
            walk(x, keys)


def data_keys():
    keys = set()
    if tomllib is None:
        return keys
    for path in glob.glob(os.path.join(ROOT, "assets/content/base/**/*.toml"), recursive=True):
        with open(path, "rb") as f:
            walk(tomllib.load(f), keys)
    return keys


def load(path):
    table = {}
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line or line.startswith("#") or "\t" not in line:
            continue
        k, v = line.split("\t", 1)
        table[k.replace("\\n", "\n")] = v.replace("\\n", "\n")
    return table


def main():
    keys = code_keys() | data_keys()
    if len(sys.argv) >= 3 and sys.argv[1] == "--check":
        table = load(sys.argv[2])
        bad = [k for k, v in table.items() if SPEC.findall(k) != SPEC.findall(v)]
        missing = sorted(k for k in keys if k not in table)
        for k in bad:
            print(f"FORMATO DISTINTO: {k!r} -> {table[k]!r}")
        print(f"{len(keys)} claves, {len(table)} traducidas, {len(missing)} sin traducir, {len(bad)} con otro formato")
        if "--faltantes" in sys.argv:
            for k in missing:
                print(k.replace("\n", "\\n"))
        sys.exit(1 if bad else 0)
    for k in sorted(keys):
        print(k.replace("\n", "\\n"))


main()
