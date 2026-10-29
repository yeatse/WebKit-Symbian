// Native Symbian command lines contain arguments only, without argv[0].
// Preserve path backslashes. Single/double quotes group spaces; no shell escapes.
#pragma once
#include <stddef.h>
inline int parseNativeArguments(char* text, char** arguments, size_t capacity)
{
    if (capacity < 2) return -1;
    size_t count = 1;
    arguments[0] = const_cast<char*>("mmf_probe");
    char* read = text;
    char* write = text;
    while (*read) {
        while (*read == ' ' || *read == '\t') ++read;
        if (!*read) break;
        if (count + 1 >= capacity) return -1;
        arguments[count++] = write;
        char quote = 0;
        while (*read) {
            char c = *read++;
            if (quote) {
                if (c == quote) quote = 0;
                else *write++ = c;
            } else if (c == '\'' || c == '"') quote = c;
            else if (c == ' ' || c == '\t') break;
            else *write++ = c;
        }
        if (quote) return -1;
        *write++ = 0;
    }
    arguments[count] = nullptr;
    return static_cast<int>(count);
}
