#include "BrowserLog.h"

#include <e32base.h>
#include <e32debug.h>
#include <pthread.h>
#include <unistd.h>
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

// The SDK runtime's mimalloc gets its memory from chunks it reserves itself,
// within a fixed address budget; these report how much of it is in use.
extern "C" size_t SymbianRuntimeMimallocAddressBudget();
extern "C" size_t SymbianRuntimeMimallocReservedBytes();
extern "C" size_t SymbianRuntimeMimallocChunkBytes();
extern "C" size_t SymbianRuntimeMimallocChunkCount();

static void LogMemory()
{
    BrowserLog("mimalloc: %u chunks, %u KB reserved, %u KB committed, budget %u KB",
        static_cast<unsigned>(SymbianRuntimeMimallocChunkCount()),
        static_cast<unsigned>(SymbianRuntimeMimallocReservedBytes() / 1024),
        static_cast<unsigned>(SymbianRuntimeMimallocChunkBytes() / 1024),
        static_cast<unsigned>(SymbianRuntimeMimallocAddressBudget() / 1024));
}

static void LogThreads()
{
    char line[512];
    int used = snprintf(line, sizeof(line), "threads (process p%d):", RProcess().Priority());
    TFullName pattern = RProcess().FullName();
    pattern.Append(_L("::*"));
    TFindThread finder(pattern);
    TFullName name;
    while (finder.Next(name) == KErrNone && used < static_cast<int>(sizeof(line)) - 1) {
        RThread thread;
        if (thread.Open(finder) != KErrNone)
            continue;
        TTimeIntervalMicroSeconds cpu(0);
        thread.GetCpuTime(cpu);
        TPtrC shortName = name.Mid(name.Find(_L("::")) + 2).Left(16);
        TBuf8<16> shortName8;
        shortName8.Copy(shortName);
        // How a thread ended: killed, terminated or panicked, the category
        // (ViewSrv, KERN-EXEC, ...) and the reason.
        char exit[48] = "";
        if (thread.ExitType() != EExitPending) {
            static const char* const types[] = { "killed", "terminated", "panicked" };
            TBuf8<KMaxExitCategoryName> category;
            category.Copy(thread.ExitCategory());
            snprintf(exit, sizeof(exit), " %s %.*s %d", types[Min(static_cast<TInt>(thread.ExitType()), 2)],
                category.Length(), reinterpret_cast<const char*>(category.Ptr()), thread.ExitReason());
        }
        used += snprintf(line + used, sizeof(line) - used, " [%.*s p%d %dms%s]", shortName8.Length(),
            reinterpret_cast<const char*>(shortName8.Ptr()), thread.Priority(), static_cast<int>(cpu.Int64() / 1000), exit);
        thread.Close();
    }
    BrowserLog("%s", line);
}

static void* ThreadWatch(void*)
{
    RThread().SetPriority(EPriorityAbsoluteHigh);
    for (;;) {
        LogThreads();
        LogMemory();
        sleep(2);
    }
    return nullptr;
}

void BrowserThreadWatchStart()
{
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setstacksize(&attributes, 16 * 1024);
    pthread_t thread;
    if (!pthread_create(&thread, &attributes, ThreadWatch, nullptr))
        pthread_detach(thread);
    pthread_attr_destroy(&attributes);
}
