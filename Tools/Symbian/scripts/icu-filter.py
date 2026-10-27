#!/usr/bin/env python3
"""Lists the ICU data items JavaScriptCore does not use, for icupkg --remove.

Usage: icu-filter.py LIST [LANG ...]

LIST is the output of `icupkg -l`. Charset converters, character names,
transliterators, spell-out rules, spoof checking, IDNA and StringPrep data are
always dropped. With LANG arguments, locale data for every other language is
dropped as well; build-icu.sh then rewrites each tree's res_index to match.
"""
import sys

TREES = ("", "brkitr/", "coll/", "curr/", "lang/", "region/", "unit/", "zone/")
# Lowercase items in the root tree that are tables rather than locales.
NOT_LOCALES = {"root", "pool", "res_index", "plurals", "metadata", "icustd", "icuver", "zoneinfo64"}


def unused(name):
    return (name.endswith((".cnv", ".spp", ".cfu")) or name in ("unames.icu", "uts46.nrm")
            or name.startswith(("translit/", "rbnf/")))


def locale_of(name):
    tree, _, base = name.rpartition("/")
    tree = tree + "/" if tree else ""
    if tree not in TREES or not base.endswith(".res"):
        return None, None
    locale = base[:-4]
    language = locale.split("_")[0]
    if locale in NOT_LOCALES or not (language.isalpha() and language.islower()):
        return None, None
    return tree, locale


def main():
    keep = set(sys.argv[2:])
    for name in (line.strip() for line in open(sys.argv[1])):
        if not name:
            continue
        if unused(name):
            print(name)
            continue
        tree, locale = locale_of(name)
        if tree is not None and keep and locale.split("_")[0] not in keep:
            print(name)


main()
