#include "BrowserLog.h"

#include <e32base.h>
#include <e32debug.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void BrowserLogOpen(const char* aDirectory)
{
    char path[256];
    char oldPath[256];
    snprintf(path, sizeof(path), "%s/log.txt", aDirectory);
    snprintf(oldPath, sizeof(oldPath), "%s/log.old.txt", aDirectory);
    remove(oldPath);
    rename(path, oldPath);
    if (!freopen(path, "w", stderr))
        return;
    // Line buffered, so a crash loses at most the line being written.
    setvbuf(stderr, nullptr, _IOLBF, 0);
}

void BrowserLog(const char* aFormat, ...)
{
    char message[512];
    va_list arguments;
    va_start(arguments, aFormat);
    vsnprintf(message, sizeof(message), aFormat, arguments);
    va_end(arguments);

    TTime now;
    now.HomeTime();
    TDateTime time = now.DateTime();
    fprintf(stderr, "%02d:%02d:%02d.%03d %s\n", time.Hour(), time.Minute(), time.Second(),
        time.MicroSecond() / 1000, message);
    RDebug::Printf("browser: %s", message);
}
