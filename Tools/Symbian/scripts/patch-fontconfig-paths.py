#!/usr/bin/env python3
"""Apply the Symbian drive-path changes to fontconfig 2.13.94, idempotently."""

import argparse
from pathlib import Path


def replace_once(source, before, after):
    if after in source:
        return source
    if source.count(before) != 1:
        raise ValueError("Unexpected fontconfig source; refusing an incomplete path patch")
    return source.replace(before, after, 1)


def patch(directory):
    path = directory / "src/fcstr.c"
    source = path.read_text()
    source = replace_once(source, "#ifdef _WIN32\n    if (*s == '\\\\' ||",
                          "#if defined(_WIN32) || defined(__SYMBIAN32__)\n    if (*s == '\\\\' ||")
    source = replace_once(source, "#ifdef _WIN32\n    {\n        FcChar8     *backslash;",
                          "#if defined(_WIN32) || defined(__SYMBIAN32__)\n    {\n        FcChar8     *backslash;")
    source = replace_once(source, "#ifdef _WIN32\n/*\n * Convert '\\\\' to '/' , remove double '/'",
                          "#if defined(_WIN32) || defined(__SYMBIAN32__)\n/*\n * Convert '\\\\' to '/' , remove double '/'")
    # Recognize the previous port's two-line patch as well as pristine 2.13.94.
    source = source.replace("if (s[0] == '/' || (isalpha (s[0]) && s[1] == ':' && s[2] == '/'))",
                            "if (s[0] == '/')")
    source = replace_once(source,
        "    return FcStrCanonAbsoluteFilename (full);\n#else\n    if (s[0] == '/')",
        """    return FcStrCanonAbsoluteFilename (full);
#elif defined(__SYMBIAN32__)
    /* Symbian accepts both separators. Preserve the drive before using the
     * Unix canonicalizer, including when getcwd itself contains backslashes. */
    FcChar8 *normalized = FcStrdup (s);
    FcChar8 *full, *file;
    FcChar8 cwd[FC_MAX_FILE_LEN + 2];
    if (!normalized)
        return NULL;
    FcConvertDosPath ((char *) normalized);
    if (FcStrIsAbsoluteFilename (normalized))
        full = normalized;
    else
    {
        if (!getcwd ((char *) cwd, FC_MAX_FILE_LEN))
        {
            FcStrFree (normalized);
            return NULL;
        }
        FcConvertDosPath ((char *) cwd);
        full = FcStrBuildFilename (cwd, normalized, NULL);
        FcStrFree (normalized);
        if (!full)
            return NULL;
    }
    file = FcStrCanonAbsoluteFilename (full);
    FcStrFree (full);
    return file;
#else
    if (s[0] == '/')""")
    internal_path = directory / "src/fcint.h"
    internal = internal_path.read_text()
    internal = replace_once(internal,
        "#else\n#  define FC_SEARCH_PATH_SEPARATOR ':'",
        """#elif defined(__SYMBIAN32__)
/* Colons belong to drive letters. Keep portable '/' within each path. */
#  define FC_SEARCH_PATH_SEPARATOR ';'
#  define FC_DIR_SEPARATOR         '/'
#  define FC_DIR_SEPARATOR_S       "/"
#else
#  define FC_SEARCH_PATH_SEPARATOR ':'""")
    config_path = directory / "src/fccfg.c"
    config = config_path.read_text()
    config = replace_once(config,
        "/* make sure there's a single separator */\n#ifdef _WIN32",
        "/* make sure there's a single separator */\n#if defined(_WIN32) || defined(__SYMBIAN32__)")
    config = replace_once(config,
        '\tstrcat ((char *) path, "\\\\");\n#else\n    if ((!path[0]',
        '\tstrcat ((char *) path, FC_DIR_SEPARATOR_S);\n#else\n    if ((!path[0]')
    # Validate every edit before replacing any file.
    path.write_text(source)
    internal_path.write_text(internal)
    config_path.write_text(config)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    patch(parser.parse_args().source)
